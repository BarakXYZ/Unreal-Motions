#include "UMEditorCommands.h"
#include "Framework/Notifications/NotificationManager.h"

// DEFINE_LOG_CATEGORY_STATIC(LogUMEditorCommands, NoLogging, All);
DEFINE_LOG_CATEGORY_STATIC(LogUMEditorCommands, Log, All);

TSharedPtr<FUMEditorCommands> FUMEditorCommands::EditorCommands =
	MakeShared<FUMEditorCommands>();

FUMEditorCommands::FUMEditorCommands()
{
	Logger.SetLogCategory(&LogUMEditorCommands);
}

FUMEditorCommands::~FUMEditorCommands()
{
}

const TSharedPtr<FUMEditorCommands> FUMEditorCommands::Get()
{
	return EditorCommands;
}

void FUMEditorCommands::ClearAllDebugMessages()
{
	// Remove regular debug messages
	if (GEngine)
		GEngine->ClearOnScreenDebugMessages();

	// Remove all Slate Notifications
	FSlateNotificationManager& NotifyManager = FSlateNotificationManager::Get();

	TArray<TSharedRef<SWindow>> Wins;
	NotifyManager.GetWindows(Wins);

	for (const auto& Win : Wins)
		Win->RequestDestroyWindow();

	// NotifyManager.SetAllowNotifications(true);
}

void FUMEditorCommands::ToggleAllowNotifications()
{
	FSlateNotificationManager& NotifyManager = FSlateNotificationManager::Get();
	if (NotifyManager.AreNotificationsAllowed())
	{
		NotifyManager.SetAllowNotifications(false);
		EditorCommands->Logger.Print("ToggleAllowNotificaiton: False");
	}
	else
	{
		NotifyManager.SetAllowNotifications(true);
		EditorCommands->Logger.Print("ToggleAllowNotificaiton: True");
	}
}
