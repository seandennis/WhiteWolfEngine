#include "Rendering/ShaderLibrary.h"

std::span<const uint32> FShaderLibrary::GetCode(std::string_view Name)
{
	for (const FEmbeddedShader& Shader : GetEmbeddedShaders())
	{
		if (Shader.Name == Name)
		{
			return Shader.Code;
		}
	}

	WW_CHECKF(false, "The engine has no shader named '{}'; shaders are the files in Engine/Shaders", Name);
	return {};
}
