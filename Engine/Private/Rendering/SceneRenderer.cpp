#include "Rendering/SceneRenderer.h"

#include "Object/ObjectGlobals.h"
#include "Rendering/CameraView.h"
#include "Rendering/DirectionalLightComponent.h"
#include "Rendering/DrawData.h"
#include "Rendering/Material.h"
#include "Rendering/PointLightComponent.h"
#include "Rendering/RenderDevice.h"
#include "Rendering/RenderTarget.h"
#include "Rendering/RenderTargetImplementation.h"
#include "Rendering/SceneRendererImplementation.h"
#include "Rendering/ShaderLibrary.h"
#include "Rendering/StaticMesh.h"
#include "Rendering/StaticMeshComponent.h"
#include "Rendering/StaticMeshVertex.h"
#include "Rendering/ViewConstants.h"
#include "Scene/Component.h"
#include "Scene/Entity.h"
#include "Scene/Scene.h"

#include <glm/geometric.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/vec4.hpp>
#include <nvrhi/nvrhi.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <format>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

// Vertices are copied to the graphics card as they are, and the pipelines read them at these offsets.
static_assert(sizeof(FStaticMeshVertex) == 24, "FStaticMeshVertex must be a position and a normal without padding");

namespace
{
	/** With reversed depth the farthest possible value is zero, and a surface hides another if its depth is greater. */
	constexpr float FarthestDepth = 0.0f;

	/**
	 * The most light the buffer a scene is lit in can hold: just under the largest 16-bit floating-point number.
	 * Anything brighter is this bright. The same as MaxSceneLight in Engine/Shaders/SceneData.glsl.
	 */
	constexpr float MaxSceneLight = 65000.0f;

	/** The fewest meshes the buffer of per-mesh data has room for. It doubles whenever a scene needs more. */
	constexpr int32 MinDrawBufferCapacity = 64;

	/** How a component without a material is drawn: as a new WMaterial would look. A test holds the two together. */
	constexpr FLinearColor DefaultBaseColor(1.0f, 1.0f, 1.0f, 1.0f);
	constexpr FLinearColor DefaultEmissiveColor(0.0f, 0.0f, 0.0f, 1.0f);
	constexpr float DefaultMetallic = 0.0f;
	constexpr float DefaultRoughness = 0.5f;

	/** The way every object faces in its own space: along -Z. A light shines this way. */
	const FVector3f ForwardDirection(0.0f, 0.0f, -1.0f);

	/** Everything of a scene that one picture needs, collected in one walk over its entities. */
	struct FSceneContents
	{
		/** The meshes to draw. DrawData holds the per-mesh data of each, at the same index. */
		std::vector<WStaticMesh*> Meshes;
		std::vector<FDrawData> DrawData;

		/** The lights, as the shaders receive them. */
		FViewConstants ViewConstants;

		/** How many lights were left out because the scene has more than the renderer supports. */
		int32 IgnoredLightCount = 0;
	};

	/** A mesh whose geometry has to be copied to the graphics card before it can be drawn. */
	struct FMeshUpload
	{
		WStaticMesh* Mesh = nullptr;
		FStaticMeshBuffers Buffers;
	};

	glm::vec4 ToVector(const FLinearColor& Color, float Scale)
	{
		return glm::vec4(Color.R * Scale, Color.G * Scale, Color.B * Scale, 0.0f);
	}

	/**
	 * Returns the matrix that takes a normal from an object's own space to world axes. Where positions are scaled,
	 * normals are scaled by the inverse, which keeps them perpendicular to a surface that is stretched.
	 */
	FMatrix4f MakeNormalMatrix(const FTransform& WorldTransform)
	{
		const FVector3f& Scale = WorldTransform.Scale;
		const FVector3f InverseScale(Scale.x != 0.0f ? 1.0f / Scale.x : 0.0f,
			Scale.y != 0.0f ? 1.0f / Scale.y : 0.0f,
			Scale.z != 0.0f ? 1.0f / Scale.z : 0.0f);

		FMatrix4f Matrix = glm::mat4_cast(WorldTransform.Rotation);
		Matrix[0] *= InverseScale.x;
		Matrix[1] *= InverseScale.y;
		Matrix[2] *= InverseScale.z;
		return Matrix;
	}

