#pragma once

#include "CoreMinimal.h"

#include "Editor.h"

#include <string>
#include <utility>

/**
 * Makes the commands that are given to an editor while it exists one step of the history: FEditor::BeginStep when
 * it is created and FEditor::EndStep when it goes out of scope.
 *
 *     {
 *         const FScopedEditorStep Step(Editor, "Delete");
 *         for (const int64 EntityId : Selection) { ...give Entity.Delete... }
 *     }
 */
class FScopedEditorStep
{
public:
	FScopedEditorStep(FEditor& InEditor, std::string StepName)
		: Editor(InEditor)
	{
		Editor.BeginStep(std::move(StepName));
	}

	~FScopedEditorStep()
	{
		Editor.EndStep();
	}

	FScopedEditorStep(const FScopedEditorStep&) = delete;
	FScopedEditorStep& operator=(const FScopedEditorStep&) = delete;

private:
	FEditor& Editor;
};
