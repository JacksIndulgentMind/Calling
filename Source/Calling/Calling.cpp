// Copyright Epic Games, Inc. All Rights Reserved.

#include "Calling.h"
#include "Core/CLLog.h"
#include "Game/CLErrorBoundary.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/CoreDelegates.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"

class FCallingModule : public FDefaultGameModuleImpl
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};

namespace
{
	FDelegateHandle SystemErrorHandle;

	void WriteCrashIncident()
	{
		UCLErrorBoundary::WriteSystemIncident();
	}
}

void FCallingModule::StartupModule()
{
	FDefaultGameModuleImpl::StartupModule();
	if (GConfig)
	{
		const FString Path = FPaths::Combine(FPaths::ProjectConfigDir(), TEXT("DefaultCalling.ini"));
		if (FPaths::FileExists(Path))
		{
			if (FConfigFile* Game = GConfig->FindConfigFile(GGameIni))
			{
				Game->Combine(Path);
			}
		}
	}
	SystemErrorHandle = FCoreDelegates::OnHandleSystemError.AddStatic(&WriteCrashIncident);
}

void FCallingModule::ShutdownModule()
{
	FCoreDelegates::OnHandleSystemError.Remove(SystemErrorHandle);
	SystemErrorHandle.Reset();
	FDefaultGameModuleImpl::ShutdownModule();
}

IMPLEMENT_PRIMARY_GAME_MODULE(FCallingModule, Calling, "Calling");