	/** Adds the draw of a mesh component to Contents, if the component shows anything. */
	void CollectMesh(const WStaticMeshComponent& Component,
		const FTransform& WorldTransform,
		const FCameraView& View,
		const FMatrix4f& RelativeToClip,
		FSceneContents& Contents)
	{
		WStaticMesh* const Mesh = Component.Mesh.Get();
		const bool bIsShown = Component.bVisible && IsValid(Mesh) && Mesh->HasGeometry();
		if (!bIsShown)
		{
			return;
		}

		FDrawData Data;
		Data.LocalToRelative = View.GetCameraRelativeMatrix(WorldTransform);
		Data.LocalToClip = RelativeToClip * Data.LocalToRelative;
		Data.NormalToRelative = MakeNormalMatrix(WorldTransform);

		const WMaterial* const Material = Component.Material.Get();
		if (IsValid(Material))
		{
			Data.BaseColor = Material->BaseColor;
			Data.EmissiveColor = Material->EmissiveColor;
			Data.MetallicRoughness = glm::vec4(Material->Metallic, Material->Roughness, 0.0f, 0.0f);
		}
		else
		{
			Data.BaseColor = DefaultBaseColor;
			Data.EmissiveColor = DefaultEmissiveColor;
			Data.MetallicRoughness = glm::vec4(DefaultMetallic, DefaultRoughness, 0.0f, 0.0f);
		}

		Contents.Meshes.push_back(Mesh);
		Contents.DrawData.push_back(Data);
	}

	/** Adds a directional light to Contents, if it shines and there is room for it. */
	void CollectDirectionalLight(
		const WDirectionalLightComponent& Light, const FTransform& WorldTransform, FSceneContents& Contents)
	{
		const bool bShines = Light.bVisible && std::isfinite(Light.Intensity) && Light.Intensity > 0.0f;
		if (!bShines)
		{
			return;
		}

		const int32 Index = Contents.ViewConstants.LightCounts.x;
		if (Index == FSceneRenderer::MaxDirectionalLights)
		{
			++Contents.IgnoredLightCount;
			return;
		}

		const FVector3f Direction = glm::normalize(WorldTransform.Rotation * ForwardDirection);
		FDirectionalLightData& Data = Contents.ViewConstants.DirectionalLights[static_cast<std::size_t>(Index)];
		Data.DirectionToLight = glm::vec4(-Direction, 0.0f);
		Data.Illuminance = ToVector(Light.Color, Light.Intensity);
		Contents.ViewConstants.LightCounts.x = Index + 1;
	}

	/** Adds a point light to Contents, if it shines and there is room for it. */
	void CollectPointLight(const WPointLightComponent& Light,
		const FTransform& WorldTransform,
		const FCameraView& View,
		FSceneContents& Contents)
	{
		const bool bShines = Light.bVisible && std::isfinite(Light.Intensity) && Light.Intensity > 0.0f;
		if (!bShines)
		{
			return;
		}

		const int32 Index = Contents.ViewConstants.LightCounts.y;
		if (Index == FSceneRenderer::MaxPointLights)
		{
			++Contents.IgnoredLightCount;
			return;
		}

		// Like every position, the light's is taken relative to the camera in double precision.
		const FVector3f RelativeLocation = FVector3f(WorldTransform.Location - View.Location);
		const float Range = std::isfinite(Light.Range) ? std::max(Light.Range, 0.0f) : 0.0f;
		FPointLightData& Data = Contents.ViewConstants.PointLights[static_cast<std::size_t>(Index)];
		Data.LocationAndRange = glm::vec4(RelativeLocation, Range);
		Data.Intensity = ToVector(Light.Color, Light.Intensity);
		Contents.ViewConstants.LightCounts.y = Index + 1;
	}

