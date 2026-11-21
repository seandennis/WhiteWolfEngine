#include "Rendering/EnvironmentCache.h"

#include "Object/ObjectGlobals.h"
#include "Rendering/HdrImage.h"
#include "Rendering/ShaderLibrary.h"

#include <glm/gtc/packing.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <span>
#include <string>
#include <string_view>
#include <utility>

namespace
{
	/** A pixel of a 16-bit floating-point texture: red, green, blue, alpha. */
	using FHalfPixel = std::array<uint16, 4>;

	/** The format environments are kept in on the graphics card. */
	constexpr nvrhi::Format EnvironmentFormat = nvrhi::Format::RGBA16_FLOAT;

	/** The most light a 16-bit float holds; the same as MaxSceneLight in SceneData.glsl. */
	constexpr float MaxLight = 65000.0f;

	constexpr uint32 CubeFaceCount = 6;

	/** What PrefilterEnvironment.frag is told about the face it draws. Must match FPrefilterConstants there. */
	struct FPrefilterConstants
	{
		int32 Face = 0;
		float Roughness = 0.0f;
		float FaceSize = 1.0f;
		float PicturePixelCount = 1.0f;
	};

	nvrhi::ShaderHandle CreateShader(nvrhi::IDevice& Device, std::string_view Name, nvrhi::ShaderType Type)
	{
		const std::span<const uint32> Code = FShaderLibrary::GetCode(Name);

		nvrhi::ShaderDesc Description;
		Description.shaderType = Type;
		Description.debugName = std::string(Name);
		return Device.createShader(Description, Code.data(), Code.size_bytes());
	}

	FHalfPixel ToHalfPixel(const FLinearColor& Color)
	{
		return {glm::packHalf1x16(std::clamp(Color.R, 0.0f, MaxLight)),
			glm::packHalf1x16(std::clamp(Color.G, 0.0f, MaxLight)),
			glm::packHalf1x16(std::clamp(Color.B, 0.0f, MaxLight)),
			glm::packHalf1x16(1.0f)};
	}

	/** Returns the picture at half the width and height, each pixel the average of the four it covers. */
	FHdrImage HalveSize(const FHdrImage& Source)
	{
		const int32 Width = std::max(Source.GetWidth() / 2, 1);
		const int32 Height = std::max(Source.GetHeight() / 2, 1);
		FHdrImage Result(Width, Height);
		for (int32 Y = 0; Y < Height; ++Y)
		{
			const int32 FirstRow = std::min(Y * 2, Source.GetHeight() - 1);
			const int32 SecondRow = std::min(Y * 2 + 1, Source.GetHeight() - 1);
			for (int32 X = 0; X < Width; ++X)
			{
				const int32 FirstColumn = std::min(X * 2, Source.GetWidth() - 1);
				const int32 SecondColumn = std::min(X * 2 + 1, Source.GetWidth() - 1);
				const FLinearColor& A = Source.GetPixel(FirstColumn, FirstRow);
				const FLinearColor& B = Source.GetPixel(SecondColumn, FirstRow);
				const FLinearColor& C = Source.GetPixel(FirstColumn, SecondRow);
				const FLinearColor& D = Source.GetPixel(SecondColumn, SecondRow);
				Result.SetPixel(X,
					Y,
					FLinearColor((A.R + B.R + C.R + D.R) * 0.25f,
						(A.G + B.G + C.G + D.G) * 0.25f,
						(A.B + B.B + C.B + D.B) * 0.25f));
			}
		}
		return Result;
	}

	/** Writes one level of a picture of light into a texture. */
	void WriteLevel(nvrhi::ICommandList& CommandList, nvrhi::ITexture* Texture, uint32 Level, const FHdrImage& Image)
	{
		std::vector<FHalfPixel> Pixels;
		Pixels.reserve(Image.GetPixels().size());
		for (const FLinearColor& Pixel : Image.GetPixels())
		{
			Pixels.push_back(ToHalfPixel(Pixel));
		}

		const std::size_t BytesPerRow = static_cast<std::size_t>(Image.GetWidth()) * sizeof(FHalfPixel);
		CommandList.writeTexture(Texture, 0, Level, Pixels.data(), BytesPerRow);
	}

