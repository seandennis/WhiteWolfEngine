#include "Ui/AssetPicker.h"

#include "Assets/AssetManager.h"
#include "Assets/AssetPath.h"
#include "Editor.h"
#include "Math/Color.h"
#include "Object/Class.h"
#include "Rendering/Texture.h"
#include "Ui/ImGuiLibrary.h"
#include "Ui/UiAsset.h"
#include "Ui/UiCommands.h"
#include "Ui/UiFrame.h"
#include "Ui/UiItems.h"
#include "Ui/UiTheme.h"

#include <algorithm>
#include <cfloat>
#include <cstddef>
#include <format>
#include <string_view>
#include <utility>

namespace
{
	/** What the line says that chooses no asset. */
	constexpr const char* NoAssetLabel = "None";

	/** What the lines below a file say that holds nothing for the property. */
	constexpr const char* NothingLabel = "Nothing of this kind";

	/** How many lines the list shows before it is scrolled. */
	constexpr float ListLineCount = 16.0f;

	/** Returns the part of an asset path that says which asset of its file it is, or the whole path if it has none. */
	std::string_view GetKey(std::string_view Path)
	{
		const std::size_t KeyStart = Path.find('#');
		return KeyStart == std::string_view::npos ? Path : Path.substr(KeyStart + 1);
	}
}

std::optional<std::string> FAssetPicker::Draw(FUiFrame& Frame, const WClass& AssetClass, const std::string& ItemName)
{
	// The button is that of a list without a field: an arrow. Such a list is placed anew in every frame, below
	// its button or above it and to the side that has the room, so one that grows when a file is opened stays on
	// the surface. It opens towards the left: the button is at the end of its row, and often at the window's edge.
	// A list of many files is scrolled rather than as long as the window.
	const float MaxHeight = ImGui::GetTextLineHeightWithSpacing() * ListLineCount;
	ImGui::SetNextWindowSizeConstraints(ImVec2(0.0f, 0.0f), ImVec2(FLT_MAX, MaxHeight));
	constexpr ImGuiComboFlags Flags = ImGuiComboFlags_NoPreview | ImGuiComboFlags_PopupAlignLeft;
	std::optional<std::string> Chosen;
	if (ImGui::BeginCombo("##Choose", nullptr, Flags))
	{
		Chosen = DrawLines(Frame, AssetClass, ItemName);
		ImGui::EndCombo();
	}

	// The list is a window of its own; ending it makes the button the last item again.
	ImGui::SetItemTooltip("%s", "Choose from the assets of the project");
	Frame.Items.NameLastItem(std::format("{}/Choose", ItemName));
	return Chosen;
}

std::optional<std::string> FAssetPicker::DrawLines(
	FUiFrame& Frame, const WClass& AssetClass, const std::string& ItemName)
{
	// Which files there are is found out when the list opens, and not again in every frame that it is open.
	const bool bIsOpening = ImGui::IsWindowAppearing();
	if (bIsOpening)
	{
		Files.clear();
		FilesLookedInto.clear();
		for (std::string& File : FUiCommands::ListAssetFiles(Frame.Editor))
		{
			if (FAssetManager::CanHoldAssetsOfClass(File, AssetClass))
			{
				Files.push_back(std::move(File));
			}
		}
	}

	std::optional<std::string> Chosen;
	const auto AddChoice = [&Frame, &ItemName, &Chosen](const std::string& Path, std::string_view Label)
	{
		// The part after ## is what Dear ImGui knows the line by: the whole path, whatever the line shows of it.
		if (ImGui::Selectable(std::format("{}##{}", Label, Path).c_str()))
		{
			Chosen = Path;
		}
		Frame.Items.NameLastItem(std::format("{}/Assets/{}", ItemName, Path));
	};

	if (ImGui::Selectable(NoAssetLabel))
	{
		Chosen = std::string();
	}
	Frame.Items.NameLastItem(std::format("{}/Assets/None", ItemName));
	if (!Files.empty())
	{
		ImGui::Separator();
	}

	for (const std::string& File : Files)
	{
		if (FAssetManager::IsOneAsset(File))
		{
			// The file is the asset. A picture is also there as a data texture, for what is no colour.
			AddChoice(File, File);
			const bool bIsPicture = FAssetManager::CanHoldAssetsOfClass(File, *WTexture::StaticClass());
			if (bIsPicture)
			{
				const std::string DataTexture =
					FAssetPath{File, std::string(FAssetManager::LinearTextureKey)}.ToString();
				AddChoice(DataTexture, DataTexture);
			}
			continue;
		}

		// A file of several assets is a line that a click opens, and it is looked into then. It is no menu: the
		// list of a menu opens under a mouse that only passes over its line, and a model may be large. The library
		// remembers which lines were open by the list, and all lists of this kind are one list to it: every line
		// is closed when the list opens, whatever was opened the last time.
		if (bIsOpening)
		{
			ImGui::SetNextItemOpen(false);
		}
		const bool bIsOpen = ImGui::TreeNodeEx(File.c_str(), ImGuiTreeNodeFlags_SpanAvailWidth);
		Frame.Items.NameLastItem(std::format("{}/Assets/{}", ItemName, File));
		if (bIsOpen)
		{
			const FFileAssets& Found = FindAssets(Frame.Editor, File, AssetClass);
			if (!Found.Error.empty())
			{
				const FColor ErrorColor = FUiTheme::GetErrorColor();
				ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(ErrorColor.R, ErrorColor.G, ErrorColor.B, ErrorColor.A));
				ImGui::TextUnformatted(Found.Error.c_str());
				ImGui::PopStyleColor();
				Frame.Items.NameLastItem(std::format("{}/Assets/{}/Error", ItemName, File));
			}
			else if (Found.Paths.empty())
			{
				ImGui::TextDisabled("%s", NothingLabel);
				Frame.Items.NameLastItem(std::format("{}/Assets/{}/Nothing", ItemName, File));
			}
			for (const std::string& Path : Found.Paths)
			{
				AddChoice(Path, GetKey(Path));
			}
			ImGui::TreePop();
		}
	}
	return Chosen;
}

const FAssetPicker::FFileAssets& FAssetPicker::FindAssets(
	FEditor& Editor, const std::string& File, const WClass& AssetClass)
{
	const auto Known = std::ranges::find(FilesLookedInto, File, &FFileAssets::File);
	if (Known != FilesLookedInto.end())
	{
		return *Known;
	}

	FFileAssets Found;
	Found.File = File;
	if (const std::optional<std::vector<FUiAsset>> Assets = FUiCommands::DescribeAssetFile(Editor, File, Found.Error))
	{
		for (const FUiAsset& Asset : *Assets)
		{
			const WClass* const Class = WClass::FindByName(Asset.ClassName);
			if (Class != nullptr && Class->IsChildOf(&AssetClass))
			{
				Found.Paths.push_back(Asset.Path);
			}
		}
	}
	FilesLookedInto.push_back(std::move(Found));
	return FilesLookedInto.back();
}
