#include "Platform/Platform.h"

#include "Platform/Glfw.h"
#include "Platform/PlatformState.h"
#include "Platform/Window.h"

constinit FLogCategory LogPlatform("LogPlatform", ELogVerbosity::Log);

namespace
{
	/** Receives every error the window system reports, during initialization as well as later. */
	void HandleWindowSystemError(int ErrorCode, const char* Description)
	{
		FPlatformState& State = FPlatformState::Get();
		State.LastError = Description != nullptr ? Description : "unknown error";
		WW_LOG(LogPlatform, Error, "The window system reported error {}: {}", ErrorCode, State.LastError);
	}
}

FPlatformState& FPlatformState::Get()
{
	static FPlatformState State;
	return State;
}

bool FPlatform::Initialize(EPlatformMode Mode, std::string& OutError)
{
	FPlatformState& State = FPlatformState::Get();
	WW_CHECKF(!State.bIsInitialized, "FPlatform::Initialize was called twice without a Shutdown in between");

	glfwSetErrorCallback(&HandleWindowSystemError);

	// The null platform creates windows in memory only. GLFW always contains it; the Linux development builds
	// contain nothing else.
	const bool bIsHeadless = Mode == EPlatformMode::Headless;
	glfwInitHint(GLFW_PLATFORM, bIsHeadless ? GLFW_PLATFORM_NULL : GLFW_ANY_PLATFORM);

	State.LastError.clear();
	if (glfwInit() != GLFW_TRUE)
	{
		OutError = State.LastError.empty() ? "the window system could not be started" : State.LastError;
		glfwSetErrorCallback(nullptr);
		return false;
	}

	State.bIsInitialized = true;
	State.Mode = Mode;
	WW_LOG(LogPlatform, Log, "The platform layer started in {} mode", bIsHeadless ? "headless" : "windowed");
	return true;
}

void FPlatform::Shutdown()
{
	FPlatformState& State = FPlatformState::Get();
	if (!State.bIsInitialized)
	{
		return;
	}

	WW_CHECKF(
		State.Windows.empty(), "FPlatform::Shutdown was called while {} window(s) still exist", State.Windows.size());

	glfwTerminate();
	glfwSetErrorCallback(nullptr);

	// Assigning a fresh state also gives back the memory its strings and lists hold.
	State = FPlatformState();
}

bool FPlatform::IsInitialized()
{
	return FPlatformState::Get().bIsInitialized;
}

EPlatformMode FPlatform::GetMode()
{
	const FPlatformState& State = FPlatformState::Get();
	WW_CHECKF(State.bIsInitialized, "FPlatform::GetMode was called before FPlatform::Initialize");
	return State.Mode;
}

void FPlatform::PollEvents()
{
	const FPlatformState& State = FPlatformState::Get();
	WW_CHECKF(State.bIsInitialized, "FPlatform::PollEvents was called before FPlatform::Initialize");

	for (FWindow* const Window : State.Windows)
	{
		Window->GetInput().BeginFrame();
	}

	glfwPollEvents();
}
