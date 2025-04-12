#include "VimTextEditorUtils.h"
#include "VimEditorSubsystem.h"

DEFINE_LOG_CATEGORY_STATIC(LogVimTextEditorUtils, Log, All); // Dev
FUMLogger FVimTextEditorUtils::Logger(&LogVimTextEditorUtils);

//------------------------------------------------------------------------------
// Word Boundary Helper Implementations
//------------------------------------------------------------------------------

// Helper to determine what counts as a word character.
// For "small word" motions (w, b) we treat alphanumeric and underscore as word characters.
bool FVimTextEditorUtils::IsWordChar(TCHAR Char)
{
	return FChar::IsAlnum(Char) || Char == TEXT('_');
}

EUMCharType FVimTextEditorUtils::GetCharType(const FString& Text, int32 Position)
{
	if (Position < 0 || Position >= Text.Len())
		return EUMCharType::Whitespace; // Default for out-of-bounds

	if (FChar::IsWhitespace(Text[Position]))
		return EUMCharType::Whitespace;
	else if (IsWordChar(Text[Position]))
		return EUMCharType::Word;
	else
		return EUMCharType::Symbol;
}

int32 FVimTextEditorUtils::SkipCharType(const FString& Text, int32 StartPos, int32 Direction, EUMCharType TypeToSkip)
{
	const int32 Len = Text.Len();
	int32		CurrentPos = StartPos;

	if (Direction > 0)
	{
		// Forward direction
		while (CurrentPos < Len && GetCharType(Text, CurrentPos) == TypeToSkip)
			++CurrentPos;
	}
	else
	{
		// Backward direction
		while (CurrentPos > 0 && GetCharType(Text, CurrentPos - 1) == TypeToSkip)
			--CurrentPos;
	}

	return CurrentPos;
}

int32 FVimTextEditorUtils::SkipNonWhitespace(const FString& Text, int32 StartPos, int32 Direction)
{
	const int32 Len = Text.Len();
	int32		CurrentPos = StartPos;

	if (Direction > 0)
	{
		// Forward direction
		while (CurrentPos < Len && !FChar::IsWhitespace(Text[CurrentPos]))
			++CurrentPos;
	}
	else
	{
		// Backward direction
		while (CurrentPos > 0 && !FChar::IsWhitespace(Text[CurrentPos - 1]))
			--CurrentPos;
	}

	return CurrentPos;
}

int32 FVimTextEditorUtils::SkipWhitespace(const FString& Text, int32 StartPos, int32 Direction)
{
	return SkipCharType(Text, StartPos, Direction, EUMCharType::Whitespace);
}

int32 FVimTextEditorUtils::FindNextWordBoundary(const FString& Text, int32 CurrentPos, bool bBigWord)
{
	if (bBigWord)
		return FindNextBigWordBoundary(Text, CurrentPos);
	else
		return FindNextSmallWordBoundary(Text, CurrentPos);
}

int32 FVimTextEditorUtils::FindPreviousWordBoundary(const FString& Text, int32 CurrentPos, bool bBigWord)
{
	if (bBigWord)
		return FindPreviousBigWordBoundary(Text, CurrentPos);
	else
		return FindPreviousSmallWordBoundary(Text, CurrentPos);
}

int32 FVimTextEditorUtils::FindNextBigWordBoundary(const FString& Text, int32 CurrentPos)
{
	const int32 Len = Text.Len();

	// Handle boundary cases
	if (CurrentPos >= Len - 1)
		return Len;

	int32 NewPos = CurrentPos + 1; // Always make progress

	// For big words, only care about whitespace vs. non-whitespace
	bool bCurrentIsWhitespace = (CurrentPos < Len)
		&& FChar::IsWhitespace(Text[CurrentPos]);

	if (bCurrentIsWhitespace)
	{
		// Skip all consecutive whitespace
		NewPos = SkipWhitespace(Text, NewPos, 1);
	}
	else
	{
		// Skip all consecutive non-whitespace, then any whitespace that follows
		NewPos = SkipNonWhitespace(Text, NewPos, 1);
		NewPos = SkipWhitespace(Text, NewPos, 1);
	}

	return NewPos;
}

