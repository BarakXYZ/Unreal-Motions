#include "ErgonomicsUtilityObject.h"
#include "Framework/Commands/InputBindingManager.h"
#include "Framework/Commands/UICommandInfo.h"
#include "Helpers.h"

void UErgonomicsUtilityObject::DebugBookmarks()
{
	const FName						   LevelViewportsContextName = TEXT("LevelViewport");
	FInputBindingManager&			   InputBindingManager = FInputBindingManager::Get();
	TArray<TSharedPtr<FUICommandInfo>> LevelViewportCommandsInfo;
	InputBindingManager.GetCommandInfosFromContext(LevelViewportsContextName, LevelViewportCommandsInfo);

	for (auto Command : LevelViewportCommandsInfo)
	{
		FString CommandStr = Command->GetCommandName().ToString();
		if (CommandStr.StartsWith("SetBookmark"))
		{
			FHelpers::NotifySuccess(FText::FromString(CommandStr));
			Command->RemoveActiveChord(EMultipleKeyBindingIndex::Primary);
		}
	}
}

void UErgonomicsUtilityObject::ConstructChords(bool bCtrl, bool bAlt, bool bShift, bool bCmd)

{
	if (!Chords.IsEmpty())
		Chords.Empty();
	Chords.Reserve(10);

	const FKey NumberKeys[] = {
		EKeys::Zero, EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four, EKeys::Five, EKeys::Six, EKeys::Seven, EKeys::Eight, EKeys::Nine
	};

	const EModifierKey::Type ModifierKeys =
		EModifierKey::FromBools(bCtrl, bAlt, bShift, bCmd);

	for (const FKey& Key : NumberKeys)
	{
		Chords.Add(FInputChord(ModifierKeys, Key));
	}
}

void UErgonomicsUtilityObject::ConstructTargetParams(ETargetSection TargetSection)
{
	switch (TargetSection)
	{
		case (ETargetSection::ViewportBookmarks):
			ContextName = TEXT("LevelViewport");
			TargetPrefix = "SetBookmark";
			break;
		case (ETargetSection::GraphEditorBookmarks):
			ContextName = TEXT("GraphEditor");
			TargetPrefix = "SetQuickJump";
			break;

		case (ETargetSection::TabNavigation):
			ContextName = TEXT("MainFrame");
			TargetPrefix = "MoveToTab";
			break;
	}
}

void UErgonomicsUtilityObject::ClearHotkeys(ETargetSection TargetSection, bool bClearSecondary)
{
	ConstructTargetParams(TargetSection);
	FInputBindingManager&			   InputBindingManager = FInputBindingManager::Get();
	TArray<TSharedPtr<FUICommandInfo>> LevelViewportCommandsInfo;
	InputBindingManager.GetCommandInfosFromContext(
		ContextName, LevelViewportCommandsInfo);

	for (auto Command : LevelViewportCommandsInfo)
	{
		FString CommandStr = Command->GetCommandName().ToString();
		if (CommandStr.StartsWith(TargetPrefix))
		{
			Command->RemoveActiveChord(EMultipleKeyBindingIndex::Primary);
			if (bClearSecondary)
				Command->RemoveActiveChord(EMultipleKeyBindingIndex::Secondary);
			FHelpers::NotifySuccess(FText::FromString("Command Clear: " + CommandStr));
		}
	}
}

void UErgonomicsUtilityObject::SetHotkeysToNums(
	ETargetSection TargetSection,
	bool		   bClearConflictingKeys,
	bool bCtrl, bool bAlt, bool bShift, bool bCmd)
{
	ConstructTargetParams(TargetSection);

	FInputBindingManager&			   InputBindingManager = FInputBindingManager::Get();
	TArray<TSharedPtr<FUICommandInfo>> LevelViewportCommandsInfo;
	InputBindingManager.GetCommandInfosFromContext(
		ContextName, LevelViewportCommandsInfo);

	ConstructChords(bCtrl, bAlt, bShift, bCmd);
	int32 i{ 0 };
	for (auto Command : LevelViewportCommandsInfo)
	{
		FString CommandStr = Command->GetCommandName().ToString();
		if (CommandStr.StartsWith(TargetPrefix) && i < 10)
		{
			if (bClearConflictingKeys)
			{
				const TSharedPtr<FUICommandInfo> CheckCommand = InputBindingManager.GetCommandInfoFromInputChord(ContextName, Chords[i], false);
				if (CheckCommand.IsValid())
					CheckCommand->RemoveActiveChord(EMultipleKeyBindingIndex::Primary);
			}
			Command->SetActiveChord(Chords[i], EMultipleKeyBindingIndex::Primary);
			FHelpers::NotifySuccess(FText::FromString("Command Set: " + CommandStr));
			++i;
		}
	}
}
