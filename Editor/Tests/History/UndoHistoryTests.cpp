#include "History/UndoHistory.h"

#include "TestSupport/ScopedLogCapture.h"

#include <doctest/doctest.h>

#include <ostream>
#include <string>
#include <vector>

namespace
{
	using FNames = std::vector<std::string>;
}

TEST_SUITE("FUndoHistory")
{
	TEST_CASE("A new history has one empty state and nothing to undo or redo")
	{
		const FUndoHistory History;

		CHECK(History.GetCurrentState().empty());
		CHECK_FALSE(History.CanUndo());
		CHECK_FALSE(History.CanRedo());
		CHECK(History.GetUndoStepNames().empty());
		CHECK(History.GetRedoStepNames().empty());
		CHECK(History.GetMemoryUse() == 0);
		CHECK(History.GetMemoryBudget() == FUndoHistory::DefaultMemoryBudget);
	}

	TEST_CASE("Recording a state that differs adds a step that can be undone")
	{
		FUndoHistory History;
		History.Reset("A");

		CHECK(History.Record("First", "B"));

		CHECK(History.GetCurrentState() == "B");
		CHECK(History.CanUndo());
		CHECK_FALSE(History.CanRedo());
		CHECK(History.GetUndoStepNames() == FNames{"First"});
	}

	TEST_CASE("Recording the state the history is in already adds nothing")
	{
		FUndoHistory History;
		History.Reset("A");

		CHECK_FALSE(History.Record("Nothing", "A"));

		CHECK_FALSE(History.CanUndo());
		CHECK(History.GetUndoStepNames().empty());

		REQUIRE(History.Record("First", "B"));
		CHECK_FALSE(History.Record("Nothing", "B"));
		CHECK(History.GetUndoStepNames() == FNames{"First"});
	}

	TEST_CASE("Undo returns the state before each step, the last step first")
	{
		FUndoHistory History;
		History.Reset("A");
		REQUIRE(History.Record("First", "B"));
		REQUIRE(History.Record("Second", "C"));
		CHECK(History.GetUndoStepNames() == FNames{"Second", "First"});

		CHECK(History.Undo() == "B");
		CHECK(History.GetCurrentState() == "B");
		CHECK(History.GetUndoStepNames() == FNames{"First"});
		CHECK(History.GetRedoStepNames() == FNames{"Second"});

		CHECK(History.Undo() == "A");
		CHECK(History.GetCurrentState() == "A");
		CHECK_FALSE(History.CanUndo());
		CHECK(History.GetRedoStepNames() == FNames{"First", "Second"});
	}

	TEST_CASE("Redo returns the state after each undone step, in the order the steps were made")
	{
		FUndoHistory History;
		History.Reset("A");
		REQUIRE(History.Record("First", "B"));
		REQUIRE(History.Record("Second", "C"));
		History.Undo();
		History.Undo();

		CHECK(History.Redo() == "B");
		CHECK(History.GetCurrentState() == "B");
		CHECK(History.CanRedo());

		CHECK(History.Redo() == "C");
		CHECK_FALSE(History.CanRedo());
		CHECK(History.GetUndoStepNames() == FNames{"Second", "First"});
	}

	TEST_CASE("A step recorded after an undo takes the place of the steps that could have been redone")
	{
		FUndoHistory History;
		History.Reset("A");
		REQUIRE(History.Record("First", "B"));
		REQUIRE(History.Record("Second", "C"));
		History.Undo();

		REQUIRE(History.Record("Other", "D"));

		CHECK_FALSE(History.CanRedo());
		CHECK(History.GetUndoStepNames() == FNames{"Other", "First"});
		CHECK(History.Undo() == "B");
		CHECK(History.Redo() == "D");
	}

	TEST_CASE("Recording the current state after an undo keeps what can be redone")
	{
		FUndoHistory History;
		History.Reset("A");
		REQUIRE(History.Record("First", "B"));
		History.Undo();

		CHECK_FALSE(History.Record("Nothing", "A"));

		CHECK(History.CanRedo());
		CHECK(History.Redo() == "B");
	}

	TEST_CASE("A state the document was in before can be recorded again as a new step")
	{
		FUndoHistory History;
		History.Reset("A");
		REQUIRE(History.Record("There", "B"));

		CHECK(History.Record("Back", "A"));

		CHECK(History.GetUndoStepNames() == FNames{"Back", "There"});
		CHECK(History.Undo() == "B");
		CHECK(History.Undo() == "A");
	}

	TEST_CASE("Reset forgets every step")
	{
		FUndoHistory History;
		History.Reset("A");
		REQUIRE(History.Record("First", "B"));
		REQUIRE(History.Record("Second", "C"));
		History.Undo();

		History.Reset("Z");

		CHECK(History.GetCurrentState() == "Z");
		CHECK_FALSE(History.CanUndo());
		CHECK_FALSE(History.CanRedo());
		CHECK(History.GetMemoryUse() == 1);
	}

	TEST_CASE("Undoing or redoing when there is no step is a fatal error")
	{
		const FScopedLogCapture Capture;
		FUndoHistory History;
		History.Reset("A");

		CHECK_THROWS_AS(History.Undo(), FFatalErrorException);
		CHECK_THROWS_AS(History.Redo(), FFatalErrorException);
		CHECK(History.GetCurrentState() == "A");
	}

	TEST_CASE("ReplaceCurrentState changes the text of the current state without adding a step")
	{
		FUndoHistory History;
		History.Reset("A");
		REQUIRE(History.Record("First", "B"));
		REQUIRE(History.Record("Second", "C"));
		History.Undo();

		History.ReplaceCurrentState("B again");

		CHECK(History.GetCurrentState() == "B again");
		CHECK(History.GetUndoStepNames() == FNames{"First"});
		CHECK(History.GetRedoStepNames() == FNames{"Second"});
		CHECK_FALSE(History.Record("Nothing", "B again"));
		CHECK(History.Undo() == "A");
		CHECK(History.Redo() == "B again");
	}

	TEST_CASE("The memory in use is the text of all states and step names")
	{
		FUndoHistory History;
		History.Reset("12345");
		CHECK(History.GetMemoryUse() == 5);

		REQUIRE(History.Record("Step", "1234567890"));
		CHECK(History.GetMemoryUse() == 5 + 4 + 10);

		// A step that can be redone still takes memory, until another step takes its place.
		History.Undo();
		CHECK(History.GetMemoryUse() == 5 + 4 + 10);
		REQUIRE(History.Record("Other", "123"));
		CHECK(History.GetMemoryUse() == 5 + 5 + 3);

		History.ReplaceCurrentState("1");
		CHECK(History.GetMemoryUse() == 5 + 5 + 1);
	}

	TEST_CASE("A history over its budget forgets its oldest steps")
	{
		FUndoHistory History;
		History.SetMemoryBudget(40);
		History.Reset("AAAAAAAAAA");
		REQUIRE(History.Record("1", "BBBBBBBBBB"));
		REQUIRE(History.Record("2", "CCCCCCCCCC"));
		// Three states of ten bytes and two names of one: 32 bytes, within the budget.
		CHECK(History.GetUndoStepNames() == FNames{"2", "1"});

		// A fourth state makes 43 bytes. The oldest state goes, which leaves 33.
		REQUIRE(History.Record("3", "DDDDDDDDDD"));

		CHECK(History.GetUndoStepNames() == FNames{"3", "2"});
		CHECK(History.GetMemoryUse() == 33);
		CHECK(History.Undo() == "CCCCCCCCCC");
		CHECK(History.Undo() == "BBBBBBBBBB");
		CHECK_FALSE(History.CanUndo());
	}

	TEST_CASE("The last step can be undone however small the budget is")
	{
		FUndoHistory History;
		History.SetMemoryBudget(0);
		History.Reset("AAAAAAAAAA");
		REQUIRE(History.Record("1", "BBBBBBBBBB"));
		REQUIRE(History.Record("2", "CCCCCCCCCC"));

		CHECK(History.GetUndoStepNames() == FNames{"2"});
		CHECK(History.Undo() == "BBBBBBBBBB");
		CHECK_FALSE(History.CanUndo());
		CHECK(History.Redo() == "CCCCCCCCCC");
	}

	TEST_CASE("Lowering the budget forgets the oldest steps at once")
	{
		FUndoHistory History;
		History.Reset("AAAAAAAAAA");
		REQUIRE(History.Record("1", "BBBBBBBBBB"));
		REQUIRE(History.Record("2", "CCCCCCCCCC"));
		REQUIRE(History.Record("3", "DDDDDDDDDD"));

		History.SetMemoryBudget(25);

		CHECK(History.GetMemoryBudget() == 25);
		CHECK(History.GetUndoStepNames() == FNames{"3"});
		CHECK(History.GetCurrentState() == "DDDDDDDDDD");
	}

	TEST_CASE("A negative budget is a fatal error")
	{
		const FScopedLogCapture Capture;
		FUndoHistory History;

		CHECK_THROWS_AS(History.SetMemoryBudget(-1), FFatalErrorException);
		CHECK(History.GetMemoryBudget() == FUndoHistory::DefaultMemoryBudget);
	}
}
