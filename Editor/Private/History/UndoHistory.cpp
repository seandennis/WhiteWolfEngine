#include "History/UndoHistory.h"

#include <utility>

FUndoHistory::FUndoHistory()
{
	Steps.emplace_back();
}

void FUndoHistory::Reset(std::string State)
{
	Steps.clear();
	Steps.push_back(FStep{std::string(), std::move(State)});
	CurrentIndex = 0;
	MemoryUse = GetSize(Steps.front());
}

bool FUndoHistory::Record(std::string StepName, std::string State)
{
	if (State == Steps[CurrentIndex].State)
	{
		return false;
	}

	// What could have been redone leads somewhere the document no longer goes.
	while (Steps.size() > CurrentIndex + 1)
	{
		MemoryUse -= GetSize(Steps.back());
		Steps.pop_back();
	}

	Steps.push_back(FStep{std::move(StepName), std::move(State)});
	MemoryUse += GetSize(Steps.back());
	++CurrentIndex;

	ForgetOldestSteps();
	return true;
}

bool FUndoHistory::CanUndo() const
{
	return CurrentIndex > 0;
}

bool FUndoHistory::CanRedo() const
{
	return CurrentIndex + 1 < Steps.size();
}

const std::string& FUndoHistory::Undo()
{
	WW_CHECKF(CanUndo(), "There is no step to undo");
	--CurrentIndex;
	return Steps[CurrentIndex].State;
}

const std::string& FUndoHistory::Redo()
{
	WW_CHECKF(CanRedo(), "There is no step to redo");
	++CurrentIndex;
	return Steps[CurrentIndex].State;
}

const std::string& FUndoHistory::GetCurrentState() const
{
	return Steps[CurrentIndex].State;
}

void FUndoHistory::ReplaceCurrentState(std::string State)
{
	FStep& Current = Steps[CurrentIndex];
	MemoryUse -= GetSize(Current);
	Current.State = std::move(State);
	MemoryUse += GetSize(Current);
}

std::vector<std::string> FUndoHistory::GetUndoStepNames() const
{
	std::vector<std::string> Names;
	Names.reserve(CurrentIndex);
	for (std::size_t Index = CurrentIndex; Index > 0; --Index)
	{
		Names.push_back(Steps[Index].Name);
	}
	return Names;
}

std::vector<std::string> FUndoHistory::GetRedoStepNames() const
{
	std::vector<std::string> Names;
	for (std::size_t Index = CurrentIndex + 1; Index < Steps.size(); ++Index)
	{
		Names.push_back(Steps[Index].Name);
	}
	return Names;
}

void FUndoHistory::SetMemoryBudget(int64 Bytes)
{
	WW_CHECKF(Bytes >= 0, "The memory budget of an undo history cannot be negative ({})", Bytes);
	MemoryBudget = Bytes;
	ForgetOldestSteps();
}

int64 FUndoHistory::GetSize(const FStep& Step)
{
	return static_cast<int64>(Step.Name.size() + Step.State.size());
}

void FUndoHistory::ForgetOldestSteps()
{
	// The oldest state goes, and the one after it becomes the state the history starts from. The current state and
	// the one before it stay, whatever they take: the last change can always be undone.
	while (MemoryUse > MemoryBudget && CurrentIndex > 1)
	{
		MemoryUse -= GetSize(Steps.front());
		Steps.erase(Steps.begin());
		--CurrentIndex;
	}
}
