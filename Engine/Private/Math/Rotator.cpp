#include "Math/Rotator.h"

#include "Math/Vector.h"

#include <glm/gtc/quaternion.hpp>
#include <glm/trigonometric.hpp>

#include <algorithm>
#include <cmath>

namespace
{
	const FVector3f RightAxis(1.0f, 0.0f, 0.0f);
	const FVector3f UpAxis(0.0f, 1.0f, 0.0f);
	const FVector3f BackAxis(0.0f, 0.0f, 1.0f);

	/**
	 * The sine of the pitch beyond which a rotation counts as straight up or down, about a twelfth of a degree
	 * from it. Closer than that, the rounding of a quaternion's single-precision numbers is too much of what is
	 * left to tell yaw and roll apart by, or to say how far from vertical the rotation is.
	 */
	constexpr double VerticalSinPitch = 0.999999;
}

FQuat FRotator::ToQuat() const
{
	const FQuat YawTurn = glm::angleAxis(glm::radians(Yaw), UpAxis);
	const FQuat PitchTurn = glm::angleAxis(glm::radians(Pitch), RightAxis);
	const FQuat RollTurn = glm::angleAxis(glm::radians(Roll), BackAxis);
	return glm::normalize(YawTurn * PitchTurn * RollTurn);
}

FRotator FRotator::FromQuat(const FQuat& Rotation)
{
	const double W = static_cast<double>(Rotation.w);
	const double X = static_cast<double>(Rotation.x);
	const double Y = static_cast<double>(Rotation.y);
	const double Z = static_cast<double>(Rotation.z);

	// The angles are read from the elements of the matrix of the rotation, which is the product of the matrices of
	// yaw, pitch and roll. With s and c for the sine and cosine of each angle, its rows are:
	//
	//     cy cz + sy sp sz    sy sp cz - cy sz    sy cp
	//     cp sz               cp cz               -sp
	//     cy sp sz - sy cz    sy sz + cy sp cz    cy cp
	const double SinPitch = std::clamp(2.0 * (W * X - Y * Z), -1.0, 1.0);

	FRotator Rotator;
	const bool bIsVertical = std::abs(SinPitch) > VerticalSinPitch;
	if (bIsVertical)
	{
		// The third column and the second row are all but zero. With no roll, the first column is (cy, 0, -sy).
		const double SinYaw = -2.0 * (X * Z - W * Y);
		const double CosYaw = 1.0 - 2.0 * (Y * Y + Z * Z);
		Rotator.Pitch = std::copysign(90.0f, static_cast<float>(SinPitch));
		Rotator.Yaw = static_cast<float>(glm::degrees(std::atan2(SinYaw, CosYaw)));
		Rotator.Roll = 0.0f;
	}
	else
	{
		Rotator.Pitch = static_cast<float>(glm::degrees(std::asin(SinPitch)));

		const double SinYawCosPitch = 2.0 * (X * Z + W * Y);
		const double CosYawCosPitch = 1.0 - 2.0 * (X * X + Y * Y);
		Rotator.Yaw = static_cast<float>(glm::degrees(std::atan2(SinYawCosPitch, CosYawCosPitch)));

		const double CosPitchSinRoll = 2.0 * (X * Y + W * Z);
		const double CosPitchCosRoll = 1.0 - 2.0 * (X * X + Z * Z);
		Rotator.Roll = static_cast<float>(glm::degrees(std::atan2(CosPitchSinRoll, CosPitchCosRoll)));
	}
	return Rotator;
}
