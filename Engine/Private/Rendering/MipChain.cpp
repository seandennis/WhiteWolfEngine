#include "Rendering/MipChain.h"

#include "Math/Color.h"
#include "Math/Srgb.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <utility>

namespace
{
	/** The amount of light each of the 256 sRGB-encoded values stands for. */
	std::array<float, 256> MakeSrgbToLinearTable()
	{
		std::array<float, 256> Table{};
		for (std::size_t Value = 0; Value < Table.size(); ++Value)
		{
			Table[Value] = FSrgb::Decode(static_cast<float>(Value) / 255.0f);
		}
		return Table;
	}

	uint8 LinearToSrgb(float Linear)
	{
		return static_cast<uint8>(std::lround(std::clamp(FSrgb::Encode(Linear), 0.0f, 1.0f) * 255.0f));
	}

	uint8 RoundToByte(float Value)
	{
		return static_cast<uint8>(std::lround(std::clamp(Value, 0.0f, 255.0f)));
	}

	/** Returns the picture at half the width and height, each pixel the average of the pixels it covers. */
	FImage HalveSize(const FImage& Source, ETextureColorSpace ColorSpace)
	{
		static const std::array<float, 256> SrgbToLinear = MakeSrgbToLinearTable();

		const int32 Width = std::max(Source.GetWidth() / 2, 1);
		const int32 Height = std::max(Source.GetHeight() / 2, 1);
		FImage Result(Width, Height);

		for (int32 Y = 0; Y < Height; ++Y)
		{
			// A picture of odd size has one more row or column than its half covers twice; the last pixel of the
			// half then also takes in that extra row or column, so that nothing of the picture is left out.
			const int32 FirstRow = Y * 2;
			const int32 LastRow =
				Y == Height - 1 ? Source.GetHeight() - 1 : std::min(FirstRow + 1, Source.GetHeight() - 1);
			for (int32 X = 0; X < Width; ++X)
			{
				const int32 FirstColumn = X * 2;
				const int32 LastColumn =
					X == Width - 1 ? Source.GetWidth() - 1 : std::min(FirstColumn + 1, Source.GetWidth() - 1);

				float Red = 0.0f;
				float Green = 0.0f;
				float Blue = 0.0f;
				float Alpha = 0.0f;
				for (int32 Row = FirstRow; Row <= LastRow; ++Row)
				{
					for (int32 Column = FirstColumn; Column <= LastColumn; ++Column)
					{
						const FColor& Pixel = Source.GetPixel(Column, Row);
						if (ColorSpace == ETextureColorSpace::Srgb)
						{
							Red += SrgbToLinear[Pixel.R];
							Green += SrgbToLinear[Pixel.G];
							Blue += SrgbToLinear[Pixel.B];
						}
						else
						{
							Red += static_cast<float>(Pixel.R);
							Green += static_cast<float>(Pixel.G);
							Blue += static_cast<float>(Pixel.B);
						}
						Alpha += static_cast<float>(Pixel.A);
					}
				}

				const float PixelCount = static_cast<float>((LastRow - FirstRow + 1) * (LastColumn - FirstColumn + 1));
				const FColor Average = ColorSpace == ETextureColorSpace::Srgb ? FColor(LinearToSrgb(Red / PixelCount),
																					LinearToSrgb(Green / PixelCount),
																					LinearToSrgb(Blue / PixelCount),
																					RoundToByte(Alpha / PixelCount))
																			  : FColor(RoundToByte(Red / PixelCount),
																					RoundToByte(Green / PixelCount),
																					RoundToByte(Blue / PixelCount),
																					RoundToByte(Alpha / PixelCount));
				Result.SetPixel(X, Y, Average);
			}
		}

		return Result;
	}
}

std::vector<FImage> FMipChain::Build(const FImage& Image, ETextureColorSpace ColorSpace)
{
	std::vector<FImage> Chain;
	if (Image.IsEmpty())
	{
		return Chain;
	}

	Chain.push_back(Image);
	while (Chain.back().GetWidth() > 1 || Chain.back().GetHeight() > 1)
	{
		FImage Smaller = HalveSize(Chain.back(), ColorSpace);
		Chain.push_back(std::move(Smaller));
	}

	return Chain;
}
