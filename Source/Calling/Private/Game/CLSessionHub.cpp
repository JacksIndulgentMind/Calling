#include "Game/CLSessionHub.h"
#include "Game/CLLobbySubsystem.h"
#include "Game/CLHubCommandRegistry.h"
#include "Game/CLAgentCodec.h"
#include "Game/CLAgentBridgeSubsystem.h"
#include "Game/CLParticipantSeat.h"
#include "Game/CLControllerPlaybook.h"
#include "Core/CLLog.h"
#include "Game/CLErrorBoundary.h"
#include "Core/CLError.h"
#include "Sockets.h"
#include "SocketSubsystem.h"
#include "IPAddress.h"
#include "Common/TcpSocketBuilder.h"
#include "Interfaces/IPv4/IPv4Address.h"
#include "Interfaces/IPv4/IPv4Endpoint.h"
#include "Misc/Base64.h"
#include "Misc/SecureHash.h"
#include "Misc/ConfigCacheIni.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Dom/JsonObject.h"

namespace
{
	constexpr int32 MaxWsBuffer = 64 * 1024;

	FString Utf8ToF(const uint8* Data, int32 Len)
	{
		const FUTF8ToTCHAR Conv(reinterpret_cast<const ANSICHAR*>(Data), Len);
		return FString(Conv.Length(), Conv.Get());
	}

	void AppendUtf8(TArray<uint8>& Out, const FString& Text)
	{
		const FTCHARToUTF8 Conv(*Text);
		Out.Append(reinterpret_cast<const uint8*>(Conv.Get()), Conv.Length());
	}
}

void UCLSessionHub::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	int32 PortIni = Port;
	GConfig->GetInt(TEXT("/Script/Calling.CLLobbySettings"), TEXT("SessionHubPort"), PortIni, GGameIni);
	Port = FMath::Clamp(PortIni, 1024, 65535);
}

void UCLSessionHub::Deinitialize()
{
	StopHost();
	Super::Deinitialize();
}

TStatId UCLSessionHub::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UCLSessionHub, STATGROUP_Tickables);
}

bool UCLSessionHub::IsTickable() const
{
	return !IsTemplate() && ListenSocket != nullptr;
}

void UCLSessionHub::StartHost()
{
#if UE_BUILD_SHIPPING
	return;
#else
	if (ListenSocket)
	{
		return;
	}
	bWantListen = true;
	FIPv4Endpoint Endpoint(FIPv4Address::Any, static_cast<uint16>(Port));
	ListenSocket = FTcpSocketBuilder(TEXT("CLSessionHub"))
		.AsReusable()
		.BoundToEndpoint(Endpoint)
		.Listening(8);
	if (!ListenSocket)
	{
		UCLErrorBoundary::ReportStatic(this, FCLError::Make(
			ECLErrorKind::NonDeterministic,
			TEXT("session_hub_bind"),
			FString::Printf(TEXT("WebSocket hub failed to bind 0.0.0.0:%d"), Port)));
		return;
	}
	int32 NewSize = 0;
	ListenSocket->SetNonBlocking(true);
	ListenSocket->SetReceiveBufferSize(MaxWsBuffer, NewSize);
	UE_LOG(LogCalling, Display, TEXT("Calling: session hub ws://0.0.0.0:%d"), Port);
#endif
}

void UCLSessionHub::StopHost()
{
	for (int32 i = Clients.Num() - 1; i >= 0; --i)
	{
		CloseClient(i);
	}
	Clients.Reset();
	if (ListenSocket)
	{
		ListenSocket->Close();
		ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(ListenSocket);
		ListenSocket = nullptr;
	}
	bWantListen = false;
}

void UCLSessionHub::Tick(float DeltaTime)
{
	(void)DeltaTime;
	if (!ListenSocket)
	{
		return;
	}
	AcceptClients();
	for (int32 i = Clients.Num() - 1; i >= 0; --i)
	{
		PumpClient(i);
	}
}

void UCLSessionHub::AcceptClients()
{
	if (!ListenSocket)
	{
		return;
	}
	bool bPending = false;
	if (!ListenSocket->HasPendingConnection(bPending) || !bPending)
	{
		return;
	}
	FSocket* Client = ListenSocket->Accept(TEXT("CLSessionHubClient"));
	if (!Client)
	{
		return;
	}
	Client->SetNonBlocking(true);
	FClient Row;
	Row.Socket = Client;
	Clients.Add(Row);
}

