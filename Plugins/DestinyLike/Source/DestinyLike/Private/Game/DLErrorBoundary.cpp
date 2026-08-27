#include "Game/DLErrorBoundary.h"
#include "Core/DLLog.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformStackWalk.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"

void UDLErrorBoundary::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	ReloadSettings();
	PurgeOldIncidents();
}

void UDLErrorBoundary::ReloadSettings()
{
	GConfig->GetString(TEXT("/Script/DestinyLike.DLErrorSettings"), TEXT("SupportContact"), SupportContact, GGameIni);
	GConfig->GetInt(TEXT("/Script/DestinyLike.DLErrorSettings"), TEXT("ErrorLogRetentionDays"), ErrorLogRetentionDays, GGameIni);
	ErrorLogRetentionDays = FMath::Clamp(ErrorLogRetentionDays, 1, 365);
}

FString UDLErrorBoundary::ErrorsDir() const
{
	return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("DestinyLike/Errors"));
}

void UDLErrorBoundary::PurgeOldIncidents() const
{
	IFileManager& Files = IFileManager::Get();
	const FString Dir = ErrorsDir();
	Files.MakeDirectory(*Dir, true);
	TArray<FString> Found;
	Files.FindFiles(Found, *FPaths::Combine(Dir, TEXT("*.log")), true, false);
	const FDateTime Cutoff = FDateTime::UtcNow() - FTimespan::FromDays(ErrorLogRetentionDays);
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

FString UDLErrorBoundary::WriteIncident(const FDLError& Error) const
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
	const FString Path = FPaths::Combine(ErrorsDir(), Id + TEXT(".log"));
	FFileHelper::SaveStringToFile(Body, *Path);
	return Id;
}

FDLError UDLErrorBoundary::Report(FDLError Error)
{
	if (Error.IsOk())
	{
		return Error;
	}

	if (Error.Kind == EDLErrorKind::User)
	{
		UE_LOG(LogDestinyLike, Warning, TEXT("User error %s: %s"), *Error.Code, *Error.Message);
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
		UE_LOG(LogDestinyLike, Error, TEXT("%s [%s] incident=%s"), *Error.Code, *Error.Message, *Error.IncidentId);
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 12.f, FColor::Red, Shown);
		}
		Error.Message = Shown;
	}

	OnReportedError.Broadcast(Error);
	return Error;
}

FDLError UDLErrorBoundary::ReportStatic(UObject* WorldContext, FDLError Error)
{
	if (Error.IsOk())
	{
		return Error;
	}
	UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
	if (UDLErrorBoundary* Boundary = GI ? GI->GetSubsystem<UDLErrorBoundary>() : nullptr)
	{
		return Boundary->Report(Error);
	}
	UE_LOG(LogDestinyLike, Error, TEXT("No error boundary: %s %s"), *Error.Code, *Error.Message);
	return Error;
}
