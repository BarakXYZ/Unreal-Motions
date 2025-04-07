#pragma once

enum class EUMEditableWidgetsFocusState : uint8
{
	None,
	SingleLine,
	MultiLine
};

enum class EUMSelectionState : uint8
{
	None,
	OneChar,
	ManyChars
};

struct FUMStringInfo
{
	int32 LineCount;
	int32 LastCharIndex;
};