int32 FVimTextEditorUtils::FindPreviousBigWordBoundary(const FString& Text, int32 CurrentPos)
{
	// Handle boundary cases
	if (CurrentPos <= 0)
		return 0;

	int32 NewPos = CurrentPos;

	// Skip any whitespace backwards
	NewPos = SkipWhitespace(Text, NewPos, -1);

	// If we're now on a non-whitespace character, find the start of this word
	if (NewPos > 0 && !FChar::IsWhitespace(Text[NewPos - 1]))
	{
		NewPos = SkipNonWhitespace(Text, NewPos, -1);
	}

	return NewPos;
}

int32 FVimTextEditorUtils::FindNextSmallWordBoundary(const FString& Text, int32 CurrentPos)
{
	const int32 Len = Text.Len();

	// Handle boundary cases
	if (CurrentPos >= Len - 1)
		return Len;

	int32 NewPos = CurrentPos + 1; // Always make progress

	// Determine current character type
	EUMCharType CurrentType = GetCharType(Text, CurrentPos);
	EUMCharType NextType = GetCharType(Text, NewPos);

	// If transitioning to a different type, that's a word boundary
	if (CurrentType != NextType)
	{
		// If moving to whitespace, skip all consecutive whitespace
		if (NextType == EUMCharType::Whitespace)
		{
			NewPos = SkipWhitespace(Text, NewPos, 1);
		}
	}
	else
	{
		// Still in same type, skip all characters of this type
		NewPos = SkipCharType(Text, NewPos, 1, CurrentType);

		// Skip any trailing whitespace
		NewPos = SkipWhitespace(Text, NewPos, 1);
	}

	return NewPos;
}

int32 FVimTextEditorUtils::FindPreviousSmallWordBoundary(const FString& Text, int32 CurrentPos)
{
	// Handle boundary cases
	if (CurrentPos <= 0)
		return 0;

	int32 NewPos = CurrentPos;

	// Skip any trailing whitespace
	NewPos = SkipWhitespace(Text, NewPos, -1);

	// If we reached the beginning after skipping whitespace, return 0
	if (NewPos == 0)
		return 0;

	// Determine the type of character we're on now
	EUMCharType CurrentType = GetCharType(Text, NewPos - 1);

	// Find the beginning of this character group
	NewPos = SkipCharType(Text, NewPos, -1, CurrentType);

	return NewPos;
}

int32 FVimTextEditorUtils::FindNextWordEnd(const FString& Text, int32 CurrentPos, bool bBigWord)
{
	if (bBigWord)
		return FindNextBigWordEnd(Text, CurrentPos);
	else
		return FindNextSmallWordEnd(Text, CurrentPos);
}

int32 FVimTextEditorUtils::FindPreviousWordEnd(const FString& Text, int32 CurrentPos, bool bBigWord)
{
	if (bBigWord)
		return FindPreviousBigWordEnd(Text, CurrentPos);
	else
		return FindPreviousSmallWordEnd(Text, CurrentPos);
}

int32 FVimTextEditorUtils::FindNextBigWordEnd(const FString& Text, int32 CurrentPos)
{
	const int32 Len = Text.Len();

	// Handle boundary cases
	if (CurrentPos >= Len - 1)
		return Len - 1;

	int32 NewPos = CurrentPos + 1; // Always make progress from current position

	// Skip any whitespace ahead
	NewPos = SkipWhitespace(Text, NewPos, 1);

	// If we're not at the end of the text
	if (NewPos < Len)
	{
		// Find the end of the next non-whitespace chunk
		const int32 EndPos = SkipNonWhitespace(Text, NewPos, 1);

		// If we found a non-whitespace chunk, position at its last character
		if (EndPos > NewPos)
			NewPos = EndPos - 1;
	}
	else
	{
		// If we're at the end, back up to the last non-whitespace character
		NewPos = Len - 1;
		while (NewPos > CurrentPos && FChar::IsWhitespace(Text[NewPos]))
			NewPos--;
	}

	return NewPos;
}

