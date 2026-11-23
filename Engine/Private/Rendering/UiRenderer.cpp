#include "Rendering/UiRenderer.h"

#include "Rendering/RenderContext.h"
#include "Rendering/RenderDevice.h"
#include "Rendering/RenderTarget.h"
#include "Rendering/RenderTargetImplementation.h"
#include "Rendering/ShaderLibrary.h"
#include "Rendering/Texture.h"
#include "Rendering/TextureCache.h"
#include "Rendering/TextureWrap.h"
#include "Rendering/UiDrawCommand.h"
#include "Rendering/UiDrawList.h"
#include "Rendering/UiVertex.h"

#include <nvrhi/nvrhi.h>

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <format>
#include <map>
#include <utility>
#include <vector>

static_assert(sizeof(FUiVertex) == 20, "FUiVertex must be laid out as Ui.vert reads it: four floats and four bytes");

namespace
{
	/** What the shaders are told with each run of triangles. The same structure is declared in UiData.glsl. */
	struct FUiConstants
	{
		/** A position in pixels, multiplied by Scale and with Translate added, is from -1 to 1 across the surface. */
		FVector2f Scale = FVector2f(1.0f);
		FVector2f Translate = FVector2f(0.0f);

		/** 1 to use the alpha of the picture, 0 to take the picture as covering everything. */
		float TextureAlpha = 1.0f;
	};

	static_assert(sizeof(FUiConstants) == 20, "FUiConstants must be laid out as UiData.glsl declares it");

	/** The buffers of vertices and indices are made for at least this many, and grow from there. */
	constexpr std::size_t MinBufferElementCount = 4096;

	/**
	 * Checks that a list names only indices and vertices it has, and that each command can be drawn onto Target.
	 * On failure OutError says what is wrong with the first command that cannot be drawn.
	 */
	bool CheckDrawList(const FUiDrawList& DrawList, const FRenderTarget& Target, std::string& OutError)
	{
		const std::size_t IndexTotal = DrawList.Indices.size();
		const std::size_t VertexTotal = DrawList.Vertices.size();

		for (std::size_t CommandIndex = 0; CommandIndex < DrawList.Commands.size(); ++CommandIndex)
		{
			const FUiDrawCommand& Command = DrawList.Commands[CommandIndex];
			if (Command.FirstIndex < 0 || Command.IndexCount < 0 || Command.VertexOffset < 0)
			{
				OutError = std::format(
					"command {} of the user interface has a negative first index, index count or vertex offset",
					CommandIndex);
				return false;
			}
			if (Command.IndexCount % 3 != 0)
			{
				OutError = std::format("command {} of the user interface has {} indices, which is not three for each "
									   "triangle",
					CommandIndex,
					Command.IndexCount);
				return false;
			}

			const std::size_t FirstIndex = static_cast<std::size_t>(Command.FirstIndex);
			const std::size_t IndexCount = static_cast<std::size_t>(Command.IndexCount);
			if (FirstIndex > IndexTotal || IndexCount > IndexTotal - FirstIndex)
			{
				OutError =
					std::format("command {} of the user interface uses {} indices from index {} on, and the list "
								"has {}",
						CommandIndex,
						IndexCount,
						FirstIndex,
						IndexTotal);
				return false;
			}

			const uint64 VertexOffset = static_cast<uint64>(Command.VertexOffset);
			for (std::size_t Index = FirstIndex; Index < FirstIndex + IndexCount; ++Index)
			{
				const uint64 Vertex = VertexOffset + DrawList.Indices[Index];
				if (Vertex >= VertexTotal)
				{
					OutError = std::format("command {} of the user interface uses vertex {}, and the list has {}",
						CommandIndex,
						Vertex,
						VertexTotal);
					return false;
				}
			}

			if (Command.Texture != nullptr && Command.RenderTarget != nullptr)
			{
				OutError = std::format(
					"command {} of the user interface has both a texture and a render target", CommandIndex);
				return false;
			}
			if (Command.RenderTarget == &Target)
			{
				OutError = std::format(
					"command {} of the user interface shows the render target that it is drawn onto", CommandIndex);
				return false;
			}
		}
		return true;
	}
}

