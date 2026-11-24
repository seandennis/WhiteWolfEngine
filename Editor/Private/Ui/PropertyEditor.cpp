#include "Ui/PropertyEditor.h"

#include "Assets/Asset.h"
#include "Commands/ScenePropertyContext.h"
#include "Editor.h"
#include "Math/Color.h"
#include "Math/LinearColor.h"
#include "Math/Srgb.h"
#include "Math/Vector.h"
#include "Object/Class.h"
#include "Object/ObjectGlobals.h"
#include "Object/ObjectPtr.h"
#include "Reflection/ArrayProperty.h"
#include "Reflection/ObjectProperty.h"
#include "Reflection/Property.h"
#include "Reflection/PropertyValueTraits.h"
#include "Reflection/WeakObjectProperty.h"
#include "Scene/Component.h"
#include "Scene/Entity.h"
#include "Scene/Scene.h"
#include "Ui/UiFrame.h"
#include "Ui/UiIcons.h"
#include "Ui/UiItems.h"
#include "Ui/UiText.h"
#include "Ui/UiTheme.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <format>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace
{
	/** The parts of a row's width that the name of a value and the value have. */
	constexpr float LabelColumnWeight = 0.38f;
	constexpr float ValueColumnWeight = 0.62f;

	// How far a value moves for each pixel that it is dragged: metres, degrees, a factor, an amount of light.
	constexpr float LocationDragSpeed = 0.05f;
	constexpr float AngleDragSpeed = 0.5f;
	constexpr float ScaleDragSpeed = 0.01f;
	constexpr float ColorDragSpeed = 0.005f;

	/** A whole number moves by one for every five pixels. */
	constexpr float IntegerDragSpeed = 0.2f;

	/** A number of unknown meaning moves by this part of itself per pixel, within the least and the greatest speed. */
	constexpr double RelativeNumberDragSpeed = 0.01;
	constexpr double MinNumberDragSpeed = 0.01;
	constexpr double MaxNumberDragSpeed = 1.0e6;

	// The most digits after the point that a number is shown with: thousandths of a metre, hundredths of a degree.
	constexpr int32 NumberDecimals = 3;
	constexpr int32 AngleDecimals = 2;

	/**
	 * The digits after the point that a dragged number is rounded to: enough to keep what a slow drag adds, and
	 * few enough to take away what adding many small steps leaves behind, as in 0.15000000000000002.
	 */
	constexpr int32 DraggedNumberDecimals = 6;

	/** How wide the mark at the left edge of a number is, as a part of the height of the letters. */
	constexpr float PartMarkWidth = 0.15f;

	constexpr std::array<const char*, 3> AxisNames = {"X", "Y", "Z"};
	constexpr std::array<const char*, 4> ChannelNames = {"R", "G", "B", "A"};

	const char* const NoReferenceLabel = "None";

	/** What the field of a reference to an asset says of an asset that is not a file of the project. */
	const char* const UnsavedAssetLabel = "Not a file";

	/** What the numbers of a value that has several did in a frame. */
	struct FPartsEdit
	{
		bool bChanged = false;

		/** The number that the user is still holding, or 0. */
		ImGuiID HeldItemId = 0;
	};

	/** Returns the type that Dear ImGui knows a number type as. */
	template <typename NumberType>
	constexpr ImGuiDataType GetDataType()
	{
		if constexpr (std::same_as<NumberType, int8>)
		{
			return ImGuiDataType_S8;
		}
		else if constexpr (std::same_as<NumberType, int16>)
		{
			return ImGuiDataType_S16;
		}
		else if constexpr (std::same_as<NumberType, int32>)
		{
			return ImGuiDataType_S32;
		}
		else if constexpr (std::same_as<NumberType, int64>)
		{
			return ImGuiDataType_S64;
		}
		else if constexpr (std::same_as<NumberType, uint8>)
		{
			return ImGuiDataType_U8;
		}
		else if constexpr (std::same_as<NumberType, uint16>)
		{
			return ImGuiDataType_U16;
		}
		else if constexpr (std::same_as<NumberType, uint32>)
		{
			return ImGuiDataType_U32;
		}
		else if constexpr (std::same_as<NumberType, uint64>)
		{
			return ImGuiDataType_U64;
		}
		else if constexpr (std::same_as<NumberType, float>)
		{
			return ImGuiDataType_Float;
		}
		else
		{
			static_assert(std::same_as<NumberType, double>, "Not a number type that can be a property");
			return ImGuiDataType_Double;
		}
	}

	/** Returns true if a value is of the property type ValueType, going by the name of its type. */
	template <typename ValueType>
	bool IsOfType(const std::string& TypeName)
	{
		return TypeName == TPropertyValueTraits<ValueType>::TypeName;
	}

	/** Begins a row of the table: the name of the value in the first column, and then on to the second. */
	void BeginRow(std::string_view Label)
	{
		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		ImGui::AlignTextToFramePadding();
		ImGui::TextUnformatted(Label.data(), Label.data() + Label.size());
		ImGui::TableNextColumn();
	}

	/**
	 * Returns the width that the widgets of a value have in the row being laid out: the rest of the column, less
	 * the room for the arrow or the cross that may stand at the end of the row.
	 */
	float GetValueWidth()
	{
		const float EndWidth = FUiIcons::GetButtonSize() + ImGui::GetStyle().ItemInnerSpacing.x;
		return std::max(ImGui::GetContentRegionAvail().x - EndWidth, 1.0f);
	}

	/** Moves on to the end of a row, wherever the widgets of the value have left off. */
	void MoveToRowEnd(float RowEndX)
	{
		ImGui::SameLine();
		ImGui::SetCursorScreenPos(ImVec2(RowEndX, ImGui::GetCursorScreenPos().y));
	}

	/**
	 * Lays out the arrow that puts a value back as it is in a new object, at the end of the row.
	 *
	 * @return True if the arrow was clicked.
	 */
	bool DrawResetArrow(FUiFrame& Frame, float RowEndX, const std::string& ItemName)
	{
		MoveToRowEnd(RowEndX);
		const bool bWasClicked = FUiIcons::ResetButton("##Reset");
		ImGui::SetItemTooltip("Reset to default");
		Frame.Items.NameLastItem(std::format("{}/Reset", ItemName));
		return bWasClicked;
	}

	/**
	 * Returns the format that shows a number with no more digits after the point than say what it is, up to a
	 * number of them: "%.0f" for 2, "%.1f" for 2.5.
	 */
	const char* GetNumberFormat(double Number, int32 MaxDecimals)
	{
		static constexpr std::array<const char*, 4> Formats = {"%.0f", "%.1f", "%.2f", "%.3f"};
		WW_CHECK(MaxDecimals >= 0 && MaxDecimals < static_cast<int32>(Formats.size()));
		if (!std::isfinite(Number))
		{
			return Formats[0];
		}

		// The number in units of the last digit that could be shown: a digit that is zero need not be.
		double Units = std::round(std::abs(Number) * std::pow(10.0, MaxDecimals));
		int32 Decimals = MaxDecimals;
		while (Decimals > 0 && std::fmod(Units, 10.0) == 0.0)
		{
			Units /= 10.0;
			--Decimals;
		}
		return Formats[static_cast<std::size_t>(Decimals)];
	}

	/**
	 * Returns true if a number reads as zero with the digits that it is shown with: it is zero, with or without a
	 * minus sign of its own, or closer to zero than half of the last digit.
	 */
	bool ReadsAsZero(double Number, int32 MaxDecimals)
	{
		return std::isfinite(Number) && std::round(std::abs(Number) * std::pow(10.0, MaxDecimals)) == 0.0;
	}

	/**
	 * Lays out a number with a point to drag or to type.
	 *
	 * @param Number The number, which is changed only to another number: what is typed may be none at all, as
	 * "nan" is, and such a thing is given to nothing.
	 * @param MaxDecimals The most digits after the point that the number is shown with.
	 * @return True if the user changed the number.
	 */
	template <std::floating_point NumberType>
	bool DragNumber(const char* Id, NumberType& Number, float Speed, int32 MaxDecimals)
	{
		// A number that reads as zero is shown as zero. Printed as it is, one that is just below zero, or a zero
		// with a minus sign of its own, would read "-0". While the number is being dragged or typed it is what it
		// is: a drag in fine steps has to go on from where the last frame left it.
		const bool bIsBeingChanged = ImGui::GetActiveID() == ImGui::GetID(Id);
		const bool bShowsZero = !bIsBeingChanged && ReadsAsZero(static_cast<double>(Number), MaxDecimals);
		NumberType NewNumber = bShowsZero ? NumberType(0) : Number;

		// The library would round a dragged number to the digits that are shown, which here are as few as the
		// number needs. It is rounded below instead, to more digits than that.
		const bool bChanged = ImGui::DragScalar(Id,
			GetDataType<NumberType>(),
			&NewNumber,
			Speed,
			nullptr,
			nullptr,
			GetNumberFormat(static_cast<double>(Number), MaxDecimals),
			ImGuiSliderFlags_NoRoundToFormat);
		if (!bChanged || !std::isfinite(NewNumber))
		{
			return false;
		}

		// A number that was typed is taken as it was typed.
		const bool bWasTyped = ImGui::TempInputIsActive(ImGui::GetItemID());
		if (!bWasTyped)
		{
			const double Scale = std::pow(10.0, DraggedNumberDecimals);
			NewNumber = static_cast<NumberType>(std::round(static_cast<double>(NewNumber) * Scale) / Scale);
		}
		// Rounding something just below zero leaves a zero with a minus sign. Zero has none here.
		Number = NewNumber == NumberType(0) ? NumberType(0) : NewNumber;
		return true;
	}

	/**
	 * Lays out the numbers of a value that has several side by side, each a number to drag or to type, with a mark
	 * in the colour of its part.
	 *
	 * @param Parts The numbers, which are changed to what the user made them.
	 * @param Names What each number is called in the names of the items: "X", "Y", "Z".
	 * @param MaxDecimals The most digits after the point that each number is shown with.
	 * @param ItemName Each number is given the name "<ItemName>/<its name>".
	 */
	template <std::floating_point NumberType, std::size_t Count>
	FPartsEdit EditParts(FUiFrame& Frame,
		std::array<NumberType, Count>& Parts,
		const std::array<const char*, Count>& Names,
		float Speed,
		int32 MaxDecimals,
		const std::string& ItemName,
		float Width)
	{
		FPartsEdit Edit;
		const float MarkWidth = std::max(1.0f, ImGui::GetFontSize() * PartMarkWidth);

		ImGui::PushMultiItemsWidths(static_cast<int>(Count), Width);
		for (std::size_t Index = 0; Index < Count; ++Index)
		{
			if (Index > 0)
			{
				ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
			}
			ImGui::PushID(static_cast<int>(Index));
			if (DragNumber("##Part", Parts[Index], Speed, MaxDecimals))
			{
				Edit.bChanged = true;
			}
			ImGui::PopID();
			ImGui::PopItemWidth();

			Frame.Items.NameLastItem(std::format("{}/{}", ItemName, Names[Index]));
			if (ImGui::IsItemActive())
			{
				Edit.HeldItemId = ImGui::GetItemID();
			}

			const FColor MarkColor = FUiTheme::GetPartColor(static_cast<int32>(Index));
			const ImVec2 Min = ImGui::GetItemRectMin();
			const ImVec2 Max = ImGui::GetItemRectMax();
			ImGui::GetWindowDrawList()->AddRectFilled(Min,
				ImVec2(Min.x + MarkWidth, Max.y),
				IM_COL32(MarkColor.R, MarkColor.G, MarkColor.B, MarkColor.A),
				ImGui::GetStyle().FrameRounding,
				ImDrawFlags_RoundLeft);
		}
		return Edit;
	}

	/**
	 * Lays out the three numbers of a vector.
	 *
	 * @param Vector The vector, which is changed to what the user made it.
	 * @param OutHeldItemId Set to the number that the user is still holding, if any; left alone otherwise.
	 * @return True if the user changed the vector.
	 */
	template <typename VectorType>
	bool EditVector(FUiFrame& Frame,
		VectorType& Vector,
		float Speed,
		const std::string& ItemName,
		float Width,
		ImGuiID& OutHeldItemId)
	{
		std::array<typename VectorType::value_type, 3> Parts = {Vector.x, Vector.y, Vector.z};
		const FPartsEdit PartsEdit = EditParts(Frame, Parts, AxisNames, Speed, NumberDecimals, ItemName, Width);
		if (PartsEdit.HeldItemId != 0)
		{
			OutHeldItemId = PartsEdit.HeldItemId;
		}
		if (!PartsEdit.bChanged)
		{
			return false;
		}
		Vector = VectorType(Parts[0], Parts[1], Parts[2]);
		return true;
	}

	/** Where the row of one part of a transform puts its widgets, and what they are named. */
	struct FPartRow
	{
		std::string ItemName;
		float ValueWidth = 0.0f;
		float EndX = 0.0f;
	};

	/** Begins the row of one part of a transform. What the row shows follows, and then ImGui::PopID. */
	FPartRow BeginPartRow(const char* Label, const std::string& TransformItemName)
	{
		BeginRow(Label);
		ImGui::PushID(Label);

		FPartRow Row;
		Row.ItemName = std::format("{}/{}", TransformItemName, Label);
		Row.ValueWidth = GetValueWidth();
		Row.EndX = ImGui::GetCursorScreenPos().x + Row.ValueWidth + ImGui::GetStyle().ItemInnerSpacing.x;
		return Row;
	}

	/**
	 * Lays out a number to drag or to type, if the value is of the type NumberType.
	 *
	 * @param OutNewValue Receives the number that the user changed it to.
	 * @return True if the value is of that type, and the widget was laid out.
	 */
	template <typename NumberType>
	bool TryEditNumber(
		const std::string& TypeName, const void* Value, float Width, std::optional<FJsonValue>& OutNewValue)
	{
		if (!IsOfType<NumberType>(TypeName))
		{
			return false;
		}

		NumberType Number = *static_cast<const NumberType*>(Value);
		ImGui::SetNextItemWidth(Width);
		bool bChanged = false;
		if constexpr (std::floating_point<NumberType>)
		{
			// What the number means is not known, so neither is how far a pixel should move it: by a part of what
			// it is, then, which moves a large number in large steps and a small one in small steps.
			const double Magnitude = std::abs(static_cast<double>(Number));
			const double Speed = std::isfinite(Magnitude)
				? std::clamp(Magnitude * RelativeNumberDragSpeed, MinNumberDragSpeed, MaxNumberDragSpeed)
				: MinNumberDragSpeed;
			bChanged = DragNumber("##Value", Number, static_cast<float>(Speed), NumberDecimals);
		}
		else
		{
			bChanged = ImGui::DragScalar("##Value", GetDataType<NumberType>(), &Number, IntegerDragSpeed);
		}

		if (bChanged)
		{
			OutNewValue = TPropertyValueTraits<NumberType>::ToJson(Number);
		}
		return true;
	}

	/** Lays out a number to drag or to type, if the value is of one of the given types; see TryEditNumber. */
	template <typename... NumberTypes>
	bool TryEditNumberOf(
		const std::string& TypeName, const void* Value, float Width, std::optional<FJsonValue>& OutNewValue)
	{
		return (TryEditNumber<NumberTypes>(TypeName, Value, Width, OutNewValue) || ...);
	}

	/** Sets OutValue to zero as JSON, if the type named is NumberType. */
	template <typename NumberType>
	bool TryMakeZero(const std::string& TypeName, FJsonValue& OutValue)
	{
		if (!IsOfType<NumberType>(TypeName))
		{
			return false;
		}
		OutValue = TPropertyValueTraits<NumberType>::ToJson(NumberType());
		return true;
	}

	template <typename... NumberTypes>
	bool TryMakeZeroOf(const std::string& TypeName, FJsonValue& OutValue)
	{
		return (TryMakeZero<NumberTypes>(TypeName, OutValue) || ...);
	}

	/**
	 * Returns the value that a new element of a list begins with, as JSON: false, zero, no text, no rotation,
	 * white, an empty list, no reference. It is what the user then changes, so all it has to be is a value of
	 * the type.
	 */
	FJsonValue MakeNewElement(const FProperty& ElementProperty)
	{
		const std::string TypeName = ElementProperty.GetTypeName();
		FJsonValue Zero;
		if (TryMakeZeroOf<int8, int16, int32, int64, uint8, uint16, uint32, uint64, float, double>(TypeName, Zero))
		{
			return Zero;
		}
		if (IsOfType<bool>(TypeName))
		{
			return TPropertyValueTraits<bool>::ToJson(false);
		}
		if (IsOfType<std::string>(TypeName))
		{
			return TPropertyValueTraits<std::string>::ToJson(std::string());
		}
		if (IsOfType<FVector>(TypeName))
		{
			return TPropertyValueTraits<FVector>::ToJson(FVector(0.0));
		}
		if (IsOfType<FVector3f>(TypeName))
		{
			return TPropertyValueTraits<FVector3f>::ToJson(FVector3f(0.0f));
		}
		if (IsOfType<FQuat>(TypeName))
		{
			return TPropertyValueTraits<FQuat>::ToJson(FQuat(1.0f, 0.0f, 0.0f, 0.0f));
		}
		if (IsOfType<FTransform>(TypeName))
		{
			return TPropertyValueTraits<FTransform>::ToJson(FTransform());
		}
		if (IsOfType<FLinearColor>(TypeName))
		{
			return TPropertyValueTraits<FLinearColor>::ToJson(FLinearColor(1.0f, 1.0f, 1.0f, 1.0f));
		}
		if (dynamic_cast<const FArrayProperty*>(&ElementProperty) != nullptr)
		{
			return FJsonValue::MakeArray();
		}
		// Anything else is a reference, and a new one refers to nothing: JSON null.
		return FJsonValue();
	}

	/** Returns a list, as JSON, with one of its elements replaced. */
	FJsonValue WithElementReplaced(const FJsonValue& List, int32 ReplacedIndex, const FJsonValue& NewElement)
	{
		FJsonValue Result = FJsonValue::MakeArray();
		int32 Index = 0;
		for (const FJsonValue& Element : List.GetElements())
		{
			Result.AddElement(Index == ReplacedIndex ? NewElement : Element);
			++Index;
		}
		return Result;
	}

	/** Returns a list, as JSON, without one of its elements. */
	FJsonValue WithElementRemoved(const FJsonValue& List, int32 RemovedIndex)
	{
		FJsonValue Result = FJsonValue::MakeArray();
		int32 Index = 0;
		for (const FJsonValue& Element : List.GetElements())
		{
			if (Index != RemovedIndex)
			{
				Result.AddElement(Element);
			}
			++Index;
		}
		return Result;
	}

	/** An entity or a component that a reference can be made to refer to, and what it is called in the list. */
	struct FReferenceChoice
	{
		int64 Id = 0;
		std::string Label;
	};

	/** Returns the entities or components of a scene that are of a class, in the order of the scene's tree. */
	std::vector<FReferenceChoice> GetReferenceChoices(const WScene& Scene, const WClass& ReferencedClass)
	{
		const bool bRefersToEntities = ReferencedClass.IsChildOf(WEntity::StaticClass());
		std::vector<FReferenceChoice> Choices;
		for (const WEntity* const Entity : Scene.GetAllEntities())
		{
			if (bRefersToEntities)
			{
				Choices.push_back({Entity->GetId(), Entity->GetName()});
				continue;
			}
			for (const TObjectPtr<WComponent>& Component : Entity->GetComponents())
			{
				if (Component->IsA(&ReferencedClass))
				{
					Choices.push_back({Component->GetId(),
						std::format(
							"{} / {}", Entity->GetName(), FUiText::MakeClassLabel(Component->GetClass()->GetName()))});
				}
			}
		}
		return Choices;
	}
}

