#include "Rendering/ShaderLibrary.h"

#include <string>

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

nvrhi::ShaderHandle FShaderLibrary::CreateShader(nvrhi::IDevice& Device, std::string_view Name, nvrhi::ShaderType Type)
{
	const std::span<const uint32> Code = GetCode(Name);

	nvrhi::ShaderDesc Description;
	Description.shaderType = Type;
	Description.debugName = std::string(Name);
	return Device.createShader(Description, Code.data(), Code.size_bytes());
}
