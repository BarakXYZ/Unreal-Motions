// Copyright 2024 BarakXYZ. All Rights Reserved.

#include "ErgonomicsBPFuncLib.h"
#include "Framework/Commands/InputBindingManager.h"
#include "Framework/Commands/UICommandList.h"
#include "Interfaces/IMainFrameModule.h"
#include "Helpers.h"
#include "ErgonomicsXYZ.h"

void UErgonomicsBPFuncLib::DebugCommands()
{
	const FName			  MainFrameContextName = TEXT("MainFrame");
	FInputBindingManager& InputBindingManager = FInputBindingManager::Get();
	FInputChord			  Command(EModifierKey::FromBools(true, false, false, false), EKeys::One);
	FInputChord			  NewCommand(EModifierKey::FromBools(true, false, true, false), EKeys::O);

	const TSharedPtr<FUICommandInfo> CheckCommand = InputBindingManager.GetCommandInfoFromInputChord(
		MainFrameContextName, Command, false);

	if (CheckCommand.IsValid())
	{
		CheckCommand->SetActiveChord(NewCommand, EMultipleKeyBindingIndex::Primary);
		FHelpers::NotifySuccess(FText::FromName(CheckCommand->GetBindingContext()));

		// Save to input bindings
		// InputBindingManager.SaveInputBindings();
	}
}
