#pragma once

#include "CoreMinimal.h"
#include "Misc/Attribute.h"

UENUM(BlueprintType)
enum class EUMHelpersLogMethod : uint8
{
	PrintToScreen UMETA(DisplayName = "Print to Screen"),
	PrintToLog	  UMETA(DisplayName = "Print to Console"),
	Bypass		  UMETA(DisplayName = "Bypass")
};

class FUMHelpers
{
public:
	/** Displays a success notification with optional hyperlink */
	static void NotifySuccess(
		const FText&   NotificationText = FText::GetEmpty(),
		const bool	   Log = true,
		const FString& HyperlinkURL = FString(),
		const FText&   HyperlinkText = FText::GetEmpty(),
		const float	   Lifetime = 3.0f);

	/**
	 * Print debug message to screen or console. You can assign a UMHelpersLogMethod in order to control debugging visibility and
	 * isolate debugging if needed.
	 * @note This is a convinient wrapper for the built-in AddOnScreenDebugMessage.
	 *
	 * @param Message		The debug message to print.
	 * @param UMHelpersLogMethod		An enum to control logging functionality. Can be then controlled to isolate or solo
	 * debugging for certain classes.
	 * @param Duration		The lifespan of the message (used only in Print to Screen debugging)
	 * @param Color			The color of the debugging message. There are also some static presets available (e.g. Lime)
	 * (used only in Print to Screen debugging)
	 * @param Size			The size of the debugging message on screen. (used only in Print to Screen debugging)
	 * @param Id			The ID of the debugging message. Can be used to then update the values of the debugging
	 * message, remove it, etc. (used only in Print to Screen debugging)
	 */
	UFUNCTION(BlueprintCallable, meta = (DevelopmentOnly), Category = "Utilities XYZ - Debugging")
	static void AddDebugMessage(
		const FString		Message = "Hello <3",
		EUMHelpersLogMethod UMHelpersLogMethod = EUMHelpersLogMethod::PrintToScreen,
		float				Duration = 6.0f,
		FLinearColor		Color = FLinearColor::Yellow,
		float				Size = 1.0f,
		const int32			Id = -1);

	UFUNCTION(BlueprintCallable, Category = "Utilities XYZ - Debugging")
	static void RemoveStringFromScreenById(
		int Id, bool& bOutSuccess, FString& OutInfoMessage);

	UFUNCTION(BlueprintCallable, Category = "Utilities XYZ - Debugging")
	static void RemoveAllStringsFromScreen();

	static void SetPluginConfigFile();

	// Variables
public:
	static constexpr FLinearColor Lime{ 0.5f, 1.0f, 0.0f };
	static FConfigFile			  ConfigFile;
	static const FString		  VimSection;
	static const FString		  DebugSection;
};
