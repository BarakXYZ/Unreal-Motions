#pragma once

#include "CoreMinimal.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/SCompoundWidget.h"
#include "UMLogger.h"
#include "Widgets/Text/STextBlock.h"

class SUMSearch : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SUMSearch)
	{
	}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	static void			   Open();
	void				   Close();
	TSharedPtr<SSearchBox> GetTextBox();
	// virtual void		   OnFocusLost(const FFocusEvent& InFocusEvent) override;
	virtual void   OnFocusChanging(const FWeakWidgetPath& PreviousFocusPath, const FWidgetPath& NewWidgetPath, const FFocusEvent& InFocusEvent) override;
	virtual void   OnFocusLost(const FFocusEvent& InFocusEvent) override;
	virtual FReply OnKeyChar(const FGeometry& MyGeometry, const FCharacterEvent& InCharacterEvent) override;

private:
	FReply OnSearchBoxKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent);
	void   ProcessBackSpaceOnKeyDown();

	void OnSearchBoxTextChanged(const FText& InText);

	void OnSearchBoxTextCommitted(const FText& InText, ETextCommit::Type CommitType);
	bool GetAllTextBlocksInActiveTab();
	bool GetAllEditableTextsInActiveTab();

	void VisualizeMatchedTargets();
	void VisualizeMatchedTextBlocks(const FRegexPattern& Pattern);
	void VisualizeMatchedEditableTexts(const FRegexPattern& Pattern);

	void FocusFirstFoundMatch();
	bool FocusFirstFoundTextBlock(const FRegexPattern& Pattern);
	bool FocusFirstFoundEditableText(const FRegexPattern& Pattern);

	void ClearAllVisualizedMatches();

	FUMLogger Logger;

	TSharedPtr<SSearchBox>				   SearchBox;
	FString								   SearchText;
	TSet<STextBlock>					   TextBlockSet;
	TMap<FString, TWeakPtr<STextBlock>>	   TextBlocksByString;
	TMap<FString, TWeakPtr<SEditableText>> EditableTextsByString;
	TArray<TWeakPtr<SWidget>>			   WeakFoundTextBlocks;
};
