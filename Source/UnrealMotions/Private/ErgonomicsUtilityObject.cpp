#include "ErgonomicsUtilityObject.h"
#include "Framework/Commands/InputBindingManager.h"
#include "Framework/Commands/UICommandInfo.h"
#include "Helpers.h"
#include "HotkeySection.h"

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

void UErgonomicsUtilityObject::ClearHotkeys(EHotkeySection Section, bool bClearSecondary)
{
	if (FHotkeyInfo* HotkeyInfo = HotkeySectionInfo.Map.Find(Section))
	{
		FInputBindingManager&			   InputBindingManager = FInputBindingManager::Get();
		TArray<TSharedPtr<FUICommandInfo>> CommandInfo;
		InputBindingManager.GetCommandInfosFromContext(
			HotkeyInfo->ContextName, CommandInfo);

		for (const TSharedPtr<FUICommandInfo>& Command : CommandInfo)
		{
			FString CommandStr = Command->GetCommandName().ToString();
			if (CommandStr.StartsWith(HotkeyInfo->CommandPrefix)) // aka conflict
			{
				Command->RemoveActiveChord(EMultipleKeyBindingIndex::Primary);
				if (bClearSecondary)
					Command->RemoveActiveChord(EMultipleKeyBindingIndex::Secondary);
				FHelpers::NotifySuccess(FText::FromString("Command Clear: " + CommandStr));
			}
		}
		OnUtilityObjectAction.Broadcast();
	}
}

void UErgonomicsUtilityObject::SetHotkeysToNums(
	EHotkeySection Section, bool bClearConflictingKeys,
	bool bCtrl, bool bAlt, bool bShift, bool bCmd)
{

	if (FHotkeyInfo* HotkeyInfo = HotkeySectionInfo.Map.Find(Section))
	{
		FInputBindingManager&			   InputBindingManager = FInputBindingManager::Get();
		TArray<TSharedPtr<FUICommandInfo>> CommandInfo;
		InputBindingManager.GetCommandInfosFromContext(
			HotkeyInfo->ContextName, CommandInfo);

		ConstructChords(bCtrl, bAlt, bShift, bCmd);
		int32 i{ 0 };
		for (const TSharedPtr<FUICommandInfo>& Command : CommandInfo)
		{
			FString CommandStr = Command->GetCommandName().ToString();
			// We are targeting only 10 commands really, so just for safety.
			if (CommandStr.StartsWith(HotkeyInfo->CommandPrefix) && i < 10)
			{
				if (bClearConflictingKeys)
				{
					const TSharedPtr<FUICommandInfo> CheckCommand = InputBindingManager.GetCommandInfoFromInputChord(
						HotkeyInfo->ContextName, Chords[i], false);
					if (CheckCommand.IsValid())
						CheckCommand->RemoveActiveChord(EMultipleKeyBindingIndex::Primary);
				}
				Command->SetActiveChord(Chords[i], EMultipleKeyBindingIndex::Primary);
				FHelpers::NotifySuccess(FText::FromString("Command Set: " + CommandStr));
				++i;
			}
		}
		OnUtilityObjectAction.Broadcast();
	}
}

void UErgonomicsUtilityObject::GetHotkeyMappingState(
	EHotkeySection Section, TArray<FString>& OutMappingState)
{
	if (FHotkeyInfo* HotkeyInfo = HotkeySectionInfo.Map.Find(Section))
	{
		FInputBindingManager&			   InputBindingManager = FInputBindingManager::Get();
		TArray<TSharedPtr<FUICommandInfo>> CommandInfo;
		InputBindingManager.GetCommandInfosFromContext(
			HotkeyInfo->ContextName, CommandInfo);

		for (const TSharedPtr<FUICommandInfo>& Command : CommandInfo)
		{
			FString CommandStr = Command->GetCommandName().ToString();
			// We are targeting only 10 commands really, so just for safety.
			if (CommandStr.StartsWith(HotkeyInfo->CommandPrefix))
			{
				CommandStr += " " + Command->GetInputText().ToString();
				OutMappingState.Add(CommandStr);
			}
		}
	}
	UEnum* EnumPtr = StaticEnum<EHotkeySection>();
	EnumPtr->NumEnums();
}

// Debug
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