void UCLSessionHub::CloseClient(int32 Index)
{
	if (!Clients.IsValidIndex(Index))
	{
		return;
	}
	if (Clients[Index].Socket)
	{
		Clients[Index].Socket->Close();
		ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(Clients[Index].Socket);
	}
	Clients.RemoveAt(Index);
}

bool UCLSessionHub::ReadHttpHeader(const TArray<uint8>& Buffer, FString& OutKey)
{
	const FString Text = Utf8ToF(Buffer.GetData(), Buffer.Num());
	const int32 End = Text.Find(TEXT("\r\n\r\n"));
	if (End == INDEX_NONE)
	{
		return false;
	}
	OutKey.Reset();
	TArray<FString> Lines;
	Text.Left(End).ParseIntoArrayLines(Lines);
	for (const FString& Line : Lines)
	{
		if (Line.StartsWith(TEXT("Sec-WebSocket-Key:"), ESearchCase::IgnoreCase))
		{
			OutKey = Line.Mid(FCString::Strlen(TEXT("Sec-WebSocket-Key:"))).TrimStartAndEnd();
			return !OutKey.IsEmpty();
		}
	}
	return false;
}

FString UCLSessionHub::MakeAcceptKey(const FString& ClientKey)
{
	const FString Concat = ClientKey + TEXT("258EAFA5-E914-47DA-95CA-C5AB0DC85B11");
	const FTCHARToUTF8 Utf8(*Concat);
	uint8 Digest[20];
	FSHA1::HashBuffer(Utf8.Get(), Utf8.Length(), Digest);
	return FBase64::Encode(Digest, 20);
}

bool UCLSessionHub::TryUpgrade(int32 Index)
{
	FClient& Client = Clients[Index];
	FString Key;
	if (!ReadHttpHeader(Client.Buffer, Key))
	{
		return Client.Buffer.Num() < MaxWsBuffer;
	}
	const FString Accept = MakeAcceptKey(Key);
	const FString Response = FString::Printf(
		TEXT("HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: %s\r\n\r\n"),
		*Accept);
	TArray<uint8> Bytes;
	AppendUtf8(Bytes, Response);
	int32 Sent = 0;
	Client.Socket->Send(Bytes.GetData(), Bytes.Num(), Sent);
	Client.bUpgraded = true;
	Client.Buffer.Reset();
	return true;
}

bool UCLSessionHub::DecodeFrame(TArray<uint8>& Buffer, FString& OutText)
{
	OutText.Reset();
	if (Buffer.Num() < 2)
	{
		return false;
	}
	const uint8 B0 = Buffer[0];
	const uint8 B1 = Buffer[1];
	const uint8 Opcode = B0 & 0x0F;
	const bool bMasked = (B1 & 0x80) != 0;
	uint64 PayloadLen = B1 & 0x7F;
	int32 Offset = 2;
	if (PayloadLen == 126)
	{
		if (Buffer.Num() < 4)
		{
			return false;
		}
		PayloadLen = (static_cast<uint64>(Buffer[2]) << 8) | Buffer[3];
		Offset = 4;
	}
	else if (PayloadLen == 127)
	{
		Buffer.Reset();
		return false;
	}
	if (!bMasked)
	{
		Buffer.Reset();
		return false;
	}
	if (Buffer.Num() < Offset + 4 + static_cast<int32>(PayloadLen))
	{
		return false;
	}
	const uint8 Mask[4] = { Buffer[Offset], Buffer[Offset + 1], Buffer[Offset + 2], Buffer[Offset + 3] };
	Offset += 4;
	TArray<uint8> Payload;
	Payload.SetNumUninitialized(static_cast<int32>(PayloadLen));
	for (uint64 i = 0; i < PayloadLen; ++i)
	{
		Payload[static_cast<int32>(i)] = Buffer[Offset + static_cast<int32>(i)] ^ Mask[i % 4];
	}
	Buffer.RemoveAt(0, Offset + static_cast<int32>(PayloadLen));
	if (Opcode == 0x8)
	{
		OutText.Reset();
		return true;
	}
	if (Opcode == 0x1)
	{
		OutText = Utf8ToF(Payload.GetData(), Payload.Num());
		return true;
	}
	return true;
}

