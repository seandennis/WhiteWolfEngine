#include "Rendering/AmbientOcclusion.h"

#include "Rendering/ShaderLibrary.h"

#include <cstddef>
#include <span>
#include <string>
#include <string_view>
#include <utility>

static_assert(sizeof(FAmbientOcclusion::FConstants) == 112,
	"FAmbientOcclusion::FConstants must be laid out as the shaders declare it");

namespace
{
	nvrhi::ShaderHandle CreateShader(nvrhi::IDevice& Device, std::string_view Name, nvrhi::ShaderType Type)
	{
		const std::span<const uint32> Code = FShaderLibrary::GetCode(Name);

		nvrhi::ShaderDesc Description;
		Description.shaderType = Type;
		Description.debugName = std::string(Name);
		return Device.createShader(Description, Code.data(), Code.size_bytes());
	}

	/** Creates a pipeline that draws one triangle over the whole of a surface with a fragment shader. */
	nvrhi::GraphicsPipelineHandle CreateFullScreenPipeline(nvrhi::IDevice& Device,
		nvrhi::IShader* VertexShader,
		std::string_view FragmentShaderName,
		nvrhi::IBindingLayout* BindingLayout,
		nvrhi::Format TargetFormat)
	{
		const nvrhi::ShaderHandle FragmentShader = CreateShader(Device, FragmentShaderName, nvrhi::ShaderType::Pixel);
		if (FragmentShader == nullptr)
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
		TargetFormats.addColorFormat(TargetFormat);
		return Device.createGraphicsPipeline(Description, TargetFormats);
	}
}

bool FAmbientOcclusion::Initialize(
	std::shared_ptr<FRenderContext> InContext, nvrhi::Format OcclusionFormat, nvrhi::Format ShowFormat)
{
	Context = std::move(InContext);
	nvrhi::IDevice& Device = *Context->GetDevice();

	// Slot 0 is the depth and slot 1 the normals; see AmbientOcclusionData.glsl.
	InputLayout =
		Device.createBindingLayout(nvrhi::BindingLayoutDesc()
									   .setVisibility(nvrhi::ShaderType::Pixel)
									   .addItem(nvrhi::BindingLayoutItem::Texture_SRV(0))
									   .addItem(nvrhi::BindingLayoutItem::Texture_SRV(1))
									   .addItem(nvrhi::BindingLayoutItem::PushConstants(0, sizeof(FConstants))));
	BlurLayout =
		Device.createBindingLayout(nvrhi::BindingLayoutDesc()
									   .setVisibility(nvrhi::ShaderType::Pixel)
									   .addItem(nvrhi::BindingLayoutItem::Texture_SRV(0))
									   .addItem(nvrhi::BindingLayoutItem::Texture_SRV(1))
									   .addItem(nvrhi::BindingLayoutItem::Texture_SRV(2))
									   .addItem(nvrhi::BindingLayoutItem::PushConstants(0, sizeof(FConstants))));
	const nvrhi::ShaderHandle VertexShader = CreateShader(Device, "FullScreen.vert", nvrhi::ShaderType::Vertex);
	if (InputLayout == nullptr || BlurLayout == nullptr || VertexShader == nullptr)
	{
		return false;
	}

	OcclusionPipeline =
		CreateFullScreenPipeline(Device, VertexShader, "AmbientOcclusion.frag", InputLayout, OcclusionFormat);
	BlurPipeline =
		CreateFullScreenPipeline(Device, VertexShader, "AmbientOcclusionBlur.frag", BlurLayout, OcclusionFormat);
	ShowPipeline = CreateFullScreenPipeline(Device, VertexShader, "ShowAmbientOcclusion.frag", BlurLayout, ShowFormat);
	return OcclusionPipeline != nullptr && BlurPipeline != nullptr && ShowPipeline != nullptr;
}