int32 FVimTextEditorUtils::FindPreviousBigWordEnd(const FString& Text, int32 CurrentPos)
{
	// Handle boundary cases
	if (CurrentPos <= 0)
		return 0;

	int32 NewPos = CurrentPos;

	// Skip whitespace backward if we're currently in whitespace
	if (NewPos < Text.Len() && FChar::IsWhitespace(Text[NewPos]))
		NewPos = SkipWhitespace(Text, NewPos, -1);

	// If we're at a non-whitespace character, we need to find the previous word
	if (NewPos > 0)
	{
		// First go back to the beginning of the current word (if we're in a word)
		if (NewPos < Text.Len() && !FChar::IsWhitespace(Text[NewPos]))
		{
			// If we're in a word, move to its beginning
			int32 WordBegin = SkipNonWhitespace(Text, NewPos, -1);

			// If we're already at the beginning of a word, we need to find the previous word
			if (WordBegin == NewPos)
			{
				// Skip any whitespace backwards
				NewPos = SkipWhitespace(Text, WordBegin, -1);

				// Find the beginning of the previous word
				if (NewPos > 0)
				{
					int32 PrevWordBegin = SkipNonWhitespace(Text, NewPos, -1);

					// Now find the end of that word
					NewPos = PrevWordBegin;
					while (NewPos < Text.Len() - 1 && !FChar::IsWhitespace(Text[NewPos + 1]))
						NewPos++;
				}
			}
			else
			{
				// If we moved to the beginning, go back to find the previous word
				NewPos = SkipWhitespace(Text, WordBegin, -1);

				// Find the beginning of the previous word
				if (NewPos > 0)
				{
					int32 PrevWordBegin = SkipNonWhitespace(Text, NewPos, -1);

					// Now find the end of that word
					NewPos = PrevWordBegin;
					while (NewPos < Text.Len() - 1 && !FChar::IsWhitespace(Text[NewPos + 1]))
						NewPos++;
				}
			}
		}
		else
		{
			// We're on whitespace or at end of string, find the previous non-whitespace character
			NewPos = SkipWhitespace(Text, NewPos, -1);

			// If we found non-whitespace, find the beginning of that word
			if (NewPos > 0)
			{
				int32 WordBegin = SkipNonWhitespace(Text, NewPos, -1);

				// Position at the end of this word
				NewPos = WordBegin;
				while (NewPos < Text.Len() - 1 && !FChar::IsWhitespace(Text[NewPos + 1]))
					NewPos++;
			}
		}
	}

	return NewPos;
}

int32 FVimTextEditorUtils::FindNextSmallWordEnd(const FString& Text, int32 CurrentPos)
{
	const int32 Len = Text.Len();

	// Handle boundary cases
	if (CurrentPos >= Len - 1)
		return Len - 1;

	int32 NewPos = CurrentPos + 1; // Always make progress from current position

	// Skip any whitespace ahead
	NewPos = SkipWhitespace(Text, NewPos, 1);

	// If we're not at the end of the text
	if (NewPos < Len)
	{
		// Get the type of the character at our new position
		EUMCharType CurrentType = GetCharType(Text, NewPos);

		// Find the last character of this type
		int32 EndPos = NewPos;
		while (EndPos < Len - 1 && GetCharType(Text, EndPos + 1) == CurrentType)
			EndPos++;

		NewPos = EndPos;
	}
	else
	{
		// If we're at the end, back up to the last non-whitespace character
		NewPos = Len - 1;
		while (NewPos > CurrentPos && FChar::IsWhitespace(Text[NewPos]))
			NewPos--;
	}

	return NewPos;
}

int32 FVimTextEditorUtils::FindPreviousSmallWordEnd(const FString& Text, int32 CurrentPos)
{
	if (CurrentPos <= 0)
		return 0;

	int32 Pos = CurrentPos;

	// If the cursor is within the text and is on whitespace,
	// or if we're at the very end, back up over any trailing whitespace.
	if ((Pos < Text.Len() && FChar::IsWhitespace(Text[Pos])) || Pos == Text.Len())
	{
		Pos = SkipWhitespace(Text, Pos, -1);
		if (Pos <= 0)
			return 0;
		return Pos - 1;
	}

	// Determine the start of the "current" word.
	// If the character immediately left of the cursor is whitespace,
	// then we are already at a word boundary.
	int32 CurrentWordStart;
	if (Pos > 0 && FChar::IsWhitespace(Text[Pos - 1]))
	{
		CurrentWordStart = Pos;
	}
	else
	{
		// We're in the middle or at the end of a word.
		// Get the type of the character immediately left of pos.
		EUMCharType LeftCharType = GetCharType(Text, Pos - 1);

		// Get the type of the current char we're at, if it doesn't match the
		// type to the left, that's a word boundary.
		EUMCharType CurrCharType = GetCharType(Text, Pos);
		if (LeftCharType != CurrCharType)
			return Pos - 1;

		// Move backward over characters of this same type.
		CurrentWordStart = SkipCharType(Text, Pos, -1, LeftCharType);
	}

	// Now skip backwards over any whitespace preceding the current word.
	int32 PrevBoundary = SkipWhitespace(Text, CurrentWordStart, -1);
	if (PrevBoundary <= 0)
		return 0;

	// The previous word is the one whose last character is just before PrevBoundary.
	// Get that word's type.
	EUMCharType PrevType = GetCharType(Text, PrevBoundary - 1);
	// Find the beginning of the previous word group.
	int32 PrevWordStart = SkipCharType(Text, PrevBoundary, -1, PrevType);

	// Scan forward from PrevWordStart until the character type changes to get its end.
	int32 PrevWordEnd = PrevWordStart;
	while (PrevWordEnd < Text.Len() && GetCharType(Text, PrevWordEnd) == PrevType)
	{
		PrevWordEnd++;
	}

	// Return the index of the last character of the previous word.
	return PrevWordEnd - 1;
}