/** Everything a user interface renderer holds. It lives here so that no NVRHI type appears in an engine header. */
struct FUiRenderer::FImplementation
{
	/** What gives the shaders one picture and the sampler it is read with. */
	struct FPictureBinding
	{
		nvrhi::BindingSetHandle BindingSet;

		/** True if the list being drawn uses the binding. One that a list did not use is released after it. */
		bool bIsUsed = false;
	};

	/** Keeps the graphics card alive for as long as this renderer exists. */
	std::shared_ptr<FRenderContext> Context;

	/** The pictures on the graphics card of every texture that has been drawn, and the samplers. */
	FTextureCache Textures;

	/** How a picture and the constants reach the shaders; see UiData.glsl and Ui.frag. */
	nvrhi::BindingLayoutHandle BindingLayout;

	/** Draws triangles over what a surface shows, blended by their alpha, without a depth buffer. */
	nvrhi::GraphicsPipelineHandle Pipeline;

	/** The vertices and indices of the list being drawn. They grow with the largest list drawn. */
	nvrhi::BufferHandle VertexBuffer;
	std::size_t VertexCapacity = 0;
	nvrhi::BufferHandle IndexBuffer;
	std::size_t IndexCapacity = 0;

	/**
	 * The bindings of the pictures that the last list used, by picture and sampler. The keys are only addresses,
	 * and stay those of the same picture and sampler because the binding set keeps both alive.
	 */
	std::map<std::pair<nvrhi::ITexture*, nvrhi::ISampler*>, FPictureBinding> PictureBindings;

	/** Makes the buffers large enough for a list. Returns false if the graphics card has no room for them. */
	bool EnsureBufferCapacity(std::size_t VertexCount, std::size_t IndexCount);

	/** Returns the binding of a picture and a sampler, creating it if need be, and marks it as used. */
	nvrhi::IBindingSet* FindOrCreateBinding(nvrhi::ITexture* Picture, nvrhi::ISampler* Sampler);
};

bool FUiRenderer::FImplementation::EnsureBufferCapacity(std::size_t VertexCount, std::size_t IndexCount)
{
	nvrhi::IDevice& Device = *Context->GetDevice();

	if (VertexCount > VertexCapacity)
	{
		const std::size_t NewCapacity = std::max(std::bit_ceil(VertexCount), MinBufferElementCount);

		nvrhi::BufferDesc Description;
		Description.byteSize = NewCapacity * sizeof(FUiVertex);
		Description.isVertexBuffer = true;
		Description.debugName = "UiVertices";
		Description.initialState = nvrhi::ResourceStates::VertexBuffer;
		Description.keepInitialState = true;

		nvrhi::BufferHandle NewBuffer = Device.createBuffer(Description);
		if (NewBuffer == nullptr)
		{
			return false;
		}
		VertexBuffer = std::move(NewBuffer);
		VertexCapacity = NewCapacity;
	}

	if (IndexCount > IndexCapacity)
	{
		const std::size_t NewCapacity = std::max(std::bit_ceil(IndexCount), MinBufferElementCount);

		nvrhi::BufferDesc Description;
		Description.byteSize = NewCapacity * sizeof(uint32);
		Description.isIndexBuffer = true;
		Description.debugName = "UiIndices";
		Description.initialState = nvrhi::ResourceStates::IndexBuffer;
		Description.keepInitialState = true;

		nvrhi::BufferHandle NewBuffer = Device.createBuffer(Description);
		if (NewBuffer == nullptr)
		{
			return false;
		}
		IndexBuffer = std::move(NewBuffer);
		IndexCapacity = NewCapacity;
	}

	return true;
}

