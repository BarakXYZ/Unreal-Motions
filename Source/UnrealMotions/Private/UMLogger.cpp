#include "UMLogger.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Kismet/GameplayStatics.h"
#include "Logging/LogVerbosity.h"
#include "Widgets/Notifications/SNotificationList.h"

FName FUMLogger::LastCategoryNameOutput = "NONE";
bool  FUMLogger::bShouldLogGlobal{ false };

FUMLogger::FUMLogger()
{
}

FUMLogger::FUMLogger(FLogCategoryBase* InLogCategory, bool InbShouldLog)
	: LogCategory(InLogCategory), LogCatName(LogCategory->GetCategoryName()), LogCatStr(LogCatName.ToString()), bShouldLog(InbShouldLog)
{
	LogCatNameLen = LogCatStr.Len(); // Initialize LogCatNameLen after LogCatStr is constructed
	ConstructDivider();
}

FUMLogger::~FUMLogger()
{
}

void FUMLogger::SetLogCategory(FLogCategoryBase* InLogCategory)
{
	LogCategory = InLogCategory;
	LogCatName = LogCategory->GetCategoryName();
	LogCatStr = LogCatName.ToString();
	LogCatNameLen = LogCatStr.Len();

	ConstructDivider();
}

void FUMLogger::ConstructDivider()
{
	const int	TotalWidth = 48;
	const int32 Padding = (TotalWidth - LogCatNameLen) / 2;

	// NOTE: We need to throw in an extra compensation astrisk because Capital
	// letters seems to be bigger than the - symbol, thus giving assymetrical
	// results. It's quite random.
	CategoryDivider =
		"\n"
		+ FString::ChrN(TotalWidth, '-') // First line
		+ "\n"
		+ FString::ChrN(Padding / 2, '*') + '*'
		+ FString::ChrN(Padding / 2, ' ')
		+ LogCatStr
		+ FString::ChrN(Padding / 2, ' ')
		+ FString::ChrN(Padding / 2, '*') + '*'
		+ "\n"
		+ FString::ChrN(TotalWidth, '-') // lastLine
		+ "\n";
}

void FUMLogger::ToggleLogging(bool InbShouldLog)
{
	bShouldLog = InbShouldLog;
}

void FUMLogger::ToggleNotifications(
	bool InbNotifyAll,
	bool InbNotifyErrors,
	bool InbNotifyWarnings)
{
	bNotifyAll = InbNotifyAll;
	bNotifyErrors = InbNotifyErrors;
	bNotifyWarnings = InbNotifyWarnings;
}

void FUMLogger::Print(
	const FString& Message,
	bool		   bPushNotify)
{
	Print(Message, ELogVerbosity::Log, bPushNotify);
}

void FUMLogger::Print(
	const FString&			  Message,
	const ELogVerbosity::Type Verbosity,
	bool					  bPushNotify,
	float					  Duration,
	FLinearColor			  Color,
	float					  Size,
	const int				  Id)
{
	if (!bShouldLogGlobal
		|| !bShouldLog
		|| LogCategory->GetVerbosity() == ELogVerbosity::NoLogging)
		return;

	PrintToConsole(Message, Verbosity);
	PrintToScreen(Message, Verbosity, 999.0f, Color, Size, Id);

	if (bPushNotify)
		Notify(Message, Verbosity, Duration);
	else
		switch (Verbosity)
		{
			case ELogVerbosity::Fatal:
			case ELogVerbosity::Error:
				if (bNotifyErrors || bNotifyAll)
					Notify(Message, Verbosity, Duration);
				break;
			case ELogVerbosity::Warning:
				if (bNotifyWarnings || bNotifyAll)
					Notify(Message, Verbosity, Duration);
				break;
			default:
				if (bNotifyAll)
					Notify(Message, Verbosity, Duration);
				break;
		}
}

void FUMLogger::PrintToConsole(
	const FString& Message, const ELogVerbosity::Type Verbosity)
{
	FMsg::Logf(
		__FILE__,
		__LINE__,
		LogCatName,
		Verbosity,
		TEXT("%s"),
		*Message); // Pure message, no need for the formatted one.
}

void FUMLogger::PrintToScreen(
	const FString&			  Message,
	const ELogVerbosity::Type Verbosity,
	float					  Duration,
	FLinearColor			  Color,
	float					  Size,
	const int				  Id)
{
	if (GEngine)
	{
		// Divide the message from the last output for clearer logging
		FString OutMsg = LastCategoryNameOutput.IsEqual(LogCatName)
			? Message
			: Message + CategoryDivider;

		// if default value, we'll deduce the color from the verbosity.
		// else retain the custom color set by the user.
		if (Color == FLinearColor::Transparent)
			Color = GetVerbosityColor(Verbosity);

		GEngine->AddOnScreenDebugMessage(
			Id, Duration, Color.ToFColor(true) /* Marks the color as RGB true */,
			*OutMsg, true, FVector2D(Size));

		LastCategoryNameOutput = LogCatName; // Track last outputted category
	}
}

