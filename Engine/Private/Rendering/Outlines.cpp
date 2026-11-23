#include "Rendering/Outlines.h"

#include "Rendering/ShaderLibrary.h"

#include <utility>

static_assert(
	sizeof(FOutlines::FConstants) == 32, "FOutlines::FConstants must be laid out as Outline.frag declares it");

bool FOutlines::Initialize(std::shared_ptr<FRenderContext> InContext,
	nvrhi::IInputLayout* MeshLayout,
	nvrhi::IShader* MeshVertexShader,
	nvrhi::IBindingLayout* SceneLayout,
	nvrhi::Format MaskFormat,
	nvrhi::Format OutputFormat)
{
	Context = std::move(InContext);
	nvrhi::IDevice& Device = *Context->GetDevice();

	MaskLayout =
		Device.createBindingLayout(nvrhi::BindingLayoutDesc()
									   .setVisibility(nvrhi::ShaderType::Pixel)
									   .addItem(nvrhi::BindingLayoutItem::Texture_SRV(0))
									   .addItem(nvrhi::BindingLayoutItem::PushConstants(0, sizeof(FConstants))));
	const nvrhi::ShaderHandle MaskShader =
		FShaderLibrary::CreateShader(Device, "OutlineMask.frag", nvrhi::ShaderType::Pixel);
	const nvrhi::ShaderHandle FullScreenShader =
		FShaderLibrary::CreateShader(Device, "FullScreen.vert", nvrhi::ShaderType::Vertex);
	const nvrhi::ShaderHandle LineShader =
		FShaderLibrary::CreateShader(Device, "Outline.frag", nvrhi::ShaderType::Pixel);
	if (MaskLayout == nullptr || MaskShader == nullptr || FullScreenShader == nullptr || LineShader == nullptr)
	{
		return false;
	}

	// The mask is of the whole of a mesh: both sides of its triangles, and whatever is in front of them.
	nvrhi::GraphicsPipelineDesc MaskDescription;
	MaskDescription.primType = nvrhi::PrimitiveType::TriangleList;
	MaskDescription.inputLayout = MeshLayout;
	MaskDescription.VS = MeshVertexShader;
	MaskDescription.PS = MaskShader;
	MaskDescription.bindingLayouts.push_back(SceneLayout);
	MaskDescription.renderState.rasterState.cullMode = nvrhi::RasterCullMode::None;
	MaskDescription.renderState.rasterState.depthClipEnable = true;
	MaskDescription.renderState.depthStencilState.depthTestEnable = false;
	MaskDescription.renderState.depthStencilState.depthWriteEnable = false;
	nvrhi::FramebufferInfo MaskFormats;
	MaskFormats.addColorFormat(MaskFormat);
	MaskPipeline = Device.createGraphicsPipeline(MaskDescription, MaskFormats);

	// The line is one triangle over the whole surface, laid over what the surface shows: where the line covers
	// only part of a pixel, the rest of the pixel stays what it was. The alpha of the surface is left alone.
	nvrhi::GraphicsPipelineDesc LineDescription;
	LineDescription.primType = nvrhi::PrimitiveType::TriangleList;
	LineDescription.VS = FullScreenShader;
	LineDescription.PS = LineShader;
	LineDescription.bindingLayouts.push_back(MaskLayout);
	LineDescription.renderState.rasterState.cullMode = nvrhi::RasterCullMode::None;
	LineDescription.renderState.depthStencilState.depthTestEnable = false;
	LineDescription.renderState.depthStencilState.depthWriteEnable = false;
	nvrhi::BlendState::RenderTarget& Blend = LineDescription.renderState.blendState.targets[0];
	Blend.blendEnable = true;
	Blend.srcBlend = nvrhi::BlendFactor::SrcAlpha;
	Blend.destBlend = nvrhi::BlendFactor::InvSrcAlpha;
	Blend.srcBlendAlpha = nvrhi::BlendFactor::Zero;
	Blend.destBlendAlpha = nvrhi::BlendFactor::One;
	nvrhi::FramebufferInfo OutputFormats;
	OutputFormats.addColorFormat(OutputFormat);
	LinePipeline = Device.createGraphicsPipeline(LineDescription, OutputFormats);

	return MaskPipeline != nullptr && LinePipeline != nullptr;
}

bool FOutlines::PrepareTarget(FOutlineTargetBindings& Bindings, nvrhi::ITexture* Mask) const
{
	if (Bindings.Owner == MaskLayout.Get())
	{
		return true;
	}

	nvrhi::BindingSetHandle NewBindingSet = Context->GetDevice()->createBindingSet(
		nvrhi::BindingSetDesc()
			.addItem(nvrhi::BindingSetItem::Texture_SRV(0, Mask))
			.addItem(nvrhi::BindingSetItem::PushConstants(0, static_cast<uint32>(sizeof(FConstants)))),
		MaskLayout);
	if (NewBindingSet == nullptr)
	{
		return false;
	}

	Bindings.Mask = std::move(NewBindingSet);
	Bindings.Owner = MaskLayout.Get();
	return true;
}

void FOutlines::Draw(nvrhi::ICommandList& CommandList,
	const FOutlineTargetBindings& Bindings,
	nvrhi::IFramebuffer* Output,
	const nvrhi::Viewport& Area,
	const FConstants& Constants) const
{
	nvrhi::GraphicsState State;
	State.pipeline = LinePipeline;
	State.framebuffer = Output;
	State.viewport.addViewportAndScissorRect(Area);
	State.bindings.push_back(Bindings.Mask);
	CommandList.setGraphicsState(State);
	CommandList.setPushConstants(&Constants, sizeof(FConstants));

	const uint32 FullScreenTriangleVertexCount = 3;
	CommandList.draw(nvrhi::DrawArguments().setVertexCount(FullScreenTriangleVertexCount));
}