bool FPropertyEditor::BeginRows(const char* Id)
{
	if (!ImGui::BeginTable(Id, 2, ImGuiTableFlags_NoSavedSettings))
	{
		return false;
	}
	ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch, LabelColumnWeight);
	ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch, ValueColumnWeight);
	return true;
}

void FPropertyEditor::EndRows()
{
	ImGui::EndTable();
}

std::optional<FPropertyChange> FPropertyEditor::DrawProperty(FUiFrame& Frame,
	const FProperty& Property,
	const void* Value,
	std::string_view Label,
	const std::string& ItemName,
	const FJsonValue* Default)
{
	// The name of the item is as good as the place of the value: no two values have the same.
	ImGui::PushID(ItemName.c_str());
	FRowsEdit Rows = DrawRows(Frame, Property, Value, Label, ItemName, Default, ERowEnd::ResetArrow);
	ImGui::PopID();

	if (!Rows.NewValue.has_value())
	{
		return std::nullopt;
	}
	FPropertyChange Change;
	Change.Value = std::move(*Rows.NewValue);
	Change.bIsReset = Rows.bIsReset;
	Change.HeldItemId = Rows.HeldItemId;
	return Change;
}

std::optional<FPropertyChange> FPropertyEditor::DrawTransform(FUiFrame& Frame,
	const FTransform& Value,
	const FTransform* Default,
	const std::string& ItemName,
	std::string_view SearchText)
{
	ImGui::PushID(ItemName.c_str());
	bool bIsReset = false;
	FValueEdit Edit = DrawTransformParts(Frame, Value, Default, ItemName, SearchText, bIsReset);
	ImGui::PopID();

	if (!Edit.NewValue.has_value())
	{
		return std::nullopt;
	}
	FPropertyChange Change;
	Change.Value = std::move(*Edit.NewValue);
	Change.bIsReset = bIsReset;
	Change.HeldItemId = Edit.HeldItemId;
	return Change;
}

