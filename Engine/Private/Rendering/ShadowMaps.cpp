#include "Rendering/ShadowMaps.h"

#include "Rendering/ShaderLibrary.h"
#include "Rendering/StaticMeshVertex.h"

#include <cstddef>
#include <span>
#include <string>
#include <utility>

namespace
{
	/**
	 * How far a shadow map pushes what it draws away from the light, in multiples of how much the depth changes
	 * from one pixel of the map to the next. Reading a soft shadow compares depths up to two and a half pixels
	 * away from a point, so a sloped surface must be pushed back by more than that not to shadow itself.
	 */
	constexpr float SlopeDepthBias = 4.0f;

	/** NVRHI applies the slope bias only if there is a constant bias too; this is the smallest there is. */
	constexpr int ConstantDepthBias = 1;
}

bool FShadowMaps::Initialize(std::shared_ptr<FRenderContext> InContext)
{
	Context = std::move(InContext);
	nvrhi::IDevice& Device = *Context->GetDevice();

	// The sampler compares the depth it is given with the map's and blends the answers of the four nearest
	// pixels, which is the first step of a soft shadow edge.
	nvrhi::SamplerDesc SamplerDescription;
	SamplerDescription.setAllFilters(true);
	SamplerDescription.setAllAddressModes(nvrhi::SamplerAddressMode::ClampToEdge);
	SamplerDescription.setReductionType(nvrhi::SamplerReductionType::Comparison);
	Sampler = Device.createSampler(SamplerDescription);

	const std::span<const uint32> Code = FShaderLibrary::GetCode("ShadowDepth.vert");
	nvrhi::ShaderDesc ShaderDescription;
	ShaderDescription.shaderType = nvrhi::ShaderType::Vertex;
	ShaderDescription.debugName = "ShadowDepth.vert";
	const nvrhi::ShaderHandle VertexShader = Device.createShader(ShaderDescription, Code.data(), Code.size_bytes());

	// The same slots as in the layout of the scene's data, of which the shadow pass needs only the two buffers.
	// Push constants count as a constant buffer, so they take the slot after the view's.
	PassBindingLayout =
		Device.createBindingLayout(nvrhi::BindingLayoutDesc()
									   .setVisibility(nvrhi::ShaderType::Vertex)
									   .setRegisterSpaceAndDescriptorSet(0)
									   .addItem(nvrhi::BindingLayoutItem::StructuredBuffer_SRV(0))
									   .addItem(nvrhi::BindingLayoutItem::ConstantBuffer(0))
									   .addItem(nvrhi::BindingLayoutItem::PushConstants(1, sizeof(FPassConstants))));
	if (Sampler == nullptr || VertexShader == nullptr || PassBindingLayout == nullptr)
	{
		return false;
	}

	// A shadow map needs only where the vertices are.
	nvrhi::VertexAttributeDesc PositionAttribute;
	PositionAttribute.name = "Position";
	PositionAttribute.format = nvrhi::Format::RGB32_FLOAT;
	PositionAttribute.offset = static_cast<uint32>(offsetof(FStaticMeshVertex, Position));
	PositionAttribute.elementStride = static_cast<uint32>(sizeof(FStaticMeshVertex));
	const nvrhi::InputLayoutHandle InputLayout = Device.createInputLayout(&PositionAttribute, 1, VertexShader);
	if (InputLayout == nullptr)
	{
		return false;
	}

	nvrhi::GraphicsPipelineDesc Description;
	Description.primType = nvrhi::PrimitiveType::TriangleList;
	Description.inputLayout = InputLayout;
	Description.VS = VertexShader;
	Description.bindingLayouts.push_back(PassBindingLayout);
	// Both sides of a surface cast a shadow: a wall seen from behind by the light is still in the way.
	Description.renderState.rasterState.cullMode = nvrhi::RasterCullMode::None;
	Description.renderState.rasterState.depthClipEnable = true;
	Description.renderState.rasterState.depthBias = ConstantDepthBias;
	Description.renderState.rasterState.slopeScaledDepthBias = SlopeDepthBias;
	Description.renderState.depthStencilState.depthTestEnable = true;
	Description.renderState.depthStencilState.depthWriteEnable = true;
	Description.renderState.depthStencilState.depthFunc = nvrhi::ComparisonFunc::LessOrEqual;

	nvrhi::FramebufferInfo TargetFormats;
	TargetFormats.setDepthFormat(Format);
	Pipeline = Device.createGraphicsPipeline(Description, TargetFormats);
	if (Pipeline == nullptr)
	{
		return false;
	}

	// One pixel that nothing is in front of, for scenes without shadows.
	if (!CreateTexture(1, 1))
	{
		return false;
	}

	nvrhi::ICommandList& CommandList = *Context->GetImmediateCommandList();
	CommandList.open();
	const bool bClearDepth = true;
	const bool bClearStencil = false;
	CommandList.clearDepthStencilTexture(Texture, nvrhi::AllSubresources, bClearDepth, FarthestDepth, bClearStencil, 0);
	CommandList.close();
	Device.executeCommandList(&CommandList);
	return true;
}

