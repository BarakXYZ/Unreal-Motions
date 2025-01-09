#pragma once

#include "Framework/Application/NavigationConfig.h"
#include "UMLogger.h"

class FUMNavigationConfig : public FNavigationConfig
{
public:
	FUMNavigationConfig()
	{
		// Enable key navigation
		bKeyNavigation = true;
		bTabNavigation = true;
		bAnalogNavigation = true;

		RemoveKeyMappings();
		AddCustomKeyMappings();
	}

	/** Removes any existing mappings for H, J, K, L keys */
	void RemoveKeyMappings()
	{
		KeyEventRules.Remove(EKeys::H);
		KeyEventRules.Remove(EKeys::J);
		KeyEventRules.Remove(EKeys::K);
		KeyEventRules.Remove(EKeys::L);

		KeyEventRules.Remove(EKeys::N);
		KeyEventRules.Remove(EKeys::P);
	}

	/** Adds custom mappings for HJKL keys */
	void AddCustomKeyMappings()
	{
		KeyEventRules.Add(EKeys::H, EUINavigation::Left);
		KeyEventRules.Add(EKeys::J, EUINavigation::Down);
		KeyEventRules.Add(EKeys::K, EUINavigation::Up);
		KeyEventRules.Add(EKeys::L, EUINavigation::Right);

		KeyEventRules.Add(EKeys::N, EUINavigation::Next);
		KeyEventRules.Add(EKeys::P, EUINavigation::Previous);
	}

	virtual EUINavigation GetNavigationDirectionFromKey(const FKeyEvent& InKeyEvent) const override
	{
		FKey				 Key = InKeyEvent.GetKey();
		const EUINavigation* NavigationDirection = KeyEventRules.Find(Key);

		// Use the helper function to generate the log message
		FString LogMessage = GenerateLogMessage(Key, NavigationDirection);

		// Log the message
		if (NavigationDirection)
		{
			UE_LOG(LogTemp, Log, TEXT("%s"), *LogMessage);
			FUMLogger::NotifySuccess(FText::FromString(LogMessage));
			return *NavigationDirection;
		}

		UE_LOG(LogTemp, Warning, TEXT("%s"), *LogMessage);
		FUMLogger::NotifySuccess(FText::FromString(LogMessage));
		return EUINavigation::Invalid;
	}

	FString GenerateLogMessage(const FKey& Key, const EUINavigation* NavigationDirection) const
	{
		if (NavigationDirection)
		{
			return FString::Printf(TEXT("Key Pressed: %s -> Action: %s"),
				*Key.GetDisplayName().ToString(),
				*StaticEnum<EUINavigation>()->GetValueAsString(*NavigationDirection));
		}
		return FString::Printf(TEXT("Key Pressed: %s has no mapped action!"), *Key.GetDisplayName().ToString());
	}
};
