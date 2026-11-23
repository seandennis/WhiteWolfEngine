#include "CoreMinimal.h"

#include "Ui/ImGuiLibrary.h"

// Called by Dear ImGui when one of its assertions fails; CMake/WhiteWolfThirdParty.cmake makes its IM_ASSERT this.
// The library asserts what the engine checks with WW_CHECK: that the code which uses it is right.
void ReportImGuiAssertionFailure(const char* Expression, const char* File, int Line)
{
	FDebug::CheckFailed(Expression, File, static_cast<int32>(Line), "An assertion of Dear ImGui failed");
}