bool FShadowMaps::BindSceneData(nvrhi::IBuffer* DrawBuffer, nvrhi::IBuffer* ViewBuffer)
{
	const bool bIsCurrent = PassBindings != nullptr && DrawBuffer == BoundDrawBuffer && ViewBuffer == BoundViewBuffer;
	if (bIsCurrent)
	{
		return true;
	}

	nvrhi::BindingSetHandle NewBindings = Context->GetDevice()->createBindingSet(
		nvrhi::BindingSetDesc()
			.addItem(nvrhi::BindingSetItem::StructuredBuffer_SRV(0, DrawBuffer))
			.addItem(nvrhi::BindingSetItem::ConstantBuffer(0, ViewBuffer))
			.addItem(nvrhi::BindingSetItem::PushConstants(1, sizeof(FPassConstants))),
		PassBindingLayout);
	if (NewBindings == nullptr)
	{
		return false;
	}

	PassBindings = std::move(NewBindings);
	BoundDrawBuffer = DrawBuffer;
	BoundViewBuffer = ViewBuffer;
	return true;
}

bool FShadowMaps::Prepare(int32 LayerCount, int32 InMapSize)
{
	const bool bIsEnough =
		Texture != nullptr && InMapSize == MapSize && LayerCount <= static_cast<int32>(LayerTargets.size());
	return bIsEnough || CreateTexture(LayerCount, InMapSize);
}

bool FShadowMaps::CreateTexture(int32 LayerCount, int32 InMapSize)
{
	nvrhi::IDevice& Device = *Context->GetDevice();

	// The maps are drawn into and then read, every picture; NVRHI moves the texture between those uses.
	nvrhi::TextureDesc Description;
	Description.width = static_cast<uint32>(InMapSize);
	Description.height = static_cast<uint32>(InMapSize);
	Description.arraySize = static_cast<uint32>(LayerCount);
	Description.dimension = nvrhi::TextureDimension::Texture2DArray;
	Description.format = Format;
	Description.isRenderTarget = true;
	Description.debugName = "ShadowMaps";
	Description.enableAutomaticStateTracking(nvrhi::ResourceStates::ShaderResource);
	nvrhi::TextureHandle NewTexture = Device.createTexture(Description);
	if (NewTexture == nullptr)
	{
		return false;
	}

	std::vector<nvrhi::FramebufferHandle> NewTargets;
	for (int32 Layer = 0; Layer < LayerCount; ++Layer)
	{
		nvrhi::FramebufferHandle Target = Device.createFramebuffer(nvrhi::FramebufferDesc().setDepthAttachment(
			nvrhi::FramebufferAttachment().setTexture(NewTexture).setArraySlice(static_cast<uint32>(Layer))));
		if (Target == nullptr)
		{
			return false;
		}
		NewTargets.push_back(std::move(Target));
	}

	Texture = std::move(NewTexture);
	LayerTargets = std::move(NewTargets);
	MapSize = InMapSize;
	return true;
}
