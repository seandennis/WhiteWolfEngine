#include "EditorApplication.h"

#include "Agent/AgentServer.h"
#include "Core/FileHelper.h"
#include "Editor.h"
#include "EditorWindow.h"
#include "EditorWindowDescription.h"
#include "Logging/Log.h"
#include "Logging/StreamLogSink.h"
#include "Object/GarbageCollector.h"
#include "Object/ObjectGlobals.h"
#include "Platform/Platform.h"
#include "Platform/PlatformMode.h"
#include "Rendering/Image.h"
#include "Rendering/ImageFile.h"
#include "Rendering/RenderTarget.h"
#include "Settings/EditorSettings.h"
#include "Ui/EditorUi.h"

#include <chrono>
#include <filesystem>
#include <memory>
#include <optional>
#include <ostream>
#include <string_view>
#include <thread>

namespace
{
	constexpr int32 SuccessExitCode = 0;
	constexpr int32 FailureExitCode = 1;
	constexpr int32 UsageErrorExitCode = 2;

	constexpr std::string_view Usage =
		"White Wolf Engine editor\n"
		"\n"
		"  WhiteWolfEditor                      Open the editor. It asks which project to work on.\n"
		"  WhiteWolfEditor --mcp                Serve an AI agent: a Model Context Protocol server on standard\n"
		"                                       input and output, in which every command of the editor is a tool.\n"
		"                                       No window is opened.\n"
		"  WhiteWolfEditor --screenshot <file>  Draw the editor without opening a window and save the picture as\n"
		"                                       a PNG file: the editor before a project is opened. Needs no\n"
		"                                       display.\n"
		"  WhiteWolfEditor --help               Print this help.\n";

	/** How many frames are drawn before a screenshot is taken: a new layout takes a few to settle. */
	constexpr int32 ScreenshotFrameCount = 4;

	/** The time between two frames of a screenshot, which nobody watches: that of a display at 60 pictures a second. */
	constexpr double ScreenshotFrameSeconds = 1.0 / 60.0;

	/** How long the editor rests between looks at a window that is minimised, where nothing paces it. */
	constexpr std::chrono::milliseconds MinimisedWait(50);

	/**
	 * Runs the editor in a window that is already open: frame after frame, until the user closes the window or,
	 * if a number of frames is given, that many have been drawn.
	 *
	 * @param FixedFrameSeconds The time each frame is taken to last; the time that really passed if not given.
	 * @return False, having written the reason to ErrorOutput, if a frame could not be drawn.
	 */
	bool RunFrames(FEditorWindow& Window,
		std::optional<int32> MaxFrameCount,
		std::optional<double> FixedFrameSeconds,
		std::ostream& ErrorOutput)
	{
		std::chrono::steady_clock::time_point LastFrameTime = std::chrono::steady_clock::now();
		int32 FrameCount = 0;
		while (!Window.ShouldClose() && (!MaxFrameCount.has_value() || FrameCount < *MaxFrameCount))
		{
			FPlatform::PollEvents();

			const std::chrono::steady_clock::time_point Now = std::chrono::steady_clock::now();
			const double ElapsedSeconds = std::chrono::duration<double>(Now - LastFrameTime).count();
			const double DeltaSeconds = FixedFrameSeconds.value_or(ElapsedSeconds);
			LastFrameTime = Now;

			std::string Error;
			if (!Window.RunFrame(DeltaSeconds, Error))
			{
				ErrorOutput << "The editor could not draw: " << Error << ".\n";
				return false;
			}
			// Objects that were destroyed since the last frame are collected a little at a time.
			FGarbageCollector::Tick(DeltaSeconds);
			++FrameCount;

			if (!Window.HasArea())
			{
				// Showing a picture is what paces the editor to the display. A minimised window is shown none.
				std::this_thread::sleep_for(MinimisedWait);
			}
		}
		return true;
	}

	/** Saves what the editor's window shows as a PNG file. */
	bool SavePicture(const FEditorWindow& Window, const std::filesystem::path& Path, std::ostream& ErrorOutput)
	{
		const FRenderTarget* const Picture = Window.GetPicture();
		FImage Image;
		std::string Error;
		const bool bSaved =
			Picture != nullptr && Picture->ReadPixels(Image, Error) && FImageFile::SavePng(Image, Path, Error);
		if (!bSaved)
		{
			ErrorOutput << "The picture of the editor was not saved: "
						<< (Picture != nullptr ? Error : "no frame was drawn") << ".\n";
		}
		return bSaved;
	}

