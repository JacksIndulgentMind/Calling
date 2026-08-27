#include "DestinyLikeModule.h"
#include "Core/DLLog.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "HAL/PlatformStackWalk.h"
#include "HAL/FileManager.h"
#include "Misc/CoreDelegates.h"

#define LOCTEXT_NAMESPACE "FDestinyLikeModule"

namespace
{
	FDelegateHandle SystemErrorHandle;

	void WriteCrashIncident()
	{
		const FString Dir = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("DestinyLike/Errors"));
		IFileManager::Get().MakeDirectory(*Dir, true);
		const FString Id = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens);
		ANSICHAR Stack[4096] = { 0 };
		FPlatformStackWalk::StackWalkAndDump(Stack, UE_ARRAY_COUNT(Stack), 0);
		FString Body = FString::Printf(TEXT("incident=%s\nkind=system\ntime=%s\n\n"),
			*Id, *FDateTime::UtcNow().ToIso8601());
		Body += UTF8_TO_TCHAR(Stack);
		FFileHelper::SaveStringToFile(Body, *FPaths::Combine(Dir, Id + TEXT("-crash.log")));
		UE_LOG(LogDestinyLike, Error, TEXT("System error incident=%s"), *Id);
	}
}

void FDestinyLikeModule::StartupModule()
{
	if (GConfig)
	{
		if (const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("DestinyLike")))
		{
			const FString Path = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Config/DefaultDestinyLike.ini"));
			if (FPaths::FileExists(Path))
			{
				if (FConfigFile* Game = GConfig->FindConfigFile(GGameIni))
				{
					Game->Combine(Path);
				}
			}
		}
	}
	SystemErrorHandle = FCoreDelegates::OnHandleSystemError.AddStatic(&WriteCrashIncident);
}

void FDestinyLikeModule::ShutdownModule()
{
	FCoreDelegates::OnHandleSystemError.Remove(SystemErrorHandle);
	SystemErrorHandle.Reset();
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FDestinyLikeModule, DestinyLike)