std::optional<std::string> FPropertyEditor::EditText(const char* Id, const std::string& Text, const char* Hint)
{
	const ImGuiID ItemId = ImGui::GetID(Id);
	const int32 FrameCount = ImGui::GetFrameCount();
	// What was typed counts only if the field was laid out in the last frame too: it may have gone away meanwhile.
	const bool bIsBeingTyped = TypedItemId == ItemId && TypedFrame + 1 >= FrameCount;

	std::string Shown = Text;
	std::string& Buffer = bIsBeingTyped ? TypedText : Shown;
	ImGui::InputTextWithHint(Id, Hint, &Buffer, ImGuiInputTextFlags_AutoSelectAll);

	std::optional<std::string> Committed;
	if (ImGui::IsItemActive())
	{
		// What is typed stays here while the field has the keyboard, and the value stays as it is.
		if (!bIsBeingTyped)
		{
			TypedText = Buffer;
		}
		TypedItemId = ItemId;
		TypedFrame = FrameCount;
	}
	else if (bIsBeingTyped)
	{
		// The user is done: with Enter, or by going elsewhere. Escape has put back what the field began with.
		if (TypedText != Text)
		{
			Committed = std::move(TypedText);
		}
		TypedItemId = 0;
		TypedText.clear();
	}
	return Committed;
}

