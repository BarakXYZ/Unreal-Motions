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
			// Only bind if not already bound
			if (!PreInputKeyDownDelegateHandle.IsValid())
			{
				FSlateApplication& App = FSlateApplication::Get();

				PreInputKeyDownDelegateHandle =
					App.OnApplicationPreInputKeyDownListener().AddUObject(
						this, &UVimEditorSubsystem::OnPreInputKeyDown);
				OutLog += "New delegate bind.";
			}
			else
				OutLog += "Delegate is already bound. Skipping.";
			// VimMode = EVimMode::Insert;
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
	// SwitchVimModes(KeyPressed);
}