	/**
	 * Walks the entities of a scene once and collects what a picture of it needs.
	 *
	 * @param bCollectLights False for a picture without lighting, which leaves the lights out.
	 */
	FSceneContents CollectScene(const WScene& Scene, const FCameraView& View, float AspectRatio, bool bCollectLights)
	{
		const FMatrix4f RelativeToClip = View.GetProjectionMatrix(AspectRatio) * View.GetViewMatrix();

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

		FSceneContents Contents;
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
				if (!IsValid(Component.Get()))
				{
					continue;
				}

				if (const WStaticMeshComponent* const Mesh = Cast<WStaticMeshComponent>(Component.Get()))
				{
					CollectMesh(*Mesh, WorldTransform, View, RelativeToClip, Contents);
				}
				else if (!bCollectLights)
				{
					continue;
				}
				else if (const WDirectionalLightComponent* const Sun =
							 Cast<WDirectionalLightComponent>(Component.Get()))
				{
					CollectDirectionalLight(*Sun, WorldTransform, Contents);
				}
				else if (const WPointLightComponent* const Lamp = Cast<WPointLightComponent>(Component.Get()))
				{
					CollectPointLight(*Lamp, WorldTransform, View, Contents);
				}
			}
		}

		return Contents;
	}

	nvrhi::ShaderHandle CreateShader(nvrhi::IDevice& Device, std::string_view Name, nvrhi::ShaderType Type)
	{
		const std::span<const uint32> Code = FShaderLibrary::GetCode(Name);

		nvrhi::ShaderDesc Description;
		Description.shaderType = Type;
		Description.debugName = std::string(Name);
		return Device.createShader(Description, Code.data(), Code.size_bytes());
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

	/**
	 * Creates a pipeline that draws static meshes with the given fragment shader, with depth testing and without
	 * the backs of triangles.
	 *
	 * @param ColorFormat The format of the surface the pipeline draws on.
	 * @param DepthFormat The format of the depth buffer that goes with it.
	 */
	nvrhi::GraphicsPipelineHandle CreateMeshPipeline(nvrhi::IDevice& Device,
		nvrhi::IInputLayout* InputLayout,
		nvrhi::IShader* VertexShader,
		nvrhi::IShader* FragmentShader,
		nvrhi::IBindingLayout* BindingLayout,
		nvrhi::Format ColorFormat,
		nvrhi::Format DepthFormat)
	{
		nvrhi::GraphicsPipelineDesc Description;
		Description.primType = nvrhi::PrimitiveType::TriangleList;
		Description.inputLayout = InputLayout;
		Description.VS = VertexShader;
		Description.PS = FragmentShader;
		Description.bindingLayouts.push_back(BindingLayout);
		// Triangles face the viewer when their corners run counter-clockwise, and the backs are not drawn.
		Description.renderState.rasterState.cullMode = nvrhi::RasterCullMode::Back;
		Description.renderState.rasterState.frontCounterClockwise = true;
		Description.renderState.rasterState.depthClipEnable = true;
		Description.renderState.depthStencilState.depthTestEnable = true;
		Description.renderState.depthStencilState.depthWriteEnable = true;
		Description.renderState.depthStencilState.depthFunc = nvrhi::ComparisonFunc::GreaterOrEqual;

		nvrhi::FramebufferInfo TargetFormats;
		TargetFormats.addColorFormat(ColorFormat);
		TargetFormats.setDepthFormat(DepthFormat);
		return Device.createGraphicsPipeline(Description, TargetFormats);
	}

	/**
	 * Creates the pipeline that fits the lit scene to the display: one triangle over the whole surface.
	 *
	 * @param OutputFormat The format of the surface the pipeline draws on.
	 */
	nvrhi::GraphicsPipelineHandle CreateTonemapPipeline(
		nvrhi::IDevice& Device, nvrhi::IBindingLayout* BindingLayout, nvrhi::Format OutputFormat)
	{
		const nvrhi::ShaderHandle VertexShader = CreateShader(Device, "FullScreen.vert", nvrhi::ShaderType::Vertex);
		const nvrhi::ShaderHandle FragmentShader = CreateShader(Device, "Tonemap.frag", nvrhi::ShaderType::Pixel);
		if (VertexShader == nullptr || FragmentShader == nullptr)
		{
			return nullptr;
		}

		nvrhi::GraphicsPipelineDesc Description;
		Description.primType = nvrhi::PrimitiveType::TriangleList;
		Description.VS = VertexShader;
		Description.PS = FragmentShader;
		Description.bindingLayouts.push_back(BindingLayout);
		Description.renderState.rasterState.cullMode = nvrhi::RasterCullMode::None;
		Description.renderState.depthStencilState.depthTestEnable = false;
		Description.renderState.depthStencilState.depthWriteEnable = false;

		nvrhi::FramebufferInfo TargetFormats;
		TargetFormats.addColorFormat(OutputFormat);
		return Device.createGraphicsPipeline(Description, TargetFormats);
	}
}