FPropertyEditor::FRowsEdit FPropertyEditor::DrawRows(FUiFrame& Frame,
	const FProperty& Property,
	const void* Value,
	std::string_view Label,
	const std::string& ItemName,
	const FJsonValue* Default,
	ERowEnd RowEnd)
{
	if (const FArrayProperty* const ArrayProperty = dynamic_cast<const FArrayProperty*>(&Property))
	{
		return DrawArray(Frame, *ArrayProperty, Value, Label, ItemName, Default, RowEnd);
	}

	FRowsEdit Rows;
	const bool bIsTransform = IsOfType<FTransform>(Property.GetTypeName());

	BeginRow(Label);
	const float ValueWidth = GetValueWidth();
	const float RowEndX = ImGui::GetCursorScreenPos().x + ValueWidth + ImGui::GetStyle().ItemInnerSpacing.x;
	if (bIsTransform)
	{
		// The row of the transform itself holds nothing but its end: its parts have the rows below it.
		ImGui::Dummy(ImVec2(ValueWidth, ImGui::GetFrameHeight()));
	}
	else
	{
		FValueEdit Edit = EditValue(Frame, Property, Value, ItemName, ValueWidth);
		Rows.NewValue = std::move(Edit.NewValue);
		Rows.HeldItemId = Edit.HeldItemId;
	}

	if (RowEnd == ERowEnd::RemoveCross)
	{
		MoveToRowEnd(RowEndX);
		Rows.bWantsRemoval = FUiIcons::RemoveButton("##Remove");
		ImGui::SetItemTooltip("Remove this element");
		Frame.Items.NameLastItem(std::format("{}/Remove", ItemName));
	}
	else if (Default != nullptr)
	{
		FScenePropertyContext Context(Frame.Editor.GetScene());
		const bool bIsChanged = Property.ExportJson(Value, Context) != *Default;
		if (bIsChanged && DrawResetArrow(Frame, RowEndX, ItemName))
		{
			Rows.NewValue = *Default;
			Rows.bIsReset = true;
			Rows.HeldItemId = 0;
		}
	}

	if (bIsTransform)
	{
		// The arrows of the parts put back the parts of the transform that a new object has.
		FTransform DefaultTransform;
		std::string Error;
		const bool bHasDefault =
			Default != nullptr && TPropertyValueTraits<FTransform>::FromJson(*Default, DefaultTransform, Error);

		ImGui::Indent();
		bool bIsPartReset = false;
		FValueEdit Parts = DrawTransformParts(Frame,
			*static_cast<const FTransform*>(Value),
			bHasDefault ? &DefaultTransform : nullptr,
			ItemName,
			std::string_view(),
			bIsPartReset);
		ImGui::Unindent();

		if (Parts.NewValue.has_value() && !Rows.NewValue.has_value())
		{
			Rows.NewValue = std::move(Parts.NewValue);
			Rows.HeldItemId = Parts.HeldItemId;
			Rows.bIsReset = bIsPartReset;
		}
	}
	return Rows;
}