	uint32 CountLevels(int32 Width, int32 Height)
	{
		uint32 LevelCount = 1;
		for (int32 Side = std::max(Width, Height); Side > 1; Side /= 2)
		{
			++LevelCount;
		}
		return LevelCount;
	}
}

bool FEnvironmentCache::Initialize(std::shared_ptr<FRenderContext> InContext)
{
	Context = std::move(InContext);
	nvrhi::IDevice& Device = *Context->GetDevice();

	// Reflections are read smoothly, across the edges of the cube's faces and between its levels. The picture
	// wraps around from its right edge to its left, and its top and bottom rows are the poles.
	nvrhi::SamplerDesc ReflectionsDescription;
	ReflectionsDescription.setAllFilters(true);
	ReflectionsDescription.setAllAddressModes(nvrhi::SamplerAddressMode::ClampToEdge);
	ReflectionsSampler = Device.createSampler(ReflectionsDescription);

	nvrhi::SamplerDesc PictureDescription;
	PictureDescription.setAllFilters(true);
	PictureDescription.setAddressU(nvrhi::SamplerAddressMode::Repeat);
	PictureDescription.setAddressV(nvrhi::SamplerAddressMode::ClampToEdge);
	PictureSampler = Device.createSampler(PictureDescription);

	PrefilterBindingLayout = Device.createBindingLayout(
		nvrhi::BindingLayoutDesc()
			.setVisibility(nvrhi::ShaderType::Pixel)
			.addItem(nvrhi::BindingLayoutItem::Texture_SRV(0))
			.addItem(nvrhi::BindingLayoutItem::Sampler(0))
			.addItem(nvrhi::BindingLayoutItem::PushConstants(0, sizeof(FPrefilterConstants))));

	const nvrhi::ShaderHandle VertexShader = CreateShader(Device, "FullScreen.vert", nvrhi::ShaderType::Vertex);
	const nvrhi::ShaderHandle FragmentShader =
		CreateShader(Device, "PrefilterEnvironment.frag", nvrhi::ShaderType::Pixel);
	if (ReflectionsSampler == nullptr || PictureSampler == nullptr || PrefilterBindingLayout == nullptr ||
		VertexShader == nullptr || FragmentShader == nullptr)
	{
		return false;
	}

	nvrhi::GraphicsPipelineDesc PipelineDescription;
	PipelineDescription.primType = nvrhi::PrimitiveType::TriangleList;
	PipelineDescription.VS = VertexShader;
	PipelineDescription.PS = FragmentShader;
	PipelineDescription.bindingLayouts.push_back(PrefilterBindingLayout);
	PipelineDescription.renderState.rasterState.cullMode = nvrhi::RasterCullMode::None;
	PipelineDescription.renderState.depthStencilState.depthTestEnable = false;
	PipelineDescription.renderState.depthStencilState.depthWriteEnable = false;

	nvrhi::FramebufferInfo TargetFormats;
	TargetFormats.addColorFormat(EnvironmentFormat);
	PrefilterPipeline = Device.createGraphicsPipeline(PipelineDescription, TargetFormats);
	if (PrefilterPipeline == nullptr)
	{
		return false;
	}

	// The black environment: one black pixel as the picture and one on each face of the cube.
	nvrhi::TextureDesc BlackPictureDescription;
	BlackPictureDescription.format = EnvironmentFormat;
	BlackPictureDescription.debugName = "BlackEnvironmentPicture";
	BlackPictureDescription.initialState = nvrhi::ResourceStates::ShaderResource;
	BlackPictureDescription.keepInitialState = true;
	Black.Picture = Device.createTexture(BlackPictureDescription);

	nvrhi::TextureDesc BlackReflectionsDescription = BlackPictureDescription;
	BlackReflectionsDescription.dimension = nvrhi::TextureDimension::TextureCube;
	BlackReflectionsDescription.arraySize = CubeFaceCount;
	BlackReflectionsDescription.debugName = "BlackEnvironmentReflections";
	Black.Reflections = Device.createTexture(BlackReflectionsDescription);
	if (Black.Picture == nullptr || Black.Reflections == nullptr)
	{
		return false;
	}

	const FHalfPixel BlackPixel = ToHalfPixel(FLinearColor(0.0f, 0.0f, 0.0f));
	nvrhi::ICommandList& CommandList = *Context->GetImmediateCommandList();
	CommandList.open();
	CommandList.writeTexture(Black.Picture, 0, 0, BlackPixel.data(), sizeof(FHalfPixel));
	for (uint32 Face = 0; Face < CubeFaceCount; ++Face)
	{
		CommandList.writeTexture(Black.Reflections, Face, 0, BlackPixel.data(), sizeof(FHalfPixel));
	}
	CommandList.close();
	Device.executeCommandList(&CommandList);
	Black.bNeedsUpload = false;
	return true;
}