nvrhi::IBindingSet* FUiRenderer::FImplementation::FindOrCreateBinding(
	nvrhi::ITexture* Picture, nvrhi::ISampler* Sampler)
{
	const std::pair<nvrhi::ITexture*, nvrhi::ISampler*> Key(Picture, Sampler);
	const auto Existing = PictureBindings.find(Key);
	if (Existing != PictureBindings.end())
	{
		Existing->second.bIsUsed = true;
		return Existing->second.BindingSet;
	}

	FPictureBinding Binding;
	Binding.bIsUsed = true;
	Binding.BindingSet = Context->GetDevice()->createBindingSet(
		nvrhi::BindingSetDesc()
			.addItem(nvrhi::BindingSetItem::Texture_SRV(0, Picture))
			.addItem(nvrhi::BindingSetItem::Sampler(0, Sampler))
			.addItem(nvrhi::BindingSetItem::PushConstants(0, static_cast<uint32>(sizeof(FUiConstants)))),
		BindingLayout);
	if (Binding.BindingSet == nullptr)
	{
		return nullptr;
	}

	nvrhi::IBindingSet* const BindingSet = Binding.BindingSet;
	PictureBindings.emplace(Key, std::move(Binding));
	return BindingSet;
}

std::unique_ptr<FUiRenderer> FUiRenderer::Create(FRenderDevice& Device, std::string& OutError)
{
	std::unique_ptr<FImplementation> NewImplementation = std::make_unique<FImplementation>();
	NewImplementation->Context = Device.Context;
	nvrhi::IDevice& RenderInterface = *NewImplementation->Context->GetDevice();

	const nvrhi::ShaderHandle VertexShader =
		FShaderLibrary::CreateShader(RenderInterface, "Ui.vert", nvrhi::ShaderType::Vertex);
	const nvrhi::ShaderHandle FragmentShader =
		FShaderLibrary::CreateShader(RenderInterface, "Ui.frag", nvrhi::ShaderType::Pixel);
	if (VertexShader == nullptr || FragmentShader == nullptr)
	{
		OutError = "the graphics card did not accept the engine's shaders for user interfaces";
		return nullptr;
	}

	// The attributes are numbered in this order, which is the order of the inputs of Ui.vert.
	std::array<nvrhi::VertexAttributeDesc, 3> Attributes;
	Attributes[0].name = "Position";
	Attributes[0].format = nvrhi::Format::RG32_FLOAT;
	Attributes[0].offset = static_cast<uint32>(offsetof(FUiVertex, Position));
	Attributes[1].name = "TextureCoordinate";
	Attributes[1].format = nvrhi::Format::RG32_FLOAT;
	Attributes[1].offset = static_cast<uint32>(offsetof(FUiVertex, TextureCoordinate));
	Attributes[2].name = "Color";
	Attributes[2].format = nvrhi::Format::RGBA8_UNORM;
	Attributes[2].offset = static_cast<uint32>(offsetof(FUiVertex, Color));
	for (nvrhi::VertexAttributeDesc& Attribute : Attributes)
	{
		Attribute.elementStride = static_cast<uint32>(sizeof(FUiVertex));
	}
	const nvrhi::InputLayoutHandle InputLayout =
		RenderInterface.createInputLayout(Attributes.data(), static_cast<uint32>(Attributes.size()), VertexShader);

	// Slot 0 of the textures is the picture and slot 0 of the samplers how it is read; the constants are read by
	// both shaders.
	NewImplementation->BindingLayout = RenderInterface.createBindingLayout(
		nvrhi::BindingLayoutDesc()
			.setVisibility(nvrhi::ShaderType::Vertex | nvrhi::ShaderType::Pixel)
			.addItem(nvrhi::BindingLayoutItem::Texture_SRV(0))
			.addItem(nvrhi::BindingLayoutItem::Sampler(0))
			.addItem(nvrhi::BindingLayoutItem::PushConstants(0, sizeof(FUiConstants))));
	if (InputLayout == nullptr || NewImplementation->BindingLayout == nullptr)
	{
		OutError = "the graphics card could not create the layouts for drawing user interfaces";
		return nullptr;
	}

	if (!NewImplementation->Textures.Initialize(NewImplementation->Context))
	{
		OutError = "the graphics card could not create the samplers and the default texture of user interfaces";
		return nullptr;
	}

	nvrhi::GraphicsPipelineDesc Description;
	Description.primType = nvrhi::PrimitiveType::TriangleList;
	Description.inputLayout = InputLayout;
	Description.VS = VertexShader;
	Description.PS = FragmentShader;
	Description.bindingLayouts.push_back(NewImplementation->BindingLayout);
	// A user interface is flat: its triangles face either way, and what is drawn later lies on top.
	Description.renderState.rasterState.cullMode = nvrhi::RasterCullMode::None;
	Description.renderState.rasterState.scissorEnable = true;
	Description.renderState.depthStencilState.depthTestEnable = false;
	Description.renderState.depthStencilState.depthWriteEnable = false;

	// A colour covers what lies behind it by its alpha. The alpha of the surface itself ends up as how much of
	// it has been covered.
	nvrhi::BlendState::RenderTarget& Blend = Description.renderState.blendState.targets[0];
	Blend.blendEnable = true;
	Blend.srcBlend = nvrhi::BlendFactor::SrcAlpha;
	Blend.destBlend = nvrhi::BlendFactor::InvSrcAlpha;
	Blend.srcBlendAlpha = nvrhi::BlendFactor::One;
	Blend.destBlendAlpha = nvrhi::BlendFactor::InvSrcAlpha;

	nvrhi::FramebufferInfo TargetFormats;
	TargetFormats.addColorFormat(FRenderTarget::FImplementation::ColorFormat);
	NewImplementation->Pipeline = RenderInterface.createGraphicsPipeline(Description, TargetFormats);
	if (NewImplementation->Pipeline == nullptr)
	{
		OutError = "the graphics card could not create the pipeline for drawing user interfaces";
		return nullptr;
	}

	return std::make_unique<FUiRenderer>(std::move(NewImplementation));
}

