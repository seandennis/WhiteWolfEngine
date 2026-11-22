#pragma once

#include "CoreMinimal.h"

#include <cstddef>
#include <string>
#include <vector>

/**
 * The states a document has been in, one after each change, so that changes can be undone and redone.
 *
 * A state is text that describes the whole document; for the editor, a scene written as JSON. The history does not
 * know what the text means. It is told the state after every change (Record), and hands back the state to return to
 * (Undo, Redo); putting that state into effect is the caller's part. Because a state describes everything, whatever
 * changed between two states is undone, with no code for the particular change.
 *
 * A history always has a current state. Each state after the first is a step, named after what led to it. The states
 * are kept until their text takes more memory than the budget; then the oldest steps are forgotten, but never the
 * last one, so that the most recent change can always be undone.
 */
class FUndoHistory
{
public:
	/** The number of bytes of state text a history keeps before it forgets its oldest steps. */
	static constexpr int64 DefaultMemoryBudget = int64{256} * 1024 * 1024;

	/** Creates a history whose only state is empty text. */
	EDITOR_API FUndoHistory();

	/** Forgets every step and makes State the only state. */
	EDITOR_API void Reset(std::string State);

	/**
	 * Tells the history what state the document is in now.
	 *
	 * If State differs from the current state, it becomes the current state, as a new step, and the steps that
	 * could have been redone are forgotten. If it is the same, nothing changed and nothing is recorded.
	 *
	 * @param StepName What led to the state, for example the name of a command.
	 * @return True if a step was recorded.
	 */
	EDITOR_API bool Record(std::string StepName, std::string State);

	/** Returns true if there is a step to undo. */
	EDITOR_API bool CanUndo() const;

	/** Returns true if there is an undone step to redo. */
	EDITOR_API bool CanRedo() const;

	/**
	 * Goes back one step. Calling it when CanUndo is false is a fatal error.
	 *
	 * @return The state before the step, which is now the current state, for the caller to put into effect.
	 */
	EDITOR_API const std::string& Undo();

	/**
	 * Goes forward one step again. Calling it when CanRedo is false is a fatal error.
	 *
	 * @return The state after the step, which is now the current state, for the caller to put into effect.
	 */
	EDITOR_API const std::string& Redo();

	/** Returns the current state. */
	EDITOR_API const std::string& GetCurrentState() const;

	/**
	 * Replaces the text of the current state without recording a step. For a caller that has put a state into
	 * effect and found that the document then describes itself in slightly different text.
	 */
	EDITOR_API void ReplaceCurrentState(std::string State);

	/** Returns the names of the steps that can be undone, the one Undo would undo first. */
	EDITOR_API std::vector<std::string> GetUndoStepNames() const;

	/** Returns the names of the steps that can be redone, the one Redo would redo first. */
	EDITOR_API std::vector<std::string> GetRedoStepNames() const;

	/** Returns the number of bytes the text of all states and step names takes. */
	int64 GetMemoryUse() const
	{
		return MemoryUse;
	}

	int64 GetMemoryBudget() const
	{
		return MemoryBudget;
	}

	/**
	 * Sets how many bytes of text the history keeps, and forgets the oldest steps if it holds more already. The
	 * current state and the one before it are kept however large they are.
	 *
	 * @param Bytes Zero or more.
	 */
	EDITOR_API void SetMemoryBudget(int64 Bytes);

private:
	/** A state and the name of the step that led to it. The name of the oldest state is not used. */
	struct FStep
	{
		std::string Name;
		std::string State;
	};

	/** Returns the bytes a step takes. */
	static int64 GetSize(const FStep& Step);

	/** Forgets the oldest steps while the history is over its budget and the last step stays undoable. */
	void ForgetOldestSteps();

	/** The states, oldest first. Never empty. */
	std::vector<FStep> Steps;

	/** The index of the current state in Steps. */
	std::size_t CurrentIndex = 0;

	int64 MemoryBudget = DefaultMemoryBudget;
	int64 MemoryUse = 0;
};