FColor FUMLogger::GetVerbosityColor(ELogVerbosity::Type Verbosity)
{
	switch (Verbosity)
	{
		case ELogVerbosity::Fatal:
			return FColor::Red;

		case ELogVerbosity::Error:
			return FColor::Orange;

		case ELogVerbosity::Warning:
			return FColor::Yellow;

		case ELogVerbosity::Display:
		case ELogVerbosity::Log:
			return FColor::White; // Standard messages

		case ELogVerbosity::Verbose:
		case ELogVerbosity::VeryVerbose:
			return FColor::Green; // Verbose logs -> informational -> green

		case ELogVerbosity::NoLogging:
		default:
			return FColor::Black; // Black for no logging or unknown
	}
}

void FUMLogger::Notify(
	const FString&			  NotificationText,
	const ELogVerbosity::Type Verbosity,
	float					  Lifetime,
	const FString&			  HyperlinkURL,
	const FText&			  HyperlinkText)
{
	static const TMap<ELogVerbosity::Type, FName> BrushByLogSymbol = {
		{ ELogVerbosity::Verbose, "Icons.SuccessWithColor" },
		{ ELogVerbosity::Error, "MessageLog.Error" },
		{ ELogVerbosity::Warning, "MessageLog.Warning" },
		{ ELogVerbosity::Display, "Icons.Info" }
	};

	FNotificationInfo Info(FText::FromString(NotificationText));

	Info.ExpireDuration = Lifetime;
	if (const FName* Brush = BrushByLogSymbol.Find(Verbosity))
		Info.Image = FCoreStyle::Get().GetBrush(*Brush);
	else
		Info.Image = FCoreStyle::Get().GetBrush("Icons.Info");

	if (!HyperlinkURL.IsEmpty())
	{
		Info.HyperlinkText = HyperlinkText;
		Info.Hyperlink = FSimpleDelegate::CreateLambda(
			[HyperlinkURL]() { FPlatformProcess::LaunchURL(*HyperlinkURL, nullptr, nullptr); });
	}

	FSlateNotificationManager::Get().AddNotification(Info);
}

/*
 * AddOnScreenDebugMessage Built-In Function & Custom Explaination.

 1. Key (Id): Can be anything really. If -1 is provided, UE will assign a unique ID to the message and still print
 it on the screen. If the same unique ID is provided twice, UE will update the value of the specified string ID
 provided.

 2. TimeToDisplay (Duration): How long the message will be shown on screen. Can be terminated earlier if the same
 string ID is updated with a shorter lifespan, or completely removed.

 3. DisplayColor (Color): The color of the message. Provided to the user by a FLinearColor since it is easier to
 adjust. The color is then casted to FColor, which is what the built-in function expects.

 4. DebugMessage (Message): i.e. printed to the screen.

 5. bNewerOnTop (set to true): Will the new message will show on top of all previous messages. The built-in function
 will only use this boolean if the ID of the message is -1. Otherwise, the message will be updated and retain its
 current position.

 6. TextScale (Size): The font size of the displayed message. Provided to the user as a float, since it doesn't
 really make sense to scale assymetrically.
*/
void FUMLogger::AddDebugMessage(
	const FString			  Message,
	EUMLogMethod			  UMLogHelpersLogMethod,
	FLogCategoryBase&		  LogCategory,
	const ELogVerbosity::Type Verbosity,
	float					  Duration,
	FLinearColor			  Color,
	float					  Size,
	const int				  Id)
{
	switch (UMLogHelpersLogMethod)
	{
		case EUMLogMethod::PrintToScreen:
		{
			if (GEngine)
			{
				GEngine->AddOnScreenDebugMessage(Id, Duration, Color.ToFColor(true) /* Marks the color as RGB true */,
					*Message, true, FVector2D(Size));
			}
			else
			{
				UE_LOG(LogTemp, Error, TEXT("LogHelpers: Cannot access GEngine"));
			}
			break;
		}
		case EUMLogMethod::PrintToLog:
		{
			FMsg::Logf(
				__FILE__,
				__LINE__,
				LogCategory.GetCategoryName(), // returns FName
				Verbosity,
				TEXT("%s"),
				*Message);
			break;
		}
		case EUMLogMethod::Bypass:
		{
			break;
		}
		default:
			UE_LOG(LogTemp, Error, TEXT("Invalid logging method!"));
	}
}

void FUMLogger::RemoveStringFromScreenById(int Id, bool& bOutSuccess, FString& OutInfoMessage)
{
	if (GEngine)
	{
		if (GEngine->OnScreenDebugMessageExists(Id))
		{
			GEngine->RemoveOnScreenDebugMessage(Id);
			bOutSuccess = true;
			OutInfoMessage = TEXT("Debug message successfully removed.");
		}
		else
		{
			bOutSuccess = false;
			OutInfoMessage = TEXT("Could not find Debug Message ID. Nothing to remove.");
		}
	}
	else
	{
		bOutSuccess = false;
		OutInfoMessage = TEXT("Failed to access GEngine.");
	}
}

void FUMLogger::RemoveAllStringsFromScreen()
{
	if (GEngine)
	{
		GEngine->ClearOnScreenDebugMessages();
		UE_LOG(LogTemp, Log, TEXT("All On Screen Debug Messages we cleared."));
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to access GEngine."));
	}
}

void FUMLogger::ToggleGlobalLogging()
{
	bShouldLogGlobal = !bShouldLogGlobal;
}
