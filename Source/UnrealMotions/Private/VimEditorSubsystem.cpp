#include "VimEditorSubsystem.h"
#include "Framework/Application/SlateApplication.h"
#include "UMHelpers.h"
#include "UMInputPreProcessor.h"

DEFINE_LOG_CATEGORY_STATIC(LogVimEditorSubsystem, Log, All); // Development

void UVimEditorSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	const FConfigFile& ConfigFile = FUMHelpers::ConfigFile;
	FString			   OutLog = "Vim Editor Subsystem Initialized: ";

	FString TestGetStr;
	bool	bStartVim = true;

	if (!ConfigFile.IsEmpty())
	{
		ConfigFile.GetBool(*FUMHelpers::VimSection, TEXT("bStartVim"), bStartVim);

		// TODO: Remove to a more general place? Debug
		ConfigFile.GetBool(*FUMHelpers::DebugSection, TEXT("bVisualLog"), bVisualLog);
	}

	ToggleVim(bStartVim);
	OutLog += bStartVim ? "Enabled by Config." : "Disabled by Config.";
	FUMHelpers::NotifySuccess(FText::FromString(OutLog), bVisualLog);
	Super::Initialize(Collection);
}

void UVimEditorSubsystem::Deinitialize()
{
	ToggleVim(false);
	Super::Deinitialize();
}

void UVimEditorSubsystem::ToggleVim(bool bEnable)
{
	FString OutLog = bEnable ? "Start Vim: " : "Stop Vim: ";

	if (FSlateApplication::IsInitialized())
	{
		if (bEnable)
		{
			TSharedRef<FUMInputPreProcessor> InputProcessor =
				MakeShared<FUMInputPreProcessor>();
			// Only bind if not already bound
			if (!PreInputKeyDownDelegateHandle.IsValid())
			{
				FSlateApplication& App = FSlateApplication::Get();
				IInputInterface*   InputInterface = App.GetInputInterface();
				// App.SetInputManager();
				App.RegisterInputPreProcessor(InputProcessor);
				PreInputKeyDownDelegateHandle =
					App.OnApplicationPreInputKeyDownListener().AddUObject(
						this, &UVimEditorSubsystem::OnPreInputKeyDown);
				OutLog += "New delegate bind.";
			}
			else
				OutLog += "Delegate is already bound. Skipping.";
			VimMode = EVimMode::Insert;
		}
		else
		{
			if (PreInputKeyDownDelegateHandle.IsValid())
			{
				FSlateApplication::Get().OnApplicationPreInputKeyDownListener().Remove(PreInputKeyDownDelegateHandle);
				PreInputKeyDownDelegateHandle.Reset(); // Clear the handle to avoid reuse
				OutLog += "Delegate resetted successfully.";
			}
			else
				OutLog += "Delegate wasn't valid / bound. Skipping.";
		}
	}

	FUMHelpers::NotifySuccess(FText::FromString(OutLog), bVisualLog);
}

void UVimEditorSubsystem::OnPreInputKeyDown(const FKeyEvent& KeyEvent)
{
	FSlateApplication& App = FSlateApplication::Get();
	FKey			   KeyPressed = KeyEvent.GetKey();
	SwitchVimModes(KeyPressed);
	if (VimMode == EVimMode::Normal)
	{
		if (KeyPressed == EKeys::H)
		{
			FKeyEvent LeftKeyEvent(
				EKeys::Left,
				FModifierKeysState(),
				0,	   // User index
				false, // Is repeat
				0,	   // Character code
				0	   // Key code
			);
			App.ProcessKeyDownEvent(LeftKeyEvent);
		}
		else if (KeyPressed == EKeys::J)
		{
			FKeyEvent DownKeyEvent(
				EKeys::Down,
				FModifierKeysState(),
				0,	   // User index
				false, // Is repeat
				0,	   // Character code
				0	   // Key code
			);
			App.ProcessKeyDownEvent(DownKeyEvent);
		}
		else if (KeyPressed == EKeys::K)
		{
			FKeyEvent UpKeyEvent(
				EKeys::Up,
				FModifierKeysState(),
				0,	   // User index
				false, // Is repeat
				0,	   // Character code
				0	   // Key code
			);
			App.ProcessKeyDownEvent(UpKeyEvent);
		}
		else if (KeyPressed == EKeys::L)
		{
			FKeyEvent RightKeyEvent(
				EKeys::Right,
				FModifierKeysState(),
				0,	   // User index
				false, // Is repeat
				0,	   // Character code
				0	   // Key code
			);
			App.ProcessKeyDownEvent(RightKeyEvent);
		}
		else if (KeyPressed == EKeys::D && KeyEvent.IsControlDown())
		{
			for (int32 i = 0; i < 6; ++i)
			{
				FKeyEvent DownKeyEvent(
					EKeys::Down,
					FModifierKeysState(),
					0,	   // User index
					false, // Is repeat
					0,	   // Character code
					0	   // Key code
				);
				App.ProcessKeyDownEvent(DownKeyEvent);
			}
		}
		else if (KeyPressed == EKeys::U && KeyEvent.IsControlDown())
		{
			for (int32 i = 0; i < 6; ++i)
			{
				FKeyEvent UpKeyEvent(
					EKeys::Up,
					FModifierKeysState(),
					0,	   // User index
					false, // Is repeat
					0,	   // Character code
					0	   // Key code
				);
				App.ProcessKeyDownEvent(UpKeyEvent);
			}
		}
	}
}

void UVimEditorSubsystem::SwitchVimModes(const FKey& KeyPressed)
{
	if (KeyPressed == EKeys::Escape)
	{
		SetMode(EVimMode::Normal);
	}
	else if (VimMode == EVimMode::Normal && KeyPressed == EKeys::I)
	{
		SetMode(EVimMode::Insert);
	}
	else if (VimMode == EVimMode::Normal && KeyPressed == EKeys::V)
	{
		SetMode(EVimMode::Visual);
	}
}

void UVimEditorSubsystem::SetMode(EVimMode NewMode)
{
	if (VimMode != NewMode)
	{
		VimMode = NewMode;
		FUMHelpers::NotifySuccess(FText::FromString(UEnum::GetValueAsString(NewMode)), bVisualLog);
	}
}