bool FAmbientOcclusion::PrepareTarget(FAmbientOcclusionTargetBindings& Bindings,
	nvrhi::ITexture* Depth,
	nvrhi::ITexture* Normals,
	nvrhi::ITexture* FirstOcclusion,
	nvrhi::ITexture* SecondOcclusion) const
{
	if (Bindings.Owner == InputLayout.Get())
	{
		return true;
	}

	nvrhi::IDevice& Device = *Context->GetDevice();
	const uint32 ConstantsSize = static_cast<uint32>(sizeof(FConstants));

	FAmbientOcclusionTargetBindings NewBindings;
	NewBindings.Input = Device.createBindingSet(nvrhi::BindingSetDesc()
													.addItem(nvrhi::BindingSetItem::Texture_SRV(0, Depth))
													.addItem(nvrhi::BindingSetItem::Texture_SRV(1, Normals))
													.addItem(nvrhi::BindingSetItem::PushConstants(0, ConstantsSize)),
		InputLayout);

	for (std::size_t Index = 0; Index < NewBindings.Blur.size(); ++Index)
	{
		nvrhi::ITexture* const Source = Index == 0 ? FirstOcclusion : SecondOcclusion;
		NewBindings.Blur[Index] =
			Device.createBindingSet(nvrhi::BindingSetDesc()
										.addItem(nvrhi::BindingSetItem::Texture_SRV(0, Depth))
										.addItem(nvrhi::BindingSetItem::Texture_SRV(1, Normals))
										.addItem(nvrhi::BindingSetItem::Texture_SRV(2, Source))
										.addItem(nvrhi::BindingSetItem::PushConstants(0, ConstantsSize)),
				BlurLayout);
	}

	if (NewBindings.Input == nullptr || NewBindings.Blur[0] == nullptr || NewBindings.Blur[1] == nullptr)
	{
		return false;
	}

	NewBindings.Owner = InputLayout.Get();
	Bindings = std::move(NewBindings);
	return true;
}

void FAmbientOcclusion::DrawFullScreen(nvrhi::ICommandList& CommandList,
	nvrhi::IGraphicsPipeline* Pipeline,
	nvrhi::IBindingSet* BindingSet,
	nvrhi::IFramebuffer* Target,
	const FConstants& Constants)
{
	nvrhi::GraphicsState State;
	State.pipeline = Pipeline;
	State.framebuffer = Target;
	State.viewport.addViewportAndScissorRect(nvrhi::Viewport(Constants.Settings.z, Constants.Settings.w));
	State.bindings.push_back(BindingSet);
	CommandList.setGraphicsState(State);
	CommandList.setPushConstants(&Constants, sizeof(FConstants));

	const uint32 FullScreenTriangleVertexCount = 3;
	CommandList.draw(nvrhi::DrawArguments().setVertexCount(FullScreenTriangleVertexCount));
}

void FAmbientOcclusion::Draw(nvrhi::ICommandList& CommandList,
	const FAmbientOcclusionTargetBindings& Bindings,
	nvrhi::IFramebuffer* FirstTarget,
	nvrhi::IFramebuffer* SecondTarget,
	const FConstants& Constants) const
{
	// The occlusion goes into the first texture; the blur across takes it to the second, and the blur down back
	// to the first.
	DrawFullScreen(CommandList, OcclusionPipeline, Bindings.Input, FirstTarget, Constants);

	FConstants BlurConstants = Constants;
	BlurConstants.Blur = glm::vec4(1.0f, 0.0f, 0.0f, 0.0f);
	DrawFullScreen(CommandList, BlurPipeline, Bindings.Blur[0], SecondTarget, BlurConstants);
	BlurConstants.Blur = glm::vec4(0.0f, 1.0f, 0.0f, 0.0f);
	DrawFullScreen(CommandList, BlurPipeline, Bindings.Blur[1], FirstTarget, BlurConstants);
}

void FAmbientOcclusion::Show(nvrhi::ICommandList& CommandList,
	const FAmbientOcclusionTargetBindings& Bindings,
	nvrhi::IFramebuffer* Output,
	const FConstants& Constants) const
{
	DrawFullScreen(CommandList, ShowPipeline, Bindings.Blur[0], Output, Constants);
}
