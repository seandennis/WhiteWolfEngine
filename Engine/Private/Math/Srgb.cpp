#include "Math/Srgb.h"

#include <cmath>

namespace
{
	// The numbers of the sRGB standard (IEC 61966-2-1): a straight piece near black, and a power curve above it.
	constexpr float LinearPieceSlope = 12.92f;
	constexpr float LinearPieceEnd = 0.0031308f;
	constexpr float EncodedLinearPieceEnd = 0.04045f;
	constexpr float CurveScale = 1.055f;
	constexpr float CurveOffset = 0.055f;
	constexpr float CurveExponent = 2.4f;
}

float FSrgb::Encode(float Linear)
{
	const float Amount = std::abs(Linear);
	const float Encoded = Amount <= LinearPieceEnd ? Amount * LinearPieceSlope
												   : CurveScale * std::pow(Amount, 1.0f / CurveExponent) - CurveOffset;
	return std::copysign(Encoded, Linear);
}

float FSrgb::Decode(float Encoded)
{
	const float Value = std::abs(Encoded);
	const float Linear = Value <= EncodedLinearPieceEnd ? Value / LinearPieceSlope
														: std::pow((Value + CurveOffset) / CurveScale, CurveExponent);
	return std::copysign(Linear, Encoded);
}
