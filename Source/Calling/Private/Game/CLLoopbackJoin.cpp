#include "Game/CLLoopbackJoin.h"
#include "Core/CLLog.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformProcess.h"
#include "HAL/FileManager.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Engine/NetDriver.h"
#include "IPAddress.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace CLLoopbackJoin
{
	bool ShowUi()
	{
#if UE_BUILD_SHIPPING
		return false;
#else
		bool bShow = true;
		GConfig->GetBool(TEXT("/Script/Calling.CLSessionSettings"), TEXT("bShowLoopbackJoin"), bShow, GGameIni);
		return bShow;
#endif
	}

	FString DefaultConnect()
	{
		FString Connect = TEXT("127.0.0.1:7777");
		GConfig->GetString(TEXT("/Script/Calling.CLSessionSettings"), TEXT("LoopbackConnect"), Connect, GGameIni);
		return Connect;
	}

	FString BeaconDir()
	{
		return FPaths::Combine(FPaths::ProjectDir(), TEXT("Saved/Calling"));
	}

	FString BeaconPath()
	{
		return FPaths::Combine(BeaconDir(), TEXT("loopback-host.json"));
	}

	FString LogPath()
	{
		return FPaths::Combine(BeaconDir(), TEXT("loopback-ipc.log"));
	}

	int32 ListenPort(UWorld* World)
	{
		if (World && World->NetDriver && World->NetDriver->GetLocalAddr().IsValid())
		{
			return World->NetDriver->GetLocalAddr()->GetPort();
		}
		if (World && World->URL.Port > 0)
		{
			return World->URL.Port;
		}
		return 7777;
	}

	int32 ResolvePort(int32 DefaultPort, const TCHAR* IniSection, const TCHAR* IniKey, const TCHAR* CmdToken)
	{
		int32 Port = DefaultPort;
		GConfig->GetInt(IniSection, IniKey, Port, GGameIni);
		FParse::Value(FCommandLine::Get(), CmdToken, Port);
		return FMath::Clamp(Port, 1024, 65535);
	}

	int32 AgentHttpPort()
	{
		return ResolvePort(18765, TEXT("/Script/Calling.CLAgentSettings"), TEXT("AgentHttpPort"), TEXT("CallingAgentHttpPort="));
	}

	int32 SessionHubPort()
	{
		return ResolvePort(18766, TEXT("/Script/Calling.CLLobbySettings"), TEXT("SessionHubPort"), TEXT("CallingSessionHubPort="));
	}

	void AppendLog(const FString& Line)
	{
		IFileManager::Get().MakeDirectory(*BeaconDir(), true);
		const FString Row = FString::Printf(TEXT("%s pid=%d %s\n"),
			*FDateTime::UtcNow().ToIso8601(), FPlatformProcess::GetCurrentProcessId(), *Line);
		FFileHelper::SaveStringToFile(Row, *LogPath(), FFileHelper::EEncodingOptions::AutoDetect,
			&IFileManager::Get(), FILEWRITE_Append);
		UE_LOG(LogCalling, Display, TEXT("Calling loopback: %s"), *Line);
	}

	void WriteBeacon(UWorld* World)
	{
		IFileManager::Get().MakeDirectory(*BeaconDir(), true);
		const int32 Port = ListenPort(World);
		const FString Connect = FString::Printf(TEXT("127.0.0.1:%d"), Port);
		TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
		Root->SetStringField(TEXT("connect"), Connect);
		Root->SetNumberField(TEXT("port"), Port);
		Root->SetNumberField(TEXT("pid"), static_cast<double>(FPlatformProcess::GetCurrentProcessId()));
		Root->SetStringField(TEXT("time"), FDateTime::UtcNow().ToIso8601());
		if (World)
		{
			Root->SetStringField(TEXT("map"), World->GetMapName());
			Root->SetStringField(TEXT("net"), World->GetNetMode() == NM_ListenServer
				? TEXT("listen") : World->GetNetMode() == NM_DedicatedServer ? TEXT("dedicated") : TEXT("other"));
		}
		FString Json;
		const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Json);
		FJsonSerializer::Serialize(Root, Writer);
		FFileHelper::SaveStringToFile(Json, *BeaconPath());
		AppendLog(FString::Printf(TEXT("beacon %s"), *Connect));
	}

	void ClearBeacon()
	{
		IFileManager::Get().Delete(*BeaconPath());
		AppendLog(TEXT("beacon cleared"));
	}

	FString ReadBeaconConnect()
	{
		FString Content;
		if (!FFileHelper::LoadFileToString(Content, *BeaconPath()))
		{
			return FString();
		}
		TSharedPtr<FJsonObject> Root;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Content);
		if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
		{
			return FString();
		}
		return Root->GetStringField(TEXT("connect"));
	}

	FString ResolveConnect(const FString& Selected)
	{
		const FString Trim = Selected.TrimStartAndEnd();
		if (Trim.IsEmpty() || Trim.StartsWith(TEXT("Loopback")))
		{
			return DefaultConnect();
		}
		if (Trim.StartsWith(TEXT("Beacon")))
		{
			const FString FromFile = ReadBeaconConnect();
			return FromFile.IsEmpty() ? DefaultConnect() : FromFile;
		}
		return Trim;
	}
}
