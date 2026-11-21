#include "Rendering/SceneRenderer.h"

#include "Object/ObjectGlobals.h"
#include "Rendering/CameraView.h"
#include "Rendering/DrawConstants.h"
#include "Rendering/Material.h"
#include "Rendering/RenderDevice.h"
#include "Rendering/RenderTarget.h"
#include "Rendering/RenderTargetImplementation.h"
#include "Rendering/SceneRendererImplementation.h"
#include "Rendering/ShaderLibrary.h"
#include "Rendering/StaticMesh.h"
#include "Rendering/StaticMeshComponent.h"
#include "Rendering/StaticMeshVertex.h"
#include "Scene/Component.h"
#include "Scene/Entity.h"
#include "Scene/Scene.h"

#include <nvrhi/nvrhi.h>

#include <cstddef>
#include <format>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

// Vertices are copied to the graphics card as they are, and the pipeline reads them at these offsets.
static_assert(sizeof(FStaticMeshVertex) == 24, "FStaticMeshVertex must be a position and a normal without padding");

namespace
{
	/** What a component is drawn with when it has no material: plain white. */
	constexpr FLinearColor DefaultBaseColor(1.0f, 1.0f, 1.0f, 1.0f);

	/** With reversed depth the farthest possible value is zero, and a surface hides another if its depth is greater. */
	constexpr float FarthestDepth = 0.0f;

	/** One mesh to draw. Only valid during the call of Render that collected it. */
	struct FMeshDraw
	{
		WStaticMesh* Mesh = nullptr;
		FDrawConstants Constants;
	};

	/** A mesh whose geometry has to be copied to the graphics card before it can be drawn. */
	struct FMeshUpload
	{
		WStaticMesh* Mesh = nullptr;
		FStaticMeshBuffers Buffers;
	};

	nvrhi::ShaderHandle CreateShader(nvrhi::IDevice& Device, std::string_view Name, nvrhi::ShaderType Type)
	{
		const std::span<const uint32> Code = FShaderLibrary::GetCode(Name);

		nvrhi::ShaderDesc Description;
		Description.shaderType = Type;
		Description.debugName = std::string(Name);
		return Device.createShader(Description, Code.data(), Code.size_bytes());
	}

	/** Collects every mesh of the scene that is to be drawn, with the matrix that places it on the screen. */
	std::vector<FMeshDraw> CollectMeshDraws(const WScene& Scene, const FCameraView& View, float AspectRatio)
	{
		const FMatrix4f ViewToClip = View.GetProjectionMatrix(AspectRatio) * View.GetViewMatrix();

		/** An entity that is still to be visited, with the world transform of its parent. */
		struct FPendingEntity
		{
			const WEntity* Entity = nullptr;
			FTransform ParentTransform;
		};

		std::vector<FPendingEntity> Pending;
		for (const TObjectPtr<WEntity>& Root : Scene.GetRootEntities())
		{
			Pending.push_back({Root.Get(), FTransform()});
		}

		std::vector<FMeshDraw> Draws;
		while (!Pending.empty())
		{
			const FPendingEntity Current = Pending.back();
			Pending.pop_back();
			if (!IsValid(Current.Entity))
			{
				continue;
			}

			const FTransform WorldTransform =
				FTransform::Combine(Current.ParentTransform, Current.Entity->GetRelativeTransform());
			for (const TObjectPtr<WEntity>& Child : Current.Entity->GetChildren())
			{
				Pending.push_back({Child.Get(), WorldTransform});
			}

			for (const TObjectPtr<WComponent>& Component : Current.Entity->GetComponents())
			{
				const WStaticMeshComponent* const MeshComponent = Cast<WStaticMeshComponent>(Component.Get());
				const bool bIsShown = IsValid(MeshComponent) && MeshComponent->bVisible;
				if (!bIsShown)
				{
					continue;
				}

				WStaticMesh* const Mesh = MeshComponent->Mesh.Get();
				if (!IsValid(Mesh) || !Mesh->HasGeometry())
				{
					continue;
				}

				const WMaterial* const Material = MeshComponent->Material.Get();

				FMeshDraw Draw;
				Draw.Mesh = Mesh;
				Draw.Constants.LocalToClip = ViewToClip * View.GetCameraRelativeMatrix(WorldTransform);
				Draw.Constants.BaseColor = IsValid(Material) ? Material->BaseColor : DefaultBaseColor;
				Draws.push_back(Draw);
			}
		}

		return Draws;
	}

