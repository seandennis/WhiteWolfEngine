// Entry point of WhiteWolfShaderCompiler. The build runs it for a module that has shaders, before the module is
// compiled.
#include "ShaderCompilerTool.h"

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

	return FShaderCompilerTool::Run(ArgumentList, std::cerr);
}
