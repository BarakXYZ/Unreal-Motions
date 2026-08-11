# `OnActiveTabChanged` parameter-order inconsistency

## Summary

Unreal Engine's tab-management paths have historically disagreed about the parameter order used by `FOnActiveTabChanged`:

- `FTabManager::FPrivateApi::OnTabForegrounded` treats the newly foregrounded tab as the first argument and the previous tab as the second.
- `FGlobalTabmanager::SetActiveTab` has emitted the previous tab first and the new tab second.
- The delegate declaration documents the new tab first and the previous tab second.

Code that consumes `FOnActiveTabChanged` should therefore verify which Engine path produced the event instead of assuming that the parameter names always reflect their runtime meaning.

## Preferred Engine-side correction

`FGlobalTabmanager::SetActiveTab` should emit the newly active tab first and the previously active tab second so its behavior matches the delegate declaration and the foregrounding path.

This note intentionally describes the observed behavior without reproducing Unreal Engine source code. Consult the corresponding `TabManager.cpp` and `TabManager.h` files in your separately licensed Unreal Engine source checkout for the exact implementation in the Engine version you use.
