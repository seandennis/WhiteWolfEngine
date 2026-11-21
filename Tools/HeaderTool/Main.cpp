// Entry point of WhiteWolfHeaderTool. The build runs it once per module, before the module is compiled.
#include "HeaderTool.h"

#include <iostream>
#include <string>
#include <vector>

int main(int ArgumentCount, char** Arguments)
{
	std::vector<std::string> ArgumentList;
	for (int Index = 1; Index < ArgumentCount; ++Index)
	{
		ArgumentList.emplace_back(Arguments[Index]);
	}

	return FHeaderTool::Run(ArgumentList, std::cerr);
}
