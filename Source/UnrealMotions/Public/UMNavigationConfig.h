#pragma once

#include "Framework/Application/NavigationConfig.h"
#include "UMHelpers.h"

class FUMNavigationConfig : public FNavigationConfig
{
public:
	FUMNavigationConfig()
	{
		// Enable key navigation
		bKeyNavigation = true;

		RemoveKeyMappings();
		AddCustomKeyMappings();
	}

	// Optional: Override to log or debug key mappings if needed
	// virtual EUINavigation GetNavigationDirectionFromKey(const FKeyEvent& InKeyEvent) const override
	// {
	// 	FKey Key = InKeyEvent.GetKey();
	// 	if (KeyEventRules.Contains(Key))
	// 	{
	// 		return KeyEventRules[Key];
	// 	}
	// 	return EUINavigation::Invalid;
	// }

	/** Removes any existing mappings for H, J, K, L keys */
	void RemoveKeyMappings()
	{
		KeyEventRules.Remove(EKeys::H);
		KeyEventRules.Remove(EKeys::J);
		KeyEventRules.Remove(EKeys::K);
		KeyEventRules.Remove(EKeys::L);
	}

	/** Adds custom mappings for HJKL keys */
	void AddCustomKeyMappings()
	{
		KeyEventRules.Add(EKeys::H, EUINavigation::Up);
		KeyEventRules.Add(EKeys::J, EUINavigation::Down);
		KeyEventRules.Add(EKeys::K, EUINavigation::Left);
		KeyEventRules.Add(EKeys::L, EUINavigation::Right);
	}

	virtual EUINavigation GetNavigationDirectionFromKey(const FKeyEvent& InKeyEvent) const override
	{
		FKey				 Key = InKeyEvent.GetKey();
		const EUINavigation* NavigationDirection = KeyEventRules.Find(Key);

		if (NavigationDirection)
		{
			// Log the key and its action to the console
			FString LogMessage = FString::Printf(TEXT("Key Pressed: %s -> Action: %s"),
				*Key.GetDisplayName().ToString(),
				*StaticEnum<EUINavigation>()->GetValueAsString(*NavigationDirection));
			UE_LOG(LogTemp, Log, TEXT("%s"), *LogMessage);
			FUMHelpers::NotifySuccess(FText::FromString(LogMessage));

			return *NavigationDirection;
		}

		// Log unmatched keys
		FString LogMessage = FString::Printf(TEXT("Key Pressed: %s has no mapped action!"), *Key.GetDisplayName().ToString());
		UE_LOG(LogTemp, Warning, TEXT("%s"), *LogMessage);
		FUMHelpers::NotifySuccess(FText::FromString(LogMessage));
		return EUINavigation::Invalid;
	}
};