	/** Creates the buffers for a mesh's geometry on the graphics card. Their content is written separately. */
	bool CreateMeshBuffers(nvrhi::IDevice& Device, WStaticMesh& Mesh, FStaticMeshBuffers& OutBuffers)
	{
		nvrhi::BufferDesc VertexDescription;
		VertexDescription.byteSize = Mesh.GetVertices().size() * sizeof(FStaticMeshVertex);
		VertexDescription.isVertexBuffer = true;
		VertexDescription.debugName = "StaticMeshVertices";
		VertexDescription.initialState = nvrhi::ResourceStates::VertexBuffer;
		VertexDescription.keepInitialState = true;

		nvrhi::BufferDesc IndexDescription;
		IndexDescription.byteSize = Mesh.GetIndices().size() * sizeof(uint32);
		IndexDescription.isIndexBuffer = true;
		IndexDescription.debugName = "StaticMeshIndices";
		IndexDescription.initialState = nvrhi::ResourceStates::IndexBuffer;
		IndexDescription.keepInitialState = true;

		OutBuffers.Mesh = &Mesh;
		OutBuffers.GeometryVersion = Mesh.GetGeometryVersion();
		OutBuffers.IndexCount = static_cast<uint32>(Mesh.GetIndices().size());
		OutBuffers.VertexBuffer = Device.createBuffer(VertexDescription);
		OutBuffers.IndexBuffer = Device.createBuffer(IndexDescription);
		return OutBuffers.VertexBuffer != nullptr && OutBuffers.IndexBuffer != nullptr;
	}
}

