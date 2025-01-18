#pragma once

#include "CoreMinimal.h"
#include "Misc/Attribute.h"

UENUM(BlueprintType)
enum class EUMLogMethod : uint8
{
	PrintToScreen UMETA(DisplayName = "Print to Screen"),
	PrintToLog	  UMETA(DisplayName = "Print to Console"),
	Bypass		  UMETA(DisplayName = "Bypass")
};

enum class EUMLogSymbol : uint8
{
	Success,
	Error,
	Warning,
	Info,
	Help
};

class FUMLogger
{
public:
	FUMLogger();
	FUMLogger(FLogCategoryBase* InLogCategory, bool InbShouldLog = true);
	~FUMLogger();

	void Print(
		const FString&			  Message,
		const ELogVerbosity::Type Verbosity = ELogVerbosity::Display,
		bool					  bPushNotify = false,
		float					  Duration = 6.0f,
		FLinearColor			  Color = FLinearColor::Transparent,
		float					  Size = 1.5f,
		const int32				  Id = -1);

	FColor GetVerbosityColor(ELogVerbosity::Type Verbosity);

	void ConstructDivider();

	void SetLogCategory(FLogCategoryBase* InLogCategory);

	void ToggleLogging(bool InbShouldLog);

	void ToggleNotifications(
		bool InbNotifyAll,
		bool InbNotifyErrors = false,
		bool InbNotifyWarnings = false);

	void PrintToConsole(
		const FString& Message, const ELogVerbosity::Type Verbosity);

	void PrintToScreen(
		const FString&			  Message,
		const ELogVerbosity::Type Verbosity,
		float					  Duration,
		FLinearColor			  Color,
		float					  Size,
		const int				  Id);

	/** Displays a success notification with optional hyperlink */
	static void NotifySuccess(
		const FText&   NotificationText = FText::GetEmpty(),
		const bool	   bShouldLog = true,
		const float	   Lifetime = 3.0f,
		const FString& HyperlinkURL = FString(),
		const FText&   HyperlinkText = FText::GetEmpty());

	static void Notify(
		const FString&			  NotificationText = "",
		const ELogVerbosity::Type Verbosity = ELogVerbosity::Display,
		float					  Lifetime = 3.0f,
		const FString&			  HyperlinkURL = FString(),
		const FText&			  HyperlinkText = FText::GetEmpty());

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
	// UFUNCTION(BlueprintCallable, meta = (DevelopmentOnly), Category = "Utilities XYZ - Debugging")
	static void AddDebugMessage(
		const FString			  Message = "Hello <3",
		EUMLogMethod			  LogMethod = EUMLogMethod::PrintToScreen,
		FLogCategoryBase&		  LogCategory = LogTemp, // Got to be passed by ref
		const ELogVerbosity::Type Verbosity = ELogVerbosity::Display,
		float					  Duration = 6.0f,
		FLinearColor			  Color = FLinearColor::Yellow,
		float					  Size = 1.0f,
		const int32				  Id = -1);

	UFUNCTION(BlueprintCallable, Category = "Utilities XYZ - Debugging")
	static void RemoveStringFromScreenById(
		int Id, bool& bOutSuccess, FString& OutInfoMessage);

	UFUNCTION(BlueprintCallable, Category = "Utilities XYZ - Debugging")
	static void RemoveAllStringsFromScreen();

	static void SetPluginConfigFile();

	static void ToggleGlobalLogging();

	// Variables
public:
	FLogCategoryBase* LogCategory;
	FName			  LogCatName;
	FString			  LogCatStr;
	int32			  LogCatNameLen;
	FString			  CategoryDivider;
	bool			  bShouldLog{ true };
	static bool		  bShouldLogGlobal;
	bool			  bNotifyAll{ false };
	bool			  bNotifyErrors{ false };
	bool			  bNotifyWarnings{ false };

	/* Track to know when to visually separate between different categories */
	static FName LastCategoryNameOutput;
};
