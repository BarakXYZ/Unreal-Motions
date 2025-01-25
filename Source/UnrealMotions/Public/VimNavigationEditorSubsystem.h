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

	/**
	 * Collect all interactable widgets from closest to farthest
	 * this will be useful to collect them in that order since we can then
	 * potentially assign more comfortable bindings to the farthest widgets
	 * (which are more likely the ones we want to travel too. I think (lol))
	 * Return true if any interactable widgets were found. False otherwise.
	 */
	bool CollectInteractableWidgets(TArray<TSharedPtr<SWidget>>& OutWidgets);

	void GenerateLabels(
		int32			 NumLabels,
		const FString&	 AllowedCharacters,
		TArray<FString>& OutLabels);

	////////////////////////////////////////////////////////////////////////////
	//							Trie Handling
	//
	/** */
	void BuildHintTrie(
		const TArray<FString>&					 HintLabels,
		const TArray<TSharedPtr<SUMHintMarker>>& HintMarkers);

	void ProcessHintInput(const FInputChord& InputChord);

	void VisualizeHints(FUMHintWidgetTrieNode* Node);

	void ResetHintState();
	//
	//							Trie Handling
	////////////////////////////////////////////////////////////////////////////

	FUMLogger			   Logger;
	FHintOverlayData	   HintOverlayData;
	FUMHintWidgetTrieNode* RootHintNode = nullptr; // Root node (unique)
	FUMHintWidgetTrieNode* CurrentHintNode = nullptr;
};