std::unique_ptr<FSceneRenderer> FSceneRenderer::Create(FRenderDevice& Device, std::string& OutError)
{
	std::unique_ptr<FImplementation> NewImplementation = std::make_unique<FImplementation>();
	NewImplementation->Context = Device.Context;
	nvrhi::IDevice& RenderInterface = *NewImplementation->Context->GetDevice();

	const nvrhi::ShaderHandle VertexShader =
		CreateShader(RenderInterface, "StaticMesh.vert", nvrhi::ShaderType::Vertex);
	const nvrhi::ShaderHandle LitShader = CreateShader(RenderInterface, "Lit.frag", nvrhi::ShaderType::Pixel);
	const nvrhi::ShaderHandle UnlitShader = CreateShader(RenderInterface, "Unlit.frag", nvrhi::ShaderType::Pixel);
	if (VertexShader == nullptr || LitShader == nullptr || UnlitShader == nullptr)
	{
		OutError = "the graphics card did not accept the engine's shaders";
		return nullptr;
	}

	// The attributes are numbered in this order, which is the order of the inputs of StaticMesh.vert.
	std::array<nvrhi::VertexAttributeDesc, 2> Attributes;
	Attributes[0].name = "Position";
	Attributes[0].format = nvrhi::Format::RGB32_FLOAT;
	Attributes[0].offset = static_cast<uint32>(offsetof(FStaticMeshVertex, Position));
	Attributes[1].name = "Normal";
	Attributes[1].format = nvrhi::Format::RGB32_FLOAT;
	Attributes[1].offset = static_cast<uint32>(offsetof(FStaticMeshVertex, Normal));
	for (nvrhi::VertexAttributeDesc& Attribute : Attributes)
	{
		Attribute.elementStride = static_cast<uint32>(sizeof(FStaticMeshVertex));
	}
	const nvrhi::InputLayoutHandle InputLayout =
		RenderInterface.createInputLayout(Attributes.data(), static_cast<uint32>(Attributes.size()), VertexShader);

	// Slot 0 of the buffers is the array of FDrawData and slot 0 of the constant buffers the FViewConstants;
	// see SceneData.glsl for how the shaders name them.
	NewImplementation->SceneBindingLayout =
		RenderInterface.createBindingLayout(nvrhi::BindingLayoutDesc()
												.setVisibility(nvrhi::ShaderType::Vertex | nvrhi::ShaderType::Pixel)
												.addItem(nvrhi::BindingLayoutItem::StructuredBuffer_SRV(0))
												.addItem(nvrhi::BindingLayoutItem::ConstantBuffer(0)));
	NewImplementation->TonemapBindingLayout =
		RenderInterface.createBindingLayout(nvrhi::BindingLayoutDesc()
												.setVisibility(nvrhi::ShaderType::Pixel)
												.addItem(nvrhi::BindingLayoutItem::Texture_SRV(0))
												.addItem(nvrhi::BindingLayoutItem::PushConstants(0, sizeof(float))));
	if (InputLayout == nullptr || NewImplementation->SceneBindingLayout == nullptr ||
		NewImplementation->TonemapBindingLayout == nullptr)
	{
		OutError = "the graphics card could not create the layouts of the renderer's vertices and shader data";
		return nullptr;
	}

	nvrhi::BufferDesc ViewBufferDescription;
	ViewBufferDescription.byteSize = sizeof(FViewConstants);
	ViewBufferDescription.isConstantBuffer = true;
	ViewBufferDescription.debugName = "ViewConstants";
	ViewBufferDescription.initialState = nvrhi::ResourceStates::ConstantBuffer;
	ViewBufferDescription.keepInitialState = true;
	NewImplementation->ViewBuffer = RenderInterface.createBuffer(ViewBufferDescription);
	if (NewImplementation->ViewBuffer == nullptr)
	{
		OutError = "the graphics card could not create the buffer for the lights of a scene";
		return nullptr;
	}

	NewImplementation->LitPipeline = CreateMeshPipeline(RenderInterface,
		InputLayout,
		VertexShader,
		LitShader,
		NewImplementation->SceneBindingLayout,
		FRenderTarget::FImplementation::SceneColorFormat,
		FRenderTarget::FImplementation::DepthFormat);
	NewImplementation->UnlitPipeline = CreateMeshPipeline(RenderInterface,
		InputLayout,
		VertexShader,
		UnlitShader,
		NewImplementation->SceneBindingLayout,
		FRenderTarget::FImplementation::ColorFormat,
		FRenderTarget::FImplementation::DepthFormat);
	NewImplementation->TonemapPipeline = CreateTonemapPipeline(
		RenderInterface, NewImplementation->TonemapBindingLayout, FRenderTarget::FImplementation::ColorFormat);
	if (NewImplementation->LitPipeline == nullptr || NewImplementation->UnlitPipeline == nullptr ||
		NewImplementation->TonemapPipeline == nullptr)
	{
		OutError = "the graphics card could not create the pipelines that draw and light meshes";
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
				   "quaternion, its field of view between 1 and 179 degrees, and its near plane and exposure "
				   "greater than zero";
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

	const bool bIsLit = ViewMode == ESceneViewMode::Lit;
	const float AspectRatio = static_cast<float>(TargetData.Width) / static_cast<float>(TargetData.Height);
	const FSceneContents Contents = CollectScene(Scene, View, AspectRatio, bIsLit);
	const int32 DrawCount = static_cast<int32>(Contents.Meshes.size());

	// Everything that can fail happens before the first command is recorded, so that a failure changes nothing.
	nvrhi::IDevice& Device = *Implementation->Context->GetDevice();
	std::vector<FMeshUpload> Uploads;
	for (WStaticMesh* const Mesh : Contents.Meshes)
	{
		const auto Existing = Implementation->MeshBuffers.find(Mesh);
		const bool bIsCurrent = Existing != Implementation->MeshBuffers.end() &&
			Existing->second.GeometryVersion == Mesh->GetGeometryVersion();
		const bool bIsPending =
			std::ranges::any_of(Uploads, [Mesh](const FMeshUpload& Upload) { return Upload.Mesh == Mesh; });
		if (bIsCurrent || bIsPending)
		{
			continue;
		}

		FMeshUpload Upload;
		Upload.Mesh = Mesh;
		if (!CreateMeshBuffers(Device, *Mesh, Upload.Buffers))
		{
			OutError = std::format("the graphics card has no room for a mesh of {} vertices and {} triangles",
				Mesh->GetVertices().size(),
				Mesh->GetTriangleCount());
			return false;
		}
		Uploads.push_back(std::move(Upload));
	}

	if (DrawCount > Implementation->DrawBufferCapacity)
	{
		int32 NewCapacity = std::max(Implementation->DrawBufferCapacity, MinDrawBufferCapacity);
		while (NewCapacity < DrawCount)
		{
			NewCapacity *= 2;
		}

		nvrhi::BufferDesc DrawBufferDescription;
		DrawBufferDescription.byteSize = static_cast<uint64>(NewCapacity) * sizeof(FDrawData);
		DrawBufferDescription.structStride = static_cast<uint32>(sizeof(FDrawData));
		DrawBufferDescription.debugName = "DrawData";
		DrawBufferDescription.initialState = nvrhi::ResourceStates::ShaderResource;
		DrawBufferDescription.keepInitialState = true;

		nvrhi::BufferHandle NewDrawBuffer = Device.createBuffer(DrawBufferDescription);
		nvrhi::BindingSetHandle NewBindingSet;
		if (NewDrawBuffer != nullptr)
		{
			NewBindingSet = Device.createBindingSet(
				nvrhi::BindingSetDesc()
					.addItem(nvrhi::BindingSetItem::StructuredBuffer_SRV(0, NewDrawBuffer))
					.addItem(nvrhi::BindingSetItem::ConstantBuffer(0, Implementation->ViewBuffer)),
				Implementation->SceneBindingLayout);
		}
		if (NewBindingSet == nullptr)
		{
			OutError = std::format("the graphics card has no room for the data of {} meshes", DrawCount);
			return false;
		}

		Implementation->DrawBuffer = std::move(NewDrawBuffer);
		Implementation->SceneBindingSet = std::move(NewBindingSet);
		Implementation->DrawBufferCapacity = NewCapacity;
	}

	if (bIsLit && TargetData.TonemapBindingLayout != Implementation->TonemapBindingLayout)
	{
		nvrhi::BindingSetHandle NewBindingSet =
			Device.createBindingSet(nvrhi::BindingSetDesc()
										.addItem(nvrhi::BindingSetItem::Texture_SRV(0, TargetData.SceneColorTexture))
										.addItem(nvrhi::BindingSetItem::PushConstants(0, sizeof(float))),
				Implementation->TonemapBindingLayout);
		if (NewBindingSet == nullptr)
		{
			OutError = "the graphics card could not bind the lit scene for tone mapping";
			return false;
		}

		TargetData.TonemapBindingSet = std::move(NewBindingSet);
		TargetData.TonemapBindingLayout = Implementation->TonemapBindingLayout;
	}

	if (Contents.IgnoredLightCount > 0 && !Implementation->bWarnedAboutLightLimit)
	{
		Implementation->bWarnedAboutLightLimit = true;
		WW_LOG(LogRendering,
			Warning,
			"Scene '{}' has more lights than one picture can be lit by ({} directional and {} point lights); {} "
			"are ignored. This is reported once per renderer",
			Scene.GetName(),
			MaxDirectionalLights,
			MaxPointLights,
			Contents.IgnoredLightCount);
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

	if (DrawCount > 0)
	{
		CommandList.writeBuffer(
			Implementation->DrawBuffer, Contents.DrawData.data(), Contents.DrawData.size() * sizeof(FDrawData));
	}
	if (bIsLit)
	{
		CommandList.writeBuffer(Implementation->ViewBuffer, &Contents.ViewConstants, sizeof(FViewConstants));
	}

	// A lit scene is drawn into the high dynamic range buffer; an unlit one straight onto the surface.
	const float MaxBackground = bIsLit ? MaxSceneLight : 1.0f;
	const nvrhi::Color ClearColor(std::clamp(BackgroundColor.R, 0.0f, MaxBackground),
		std::clamp(BackgroundColor.G, 0.0f, MaxBackground),
		std::clamp(BackgroundColor.B, 0.0f, MaxBackground),
		std::clamp(BackgroundColor.A, 0.0f, 1.0f));
	CommandList.clearTextureFloat(
		bIsLit ? TargetData.SceneColorTexture : TargetData.Texture, nvrhi::AllSubresources, ClearColor);
	const bool bClearDepth = true;
	const bool bClearStencil = false;
	CommandList.clearDepthStencilTexture(
		TargetData.DepthTexture, nvrhi::AllSubresources, bClearDepth, FarthestDepth, bClearStencil, 0);

	const nvrhi::Viewport Viewport(static_cast<float>(TargetData.Width), static_cast<float>(TargetData.Height));

	if (DrawCount > 0)
	{
		nvrhi::GraphicsState State;
		State.pipeline = bIsLit ? Implementation->LitPipeline : Implementation->UnlitPipeline;
		State.framebuffer = bIsLit ? TargetData.SceneFramebuffer : TargetData.Framebuffer;
		State.viewport.addViewportAndScissorRect(Viewport);
		State.bindings.push_back(Implementation->SceneBindingSet);
		State.vertexBuffers.resize(1);

		for (int32 DrawIndex = 0; DrawIndex < DrawCount; ++DrawIndex)
		{
			const FStaticMeshBuffers& Buffers =
				Implementation->MeshBuffers.at(Contents.Meshes[static_cast<std::size_t>(DrawIndex)]);
			State.vertexBuffers[0] =
				nvrhi::VertexBufferBinding().setBuffer(Buffers.VertexBuffer).setSlot(0).setOffset(0);
			State.indexBuffer = nvrhi::IndexBufferBinding()
									.setBuffer(Buffers.IndexBuffer)
									.setFormat(nvrhi::Format::R32_UINT)
									.setOffset(0);
			CommandList.setGraphicsState(State);

			// The instance number is how the shaders find this mesh's FDrawData.
			CommandList.drawIndexed(nvrhi::DrawArguments()
										.setVertexCount(Buffers.IndexCount)
										.setStartInstanceLocation(static_cast<uint32>(DrawIndex)));
		}
	}

	if (bIsLit)
	{
		nvrhi::GraphicsState State;
		State.pipeline = Implementation->TonemapPipeline;
		State.framebuffer = TargetData.OutputFramebuffer;
		State.viewport.addViewportAndScissorRect(Viewport);
		State.bindings.push_back(TargetData.TonemapBindingSet);
		CommandList.setGraphicsState(State);
		CommandList.setPushConstants(&View.Exposure, sizeof(float));

		const uint32 FullScreenTriangleVertexCount = 3;
		CommandList.draw(nvrhi::DrawArguments().setVertexCount(FullScreenTriangleVertexCount));
	}

	CommandList.close();
	Device.executeCommandList(&CommandList);
	Device.runGarbageCollection();

	Implementation->Statistics.DrawnMeshCount = DrawCount;
	Implementation->Statistics.UploadedMeshCount = static_cast<int32>(Uploads.size());
	Implementation->Statistics.CachedMeshCount = static_cast<int32>(Implementation->MeshBuffers.size());
	Implementation->Statistics.DirectionalLightCount = Contents.ViewConstants.LightCounts.x;
	Implementation->Statistics.PointLightCount = Contents.ViewConstants.LightCounts.y;
	return true;
}

const FSceneRenderStatistics& FSceneRenderer::GetStatistics() const
{
	return Implementation->Statistics;
}
