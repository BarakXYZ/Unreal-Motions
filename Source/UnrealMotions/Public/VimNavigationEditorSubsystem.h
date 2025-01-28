#pragma once

#include "Framework/Application/SlateApplication.h"
#include "UMLogger.h"
#include "SUMHintOverlay.h"
#include "VimInputProcessor.h"
#include "UMHintWidgetTrieNode.h"
#include "EditorSubsystem.h"
#include "VimNavigationEditorSubsystem.generated.h"

struct FHintOverlayData
{
	TWeakPtr<SUMHintOverlay> HintOverlay;
	TWeakPtr<SWindow>		 AssociatedWindow;
	bool					 bIsHintOverlayDisplayed;

	// Default Constructor
	FHintOverlayData()
		: HintOverlay(nullptr)
		, AssociatedWindow(nullptr)
		, bIsHintOverlayDisplayed(false)
	{
	}

	// Parameterized Constructor
	FHintOverlayData(
		TWeakPtr<SUMHintOverlay> InHintOverlay,
		TWeakPtr<SWindow>		 InAssociatedWindow,
		bool					 bIsDisplayed = true)
		: HintOverlay(InHintOverlay)
		, AssociatedWindow(InAssociatedWindow)
		, bIsHintOverlayDisplayed(bIsDisplayed)
	{
	}

	~FHintOverlayData()
	{
		Reset();
	}

	void Reset()
	{
		HintOverlay.Reset();
		AssociatedWindow.Reset();
		bIsHintOverlayDisplayed = false;
	}

	bool IsDisplayed() { return bIsHintOverlayDisplayed; }
};

/**
 *
 */
UCLASS()
class UNREALMOTIONS_API UVimNavigationEditorSubsystem : public UEditorSubsystem
{
public:
	GENERATED_BODY()
	/**
	 * Depending on if Vim is enabled in the config, will control if the
	 * subsystem should be created.
	 */
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;

	/**
	 */
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	virtual void Deinitialize() override;

	void BindVimCommands();

	void OnVimModeChanged(const EVimMode NewVimMode);

	void NavigatePanelTabs(
		FSlateApplication& SlateApp, const FKeyEvent& InKey);

	// Collect all focusable widgets (buttons, hyperlinks, lists, etc.)
	// Create a small widget for each of them with a unique 2 characters id.
	// This widget (or in collaboration with the VimProcessor) should listen to
	// the user input and detect when a unique 2 character combination is pressed.
	// Once executed, the widget that is bound to the 2 char id should be focused.
	// All widget dissapear.
	void FlashHintMarkers(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent);

	void FlashHintMarkersMultiWindow(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent);

	/**
	 * Single-Window Edition:
	 * Collects all interactable widgets within the currently active window.
	 *
	 * @param OutWidgets  Array to store the collected interactable widgets.
	 * @return true if any interactable widgets were found, false otherwise.
	 */
	bool CollectInteractableWidgets(TArray<TSharedPtr<SWidget>>& OutWidgets);

	/**
	 * Multi-Window Edition:
	 * Collects interactable widgets for all visible windows
	 * (not just the currently active one). Ignores non-visible windows.
	 *
	 * @param OutWidgets     Array of arrays, where each inner array contains
	 * widgets for a specific window.
	 * @param ParentWindows  Array of parent windows, synchronized with the index
	 * of each widgets array. Each entry corresponds to the parent window of the
	 * widgets in OutWidgets.
	 * @return true if OutWidgets contains at least one non-empty array, false
	 * otherwise.
	 */
	bool CollectInteractableWidgets(
		TArray<TArray<TSharedPtr<SWidget>>>& OutWidgets,
		TArray<TSharedRef<SWindow>>&		 ParentWindows);

	TArray<FString> GenerateLabels(int32 NumLabels);

	////////////////////////////////////////////////////////////////////////////
	//							Trie Handling
	//
	bool BuildHintTrie(
		const TArray<FString>&					 HintLabels,
		const TArray<TSharedRef<SUMHintMarker>>& HintMarkers);

	bool CheckCharToKeyConversion(const TCHAR InChar, const FKey& InKey);

	void ProcessHintInputBase(
		FSlateApplication&							SlateApp,
		const FKeyEvent&							InKeyEvent,
		TFunction<void()>							ResetHintMarkersFunc,
		TFunction<void(const TSharedRef<SWidget>&)> HandleWidgetExecutionFunc);

	void ProcessHintInput(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent);

	void ProcessHintInputMultiWindow(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent);

	void VisualizeHints(const TSharedRef<FUMHintWidgetTrieNode> Node);
	bool OnBackSpaceHintMarkers(const FKeyEvent& InKeyEvent);

	void ResetHintMarkersCore();

	void ResetHintMarkers();
	void ResetHintMarkersMultiWindow();
	//
	//							Trie Handling
	////////////////////////////////////////////////////////////////////////////

	FUMLogger						Logger;
	FHintOverlayData				HintOverlayData;
	TArray<FHintOverlayData>		PerWindowHintOverlayData;
	TArray<TWeakPtr<SUMHintMarker>> PressedHintMarkers;

	/** The root of our hint-marker Trie. */
	TSharedPtr<FUMHintWidgetTrieNode> RootHintNode;

	/** Which node we are currently traversing after partial input. */
	TSharedPtr<FUMHintWidgetTrieNode> CurrentHintNode;
};