FPropertyEditor::FRowsEdit FPropertyEditor::DrawArray(FUiFrame& Frame,
	const FArrayProperty& Property,
	const void* Value,
	std::string_view Label,
	const std::string& ItemName,
	const FJsonValue* Default,
	ERowEnd RowEnd)
{
	FRowsEdit Rows;
	FScenePropertyContext Context(Frame.Editor.GetScene());
	const FJsonValue Current = Property.ExportJson(Value, Context);
	const int32 Num = Property.GetNum(Value);
	const FProperty& ElementProperty = Property.GetElementProperty();

	BeginRow(Label);
	const float RowEndX = ImGui::GetCursorScreenPos().x + GetValueWidth() + ImGui::GetStyle().ItemInnerSpacing.x;
	ImGui::AlignTextToFramePadding();
	ImGui::TextDisabled("%d %s", Num, Num == 1 ? "element" : "elements");
	ImGui::SameLine();
	if (FUiIcons::AddButton("##Add"))
	{
		FJsonValue Longer = Current;
		Longer.AddElement(MakeNewElement(ElementProperty));
		Rows.NewValue = std::move(Longer);
	}
	ImGui::SetItemTooltip("Add an element");
	Frame.Items.NameLastItem(std::format("{}/Add", ItemName));

	if (RowEnd == ERowEnd::RemoveCross)
	{
		MoveToRowEnd(RowEndX);
		Rows.bWantsRemoval = FUiIcons::RemoveButton("##Remove");
		ImGui::SetItemTooltip("Remove this element");
		Frame.Items.NameLastItem(std::format("{}/Remove", ItemName));
	}
	else if (Default != nullptr && Current != *Default && DrawResetArrow(Frame, RowEndX, ItemName))
	{
		Rows.NewValue = *Default;
		Rows.bIsReset = true;
	}

	ImGui::Indent();
	for (int32 Index = 0; Index < Num; ++Index)
	{
		ImGui::PushID(Index);
		FRowsEdit Element = DrawRows(Frame,
			ElementProperty,
			Property.GetElementPtr(Value, Index),
			std::to_string(Index),
			std::format("{}/{}", ItemName, Index),
			nullptr,
			ERowEnd::RemoveCross);
		ImGui::PopID();

		// One thing is done in a frame: the first that the user asked for.
		if (Rows.NewValue.has_value())
		{
			continue;
		}
		if (Element.bWantsRemoval)
		{
			Rows.NewValue = WithElementRemoved(Current, Index);
		}
		else if (Element.NewValue.has_value())
		{
			Rows.NewValue = WithElementReplaced(Current, Index, *Element.NewValue);
			Rows.HeldItemId = Element.HeldItemId;
		}
	}
	ImGui::Unindent();
	return Rows;
}