const FEnvironmentResource* FEnvironmentCache::FindOrCreate(WEnvironmentMap* Environment)
{
	if (!IsValid(Environment) || !Environment->HasImage())
	{
		return &Black;
	}

	const auto Existing = Resources.find(Environment);
	if (Existing != Resources.end() && Existing->second.ImageVersion == Environment->GetImageVersion())
	{
		return &Existing->second;
	}

	FEnvironmentResource Resource;
	Resource.Environment = Environment;
	Resource.ImageVersion = Environment->GetImageVersion();
	if (!CreateResource(Environment->GetImage(), Resource))
	{
		return nullptr;
	}

	return &Resources.insert_or_assign(Environment, std::move(Resource)).first->second;
}

bool FEnvironmentCache::CreateResource(const FHdrImage& Image, FEnvironmentResource& OutResource) const
{
	nvrhi::IDevice& Device = *Context->GetDevice();

	nvrhi::TextureDesc PictureDescription;
	PictureDescription.width = static_cast<uint32>(Image.GetWidth());
	PictureDescription.height = static_cast<uint32>(Image.GetHeight());
	PictureDescription.mipLevels = CountLevels(Image.GetWidth(), Image.GetHeight());
	PictureDescription.format = EnvironmentFormat;
	PictureDescription.debugName = "EnvironmentPicture";
	PictureDescription.initialState = nvrhi::ResourceStates::ShaderResource;
	PictureDescription.keepInitialState = true;
	OutResource.Picture = Device.createTexture(PictureDescription);
	OutResource.PicturePixelCount = static_cast<float>(Image.GetWidth()) * static_cast<float>(Image.GetHeight());
	OutResource.PictureHeight = Image.GetHeight();

	// The cube map is drawn to when it is made and read ever after; NVRHI moves it between those uses.
	nvrhi::TextureDesc ReflectionsDescription;
	ReflectionsDescription.width = static_cast<uint32>(ReflectionSize);
	ReflectionsDescription.height = static_cast<uint32>(ReflectionSize);
	ReflectionsDescription.mipLevels = static_cast<uint32>(ReflectionLevelCount);
	ReflectionsDescription.dimension = nvrhi::TextureDimension::TextureCube;
	ReflectionsDescription.arraySize = CubeFaceCount;
	ReflectionsDescription.format = EnvironmentFormat;
	ReflectionsDescription.isRenderTarget = true;
	ReflectionsDescription.debugName = "EnvironmentReflections";
	ReflectionsDescription.enableAutomaticStateTracking(nvrhi::ResourceStates::ShaderResource);
	OutResource.Reflections = Device.createTexture(ReflectionsDescription);
	if (OutResource.Picture == nullptr || OutResource.Reflections == nullptr)
	{
		return false;
	}

	OutResource.PictureBindings =
		Device.createBindingSet(nvrhi::BindingSetDesc()
									.addItem(nvrhi::BindingSetItem::Texture_SRV(0, OutResource.Picture))
									.addItem(nvrhi::BindingSetItem::Sampler(0, PictureSampler))
									.addItem(nvrhi::BindingSetItem::PushConstants(0, sizeof(FPrefilterConstants))),
			PrefilterBindingLayout);
	if (OutResource.PictureBindings == nullptr)
	{
		return false;
	}

	// One target per face of each level, in the order they are drawn: level by level, face by face.
	for (uint32 Level = 0; Level < static_cast<uint32>(ReflectionLevelCount); ++Level)
	{
		for (uint32 Face = 0; Face < CubeFaceCount; ++Face)
		{
			nvrhi::FramebufferHandle Target = Device.createFramebuffer(
				nvrhi::FramebufferDesc().addColorAttachment(nvrhi::FramebufferAttachment()
																.setTexture(OutResource.Reflections)
																.setArraySlice(Face)
																.setMipLevel(Level)));
			if (Target == nullptr)
			{
				return false;
			}
			OutResource.ReflectionTargets.push_back(std::move(Target));
		}
	}

	return true;
}