bool FVimTextEditorUtils::GetAbsWordBoundaries(const FString& Text, int32 CurrentPos, TPair<int32, int32>& OutWordBoundaries, const bool bIncludeTrailingSpaces)
{
	const int32 TextLen = Text.Len();
	if (CurrentPos <= 0 || CurrentPos > TextLen || TextLen <= 0)
		return false;

	int32 Pos = CurrentPos;

	// if currently on whitespace, the boundaries are until the next and previous
	// non-space characters
	if (FChar::IsWhitespace(Text[Pos]))
	{
		const int32 BoundaryStart = SkipWhitespace(Text, Pos, -1);
		const int32 BoundaryEnd = SkipWhitespace(Text, Pos, 1);
		OutWordBoundaries = TPair<int32, int32>(BoundaryStart, BoundaryEnd);
		return true;
	}
	else if (FChar::IsAlnum(Text[Pos]))
	{
		const int32 BoundaryStart = SkipCharType(Text, Pos, -1, EUMCharType::Word);
		const int32 BoundaryEnd = SkipCharType(Text, Pos, 1, EUMCharType::Word);
		OutWordBoundaries = TPair<int32, int32>(BoundaryStart, BoundaryEnd);
		return true;
	}
	else if (FChar::IsPunct(Text[Pos]))
	{
		const int32 BoundaryStart = SkipCharType(Text, Pos, -1, EUMCharType::Symbol);
		const int32 BoundaryEnd = SkipCharType(Text, Pos, 1, EUMCharType::Symbol);
		OutWordBoundaries = TPair<int32, int32>(BoundaryStart, BoundaryEnd);
		return true;
	}

	return true;
}

//------------------------------------------------------------------------------
// Text Location Helpers for Multi-line Editables
//------------------------------------------------------------------------------

// Convert a FTextLocation into an absolute offset.
int32 FVimTextEditorUtils::TextLocationToAbsoluteOffset(const FString& Text, const FTextLocation& Location)
{
	TArray<FString> Lines;
	Text.ParseIntoArray(Lines, TEXT("\n"), false /*Consider Empty Lines*/);
	int32 Offset = 0;
	for (int32 i = 0; i < Location.GetLineIndex(); ++i)
	{
		Offset += Lines[i].Len() + 1;
	}
	Offset += Location.GetOffset();
	return Offset;
}

// Convert an absolute offset into a FTextLocation (line index and offset).
void FVimTextEditorUtils::AbsoluteOffsetToTextLocation(const FString& Text, int32 AbsoluteOffset, FTextLocation& OutLocation)
{
	TArray<FString> Lines;
	Text.ParseIntoArray(Lines, TEXT("\n"), false /*Consider Empty Lines*/);
	int32 Offset = AbsoluteOffset;
	for (int32 LineIndex = 0; LineIndex < Lines.Num(); ++LineIndex)
	{
		// If the offset is within this line, that’s our location.
		if (Offset <= Lines[LineIndex].Len())
		{
			OutLocation = FTextLocation(LineIndex, Offset);
			return;
		}
		// Subtract the length of this line plus one for the newline.
		Offset -= (Lines[LineIndex].Len() + 1);
	}
	// Fallback: place at end of last line.
	OutLocation = FTextLocation(Lines.Num() - 1, Lines.Last().Len());
}
