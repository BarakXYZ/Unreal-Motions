// FModifierKeysStateHash.h (or wherever is convenient)
#pragma once

#include "InputCoreTypes.h"
#include "CoreMinimal.h"

/** Equality operator for FModifierKeysState (not provided by default). */
FORCEINLINE bool operator==(const FModifierKeysState& LHS, const FModifierKeysState& RHS)
{
	return LHS.IsShiftDown() == RHS.IsShiftDown() && LHS.IsAltDown() == RHS.IsAltDown() && LHS.IsControlDown() == RHS.IsControlDown() && LHS.IsCommandDown() == RHS.IsCommandDown() && LHS.AreCapsLocked() == RHS.AreCapsLocked();
	// (We typically ignore "IsRepeat()" in the standard FModifierKeysState
	//  because it’s not actually used by the engine to represent an ongoing
	//  'held' state.  But if you want to incorporate that, feel free.)
}

/** Hash function for FModifierKeysState. */
FORCEINLINE uint32 GetTypeHash(const FModifierKeysState& InState)
{
	// We’ll fold its booleans into a bitmask.
	uint8 Flags = 0;
	Flags |= (InState.IsShiftDown() ? 1 << 0 : 0);
	Flags |= (InState.IsAltDown() ? 1 << 1 : 0);
	Flags |= (InState.IsControlDown() ? 1 << 2 : 0);
	Flags |= (InState.IsCommandDown() ? 1 << 3 : 0);
	Flags |= (InState.AreCapsLocked() ? 1 << 4 : 0);

	return ::GetTypeHash(Flags);
}
