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

	FString GetText()
	{
		return Content;
	}

	EUMYankType GetType()
	{
		return Type;
	}
};