void UCLSessionHub::SendText(int32 Index, const FString& Text)
{
	if (!Clients.IsValidIndex(Index) || !Clients[Index].Socket || !Clients[Index].bUpgraded)
	{
		return;
	}
	TArray<uint8> Payload;
	AppendUtf8(Payload, Text);
	TArray<uint8> Frame;
	Frame.Add(0x81);
	if (Payload.Num() < 126)
	{
		Frame.Add(static_cast<uint8>(Payload.Num()));
	}
	else
	{
		Frame.Add(126);
		Frame.Add(static_cast<uint8>((Payload.Num() >> 8) & 0xFF));
		Frame.Add(static_cast<uint8>(Payload.Num() & 0xFF));
	}
	Frame.Append(Payload);
	int32 Sent = 0;
	Clients[Index].Socket->Send(Frame.GetData(), Frame.Num(), Sent);
}

void UCLSessionHub::HandleText(int32 Index, const FString& Text)
{
	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		SendText(Index, TEXT("{\"ok\":false,\"error\":\"invalid_json\"}"));
		return;
	}
	UCLLobbySubsystem* Lobby = GetGameInstance()->GetSubsystem<UCLLobbySubsystem>();
	if (!Lobby)
	{
		SendText(Index, TEXT("{\"ok\":false,\"error\":\"no_lobby\"}"));
		return;
	}
	const TSharedRef<FJsonObject> Out = FCLHubCommandRegistry::Dispatch(Lobby, Root, &Clients[Index].SeatId);
	FString Json = CLAgentCodec::JsonToString(Out);
	SendText(Index, Json);
}

void UCLSessionHub::PushSnapshots(ECLHubSnapshotReason Reason, const FGuid& OnlySeat)
{
	UCLLobbySubsystem* Lobby = GetGameInstance() ? GetGameInstance()->GetSubsystem<UCLLobbySubsystem>() : nullptr;
	UCLAgentBridgeSubsystem* Bridge = GetGameInstance() ? GetGameInstance()->GetSubsystem<UCLAgentBridgeSubsystem>() : nullptr;
	if (!Lobby || !Bridge || Clients.Num() == 0)
	{
		return;
	}

	const TCHAR* ReasonName = TEXT("stale");
	switch (Reason)
	{
	case ECLHubSnapshotReason::LobbyDirty: ReasonName = TEXT("lobbyDirty"); break;
	case ECLHubSnapshotReason::LowLookahead: ReasonName = TEXT("lowLookahead"); break;
	default: break;
	}

	for (int32 i = 0; i < Clients.Num(); ++i)
	{
		if (!Clients[i].bUpgraded || !Clients[i].SeatId.IsValid())
		{
			continue;
		}
		if (OnlySeat.IsValid() && Clients[i].SeatId != OnlySeat)
		{
			continue;
		}
		const UCLParticipantSeat* Seat = Lobby->FindSeat(Clients[i].SeatId);
		if (!Seat || !Seat->GetPlaybook() || !Seat->GetPlaybook()->WantsHubSnapshot(Reason))
		{
			continue;
		}
		const TSharedRef<FJsonObject> Root = Bridge->BuildStateJson(Clients[i].SeatId);
		Root->SetStringField(TEXT("type"), TEXT("state"));
		Root->SetStringField(TEXT("reason"), ReasonName);
		FString Json;
		const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Json);
		FJsonSerializer::Serialize(Root, Writer);
		SendText(i, Json);
	}
}

void UCLSessionHub::PumpClient(int32 Index)
{
	FClient& Client = Clients[Index];
	if (!Client.Socket)
	{
		CloseClient(Index);
		return;
	}
	uint32 Pending = 0;
	while (Client.Socket->HasPendingData(Pending) && Pending > 0)
	{
		TArray<uint8> Chunk;
		Chunk.SetNumUninitialized(static_cast<int32>(FMath::Min(Pending, static_cast<uint32>(4096))));
		int32 Read = 0;
		if (!Client.Socket->Recv(Chunk.GetData(), Chunk.Num(), Read) || Read <= 0)
		{
			CloseClient(Index);
			return;
		}
		Client.Buffer.Append(Chunk.GetData(), Read);
		if (Client.Buffer.Num() > MaxWsBuffer)
		{
			CloseClient(Index);
			return;
		}
	}

	if (!Client.bUpgraded)
	{
		if (!TryUpgrade(Index))
		{
			if (Client.Buffer.Num() > 4096)
			{
				CloseClient(Index);
			}
		}
		return;
	}

	FString Text;
	while (DecodeFrame(Client.Buffer, Text))
	{
		if (Text.IsEmpty() && Client.Buffer.Num() == 0)
		{
			CloseClient(Index);
			return;
		}
		if (!Text.IsEmpty())
		{
			HandleText(Index, Text);
		}
		if (!Clients.IsValidIndex(Index))
		{
			return;
		}
	}
}
