#include "Game/CLGameModeCatalog.h"
#include "AI/CLTaskMarker.h"
#include "Core/CLLog.h"
#include "Game/CLErrorBoundary.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Dom/JsonObject.h"
#include "Engine/World.h"

void UCLGameModeCatalog::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	LoadFiles();
}

bool UCLGameModeCatalog::LoadFiles()
{
	bLoaded = LoadMapsDir() && LoadModesDir();
	return bLoaded;
}

namespace
{
	FName JsonName(const TSharedPtr<FJsonObject>& Obj, const TCHAR* Key)
	{
		FString S;
		if (Obj->TryGetStringField(Key, S) && !S.IsEmpty())
		{
			return FName(*S);
		}
		return NAME_None;
	}

	void JsonNameArray(const TSharedPtr<FJsonObject>& Obj, const TCHAR* Key, TArray<FName>& Out)
	{
		Out.Reset();
		const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
		if (!Obj->TryGetArrayField(Key, Arr) || !Arr)
		{
			return;
		}
		for (const TSharedPtr<FJsonValue>& V : *Arr)
		{
			if (V.IsValid())
			{
				const FString S = V->AsString();
				if (!S.IsEmpty())
				{
					Out.Add(FName(*S));
				}
			}
		}
	}
}

bool UCLGameModeCatalog::LoadMapsDir()
{
	Maps.Reset();
	const FString Dir = FPaths::Combine(FPaths::ProjectConfigDir(), TEXT("Maps"));
	TArray<FString> Files;
	IFileManager::Get().FindFiles(Files, *FPaths::Combine(Dir, TEXT("*.json")), true, false);
	for (const FString& File : Files)
	{
		const FString Path = FPaths::Combine(Dir, File);
		FString Text;
		if (!FFileHelper::LoadFileToString(Text, *Path))
		{
			continue;
		}
		TSharedPtr<FJsonObject> Root;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
		if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
		{
			UCLErrorBoundary::ReportStatic(this, FCLError::Make(
				ECLErrorKind::NonDeterministic, TEXT("map_catalog_parse"), Path));
			continue;
		}
		FCLMapCatalogEntry Entry;
		Entry.Id = JsonName(Root, TEXT("id"));
		Root->TryGetStringField(TEXT("umap"), Entry.Umap);
		if (Root->HasField(TEXT("minPlayers")))
		{
			Entry.MinPlayers = static_cast<int32>(Root->GetNumberField(TEXT("minPlayers")));
		}
		if (Root->HasField(TEXT("maxPlayers")))
		{
			Entry.MaxPlayers = static_cast<int32>(Root->GetNumberField(TEXT("maxPlayers")));
		}
		JsonNameArray(Root, TEXT("supportedGameModes"), Entry.SupportedGameModes);
		const TSharedPtr<FJsonObject>* Markers = nullptr;
		if (Root->TryGetObjectField(TEXT("markers"), Markers) && Markers && Markers->IsValid())
		{
			for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : (*Markers)->Values)
			{
				TArray<FName> Tags;
				if (Pair.Value.IsValid())
				{
					for (const TSharedPtr<FJsonValue>& V : Pair.Value->AsArray())
					{
						if (V.IsValid())
						{
							const FString S = V->AsString();
							if (!S.IsEmpty())
							{
								Tags.Add(FName(*S));
							}
						}
					}
				}
				Entry.MarkerTags.Add(FName(*Pair.Key), MoveTemp(Tags));
			}
		}
		if (!Entry.Id.IsNone())
		{
			Maps.Add(Entry.Id, MoveTemp(Entry));
		}
	}
	return Maps.Num() > 0;
}

