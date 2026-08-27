#include "Game/CLErrorBoundary.h"
#include "Core/CLLog.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformStackWalk.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"

void UCLErrorBoundary::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	ReloadSettings();
	PurgeOldIncidents();
}

void UCLErrorBoundary::ReloadSettings()
{
	GConfig->GetString(TEXT("/Script/Calling.CLErrorSettings"), TEXT("SupportContact"), SupportContact, GGameIni);
	GConfig->GetInt(TEXT("/Script/Calling.CLErrorSettings"), TEXT("ErrorLogRetentionDays"), ErrorLogRetentionDays, GGameIni);
	ErrorLogRetentionDays = FMath::Clamp(ErrorLogRetentionDays, 1, 365);
}

FString UCLErrorBoundary::ErrorsDirPath()
{
	return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Calling/Errors"));
}

int32 UCLErrorBoundary::RetentionDaysFromIni()
{
	int32 Days = 14;
	if (GConfig)
	{
		GConfig->GetInt(TEXT("/Script/Calling.CLErrorSettings"), TEXT("ErrorLogRetentionDays"), Days, GGameIni);
	}
	return FMath::Clamp(Days, 1, 365);
}

FString UCLErrorBoundary::ErrorsDir() const
{
	return ErrorsDirPath();
}

void UCLErrorBoundary::PurgeOldIncidentFiles()
{
	IFileManager& Files = IFileManager::Get();
	const FString Dir = ErrorsDirPath();
	Files.MakeDirectory(*Dir, true);
	TArray<FString> Found;
	Files.FindFiles(Found, *FPaths::Combine(Dir, TEXT("*.log")), true, false);
	const FDateTime Cutoff = FDateTime::UtcNow() - FTimespan::FromDays(RetentionDaysFromIni());
	for (const FString& Name : Found)
	{
		const FString Path = FPaths::Combine(Dir, Name);
		const FDateTime Stamp = Files.GetTimeStamp(*Path);
		if (Stamp != FDateTime::MinValue() && Stamp < Cutoff)
		{
			Files.Delete(*Path);
		}
	}
}

void UCLErrorBoundary::PurgeOldIncidents() const
{
	PurgeOldIncidentFiles();
}

FString UCLErrorBoundary::WriteIncidentFile(const FCLError& Error, const TCHAR* FileSuffix)
{
	const FString Id = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens);
	ANSICHAR Stack[4096] = { 0 };
	FPlatformStackWalk::StackWalkAndDump(Stack, UE_ARRAY_COUNT(Stack), 0);
	FString Body;
	Body += FString::Printf(TEXT("incident=%s\nkind=%d\ncode=%s\nmessage=%s\ntime=%s\n\n"),
		*Id,
		static_cast<int32>(Error.Kind),
		*Error.Code,
		*Error.Message,
		*FDateTime::UtcNow().ToIso8601());
	Body += UTF8_TO_TCHAR(Stack);
	const FString Path = FPaths::Combine(ErrorsDirPath(), Id + FileSuffix + TEXT(".log"));
	IFileManager::Get().MakeDirectory(*ErrorsDirPath(), true);
	FFileHelper::SaveStringToFile(Body, *Path);
	return Id;
}

FString UCLErrorBoundary::WriteIncident(const FCLError& Error) const
{
	return WriteIncidentFile(Error, TEXT(""));
}

FString UCLErrorBoundary::WriteSystemIncident()
{
	PurgeOldIncidentFiles();
	const FCLError Error = FCLError::Make(
		ECLErrorKind::Logic,
		TEXT("system_error"),
		TEXT("Unhandled system error"));
	const FString Id = WriteIncidentFile(Error, TEXT("-crash"));
	UE_LOG(LogCalling, Error, TEXT("System error incident=%s"), *Id);
	return Id;
}

FCLError UCLErrorBoundary::Report(FCLError Error)
{
	if (Error.IsOk())
	{
		return Error;
	}

	if (Error.Kind == ECLErrorKind::User)
	{
		UE_LOG(LogCalling, Warning, TEXT("User error %s: %s"), *Error.Code, *Error.Message);
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 8.f, FColor::Yellow, Error.Message);
		}
	}
	else
	{
		Error.IncidentId = WriteIncident(Error);
		const FString Shown = FString::Printf(
			TEXT("%s (incident %s). %s"),
			*Error.Message,
			*Error.IncidentId,
			*SupportContact);
		UE_LOG(LogCalling, Error, TEXT("%s [%s] incident=%s"), *Error.Code, *Error.Message, *Error.IncidentId);
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 12.f, FColor::Red, Shown);
		}
		Error.Message = Shown;
	}

	OnReportedError.Broadcast(Error);
	return Error;
}

FCLError UCLErrorBoundary::ReportStatic(UObject* WorldContext, FCLError Error)
{
	if (Error.IsOk())
	{
		return Error;
	}
	UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
	if (UCLErrorBoundary* Boundary = GI ? GI->GetSubsystem<UCLErrorBoundary>() : nullptr)
	{
		return Boundary->Report(Error);
	}
	UE_LOG(LogCalling, Error, TEXT("No error boundary: %s %s"), *Error.Code, *Error.Message);
	return Error;
}