FPropertyEditor::FValueEdit FPropertyEditor::DrawTransformParts(FUiFrame& Frame,
	const FTransform& Value,
	const FTransform* Default,
	const std::string& ItemName,
	std::string_view SearchText,
	bool& bOutIsReset)
{
	FValueEdit Edit;
	FTransform NewValue = Value;
	bool bChanged = false;
	bOutIsReset = false;

	if (FUiText::ContainsIgnoringCase("Location", SearchText))
	{
		const FPartRow Row = BeginPartRow("Location", ItemName);
		if (EditVector(Frame, NewValue.Location, LocationDragSpeed, Row.ItemName, Row.ValueWidth, Edit.HeldItemId))
		{
			bChanged = true;
		}
		const bool bIsChanged = Default != nullptr && Value.Location != Default->Location;
		if (bIsChanged && DrawResetArrow(Frame, Row.EndX, Row.ItemName))
		{
			NewValue.Location = Default->Location;
			bChanged = true;
			bOutIsReset = true;
		}
		ImGui::PopID();
	}

	if (FUiText::ContainsIgnoringCase("Rotation", SearchText))
	{
		const FPartRow Row = BeginPartRow("Rotation", ItemName);
		ImGuiID HeldItemId = 0;
		if (const std::optional<FQuat> Turned =
				EditRotation(Frame, Value.Rotation, Row.ItemName, Row.ValueWidth, HeldItemId))
		{
			NewValue.Rotation = *Turned;
			bChanged = true;
		}
		if (HeldItemId != 0)
		{
			Edit.HeldItemId = HeldItemId;
		}
		const bool bIsChanged = Default != nullptr && Value.Rotation != Default->Rotation;
		if (bIsChanged && DrawResetArrow(Frame, Row.EndX, Row.ItemName))
		{
			NewValue.Rotation = Default->Rotation;
			bChanged = true;
			bOutIsReset = true;
		}
		ImGui::PopID();
	}

	if (FUiText::ContainsIgnoringCase("Scale", SearchText))
	{
		const FPartRow Row = BeginPartRow("Scale", ItemName);
		if (EditVector(Frame, NewValue.Scale, ScaleDragSpeed, Row.ItemName, Row.ValueWidth, Edit.HeldItemId))
		{
			bChanged = true;
		}
		const bool bIsChanged = Default != nullptr && Value.Scale != Default->Scale;
		if (bIsChanged && DrawResetArrow(Frame, Row.EndX, Row.ItemName))
		{
			NewValue.Scale = Default->Scale;
			bChanged = true;
			bOutIsReset = true;
		}
		ImGui::PopID();
	}
	if (bChanged)
	{
		Edit.NewValue = TPropertyValueTraits<FTransform>::ToJson(NewValue);
		if (bOutIsReset)
		{
			Edit.HeldItemId = 0;
		}
	}
	return Edit;
}