std::unique_ptr<FSceneRenderer> FSceneRenderer::Create(FRenderDevice& Device, std::string& OutError)
{
	std::unique_ptr<FImplementation> NewImplementation = std::make_unique<FImplementation>();
	NewImplementation->Context = Device.Context;
	nvrhi::IDevice& RenderInterface = *NewImplementation->Context->GetDevice();

	const nvrhi::ShaderHandle VertexShader =
		CreateShader(RenderInterface, "StaticMesh.vert", nvrhi::ShaderType::Vertex);
	const nvrhi::ShaderHandle UnlitShader = CreateShader(RenderInterface, "Unlit.frag", nvrhi::ShaderType::Pixel);
	if (VertexShader == nullptr || UnlitShader == nullptr)
	{
		OutError = "the graphics card did not accept the engine's shaders";
		return nullptr;
	}

	// The attributes are numbered in this order, which is the order of the inputs of StaticMesh.vert.
	nvrhi::VertexAttributeDesc PositionAttribute;
	PositionAttribute.name = "Position";
	PositionAttribute.format = nvrhi::Format::RGB32_FLOAT;
	PositionAttribute.offset = static_cast<uint32>(offsetof(FStaticMeshVertex, Position));
	PositionAttribute.elementStride = static_cast<uint32>(sizeof(FStaticMeshVertex));
	const nvrhi::InputLayoutHandle InputLayout = RenderInterface.createInputLayout(&PositionAttribute, 1, VertexShader);

	const uint32 ConstantsSize = static_cast<uint32>(sizeof(FDrawConstants));
	NewImplementation->BindingLayout =
		RenderInterface.createBindingLayout(nvrhi::BindingLayoutDesc()
												.setVisibility(nvrhi::ShaderType::Vertex | nvrhi::ShaderType::Pixel)
												.addItem(nvrhi::BindingLayoutItem::PushConstants(0, ConstantsSize)));
	if (InputLayout == nullptr || NewImplementation->BindingLayout == nullptr)
	{
		OutError = "the graphics card could not create the layout of the renderer's vertices and constants";
		return nullptr;
	}

	NewImplementation->BindingSet = RenderInterface.createBindingSet(
		nvrhi::BindingSetDesc().addItem(nvrhi::BindingSetItem::PushConstants(0, ConstantsSize)),
		NewImplementation->BindingLayout);

	nvrhi::GraphicsPipelineDesc PipelineDescription;
	PipelineDescription.primType = nvrhi::PrimitiveType::TriangleList;
	PipelineDescription.inputLayout = InputLayout;
	PipelineDescription.VS = VertexShader;
	PipelineDescription.PS = UnlitShader;
	PipelineDescription.bindingLayouts.push_back(NewImplementation->BindingLayout);
	// Triangles face the viewer when their corners run counter-clockwise, and the backs are not drawn.
	PipelineDescription.renderState.rasterState.cullMode = nvrhi::RasterCullMode::Back;
	PipelineDescription.renderState.rasterState.frontCounterClockwise = true;
	PipelineDescription.renderState.rasterState.depthClipEnable = true;
	PipelineDescription.renderState.depthStencilState.depthTestEnable = true;
	PipelineDescription.renderState.depthStencilState.depthWriteEnable = true;
	PipelineDescription.renderState.depthStencilState.depthFunc = nvrhi::ComparisonFunc::GreaterOrEqual;

	// Every render target has these formats, so one pipeline serves them all.
	nvrhi::FramebufferInfo TargetFormats;
	TargetFormats.addColorFormat(FRenderTarget::FImplementation::ColorFormat);
	TargetFormats.setDepthFormat(FRenderTarget::FImplementation::DepthFormat);

	NewImplementation->UnlitPipeline = RenderInterface.createGraphicsPipeline(PipelineDescription, TargetFormats);
	if (NewImplementation->BindingSet == nullptr || NewImplementation->UnlitPipeline == nullptr)
	{
		OutError = "the graphics card could not create the pipeline that draws meshes";
		return nullptr;
	}

	return std::make_unique<FSceneRenderer>(std::move(NewImplementation));
}

FSceneRenderer::FSceneRenderer(std::unique_ptr<FImplementation> InImplementation)
	: Implementation(std::move(InImplementation))
{
}

FSceneRenderer::~FSceneRenderer() = default;

