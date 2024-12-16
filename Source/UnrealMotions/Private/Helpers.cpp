#include "Helpers.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Kismet/GameplayStatics.h"
#include "Widgets/Notifications/SNotificationList.h"

const FString FHelpers::VimSection = TEXT("/Script/Vim");
const FString FHelpers::DebugSection = TEXT("/Script/Debug");
FConfigFile	  FHelpers::ConfigFile;

void FHelpers::NotifySuccess(
	const FText&   NotificationText,
	const bool	   Log,
	const FString& HyperlinkURL,
	const FText&   HyperlinkText,
	const float	   Lifetime)
{
	if (Log)
	{
		FNotificationInfo Info(NotificationText);

		Info.ExpireDuration = Lifetime;
		Info.Image = FCoreStyle::Get().GetBrush("Icons.SuccessWithColor");

		if (!HyperlinkURL.IsEmpty())
		{
			Info.HyperlinkText = HyperlinkText;
			Info.Hyperlink = FSimpleDelegate::CreateLambda(
				[HyperlinkURL]() { FPlatformProcess::LaunchURL(*HyperlinkURL, nullptr, nullptr); });
		}

		FSlateNotificationManager::Get().AddNotification(Info);
	}
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
void FHelpers::AddDebugMessage(
	const FString Message,
	ELogMethod	  LogMethod,
	float		  Duration,
	FLinearColor  Color,
	float		  Size,
	const int	  Id)
{

	// if (!LogCategory)
	// {
	// 	LogCategory = &LogTemp; // Default to LogTemp if null
	// }

	switch (LogMethod)
	{
		case ELogMethod::PrintToScreen:
		{
			if (GEngine)
			{
				GEngine->AddOnScreenDebugMessage(Id, Duration, Color.ToFColor(true) /* Marks the color as RGB true */,
					*Message, true, FVector2D(Size));
			}
			else
			{
				UE_LOG(LogTemp, Error, TEXT("Failed to access GEngine."));
			}
			break;
		}
		case ELogMethod::PrintToLog:
		{
			UE_LOG(LogTemp, Log, TEXT("%s"), *Message);
			break;
		}
		case ELogMethod::Bypass:
		{
			break;
		}
		default:
			UE_LOG(LogTemp, Error, TEXT("Invalid logging method!"));
	}
}

void FHelpers::RemoveStringFromScreenById(int Id, bool& bOutSuccess, FString& OutInfoMessage)
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

void FHelpers::RemoveAllStringsFromScreen()
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

void FHelpers::SetPluginConfigFile()
{
	FString PluginConfigDir = FPaths::ProjectPluginsDir() / TEXT("ErgonomicsXYZ") / TEXT("Config");
	FString ConfigPath = PluginConfigDir / TEXT("DefaultErgonomicsXYZ.ini");

	// Use platform-specific path separators
	FPaths::MakeStandardFilename(ConfigPath);

	ConfigFile.Read(ConfigPath);
	if (ConfigFile.IsEmpty())
		UE_LOG(LogTemp, Error, TEXT("Unreal Motions Config file is Empty!"));
}