FPropertyEditor::FValueEdit FPropertyEditor::EditValue(
	FUiFrame& Frame, const FProperty& Property, const void* Value, const std::string& ItemName, float Width)
{
	FValueEdit Edit;
	const std::string TypeName = Property.GetTypeName();

	if (IsOfType<bool>(TypeName))
	{
		bool bIsOn = *static_cast<const bool*>(Value);
		if (ImGui::Checkbox("##Value", &bIsOn))
		{
			Edit.NewValue = TPropertyValueTraits<bool>::ToJson(bIsOn);
		}
		Frame.Items.NameLastItem(ItemName);
	}
	else if (TryEditNumberOf<int8, int16, int32, int64, uint8, uint16, uint32, uint64, float, double>(
				 TypeName, Value, Width, Edit.NewValue))
	{
		Frame.Items.NameLastItem(ItemName);
		if (ImGui::IsItemActive())
		{
			Edit.HeldItemId = ImGui::GetItemID();
		}
	}
	else if (IsOfType<std::string>(TypeName))
	{
		ImGui::SetNextItemWidth(Width);
		if (std::optional<std::string> Typed = EditText("##Value", *static_cast<const std::string*>(Value), ""))
		{
			Edit.NewValue = TPropertyValueTraits<std::string>::ToJson(*Typed);
		}
		Frame.Items.NameLastItem(ItemName);
	}
	else if (IsOfType<FVector>(TypeName))
	{
		FVector Vector = *static_cast<const FVector*>(Value);
		if (EditVector(Frame, Vector, LocationDragSpeed, ItemName, Width, Edit.HeldItemId))
		{
			Edit.NewValue = TPropertyValueTraits<FVector>::ToJson(Vector);
		}
	}
	else if (IsOfType<FVector3f>(TypeName))
	{
		FVector3f Vector = *static_cast<const FVector3f*>(Value);
		if (EditVector(Frame, Vector, LocationDragSpeed, ItemName, Width, Edit.HeldItemId))
		{
			Edit.NewValue = TPropertyValueTraits<FVector3f>::ToJson(Vector);
		}
	}
	else if (IsOfType<FQuat>(TypeName))
	{
		const FQuat& Rotation = *static_cast<const FQuat*>(Value);
		if (const std::optional<FQuat> Turned = EditRotation(Frame, Rotation, ItemName, Width, Edit.HeldItemId))
		{
			Edit.NewValue = TPropertyValueTraits<FQuat>::ToJson(*Turned);
		}
	}
	else if (IsOfType<FLinearColor>(TypeName))
	{
		Edit = EditColor(Frame, *static_cast<const FLinearColor*>(Value), ItemName, Width);
	}
	else if (const FObjectProperty* const ObjectProperty = dynamic_cast<const FObjectProperty*>(&Property))
	{
		Edit = EditReference(
			Frame, *ObjectProperty->GetPropertyClass(), ObjectProperty->GetReferencedObject(Value), ItemName, Width);
	}
	else if (const FWeakObjectProperty* const WeakProperty = dynamic_cast<const FWeakObjectProperty*>(&Property))
	{
		Edit = EditReference(
			Frame, *WeakProperty->GetPropertyClass(), WeakProperty->GetReferencedObject(Value), ItemName, Width);
	}
	else
	{
		// A type that has no widget is named, so that the property is not missing without a trace.
		ImGui::AlignTextToFramePadding();
		ImGui::TextDisabled("%s", TypeName.c_str());
	}
	return Edit;
}

std::optional<FQuat> FPropertyEditor::EditRotation(
	FUiFrame& Frame, const FQuat& Rotation, const std::string& ItemName, float Width, ImGuiID& OutHeldItemId)
{
	const ImGuiID ItemId = ImGui::GetID("##Angles");
	const int32 FrameCount = ImGui::GetFrameCount();
	const bool bIsBeingTurned = TurnedItemId == ItemId && TurnedFrame + 1 >= FrameCount;
	const FRotator Angles = bIsBeingTurned ? TurnedAngles : FRotator::FromQuat(Rotation);

	// X, Y and Z are the angles around those axes: pitch, yaw and roll.
	std::array<float, 3> Parts = {Angles.Pitch, Angles.Yaw, Angles.Roll};
	const FPartsEdit PartsEdit = EditParts(Frame, Parts, AxisNames, AngleDragSpeed, AngleDecimals, ItemName, Width);

	FRotator NewAngles;
	NewAngles.Pitch = Parts[0];
	NewAngles.Yaw = Parts[1];
	NewAngles.Roll = Parts[2];

	OutHeldItemId = PartsEdit.HeldItemId;
	if (PartsEdit.HeldItemId != 0)
	{
		// The angles are the user's for as long as one of them is held: read back from the rotation, a pitch that
		// is dragged past straight up would jump, and a yaw of 190 degrees would turn into one of -170.
		TurnedItemId = ItemId;
		TurnedFrame = FrameCount;
		TurnedAngles = NewAngles;
	}
	else if (bIsBeingTurned)
	{
		TurnedItemId = 0;
	}

	if (!PartsEdit.bChanged)
	{
		return std::nullopt;
	}
	return NewAngles.ToQuat();
}

FPropertyEditor::FValueEdit FPropertyEditor::EditColor(
	FUiFrame& Frame, const FLinearColor& Color, const std::string& ItemName, float Width)
{
	FValueEdit Edit;

	// The button shows the colour as it looks: encoded as a display takes it, and no brighter than white.
	const ImVec4 Shown(std::clamp(FSrgb::Encode(Color.R), 0.0f, 1.0f),
		std::clamp(FSrgb::Encode(Color.G), 0.0f, 1.0f),
		std::clamp(FSrgb::Encode(Color.B), 0.0f, 1.0f),
		std::clamp(Color.A, 0.0f, 1.0f));
	constexpr ImGuiColorEditFlags ButtonFlags = ImGuiColorEditFlags_AlphaPreviewHalf | ImGuiColorEditFlags_NoTooltip;
	if (ImGui::ColorButton("##Value", Shown, ButtonFlags, ImVec2(Width, ImGui::GetFrameHeight())))
	{
		ImGui::OpenPopup("##Picker");
	}
	Frame.Items.NameLastItem(ItemName);

	if (!ImGui::BeginPopup("##Picker"))
	{
		return Edit;
	}

	FLinearColor NewColor = Color;
	bool bChanged = false;

	// A colour is picked among encoded values, where equal steps look equal.
	std::array<float, 4> Encoded = {FSrgb::Encode(Color.R), FSrgb::Encode(Color.G), FSrgb::Encode(Color.B), Color.A};
	constexpr ImGuiColorEditFlags PickerFlags = ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_NoInputs |
		ImGuiColorEditFlags_NoSidePreview | ImGuiColorEditFlags_NoSmallPreview | ImGuiColorEditFlags_NoLabel |
		ImGuiColorEditFlags_NoOptions | ImGuiColorEditFlags_Float | ImGuiColorEditFlags_HDR;
	if (ImGui::ColorPicker4("##Picker", Encoded.data(), PickerFlags))
	{
		NewColor =
			FLinearColor(FSrgb::Decode(Encoded[0]), FSrgb::Decode(Encoded[1]), FSrgb::Decode(Encoded[2]), Encoded[3]);
		bChanged = true;
	}
	const float PickerWidth = ImGui::GetItemRectSize().x;
	Frame.Items.NameLastItem(std::format("{}/Picker", ItemName));
	if (ImGui::IsItemActive())
	{
		Edit.HeldItemId = ImGui::GetItemID();
	}

	// The numbers are the amounts of light themselves, which may be more than white's.
	std::array<float, 4> Amounts = {Color.R, Color.G, Color.B, Color.A};
	const FPartsEdit PartsEdit =
		EditParts(Frame, Amounts, ChannelNames, ColorDragSpeed, NumberDecimals, ItemName, PickerWidth);
	if (PartsEdit.bChanged)
	{
		NewColor = FLinearColor(Amounts[0], Amounts[1], Amounts[2], Amounts[3]);
		bChanged = true;
	}
	if (PartsEdit.HeldItemId != 0)
	{
		Edit.HeldItemId = PartsEdit.HeldItemId;
	}
	ImGui::EndPopup();

	if (bChanged)
	{
		// There is no less light than none, and nothing is more opaque than opaque.
		NewColor.R = std::max(NewColor.R, 0.0f);
		NewColor.G = std::max(NewColor.G, 0.0f);
		NewColor.B = std::max(NewColor.B, 0.0f);
		NewColor.A = std::clamp(NewColor.A, 0.0f, 1.0f);
		Edit.NewValue = TPropertyValueTraits<FLinearColor>::ToJson(NewColor);
	}
	return Edit;
}

