#include "Platform/Window.h"

#include "Platform/GlfwNative.h"
#include "Platform/Platform.h"
#include "Platform/PlatformState.h"

#include <cmath>
#include <format>
#include <memory>
#include <string>
#include <utility>
#include <vector>

/** Everything a window holds. It lives here so that no GLFW type appears in an engine header. */
struct FWindow::FImplementation
{
	GLFWwindow* Handle = nullptr;
	FInputState Input;
	std::string Title;
	int32 Width = 0;
	int32 Height = 0;
	float ContentScale = 1.0f;

	/** Takes in what the window system says the scale of the window's display is, if it is a scale at all. */
	void SetContentScale(float NewScale)
	{
		if (std::isfinite(NewScale) && NewScale > 0.0f)
		{
			ContentScale = NewScale;
		}
	}

	/** Returns the implementation of the window that a GLFW callback was called for. */
	static FImplementation& From(GLFWwindow* WindowHandle)
	{
		return *static_cast<FImplementation*>(glfwGetWindowUserPointer(WindowHandle));
	}

	static void HandleKey(GLFWwindow* WindowHandle, int Key, int /*ScanCode*/, int Action, int /*Modifiers*/)
	{
		// A key that is held down repeats; that is of interest to text entry, not to the state of the key.
		const bool bIsKnownKey = Key > 0 && Key <= static_cast<int>(EKey::Last);
		if (bIsKnownKey && Action != GLFW_REPEAT)
		{
			From(WindowHandle)
				.Input.HandleKeyEvent(
					static_cast<EKey>(Key), Action == GLFW_PRESS ? EInputEvent::Pressed : EInputEvent::Released);
		}
	}

	static void HandleCharacter(GLFWwindow* WindowHandle, unsigned int CodePoint)
	{
		From(WindowHandle).Input.HandleTypedCharacter(static_cast<char32_t>(CodePoint));
	}

	static void HandleMouseButton(GLFWwindow* WindowHandle, int Button, int Action, int /*Modifiers*/)
	{
		const bool bIsKnownButton = Button >= 0 && Button <= static_cast<int>(EMouseButton::Last);
		if (bIsKnownButton)
		{
			From(WindowHandle)
				.Input.HandleMouseButtonEvent(static_cast<EMouseButton>(Button),
					Action == GLFW_PRESS ? EInputEvent::Pressed : EInputEvent::Released);
		}
	}

	static void HandleCursorPosition(GLFWwindow* WindowHandle, double X, double Y)
	{
		From(WindowHandle).Input.HandleMouseMove(FVector2D(X, Y));
	}

	static void HandleCursorEnter(GLFWwindow* WindowHandle, int Entered)
	{
		// Entering is followed by a position, which says all there is to say.
		if (Entered == GLFW_FALSE)
		{
			From(WindowHandle).Input.HandleMouseLeave();
		}
	}

	static void HandleScroll(GLFWwindow* WindowHandle, double X, double Y)
	{
		From(WindowHandle).Input.HandleScroll(FVector2D(X, Y));
	}

	static void HandleFramebufferSize(GLFWwindow* WindowHandle, int NewWidth, int NewHeight)
	{
		FImplementation& Window = From(WindowHandle);
		Window.Width = NewWidth;
		Window.Height = NewHeight;
	}

	static void HandleContentScale(GLFWwindow* WindowHandle, float ScaleX, float /*ScaleY*/)
	{
		// Displays have the same scale in both directions; the engine has one number for it.
		From(WindowHandle).SetContentScale(ScaleX);
	}

	static void HandleFocus(GLFWwindow* WindowHandle, int Focused)
	{
		// Keys released while another window has the focus are never reported to this one.
		if (Focused == GLFW_FALSE)
		{
			From(WindowHandle).Input.ReleaseAll();
		}
	}
};