bool FSceneRenderer::Render(const WScene& Scene, const FCameraView& View, FRenderTarget& Target, std::string& OutError)
{
	if (!View.IsValid())
	{
		OutError = "the camera view is not valid: its location and rotation must be finite, its rotation a unit "
				   "quaternion, its field of view between 1 and 179 degrees and its near plane greater than zero";
		return false;
	}
	if (!IsValid(&Scene))
	{
		OutError = "the scene has been destroyed";
		return false;
	}

	FRenderTarget::FImplementation& TargetData = *Target.Implementation;
	WW_CHECKF(TargetData.Context == Implementation->Context,
		"A render target can only be drawn to by a renderer of the render device that created it");

	// The copy of a mesh on the graphics card is released once the mesh itself is gone.
	std::erase_if(Implementation->MeshBuffers,
		[](const std::pair<const WStaticMesh* const, FStaticMeshBuffers>& Entry)
		{ return Entry.second.Mesh.Get() == nullptr; });

	const float AspectRatio = static_cast<float>(TargetData.Width) / static_cast<float>(TargetData.Height);
	const std::vector<FMeshDraw> Draws = CollectMeshDraws(Scene, View, AspectRatio);

	// Everything that can fail happens before the first command is recorded, so that a failure changes nothing.
	nvrhi::IDevice& Device = *Implementation->Context->GetDevice();
	std::vector<FMeshUpload> Uploads;
	for (const FMeshDraw& Draw : Draws)
	{
		const auto Existing = Implementation->MeshBuffers.find(Draw.Mesh);
		const bool bIsCurrent = Existing != Implementation->MeshBuffers.end() &&
			Existing->second.GeometryVersion == Draw.Mesh->GetGeometryVersion();
		bool bIsPending = false;
		for (const FMeshUpload& Upload : Uploads)
		{
			bIsPending = bIsPending || Upload.Mesh == Draw.Mesh;
		}
		if (bIsCurrent || bIsPending)
		{
			continue;
		}

		FMeshUpload Upload;
		Upload.Mesh = Draw.Mesh;
		if (!CreateMeshBuffers(Device, *Draw.Mesh, Upload.Buffers))
		{
			OutError = std::format("the graphics card has no room for a mesh of {} vertices and {} triangles",
				Draw.Mesh->GetVertices().size(),
				Draw.Mesh->GetTriangleCount());
			return false;
		}
		Uploads.push_back(std::move(Upload));
	}

	nvrhi::ICommandList& CommandList = *Implementation->Context->GetImmediateCommandList();
	CommandList.open();

	for (FMeshUpload& Upload : Uploads)
	{
		const std::vector<FStaticMeshVertex>& Vertices = Upload.Mesh->GetVertices();
		const std::vector<uint32>& Indices = Upload.Mesh->GetIndices();
		CommandList.writeBuffer(
			Upload.Buffers.VertexBuffer, Vertices.data(), Vertices.size() * sizeof(FStaticMeshVertex));
		CommandList.writeBuffer(Upload.Buffers.IndexBuffer, Indices.data(), Indices.size() * sizeof(uint32));
		Implementation->MeshBuffers.insert_or_assign(Upload.Mesh, std::move(Upload.Buffers));
	}

	CommandList.clearTextureFloat(TargetData.Texture,
		nvrhi::AllSubresources,
		nvrhi::Color(BackgroundColor.R, BackgroundColor.G, BackgroundColor.B, BackgroundColor.A));
	const bool bClearDepth = true;
	const bool bClearStencil = false;
	CommandList.clearDepthStencilTexture(
		TargetData.DepthTexture, nvrhi::AllSubresources, bClearDepth, FarthestDepth, bClearStencil, 0);

	nvrhi::GraphicsState State;
	State.pipeline = Implementation->UnlitPipeline;
	State.framebuffer = TargetData.Framebuffer;
	State.viewport.addViewportAndScissorRect(
		nvrhi::Viewport(static_cast<float>(TargetData.Width), static_cast<float>(TargetData.Height)));
	State.bindings.push_back(Implementation->BindingSet);
	State.vertexBuffers.resize(1);

	for (const FMeshDraw& Draw : Draws)
	{
		const FStaticMeshBuffers& Buffers = Implementation->MeshBuffers.at(Draw.Mesh);
		State.vertexBuffers[0] = nvrhi::VertexBufferBinding().setBuffer(Buffers.VertexBuffer).setSlot(0).setOffset(0);
		State.indexBuffer =
			nvrhi::IndexBufferBinding().setBuffer(Buffers.IndexBuffer).setFormat(nvrhi::Format::R32_UINT).setOffset(0);

		CommandList.setGraphicsState(State);
		CommandList.setPushConstants(&Draw.Constants, sizeof(FDrawConstants));
		CommandList.drawIndexed(nvrhi::DrawArguments().setVertexCount(Buffers.IndexCount));
	}

	CommandList.close();
	Device.executeCommandList(&CommandList);
	Device.runGarbageCollection();

	Implementation->Statistics.DrawnMeshCount = static_cast<int32>(Draws.size());
	Implementation->Statistics.UploadedMeshCount = static_cast<int32>(Uploads.size());
	Implementation->Statistics.CachedMeshCount = static_cast<int32>(Implementation->MeshBuffers.size());
	return true;
}

const FSceneRenderStatistics& FSceneRenderer::GetStatistics() const
{
	return Implementation->Statistics;
}