bool UCLGameModeCatalog::LoadModesDir()
{
	Modes.Reset();
	const FString Dir = FPaths::Combine(FPaths::ProjectConfigDir(), TEXT("GameModes"));
	TArray<FString> Files;
	IFileManager::Get().FindFiles(Files, *FPaths::Combine(Dir, TEXT("*.json")), true, false);
	for (const FString& File : Files)
	{
		const FString Path = FPaths::Combine(Dir, File);
		FString Text;
		if (!FFileHelper::LoadFileToString(Text, *Path))
		{
			continue;
		}
		TSharedPtr<FJsonObject> Root;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
		if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
		{
			UCLErrorBoundary::ReportStatic(this, FCLError::Make(
				ECLErrorKind::NonDeterministic, TEXT("game_mode_parse"), Path));
			continue;
		}
		FCLGameModeDef Mode;
		Mode.Id = JsonName(Root, TEXT("id"));
		Mode.Kind = JsonName(Root, TEXT("kind"));
		JsonNameArray(Root, TEXT("requireTags"), Mode.RequireTags);
		const TSharedPtr<FJsonObject>* Combat = nullptr;
		if (Root->TryGetObjectField(TEXT("combat"), Combat) && Combat && Combat->IsValid())
		{
			if ((*Combat)->HasField(TEXT("teamFinalBlows")))
			{
				Mode.TeamFinalBlows = static_cast<int32>((*Combat)->GetNumberField(TEXT("teamFinalBlows")));
			}
		}
		const TSharedPtr<FJsonObject>* Space = nullptr;
		if (Root->TryGetObjectField(TEXT("space"), Space) && Space && Space->IsValid())
		{
			Mode.OccupyTag = JsonName(*Space, TEXT("occupyTag"));
			if ((*Space)->HasField(TEXT("rotateSeconds")))
			{
				Mode.RotateSeconds = static_cast<float>((*Space)->GetNumberField(TEXT("rotateSeconds")));
			}
		}
		const TSharedPtr<FJsonObject>* Win = nullptr;
		if (Root->TryGetObjectField(TEXT("win"), Win) && Win && Win->IsValid())
		{
			(*Win)->TryGetBoolField(TEXT("stealIfTenWithoutShrine"), Mode.bStealIfTenWithoutShrine);
			(*Win)->TryGetBoolField(TEXT("failIfEitherTeamKillsZero"), Mode.bFailIfEitherTeamKillsZero);
		}
		if (Root->HasField(TEXT("failTimeoutSeconds")))
		{
			Mode.FailTimeoutSeconds = static_cast<float>(Root->GetNumberField(TEXT("failTimeoutSeconds")));
		}
		if (!Mode.Id.IsNone())
		{
			Modes.Add(Mode.Id, MoveTemp(Mode));
		}
	}
	return Modes.Num() > 0;
}

const FCLMapCatalogEntry* UCLGameModeCatalog::FindMap(FName Id) const
{
	return Maps.Find(Id);
}

const FCLMapCatalogEntry* UCLGameModeCatalog::FindMapByUmap(const FString& Umap) const
{
	for (const TPair<FName, FCLMapCatalogEntry>& Pair : Maps)
	{
		if (Pair.Value.Umap == Umap)
		{
			return &Pair.Value;
		}
	}
	return nullptr;
}

const FCLGameModeDef* UCLGameModeCatalog::FindMode(FName Id) const
{
	return Modes.Find(Id);
}

void UCLGameModeCatalog::ApplyMarkerTags(UWorld* World, FName MapId) const
{
	const FCLMapCatalogEntry* Map = FindMap(MapId);
	if (!World || !Map)
	{
		return;
	}
	for (const TPair<FName, TArray<FName>>& Pair : Map->MarkerTags)
	{
		if (ACLTaskMarker* Marker = ACLTaskMarker::FindById(World, Pair.Key))
		{
			Marker->ObjectiveTags = Pair.Value;
		}
	}
}

FCLStatus UCLGameModeCatalog::Validate(UWorld* World, FName MapId, FName ModeId) const
{
	const FCLMapCatalogEntry* Map = FindMap(MapId);
	if (!Map)
	{
		return FCLStatus::Fail(ECLErrorKind::Logic, TEXT("map_unknown"), MapId.ToString());
	}
	const FCLGameModeDef* Mode = FindMode(ModeId);
	if (!Mode)
	{
		return FCLStatus::Fail(ECLErrorKind::Logic, TEXT("game_mode_unknown"), ModeId.ToString());
	}
	if (!Map->SupportedGameModes.Contains(ModeId))
	{
		return FCLStatus::Fail(ECLErrorKind::Logic, TEXT("game_mode_unsupported"),
			FString::Printf(TEXT("%s not on %s"), *ModeId.ToString(), *MapId.ToString()));
	}
	if (!World)
	{
		return FCLStatus::Fail(ECLErrorKind::NonDeterministic, TEXT("game_mode_no_world"), TEXT(""));
	}
	for (const FName Tag : Mode->RequireTags)
	{
		if (!ACLTaskMarker::FindByTag(World, Tag))
		{
			return FCLStatus::Fail(ECLErrorKind::Logic, TEXT("game_mode_missing_tag"), Tag.ToString());
		}
	}
	return FCLStatus::Ok();
}
