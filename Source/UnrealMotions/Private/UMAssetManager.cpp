#include "UMAssetManager.h"
#include "Interfaces/IMainFrameModule.h"
#include "Framework/Commands/UICommandInfo.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Notifications/NotificationManager.h"
#include "UMSlateHelpers.h"
#include "Widgets/Accessibility/SlateCoreAccessibleWidgets.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Widgets/Input/SButton.h"
#include "UMLogger.h"
#include "UMSlateHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogUMAssetManager, NoLogging, All); // Prod
// DEFINE_LOG_CATEGORY_STATIC(LogUMAsset, Log, All); // Dev

#define LOCTEXT_NAMESPACE "UMAssetManager"

TSharedPtr<FUMAssetManager>
	FUMAssetManager::AssetManager =
		MakeShared<FUMAssetManager>();

FUMAssetManager::FUMAssetManager()
{
	FInputBindingManager&		InputBindingManager = FInputBindingManager::Get();
	IMainFrameModule&			MainFrameModule = IMainFrameModule::Get();
	TSharedRef<FUICommandList>& CommandList =
		MainFrameModule.GetMainFrameCommandBindings();
	TSharedPtr<FBindingContext> MainFrameContext =
		InputBindingManager.GetContextByName(MainFrameContextName);

	RegisterAssetCommands(MainFrameContext);
	MapAssetCommands(CommandList);
	/** We can know the name of the context by looking at the constructor
	 * of the TCommands that its extending. e.g. SystemWideCommands, MainFrame...
	 * */
}

FUMAssetManager::~FUMAssetManager()
{
}

FUMAssetManager& FUMAssetManager::Get()
{
	if (!FUMAssetManager::AssetManager.IsValid())
	{
		FUMAssetManager::AssetManager =
			MakeShared<FUMAssetManager>();
	}
	return *FUMAssetManager::AssetManager;
}

bool FUMAssetManager::IsInitialized()
{
	return FUMAssetManager::AssetManager.IsValid();
}

void FUMAssetManager::MapAssetCommands(
	const TSharedRef<FUICommandList>& CommandList)
{
	CommandList->MapAction(CmdInfoOpenPreviouslyOpenAssets,
		FExecuteAction::CreateLambda(
			[this]() {
				Call_RestorePreviouslyOpenAssets();
			}));
}

void FUMAssetManager::Call_RestorePreviouslyOpenAssets()
{
	FSlateApplication&		   SlateApp = FSlateApplication::Get();
	FSlateNotificationManager& NoteMngr = FSlateNotificationManager::Get();

	TArray<TSharedRef<SWindow>> NoteWins;
	NoteMngr.GetWindows(NoteWins);
	if (NoteWins.IsEmpty())
		return;

	// Toggle (focus) OUT if already focused on one of the notification items.
	for (const TSharedRef<SWindow>& Win : NoteWins)
	{
		if (Win->HasAnyUserFocusOrFocusedDescendants())
		{
			SlateApp.ClearAllUserFocus();
			if (LastFocusedWidget.IsValid())
				SlateApp.SetAllUserFocus(
					LastFocusedWidget.Pin(), EFocusCause::Navigation);
			if (LastFocusedNotificationItem.IsValid())
				ToggleNotificationVisualSelection(
					LastFocusedNotificationItem.Pin().ToSharedRef(), false);
			return;
		}
	}
	// Store reference for later in case we want to toggle back to the last widget
	LastFocusedWidget = SlateApp.GetUserFocusedWidget(0);

	// for (const TSharedRef<SWindow>& Win : NoteWins)
	// {
	// 	TSharedPtr<SWidget> ContentWidget = Win->GetContent().ToSharedPtr();
	// 	TWeakPtr<SWidget>	FoundButton = nullptr;
	// 	if (FUMWidgetHelpers::TraverseWidgetTree(
	// 			ContentWidget,
	// 			FoundButton,
	// 			"SButton"))
	// 	{
	// 		return;
	// 	}
	// }
	// return;

	TSharedRef<SWidget> ContentWidget = NoteWins[0]->GetContent();
	TWeakPtr<SWidget>	FoundButton = nullptr;

	TSharedPtr<SNotificationList> NoteList =
		StaticCastSharedRef<SNotificationList>(ContentWidget);
	if (!NoteList.IsValid())
		return;

	FChildren* NoteListChilds = ContentWidget->GetChildren();
	if (!NoteListChilds || NoteListChilds->Num() == 0)
		return;

	TSharedPtr<SVerticalBox> VerBoxNotes =
		StaticCastSharedRef<SVerticalBox>(NoteListChilds->GetChildAt(0));
	if (!VerBoxNotes.IsValid())
		return;

	if (VerBoxNotes->NumSlots() == 0)
		return;

	TSharedPtr<SNotificationItem> NItem =
		StaticCastSharedRef<SNotificationItem>(
			VerBoxNotes->GetSlot(0).GetWidget());
	if (!NItem.IsValid())
		return;
	LastFocusedNotificationItem = NItem.ToWeakPtr(); // To toggle visual focus

	ToggleNotificationVisualSelection(NItem.ToSharedRef(), true);

	TSharedPtr<SWidget> Button;
	if (FUMSlateHelpers::TraverseWidgetTree(
			VerBoxNotes->GetSlot(0).GetWidget(),
			Button,
			"SButton"))
	{
		if (!Button.IsValid())
			return;
		SlateApp.ClearAllUserFocus();
		// We use Navigation as the FocusCause to visually show the selection.
		SlateApp.SetAllUserFocus(Button, EFocusCause::Navigation);
	}

	// UAssetEditorSubsystem* AssetEditorSubsystem =
	// 	GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
}

void FUMAssetManager::ToggleNotificationVisualSelection(
	const TSharedRef<SNotificationItem> Item, const bool bToggleOn)
{
	if (bToggleOn)
	{
		Item->SetForegroundColor(FSlateColor(FLinearColor(1.0f, 1.0f, 1.0f, 1.0f)));
		Item->SetColorAndOpacity(FLinearColor(1.0f, 1.0f, 1.0f, 1.0f));
	}
	else
	{
		Item->SetForegroundColor(FSlateColor(FLinearColor(
			1.0f, 1.0f, 1.0f, 0.2f)));
		Item->SetColorAndOpacity(FLinearColor(
			1.0f, 1.0f, 1.0f, 0.2f));
	}

	// For future reference
	// NItem->Pulse(FLinearColor::Red); // Can't see anything
	// NItem->SetText(FText::FromString("<3")); // Works
	// NItem->GetForegroundColor();  // Can get it and then set it to smthng
	// NItem->SetForegroundColor(FSlateColor(FLinearColor(1.0f, 1.0f, 1.0f, 1.0f)));
}

void FUMAssetManager::OnNotificationsWindowFocusLost()
{
}

void FUMAssetManager::RegisterAssetCommands(const TSharedPtr<FBindingContext>& MainFrameContext)
{
	UI_COMMAND_EXT(MainFrameContext.Get(), CmdInfoOpenPreviouslyOpenAssets,
		"OpenPreviouslyOpenAssets", "Open Previously Open Assets",
		"Opens previously opened assets.",
		EUserInterfaceActionType::Button,
		FInputChord(EModifierKey::FromBools(true, true, false, false),
			EKeys::P));
}

#undef LOCTEXT_NAMESPACE