FUiRenderer::FUiRenderer(std::unique_ptr<FImplementation> InImplementation)
	: Implementation(std::move(InImplementation))
{
}

FUiRenderer::~FUiRenderer() = default;

bool FUiRenderer::Draw(const FUiDrawList& DrawList, FRenderTarget& Target, std::string& OutError)
{
	FRenderTarget::FImplementation& TargetData = *Target.Implementation;
	WW_CHECKF(TargetData.Context == Implementation->Context,
		"A render target can only be drawn to by a renderer of the render device that created it");

	if (!CheckDrawList(DrawList, Target, OutError))
	{
		return false;
	}

	FTextureCache& Textures = Implementation->Textures;
	Textures.ReleaseUnused();
	for (std::pair<const std::pair<nvrhi::ITexture*, nvrhi::ISampler*>, FImplementation::FPictureBinding>& Entry :
		Implementation->PictureBindings)
	{
		Entry.second.bIsUsed = false;
	}

	/** What one command is drawn with, once its picture has been found. */
	struct FPreparedCommand
	{
		const FUiDrawCommand* Command = nullptr;
		nvrhi::IBindingSet* BindingSet = nullptr;
		nvrhi::Rect Clip;
		float TextureAlpha = 1.0f;
	};

	std::vector<FPreparedCommand> Prepared;
	Prepared.reserve(DrawList.Commands.size());
	for (const FUiDrawCommand& Command : DrawList.Commands)
	{
		// What a command draws outside the surface, or with an empty rectangle, is nothing.
		const nvrhi::Rect Clip(std::max(Command.ClipLeft, 0),
			std::min(Command.ClipRight, TargetData.Width),
			std::max(Command.ClipTop, 0),
			std::min(Command.ClipBottom, TargetData.Height));
		if (Command.IndexCount == 0 || Clip.minX >= Clip.maxX || Clip.minY >= Clip.maxY)
		{
			continue;
		}

		nvrhi::ITexture* Picture = nullptr;
		nvrhi::ISampler* Sampler = nullptr;
		float TextureAlpha = 1.0f;
		if (Command.RenderTarget != nullptr)
		{
			const FRenderTarget::FImplementation& Source = *Command.RenderTarget->Implementation;
			WW_CHECKF(Source.Context == Implementation->Context,
				"A render target can only be shown by a renderer of the render device that created it");
			Picture = Source.Texture;
			Sampler = Textures.GetSampler(ETextureWrap::ClampToEdge, ETextureWrap::ClampToEdge);
			TextureAlpha = 0.0f;
		}
		else
		{
			Picture = Textures.FindOrCreate(Command.Texture);
			if (Picture == nullptr)
			{
				OutError = "the graphics card has no room for a texture of the user interface";
				return false;
			}
			// The white texture stands in for a missing one, and how a single pixel wraps does not matter.
			const bool bHasOwnTexture = Picture != Textures.GetWhiteTexture();
			Sampler = bHasOwnTexture ? Textures.GetSampler(Command.Texture->WrapU, Command.Texture->WrapV)
									 : Textures.GetSampler(ETextureWrap::Repeat, ETextureWrap::Repeat);
		}

		nvrhi::IBindingSet* const BindingSet = Implementation->FindOrCreateBinding(Picture, Sampler);
		if (BindingSet == nullptr)
		{
			OutError = "the graphics card could not bind a texture of the user interface";
			return false;
		}
		Prepared.push_back({&Command, BindingSet, Clip, TextureAlpha});
	}

	if (!Prepared.empty())
	{
		if (!Implementation->EnsureBufferCapacity(DrawList.Vertices.size(), DrawList.Indices.size()))
		{
			OutError = std::format("the graphics card has no room for the {} vertices and {} indices of the user "
								   "interface",
				DrawList.Vertices.size(),
				DrawList.Indices.size());
			return false;
		}

		nvrhi::IDevice& Device = *Implementation->Context->GetDevice();
		nvrhi::ICommandList& CommandList = *Implementation->Context->GetImmediateCommandList();
		CommandList.open();

		Textures.WritePending(CommandList);
		CommandList.writeBuffer(
			Implementation->VertexBuffer, DrawList.Vertices.data(), DrawList.Vertices.size() * sizeof(FUiVertex));
		CommandList.writeBuffer(
			Implementation->IndexBuffer, DrawList.Indices.data(), DrawList.Indices.size() * sizeof(uint32));

		// Pixels count from the top left corner of the surface, and NVRHI's surfaces have their top at 1.
		const float Width = static_cast<float>(TargetData.Width);
		const float Height = static_cast<float>(TargetData.Height);
		FUiConstants Constants;
		Constants.Scale = FVector2f(2.0f / Width, -2.0f / Height);
		Constants.Translate = FVector2f(-1.0f, 1.0f);

		nvrhi::GraphicsState State;
		State.pipeline = Implementation->Pipeline;
		State.framebuffer = TargetData.OutputFramebuffer;
		State.bindings.resize(1);
		State.vertexBuffers.push_back(
			nvrhi::VertexBufferBinding().setBuffer(Implementation->VertexBuffer).setSlot(0).setOffset(0));
		State.indexBuffer = nvrhi::IndexBufferBinding()
								.setBuffer(Implementation->IndexBuffer)
								.setFormat(nvrhi::Format::R32_UINT)
								.setOffset(0);

		const nvrhi::Viewport Viewport(Width, Height);
		for (const FPreparedCommand& Entry : Prepared)
		{
			State.viewport = nvrhi::ViewportState().addViewport(Viewport).addScissorRect(Entry.Clip);
			State.bindings[0] = Entry.BindingSet;
			CommandList.setGraphicsState(State);

			// Setting the state forgets the constants, so they are given again for every command.
			Constants.TextureAlpha = Entry.TextureAlpha;
			CommandList.setPushConstants(&Constants, sizeof(FUiConstants));

			CommandList.drawIndexed(nvrhi::DrawArguments()
										.setVertexCount(static_cast<uint32>(Entry.Command->IndexCount))
										.setStartIndexLocation(static_cast<uint32>(Entry.Command->FirstIndex))
										.setStartVertexLocation(static_cast<uint32>(Entry.Command->VertexOffset)));
		}

		CommandList.close();
		Device.executeCommandList(&CommandList);
		Device.runGarbageCollection();
	}

	// A picture that this list did not show is not kept on its account.
	std::erase_if(Implementation->PictureBindings,
		[](const std::pair<const std::pair<nvrhi::ITexture*, nvrhi::ISampler*>, FImplementation::FPictureBinding>&
				Entry) { return !Entry.second.bIsUsed; });
	return true;
}