std::unique_ptr<FWindow> FWindow::Create(const FWindowDescription& Description, std::string& OutError)
{
	FPlatformState& State = FPlatformState::Get();
	if (!State.bIsInitialized)
	{
		OutError = "the platform layer is not initialized; call FPlatform::Initialize first";
		return nullptr;
	}
	if (Description.Width <= 0 || Description.Height <= 0)
	{
		OutError = std::format("a window cannot be {} by {} in size", Description.Width, Description.Height);
		return nullptr;
	}

	// The renderer draws with Vulkan, so the window gets no OpenGL context.
	glfwDefaultWindowHints();
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, Description.bResizable ? GLFW_TRUE : GLFW_FALSE);
	glfwWindowHint(GLFW_MAXIMIZED, Description.bMaximized ? GLFW_TRUE : GLFW_FALSE);

	State.LastError.clear();
	GLFWwindow* const Handle =
		glfwCreateWindow(Description.Width, Description.Height, Description.Title.c_str(), nullptr, nullptr);
	if (Handle == nullptr)
	{
		OutError = State.LastError.empty() ? "the window system could not create the window" : State.LastError;
		return nullptr;
	}

	std::unique_ptr<FImplementation> NewImplementation = std::make_unique<FImplementation>();
	NewImplementation->Handle = Handle;
	NewImplementation->Title = Description.Title;
	glfwGetFramebufferSize(Handle, &NewImplementation->Width, &NewImplementation->Height);

	float ScaleX = 1.0f;
	float ScaleY = 1.0f;
	glfwGetWindowContentScale(Handle, &ScaleX, &ScaleY);
	NewImplementation->SetContentScale(ScaleX);

	glfwSetWindowUserPointer(Handle, NewImplementation.get());
	glfwSetKeyCallback(Handle, &FImplementation::HandleKey);
	glfwSetCharCallback(Handle, &FImplementation::HandleCharacter);
	glfwSetMouseButtonCallback(Handle, &FImplementation::HandleMouseButton);
	glfwSetCursorPosCallback(Handle, &FImplementation::HandleCursorPosition);
	glfwSetCursorEnterCallback(Handle, &FImplementation::HandleCursorEnter);
	glfwSetScrollCallback(Handle, &FImplementation::HandleScroll);
	glfwSetFramebufferSizeCallback(Handle, &FImplementation::HandleFramebufferSize);
	glfwSetWindowContentScaleCallback(Handle, &FImplementation::HandleContentScale);
	glfwSetWindowFocusCallback(Handle, &FImplementation::HandleFocus);

	std::unique_ptr<FWindow> Window = std::make_unique<FWindow>(std::move(NewImplementation));
	State.Windows.push_back(Window.get());
	return Window;
}

FWindow::FWindow(std::unique_ptr<FImplementation> InImplementation)
	: Implementation(std::move(InImplementation))
{
}

FWindow::~FWindow()
{
	std::erase(FPlatformState::Get().Windows, this);
	glfwDestroyWindow(Implementation->Handle);
}

const std::string& FWindow::GetTitle() const
{
	return Implementation->Title;
}

void FWindow::SetTitle(const std::string& NewTitle)
{
	Implementation->Title = NewTitle;
	glfwSetWindowTitle(Implementation->Handle, NewTitle.c_str());
}

int32 FWindow::GetWidth() const
{
	return Implementation->Width;
}

int32 FWindow::GetHeight() const
{
	return Implementation->Height;
}

float FWindow::GetContentScale() const
{
	return Implementation->ContentScale;
}

void FWindow::SetSize(int32 NewWidth, int32 NewHeight)
{
	if (NewWidth <= 0 || NewHeight <= 0)
	{
		WW_LOG(LogPlatform, Warning, "A window cannot be resized to {} by {}", NewWidth, NewHeight);
		return;
	}

	glfwSetWindowSize(Implementation->Handle, NewWidth, NewHeight);
}

bool FWindow::ShouldClose() const
{
	return glfwWindowShouldClose(Implementation->Handle) == GLFW_TRUE;
}

void FWindow::RequestClose()
{
	glfwSetWindowShouldClose(Implementation->Handle, GLFW_TRUE);
}

void* FWindow::GetNativeHandle() const
{
#if defined(_WIN32)
	// GLFW has a window of the operating system only when it runs on the operating system's window system.
	if (FPlatformState::Get().Mode == EPlatformMode::Windowed)
	{
		return glfwGetWin32Window(Implementation->Handle);
	}
#endif
	return nullptr;
}

FInputState& FWindow::GetInput()
{
	return Implementation->Input;
}

const FInputState& FWindow::GetInput() const
{
	return Implementation->Input;
}
