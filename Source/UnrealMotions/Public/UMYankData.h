#include "VimTextTypes.h"
#include "CoreMinimal.h"

// Enum for yank types
enum class EUMYankType
{
	Characterwise,
	Linewise,
	Blockwise
};

// Structure for storing a yank
struct FUMYankData
{
	FString		Content; // The actual yanked text
	EUMYankType Type;	 // The type of yank

	// Default constructor
	FUMYankData()
		: Content("")
		, Type(EUMYankType::Characterwise)
	{
	}

	FUMYankData(const FString& InContent, EUMYankType InType)
		: Content(InContent)
		, Type(InType)
	{
	}

	FString GetText(const EUMEditableWidgetsFocusState InEditableType)
	{
		switch (InEditableType)
		{
			case EUMEditableWidgetsFocusState::SingleLine:
				return Content;

			case EUMEditableWidgetsFocusState::MultiLine:
				// return Content + "\n";
				return Content;

			default:
				return "";
		}
	}

	EUMYankType GetType()
	{
		return Type;
	}
};