void FEnvironmentCache::ReleaseUnused()
{
	std::erase_if(Resources,
		[](const std::pair<const WEnvironmentMap* const, FEnvironmentResource>& Entry)
		{ return Entry.second.Environment.Get() == nullptr; });
}

int32 FEnvironmentCache::ProcessPending(nvrhi::ICommandList& CommandList)
{
	int32 ProcessedCount = 0;
	for (std::pair<const WEnvironmentMap* const, FEnvironmentResource>& Entry : Resources)
	{
		FEnvironmentResource& Resource = Entry.second;
		const WEnvironmentMap* const Environment = Resource.Environment.Get();
		const bool bIsToBeProcessed =
			Resource.bNeedsUpload && Environment != nullptr && Environment->GetImageVersion() == Resource.ImageVersion;
		if (!bIsToBeProcessed)
		{
			continue;
		}

		// The picture, and each smaller version made from the one before.
		const uint32 PictureLevelCount = Resource.Picture->getDesc().mipLevels;
		FHdrImage LevelImage = Environment->GetImage();
		for (uint32 Level = 0; Level < PictureLevelCount; ++Level)
		{
			WriteLevel(CommandList, Resource.Picture, Level, LevelImage);
			if (Level + 1 < PictureLevelCount)
			{
				LevelImage = HalveSize(LevelImage);
			}
		}

		// The reflections: every face of every level is drawn from the picture, each level for a rougher surface.
		nvrhi::GraphicsState State;
		State.pipeline = PrefilterPipeline;
		State.bindings.push_back(Resource.PictureBindings);
		State.viewport.viewports.resize(1);
		State.viewport.scissorRects.resize(1);
		for (uint32 Level = 0; Level < static_cast<uint32>(ReflectionLevelCount); ++Level)
		{
			const float FaceSize = static_cast<float>(ReflectionSize >> Level);
			const nvrhi::Viewport Viewport(FaceSize, FaceSize);
			State.viewport.viewports[0] = Viewport;
			State.viewport.scissorRects[0] = nvrhi::Rect(Viewport);

			for (uint32 Face = 0; Face < CubeFaceCount; ++Face)
			{
				State.framebuffer = Resource.ReflectionTargets[Level * CubeFaceCount + Face];
				CommandList.setGraphicsState(State);

				FPrefilterConstants Constants;
				Constants.Face = static_cast<int32>(Face);
				Constants.Roughness = static_cast<float>(Level) / static_cast<float>(ReflectionLevelCount - 1);
				Constants.FaceSize = FaceSize;
				Constants.PicturePixelCount = Resource.PicturePixelCount;
				CommandList.setPushConstants(&Constants, sizeof(FPrefilterConstants));

				const uint32 FullScreenTriangleVertexCount = 3;
				CommandList.draw(nvrhi::DrawArguments().setVertexCount(FullScreenTriangleVertexCount));
			}
		}

		Resource.ReflectionTargets.clear();
		Resource.PictureBindings = nullptr;
		Resource.bNeedsUpload = false;
		++ProcessedCount;
	}

	return ProcessedCount;
}
