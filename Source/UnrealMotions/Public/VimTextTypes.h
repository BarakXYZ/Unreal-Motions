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

// Helper struct to group position data and compare cursor locations
struct VisualModePositions
{
	int32 CurrLineIndex;
	int32 CurrOffset;
	int32 StartLine;
	int32 StartOffset;
	bool  bIsMovingUp;
	int32 MaxLine;

	int32 GetTargetLine() const
	{
		return bIsMovingUp
			? FMath::Max(0, CurrLineIndex - 1)
			: FMath::Min(CurrLineIndex + 1, MaxLine);
	}

	bool IsOnStartLine() const { return CurrLineIndex == StartLine; }
	bool IsAboveStartLine() const { return GetTargetLine() < StartLine; }
	bool IsBelowStartLine() const { return GetTargetLine() > StartLine; }
};
