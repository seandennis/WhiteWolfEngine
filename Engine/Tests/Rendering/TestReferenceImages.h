#pragma once

#include "CoreMinimal.h"

#include "Core/FileHelper.h"
#include "Rendering/Image.h"
#include "Rendering/ImageDifference.h"
#include "Rendering/ImageFile.h"

#include <doctest/doctest.h>

#include <filesystem>
#include <string>

// Comparing a picture that a test drew with a reference image that is stored in the repository. Two graphics cards
// never draw exactly the same picture, so the comparison allows a small difference; see docs/DECISIONS.md, D-034,
// for the tolerances and for how a reference image is replaced.

/** By how many steps of 255 a channel may differ from the reference before its pixel counts as different. */
inline constexpr int32 ReferenceChannelTolerance = 12;

/** The fraction of the pixels that may differ from the reference by more than the tolerance. */
inline constexpr double ReferenceMaxDifferentPixelFraction = 0.02;

/**
 * Checks a picture against a reference image. If it does not match, or there is no reference image yet, the test
 * fails and the picture is saved in the output directory, where it can be looked at and, if it is right, copied
 * over the reference image.
 *
 * @param ReferenceDirectory Where the reference images are, in the repository.
 * @param OutputDirectory Where a picture that does not match is saved, in the build directory.
 * @param Name The name of the reference image, without ".png".
 * @param OutputSuffix Added to the name of the saved picture, so that a test that checks several pictures against
 * one reference image does not save one over the other.
 */
inline void CheckAgainstReferenceImage(const FImage& Actual,
	const std::filesystem::path& ReferenceDirectory,
	const std::filesystem::path& OutputDirectory,
	const std::string& Name,
	const std::string& OutputSuffix = "")
{
	const std::filesystem::path ReferencePath = ReferenceDirectory / (Name + ".png");
	const std::filesystem::path OutputPath = OutputDirectory / (Name + OutputSuffix + ".png");

	FImage Reference;
	std::string Error;
	const bool bHasReference = FImageFile::Load(ReferencePath, Reference, Error);
	const FImageDifference Difference = FImageDifference::Measure(Actual, Reference, ReferenceChannelTolerance);
	const bool bMatches = bHasReference && Difference.IsWithin(ReferenceMaxDifferentPixelFraction);
	if (!bMatches)
	{
		std::string SaveError;
		const bool bSaved = FImageFile::SavePng(Actual, OutputPath, SaveError);
		INFO("Reference image: ", FFileHelper::PathToUtf8(ReferencePath));
		INFO("What was drawn: ", bSaved ? FFileHelper::PathToUtf8(OutputPath) : SaveError);
		INFO("Difference: ", bHasReference ? Difference.Describe() : Error);
		CHECK(bMatches);
	}
}
