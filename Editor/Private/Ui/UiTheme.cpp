#include "Ui/UiTheme.h"

#include "Ui/ImGuiLibrary.h"

namespace
{
	/** Returns a colour from its sRGB bytes, as it would be written in a style sheet. */
	constexpr ImVec4 Rgb(int32 Red, int32 Green, int32 Blue, float Alpha = 1.0f)
	{
		return ImVec4(static_cast<float>(Red) / 255.0f,
			static_cast<float>(Green) / 255.0f,
			static_cast<float>(Blue) / 255.0f,
			Alpha);
	}

	// From the darkest, which lies behind everything, to the lightest, which is under the mouse.
	constexpr ImVec4 Backdrop = Rgb(21, 21, 21);
	constexpr ImVec4 Recess = Rgb(15, 15, 15);
	constexpr ImVec4 Panel = Rgb(36, 36, 36);
	constexpr ImVec4 Raised = Rgb(47, 47, 47);
	constexpr ImVec4 Control = Rgb(56, 56, 56);
	constexpr ImVec4 Hovered = Rgb(87, 87, 87);
	constexpr ImVec4 Outline = Rgb(10, 10, 10);

	constexpr ImVec4 Text = Rgb(192, 192, 192);
	constexpr ImVec4 TextDimmed = Rgb(109, 109, 109);

	// The accent: what is selected, focused or about to be dropped on.
	constexpr ImVec4 Accent = Rgb(0, 112, 224);
	constexpr ImVec4 AccentHovered = Rgb(14, 134, 255);
	constexpr ImVec4 AccentDimmed = Rgb(64, 87, 111);

	/** The height of the letters of the user interface, in pixels. */
	constexpr float FontSize = 15.0f;
}

void FUiTheme::AddFonts()
{
	// The font that comes with Dear ImGui, in its scalable form: its letters are drawn for whatever size is asked.
	ImGui::GetIO().Fonts->AddFontDefaultVector();
}

void FUiTheme::Apply(float Scale)
{
	// Started from the library's own style every time, so that applying a scale twice does not scale twice.
	ImGuiStyle Style;
	ImGui::StyleColorsDark(&Style);

	Style.WindowPadding = ImVec2(8.0f, 8.0f);
	Style.FramePadding = ImVec2(6.0f, 4.0f);
	Style.ItemSpacing = ImVec2(8.0f, 5.0f);
	Style.ItemInnerSpacing = ImVec2(6.0f, 4.0f);
	Style.IndentSpacing = 16.0f;
	Style.ScrollbarSize = 12.0f;
	Style.GrabMinSize = 10.0f;

	// Panels are docked against each other, so they are square; what floats or is clicked is slightly rounded.
	Style.WindowRounding = 0.0f;
	Style.ChildRounding = 0.0f;
	Style.FrameRounding = 3.0f;
	Style.PopupRounding = 4.0f;
	Style.ScrollbarRounding = 6.0f;
	Style.GrabRounding = 3.0f;
	Style.TabRounding = 4.0f;

	Style.WindowBorderSize = 1.0f;
	Style.FrameBorderSize = 0.0f;
	Style.PopupBorderSize = 1.0f;
	Style.TabBarBorderSize = 1.0f;
	Style.TabBarOverlineSize = 2.0f;
	Style.DockingSeparatorSize = 2.0f;

	// A panel is closed from the Window menu and moved by its tab; it needs no button for a menu of its own.
	Style.WindowMenuButtonPosition = ImGuiDir_None;

	ImVec4* const Colors = Style.Colors;
	Colors[ImGuiCol_Text] = Text;
	Colors[ImGuiCol_TextDisabled] = TextDimmed;
	Colors[ImGuiCol_WindowBg] = Panel;
	Colors[ImGuiCol_ChildBg] = Rgb(0, 0, 0, 0.0f);
	Colors[ImGuiCol_PopupBg] = Raised;
	Colors[ImGuiCol_Border] = Outline;
	Colors[ImGuiCol_BorderShadow] = Rgb(0, 0, 0, 0.0f);
	Colors[ImGuiCol_FrameBg] = Recess;
	Colors[ImGuiCol_FrameBgHovered] = Rgb(26, 26, 26);
	Colors[ImGuiCol_FrameBgActive] = Rgb(26, 26, 26);
	Colors[ImGuiCol_TitleBg] = Backdrop;
	Colors[ImGuiCol_TitleBgActive] = Backdrop;
	Colors[ImGuiCol_TitleBgCollapsed] = Backdrop;
	Colors[ImGuiCol_MenuBarBg] = Backdrop;
	Colors[ImGuiCol_ScrollbarBg] = Rgb(26, 26, 26);
	Colors[ImGuiCol_ScrollbarGrab] = Control;
	Colors[ImGuiCol_ScrollbarGrabHovered] = Hovered;
	Colors[ImGuiCol_ScrollbarGrabActive] = Rgb(128, 128, 128);
	Colors[ImGuiCol_CheckMark] = Accent;
	Colors[ImGuiCol_SliderGrab] = Hovered;
	Colors[ImGuiCol_SliderGrabActive] = Accent;
	Colors[ImGuiCol_Button] = Control;
	Colors[ImGuiCol_ButtonHovered] = Hovered;
	Colors[ImGuiCol_ButtonActive] = Raised;
	Colors[ImGuiCol_Header] = Accent;
	Colors[ImGuiCol_HeaderHovered] = Control;
	Colors[ImGuiCol_HeaderActive] = AccentHovered;
	Colors[ImGuiCol_Separator] = Control;
	Colors[ImGuiCol_SeparatorHovered] = AccentHovered;
	Colors[ImGuiCol_SeparatorActive] = Accent;
	Colors[ImGuiCol_ResizeGrip] = Rgb(0, 0, 0, 0.0f);
	Colors[ImGuiCol_ResizeGripHovered] = AccentHovered;
	Colors[ImGuiCol_ResizeGripActive] = Accent;
	Colors[ImGuiCol_InputTextCursor] = Text;
	Colors[ImGuiCol_Tab] = Backdrop;
	Colors[ImGuiCol_TabHovered] = Raised;
	Colors[ImGuiCol_TabSelected] = Panel;
	Colors[ImGuiCol_TabSelectedOverline] = Accent;
	Colors[ImGuiCol_TabDimmed] = Backdrop;
	Colors[ImGuiCol_TabDimmedSelected] = Panel;
	Colors[ImGuiCol_TabDimmedSelectedOverline] = Rgb(0, 0, 0, 0.0f);
	Colors[ImGuiCol_DockingPreview] = Rgb(0, 112, 224, 0.5f);
	Colors[ImGuiCol_DockingEmptyBg] = Backdrop;
	Colors[ImGuiCol_TableHeaderBg] = Raised;
	Colors[ImGuiCol_TableBorderStrong] = Outline;
	Colors[ImGuiCol_TableBorderLight] = Rgb(26, 26, 26);
	Colors[ImGuiCol_TextSelectedBg] = AccentDimmed;
	Colors[ImGuiCol_DragDropTarget] = AccentHovered;
	Colors[ImGuiCol_NavCursor] = AccentHovered;
	Colors[ImGuiCol_ModalWindowDimBg] = Rgb(0, 0, 0, 0.5f);

	Style.FontSizeBase = FontSize;
	Style.ScaleAllSizes(Scale);
	Style.FontScaleDpi = Scale;
	ImGui::GetStyle() = Style;
}
