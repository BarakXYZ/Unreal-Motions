# Analysis of `OnActiveTabChanged` vs. `OnTabForegrounded`

## TL;DR:
There is an inconsistency in how parameters are passed to the `FOnActiveTabChanged` delegate in Unreal Engine's `TabManager` system. Specifically:
- **`OnTabForegrounded`** correctly follows the documented order of parameters.
- **`SetActiveTab`** reverses the parameter order, causing a conflict.

This inconsistency can lead to confusion and potential misuse. Below is a detailed breakdown.

---

### Code Samples

#### `TabManager.cpp` (Line 718)
```cpp
void FTabManager::FPrivateApi::OnTabForegrounded(const TSharedPtr<SDockTab>& NewForegroundTab, const TSharedPtr<SDockTab>& BackgroundedTab)
{
    // Notice we first pass in the NewTab, then the old one.
    TabManager.OnTabForegrounded(NewForegroundTab, BackgroundedTab);
}
```

#### `TabManager.cpp` (Line 2539)
```cpp
void FGlobalTabmanager::SetActiveTab(const TSharedPtr<class SDockTab>& NewActiveTab)
{
    const bool bShouldApplyChange = CanSetAsActiveTab(NewActiveTab);

    TSharedPtr<SDockTab> CurrentlyActiveTab = GetActiveTab();

    if (bShouldApplyChange && (CurrentlyActiveTab != NewActiveTab))
    {
        if (NewActiveTab.IsValid())
        {
            NewActiveTab->UpdateActivationTime();
        }

        // Yet here we pass firstly the current (old tab), then the NewActiveTab.
        OnActiveTabChanged.Broadcast(CurrentlyActiveTab, NewActiveTab);
        ActiveTabPtr = NewActiveTab;
    }
}
```

#### `TabManager.h` (Line 25)
```cpp
// Delegate declaration specifies that the active tab should be passed first,
// followed by the old tab.
DECLARE_MULTICAST_DELEGATE_TwoParams(
    FOnActiveTabChanged,
    /** Newly active tab */
    TSharedPtr<SDockTab>,
    /** Previously active tab */
    TSharedPtr<SDockTab>
);
```

---

### Analysis

#### `OnTabForegrounded`
- **Implementation:**
  - Parameters are passed in the documented order:
    - **First parameter:** Newly active tab (`NewForegroundTab`).
    - **Second parameter:** Previously active tab (`BackgroundedTab`).
- **Consistency:** Matches the specification in `FOnActiveTabChanged`.

#### `SetActiveTab`
- **Implementation:**
  - Parameters are passed in reverse order:
    - **First parameter:** Previously active tab (`CurrentlyActiveTab`).
    - **Second parameter:** Newly active tab (`NewActiveTab`).
- **Inconsistency:** This does not align with the delegate specification, leading to potential confusion.

#### Delegate Declaration
- As per the `DECLARE_MULTICAST_DELEGATE_TwoParams` documentation:
  - **First parameter:** Newly active tab.
  - **Second parameter:** Previously active tab.

---

### Implications

**Inconsistent Parameter Order:**
   - Any code relying on `FOnActiveTabChanged` might interpret parameters incorrectly, depending on whether the call originates from `OnTabForegrounded` or `SetActiveTab`.
   - Developers need to check the source function to understand the parameter order, which is error-prone.

---

### Proposed Fix

#### Update `SetActiveTab`
Modify the parameter order in `SetActiveTab` to align with the delegate declaration:
```cpp
OnActiveTabChanged.Broadcast(NewActiveTab, CurrentlyActiveTab);
```
---
