	// This may be potentially useful for if we want to track pop-up windows
	// The returned value is used if we want to unregister
	// FDelegateHandle MyWindowActionHandle = FSlateApplication::Get().RegisterOnWindowActionNotification(
	// 	FOnWindowAction::CreateLambda([](const TSharedRef<FGenericWindow>& Window, EWindowAction::Type Action) -> bool {
	// 		const TArray<TSharedRef<SWindow>>& Wins =
	// 			FSlateApplication::Get().GetTopLevelWindows();

	// 		TSet<FGenericWindow*> GenWinsSet;
	// 		for (const auto& Win : Wins)
	// 		{
	// 			if (const auto GenWin = Win->GetNativeWindow())
	// 				GenWinsSet.Add(GenWin.Get());
	// 		}

	// 		if (GenWinsSet.Contains(&Window.Get()))
	// 			switch (Action)
	// 			{
	// 				case EWindowAction::ClickedNonClientArea:
	// 					FUMHelpers::NotifySuccess(
	// 						FText::FromString("Non-client area clicked on window"));
	// 					return false; // Allow the action to proceed.

	// 				case EWindowAction::Maximize:
	// 					FUMHelpers::NotifySuccess(
	// 						FText::FromString("Window maximized"));
	// 					return false; // Allow the maximize action.

	// 				case EWindowAction::Restore:
	// 					FUMHelpers::NotifySuccess(
	// 						FText::FromString("Window restored"));
	// 					return false; // Allow the restore action.

	// 				case EWindowAction::WindowMenu:
	// 					FUMHelpers::NotifySuccess(
	// 						FText::FromString("Window menu accessed"));
	// 					return true; // Block the action.

	// 				default:
	// 					FUMHelpers::NotifySuccess(
	// 						FText::FromString("Default"));
	// 					return false; // Default behavior.
	// 			}
	// 		return true;
	// 	}));

