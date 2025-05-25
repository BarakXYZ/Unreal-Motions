#pragma once
#include "Framework/Text/TextLayout.h"
#include "UMLogger.h"

/**
 * Character type enumeration used for word boundary detection
 */
enum class EUMCharType
{
	Word,	   // Alphanumeric or underscore
	Symbol,	   // Non-word, non-whitespace (punctuation, etc.)
	Whitespace // Space, tab, etc.
};

class FVimTextEditorUtils
{
public:
	//							~ Word Navigation ~
	//
	static bool IsWordChar(TCHAR Char);

	// Public word boundary functions

	// Helper functions

	/**
	 * Determines the character type at a given position in text
	 *
	 * @param Text - The string to analyze
	 * @param Position - The position to check
	 * @return The character type
	 */
	static EUMCharType GetCharType(const FString& Text, int32 Position);

	/**
	 * Skips consecutive characters of the specified type in the given direction
	 *
	 * @param Text - The string to traverse
	 * @param StartPos - The starting position
	 * @param Direction - 1 for forward, -1 for backward
	 * @param TypeToSkip - The character type to skip
	 * @return The new position after skipping
	 */
	static int32 SkipCharType(const FString& Text, int32 StartPos, int32 Direction, EUMCharType TypeToSkip);

	/**
	 * Skips consecutive non-whitespace characters in the given direction
	 *
	 * @param Text - The string to traverse
	 * @param StartPos - The starting position
	 * @param Direction - 1 for forward, -1 for backward
	 * @return The new position after skipping
	 */
	static int32 SkipNonWhitespace(const FString& Text, int32 StartPos, int32 Direction);

	/**
	 * Skips whitespace in the given direction
	 *
	 * @param Text - The string to traverse
	 * @param StartPos - The starting position
	 * @param Direction - 1 for forward, -1 for backward
	 * @return The new position after skipping whitespace
	 */
	static int32 SkipWhitespace(const FString& Text, int32 StartPos, int32 Direction);

	/**
	 * Returns the absolute offset (0-based) for the next word boundary.
	 * bBigWord == false means using "small word" rules (w), where punctuation is a delimiter;
	 * bBigWord == true means whitespace-delimited words (W).
	 *
	 * @param Text - The string to search
	 * @param CurrentPos - The current position
	 * @param bBigWord - Whether to use "big word" rules
	 * @return The position of the next word boundary
	 */
	static int32 FindNextWordBoundary(const FString& Text, int32 CurrentPos, bool bBigWord);

	/**
	 * Returns the absolute offset for the previous word boundary.
	 *
	 * @param Text - The string to search
	 * @param CurrentPos - The current position
	 * @param bBigWord - Whether to use "big word" rules
	 * @return The position of the previous word boundary
	 */
	static int32 FindPreviousWordBoundary(const FString& Text, int32 CurrentPos, bool bBigWord);

	/**
	 * Finds the next boundary of a "big word" (W)
	 *
	 * @param Text - The string to traverse
	 * @param CurrentPos - The current position
	 * @return The position of the next big word boundary
	 */
	static int32 FindNextBigWordBoundary(const FString& Text, int32 CurrentPos);

	/**
	 * Finds the previous boundary of a "big word" (B)
	 *
	 * @param Text - The string to traverse
	 * @param CurrentPos - The current position
	 * @return The position of the previous big word boundary
	 */
	static int32 FindPreviousBigWordBoundary(const FString& Text, int32 CurrentPos);

	/**
	 * Finds the next boundary of a "small word" (w)
	 *
	 * @param Text - The string to traverse
	 * @param CurrentPos - The current position
	 * @return The position of the next small word boundary
	 */
	static int32 FindNextSmallWordBoundary(const FString& Text, int32 CurrentPos);

	/**
	 * Finds the previous boundary of a "small word" (b)
	 *
	 * @param Text - The string to traverse
	 * @param CurrentPos - The current position
	 * @return The position of the previous small word boundary
	 */
	static int32 FindPreviousSmallWordBoundary(const FString& Text, int32 CurrentPos);

	// New functions for word ending

	/**
	 * Returns the absolute offset (0-based) for the next word end.
	 * bBigWord == false means using "small word" rules (e), where punctuation is a delimiter;
	 * bBigWord == true means whitespace-delimited words (E).
	 *
	 * @param Text - The string to search
	 * @param CurrentPos - The current position
	 * @param bBigWord - Whether to use "big word" rules
	 * @return The position of the next word end
	 */
	static int32 FindNextWordEnd(const FString& Text, int32 CurrentPos, bool bBigWord);

	/**
	 * Returns the absolute offset for the previous word end.
	 *
	 * @param Text - The string to search
	 * @param CurrentPos - The current position
	 * @param bBigWord - Whether to use "big word" rules
	 * @return The position of the previous word end
	 */
	static int32 FindPreviousWordEnd(const FString& Text, int32 CurrentPos, bool bBigWord);

	/**
	 * Finds the next ending of a "big word" (E)
	 *
	 * @param Text - The string to traverse
	 * @param CurrentPos - The current position
	 * @return The position of the next big word end
	 */
	static int32 FindNextBigWordEnd(const FString& Text, int32 CurrentPos);

	/**
	 * Finds the previous ending of a "big word" (gE)
	 *
	 * @param Text - The string to traverse
	 * @param CurrentPos - The current position
	 * @return The position of the previous big word end
	 */
	static int32 FindPreviousBigWordEnd(const FString& Text, int32 CurrentPos);

	/**
	 * Finds the next ending of a "small word" (e)
	 *
	 * @param Text - The string to traverse
	 * @param CurrentPos - The current position
	 * @return The position of the next small word end
	 */
	static int32 FindNextSmallWordEnd(const FString& Text, int32 CurrentPos);

	/**
	 * Finds the previous ending of a "small word" (ge)
	 *
	 * @param Text - The string to traverse
	 * @param CurrentPos - The current position
	 * @return The position of the previous small word end
	 */
	static int32 FindPreviousSmallWordEnd(const FString& Text, int32 CurrentPos);

	static bool GetAbsWordBoundaries(const FString& Text, int32 CurrentPos, TPair<int32, int32>& OutWordBoundaries, const bool bIncludeTrailingSpaces);

	static void AbsoluteOffsetToTextLocation(const FString& Text, int32 AbsoluteOffset, FTextLocation& OutLocation);

	static int32 TextLocationToAbsoluteOffset(const FString& Text, const FTextLocation& Location);

	static void DetermineVimModeForSingleLineEncounter();

	static FUMLogger Logger;
};