	/**
	 * Opens the editor's window and runs the editor in it.
	 *
	 * @param ScreenshotPath If given, the window exists in memory only, a few frames are drawn and the picture is
	 * saved there. Otherwise the window is on the desktop and the editor runs until the user closes it.
	 */
	int32 RunInWindow(const std::optional<std::filesystem::path>& ScreenshotPath, std::ostream& ErrorOutput)
	{
		const std::shared_ptr<FStreamLogSink> LogSink =
			std::make_shared<FStreamLogSink>(ErrorOutput, ELogVerbosity::Warning);
		FLog::AddSink(LogSink);

		const bool bIsScreenshot = ScreenshotPath.has_value();
		int32 ExitCode = FailureExitCode;
		std::string Error;
		if (!FPlatform::Initialize(bIsScreenshot ? EPlatformMode::Headless : EPlatformMode::Windowed, Error))
		{
			ErrorOutput << "The editor could not open a window: " << Error << ".\n";
		}
		else
		{
			{
				FEditor Editor;
				// On the desktop the editor takes the whole screen, as a program to work in does. A screenshot has
				// the size the window is restored to.
				FEditorWindowDescription Description;
				Description.bMaximized = !bIsScreenshot;
				const std::unique_ptr<FEditorWindow> Window = FEditorWindow::Create(Editor, Description, Error);
				if (Window == nullptr)
				{
					ErrorOutput << "The editor could not open its window: " << Error << ".\n";
				}
				else if (bIsScreenshot)
				{
					const bool bSucceeded =
						RunFrames(*Window, ScreenshotFrameCount, ScreenshotFrameSeconds, ErrorOutput) &&
						SavePicture(*Window, *ScreenshotPath, ErrorOutput);
					ExitCode = bSucceeded ? SuccessExitCode : FailureExitCode;
				}
				else
				{
					// The editor that a person works in remembers which projects were opened, from one run to the
					// next, and begins by asking which one to work on: a game is made in a project.
					Editor.SetSettingsFile(FEditorSettings::GetUserFile());
					Window->GetUi().AskForProject();

					const bool bSucceeded = RunFrames(*Window, std::nullopt, std::nullopt, ErrorOutput);
					ExitCode = bSucceeded ? SuccessExitCode : FailureExitCode;
				}
			}
			// The editor's scene is destroyed with the editor; this returns its memory before the process ends.
			CollectGarbage();
			FPlatform::Shutdown();
		}

		FLog::RemoveSink(LogSink);
		return ExitCode;
	}

	/** Serves an agent on the given streams until the input ends. */
	void ServeAgent(std::istream& Input, std::ostream& Output, std::ostream& ErrorOutput)
	{
		// Standard output belongs to the protocol, so what the log has to say goes to the error stream.
		const std::shared_ptr<FStreamLogSink> LogSink =
			std::make_shared<FStreamLogSink>(ErrorOutput, ELogVerbosity::Warning);
		FLog::AddSink(LogSink);
		{
			FEditor Editor;
			FAgentServer Server(Editor);
			Server.Run(Input, Output);
		}
		// The editor's scene is destroyed with the editor; this returns its memory before the process ends.
		CollectGarbage();
		FLog::RemoveSink(LogSink);
	}
}

int32 FEditorApplication::Run(
	const std::vector<std::string>& Arguments, std::istream& Input, std::ostream& Output, std::ostream& ErrorOutput)
{
	if (Arguments.empty())
	{
		return RunInWindow(std::nullopt, ErrorOutput);
	}
	if (Arguments.size() == 1 && Arguments[0] == "--mcp")
	{
		ServeAgent(Input, Output, ErrorOutput);
		return SuccessExitCode;
	}
	if (Arguments.size() == 2 && Arguments[0] == "--screenshot")
	{
		return RunInWindow(FFileHelper::PathFromUtf8(Arguments[1]), ErrorOutput);
	}
	if (Arguments.size() == 1 && Arguments[0] == "--help")
	{
		Output << Usage;
		return SuccessExitCode;
	}

	ErrorOutput << "The editor does not understand its command line.\n\n" << Usage;
	return UsageErrorExitCode;
}
