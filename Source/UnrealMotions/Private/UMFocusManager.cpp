#include "UMFocusManager.h"
#include "Framework/Docking/TabManager.h"
#include "UMHelpers.h"

TSharedPtr<FUMFocusManager> FUMFocusManager::FocusManager =
	MakeShared<FUMFocusManager>();

FUMFocusManager::FUMFocusManager()
{
	FCoreDelegates::OnPostEngineInit.AddRaw(
		this, &FUMFocusManager::RegisterSlateEvents);
}

FUMFocusManager::~FUMFocusManager()
{
}

void FUMFocusManager::RegisterSlateEvents()
{
	FSlateApplication& SlateApp = FSlateApplication::Get();

	SlateApp.OnFocusChanging()
		.AddRaw(this, &FUMFocusManager::OnFocusChanged);
}

void FUMFocusManager::OnFocusChanged(const FFocusEvent& FocusEvent, const FWeakWidgetPath& OldWidgetPath,
	const TSharedPtr<SWidget>& OldWidget, const FWidgetPath& NewWidgetPath,
	const TSharedPtr<SWidget>& NewWidget)
{
	FUMHelpers::NotifySuccess(
		FText::FromString("Focus Manager On Focused Changed"));

	if (NewWidget.IsValid() && NewWidget != ActiveWidget)
		ActiveWidget = NewWidget;

	if (ActiveMinorTab.IsValid())
	{
		const TSharedPtr<SDockTab>& MinorTab = ActiveMinorTab.Pin();
		if (MinorTab)
		{
			const uint64& TabId = MinorTab->GetId();
			if (LastActiveWidgetByMinorTabId.FindRef(TabId) != NewWidget)
				LastActiveWidgetByMinorTabId.Add(TabId, NewWidget);
		}
	}
	else
	{ // Fallback to the globally active minor tab
		if (const auto& NewMinorTab = FGlobalTabmanager::Get()->GetActiveTab())
		{
			const uint64& GlobalTabId = NewMinorTab->GetId();
			if (LastActiveWidgetByMinorTabId.FindRef(GlobalTabId) != NewWidget)
				LastActiveWidgetByMinorTabId.Add(GlobalTabId, NewWidget);

			ActiveMinorTab = NewMinorTab;
		}
		else
		{
			FUMHelpers::NotifySuccess(
				FText::FromString("No active Minor Tab or Global Tab found."));
		}
	}
}
