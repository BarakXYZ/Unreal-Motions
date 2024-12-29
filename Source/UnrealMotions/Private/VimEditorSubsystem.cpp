#include "VimEditorSubsystem.h"
#include "Framework/Application/SlateApplication.h"
#include "UMHelpers.h"
#include "UMInputPreProcessor.h"

DEFINE_LOG_CATEGORY_STATIC(LogVimEditorSubsystem, Log, All); // Development

TMap<FKey, FKey> UVimEditorSubsystem::VimToArrowKeys = {
	{ EKeys::H, FKey(EKeys::Left) }, { EKeys::J, FKey(EKeys::Down) },
	{ EKeys::K, FKey(EKeys::Up) }, { EKeys::L, FKey(EKeys::Right) }
};

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

	VimSubWeak = this;
	InputPP = FUMInputPreProcessor::Get().ToWeakPtr();
	BindCommands();
	Super::Initialize(Collection);
}

void UVimEditorSubsystem::Deinitialize()
{
	ToggleVim(false);
	Super::Deinitialize();
}

void UVimEditorSubsystem::OnResetSequence()
{
	CountBuffer.Empty();
}
void UVimEditorSubsystem::OnCountPrefix(FString AddedCount)
{
	CountBuffer += AddedCount; // Build the buffer: 1 + 7 + 3 (173);
}

void UVimEditorSubsystem::ToggleVim(bool bEnable)
{
	FString OutLog = bEnable ? "Start Vim: " : "Stop Vim: ";

	if (FSlateApplication::IsInitialized())
	{
		if (bEnable)
		{
			// TODO:
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

bool UVimEditorSubsystem::MapVimToArrowNavigation(
	const FKeyEvent& InKeyEvent, FKeyEvent& OutKeyEvent)
{
	const FKey* MappedKey = VimToArrowKeys.Find(InKeyEvent.GetKey());
	if (!MappedKey)
		return false;
	OutKeyEvent = FKeyEvent(
		*MappedKey,
		InKeyEvent.GetModifierKeys(),
		0,	   // User index
		false, // Is repeat
		0,	   // Character code
		0	   // Key code
	);
	return true;
}

void UVimEditorSubsystem::ProcessVimNavigationInput(
	FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent)
{
	FKeyEvent OutKeyEvent;
	MapVimToArrowNavigation(InKeyEvent, OutKeyEvent);

	// DebugKeyEvent(OutKeyEvent);

	uint64 Count{ 1 }; // Single or multiple (minimum 1)
	if (!CountBuffer.IsEmpty())
		Count = FCString::Atoi(*CountBuffer);

	for (uint64 i{ 0 }; i < Count; ++i)
	{
		FUMInputPreProcessor::ToggleNativeInputHandling(true);
		SlateApp.ProcessKeyDownEvent(OutKeyEvent);
	}
}

void UVimEditorSubsystem::DeleteItem(
	FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent)
{
	// EKeys::Delete;
	FKeyEvent DeleteEvent(
		FKey(EKeys::Delete),
		FModifierKeysState(),
		0, 0, 0, 0);
	FUMInputPreProcessor::ToggleNativeInputHandling(true);
	// SlateApp.ProcessKeyDownEvent(DeleteEvent);

	// UWorld* EditorWorld = nullptr;

	// // Example: Get the “editor world context” if one is available.
	// if (GEditor)
	// {
	// 	FUMHelpers::NotifySuccess(FText::FromString("GEDITOR"));
	// 	// This returns the first world used by the Editor (the level you see open).
	// 	FWorldContext& EditorWorldContext = GEditor->GetEditorWorldContext();
	// 	EditorWorld = EditorWorldContext.World();
	// }

	// if (EditorWorld)
	// {
	// 	FUMHelpers::NotifySuccess(FText::FromString("EditorWorld"));
	// 	FTimerHandle TimerHandle;
	// 	EditorWorld->GetTimerManager().SetTimer(
	// 		TimerHandle,
	// 		FTimerDelegate::CreateLambda([this, &SlateApp]() {
	// 			// This is your timer callback, runs once after 3 seconds in the Editor world
	// 			SimulateMultipleTabEvent(SlateApp, 2);
	// 		}),
	// 		3.0f, // Delay
	// 		false // bLoop
	// 	);
	// }
	SlateApp.ProcessKeyDownEvent(DeleteEvent); // Will just block the entire process until the delete window is handled, thus not really helping.
}

void UVimEditorSubsystem::BindCommands()
{
	using VimSub = UVimEditorSubsystem;

	if (!InputPP.IsValid())
		return;
	FUMInputPreProcessor& Input = *InputPP.Pin();

	// Listeners
	Input.OnResetSequence.AddUObject(
		this, &VimSub::OnResetSequence);

	Input.OnCountPrefix.AddUObject(
		this, &VimSub::OnCountPrefix);

	// ~ Commands ~ //
	//
	// Delete item - Simulate the Delete key (WIP)
	Input.AddKeyBinding_KeyEvent(
		{ EKeys::X },
		VimSubWeak, &VimSub::DeleteItem);

	//  Move HJKL
	Input.AddKeyBinding_KeyEvent(
		{ EKeys::H },
		VimSubWeak, &VimSub::ProcessVimNavigationInput);
	Input.AddKeyBinding_KeyEvent(
		{ EKeys::J },
		VimSubWeak, &VimSub::ProcessVimNavigationInput);
	Input.AddKeyBinding_KeyEvent(
		{ EKeys::K },
		VimSubWeak, &VimSub::ProcessVimNavigationInput);
	Input.AddKeyBinding_KeyEvent(
		{ EKeys::L },
		VimSubWeak, &VimSub::ProcessVimNavigationInput);

	// Selection + Move HJKL
	Input.AddKeyBinding_KeyEvent(
		{ FInputChord(EModifierKey::Shift, EKeys::H) },
		VimSubWeak, &VimSub::ProcessVimNavigationInput);
	Input.AddKeyBinding_KeyEvent(
		{ FInputChord(EModifierKey::Shift, EKeys::J) },
		VimSubWeak, &VimSub::ProcessVimNavigationInput);
	Input.AddKeyBinding_KeyEvent(
		{ FInputChord(EModifierKey::Shift, EKeys::K) },
		VimSubWeak, &VimSub::ProcessVimNavigationInput);
	Input.AddKeyBinding_KeyEvent(
		{ FInputChord(EModifierKey::Shift, EKeys::L) },
		VimSubWeak, &VimSub::ProcessVimNavigationInput);
}