FPropertyEditor::FValueEdit FPropertyEditor::EditReference(
	FUiFrame& Frame, const WClass& ReferencedClass, const WObject* Referenced, const std::string& ItemName, float Width)
{
	FValueEdit Edit;

	if (ReferencedClass.IsChildOf(WAsset::StaticClass()))
	{
		// An asset is referred to by its path in the project, and an empty field refers to none. An asset that
		// was made by code and is no file has no path to show; the field says so where it would say "None".
		const WAsset* const Asset = Cast<WAsset>(Referenced);
		const std::string Path = Asset != nullptr ? Asset->GetAssetPath() : std::string();
		const char* const Hint = Asset != nullptr && Path.empty() ? UnsavedAssetLabel : NoReferenceLabel;
		const float ButtonWidth = ImGui::GetFrameHeight();
		const float ButtonSpacing = ImGui::GetStyle().ItemInnerSpacing.x;
		// Never less than a pixel: a width below zero is no width to the library, but a distance to keep from the
		// end of the line, and the button would then be where nobody sees it.
		constexpr float LeastWidth = 1.0f;
		ImGui::SetNextItemWidth(std::max(Width - ButtonWidth - ButtonSpacing, LeastWidth));
		if (const std::optional<std::string> Typed = EditText("##Value", Path, Hint))
		{
			Edit.NewValue = Typed->empty() ? FJsonValue() : FJsonValue(*Typed);
		}
		Frame.Items.NameLastItem(ItemName);

		// Beside the path, the button that opens the list of what there is to choose from.
		ImGui::SameLine(0.0f, ButtonSpacing);
		if (const std::optional<std::string> Chosen = AssetPicker.Draw(Frame, ReferencedClass, ItemName))
		{
			Edit.NewValue = Chosen->empty() ? FJsonValue() : FJsonValue(*Chosen);
		}
		return Edit;
	}

	const bool bIsInScenes =
		ReferencedClass.IsChildOf(WEntity::StaticClass()) || ReferencedClass.IsChildOf(WComponent::StaticClass());
	if (!bIsInScenes)
	{
		// Nothing in a scene is of this class, so there is nothing to choose from.
		ImGui::AlignTextToFramePadding();
		ImGui::TextDisabled("%s", ReferencedClass.GetName().c_str());
		return Edit;
	}

	// An entity or a component of the scene is referred to by its identifier.
	const std::vector<FReferenceChoice> Choices = GetReferenceChoices(Frame.Editor.GetScene(), ReferencedClass);
	std::optional<int64> CurrentId;
	if (const WEntity* const Entity = Cast<WEntity>(Referenced))
	{
		CurrentId = Entity->GetId();
	}
	else if (const WComponent* const Component = Cast<WComponent>(Referenced))
	{
		CurrentId = Component->GetId();
	}
	const auto CurrentChoice = std::ranges::find(
		Choices, CurrentId, [](const FReferenceChoice& Choice) { return std::optional<int64>(Choice.Id); });
	const char* const Preview = CurrentChoice != Choices.end() ? CurrentChoice->Label.c_str() : NoReferenceLabel;

	ImGui::SetNextItemWidth(Width);
	if (ImGui::BeginCombo("##Value", Preview))
	{
		if (ImGui::Selectable(NoReferenceLabel, !CurrentId.has_value()))
		{
			Edit.NewValue = FJsonValue();
		}
		Frame.Items.NameLastItem(std::format("{}/None", ItemName));

		for (const FReferenceChoice& Choice : Choices)
		{
			// Told apart by identifier, so that two entities may have the same name.
			ImGui::PushID(reinterpret_cast<const void*>(static_cast<std::intptr_t>(Choice.Id)));
			if (ImGui::Selectable(Choice.Label.c_str(), Choice.Id == CurrentId))
			{
				Edit.NewValue = FJsonValue(Choice.Id);
			}
			Frame.Items.NameLastItem(std::format("{}/{}", ItemName, Choice.Id));
			ImGui::PopID();
		}
		ImGui::EndCombo();
	}
	// The list is a window of its own; ending it makes the field the last item again.
	Frame.Items.NameLastItem(ItemName);
	return Edit;
}
