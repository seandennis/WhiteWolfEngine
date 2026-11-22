// Entry point of WhiteWolfEditor, the editor's executable. Everything it does is in the Editor module, where the
// unit tests can reach it; see FEditorApplication.
#include "Core/ProcessDiagnostics.h"
#include "EditorApplication.h"

#include <iostream>
#include <string>
#include <vector>

int main(int ArgumentCount, char** Arguments)
{
	// An agent that starts the editor is not there to answer a dialog: a crash must end the process.
	FProcessDiagnostics::DisableErrorDialogs();
	FProcessDiagnostics::EnableMemoryLeakReport();

	std::vector<std::string> ArgumentList;
	for (int Index = 1; Index < ArgumentCount; ++Index)
	{
		ArgumentList.emplace_back(Arguments[Index]);
	}

	return FEditorApplication::Run(ArgumentList, std::cin, std::cout, std::cerr);
}
