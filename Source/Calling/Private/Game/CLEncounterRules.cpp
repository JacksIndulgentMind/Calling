#include "Game/CLEncounterRules.h"

namespace
{
	FName JsonName(const TSharedPtr<FJsonObject>& Obj, const TCHAR* Key)
	{
		FString S;
		if (Obj.IsValid() && Obj->TryGetStringField(Key, S) && !S.IsEmpty())
		{
			return FName(*S);
		}
		return NAME_None;
	}

	TSharedPtr<FCLShrineClashEncounter> ParseShrineClash(const TSharedPtr<FJsonObject>& Obj)
	{
		TSharedPtr<FCLShrineClashEncounter> E = MakeShared<FCLShrineClashEncounter>();
		E->Id = JsonName(Obj, TEXT("id"));
		if (Obj->HasField(TEXT("teamFinalBlows")))
		{
			E->TeamFinalBlows = static_cast<int32>(Obj->GetNumberField(TEXT("teamFinalBlows")));
		}
		E->OccupyTag = JsonName(Obj, TEXT("occupyTag"));
		if (Obj->HasField(TEXT("rotateSeconds")))
		{
			E->RotateSeconds = static_cast<float>(Obj->GetNumberField(TEXT("rotateSeconds")));
		}
		Obj->TryGetBoolField(TEXT("stealIfTenWithoutShrine"), E->bStealIfTenWithoutShrine);
		Obj->TryGetBoolField(TEXT("failIfEitherTeamKillsZero"), E->bFailIfEitherTeamKillsZero);
		if (Obj->HasField(TEXT("failTimeoutSeconds")))
		{
			E->FailTimeoutSeconds = static_cast<float>(Obj->GetNumberField(TEXT("failTimeoutSeconds")));
		}
		return E;
	}

	void ParseOffVolume(const TSharedPtr<FJsonObject>& Occupy, FCLOffVolumeDot& Out)
	{
		Out = FCLOffVolumeDot();
		const TSharedPtr<FJsonObject>* Off = nullptr;
		if (!Occupy.IsValid() || !Occupy->TryGetObjectField(TEXT("offVolume"), Off) || !Off || !Off->IsValid())
		{
			return;
		}
		Out.bEnabled = true;
		Out.Type = JsonName(*Off, TEXT("type"));
		if (Out.Type.IsNone())
		{
			Out.Type = FName(TEXT("damageOverTime"));
		}
		if ((*Off)->HasField(TEXT("graceSeconds")))
		{
			Out.GraceSeconds = static_cast<float>((*Off)->GetNumberField(TEXT("graceSeconds")));
		}
		if ((*Off)->HasField(TEXT("damagePerSecond")))
		{
			Out.DamagePerSecond = static_cast<float>((*Off)->GetNumberField(TEXT("damagePerSecond")));
		}
	}

	void ParseSpawner(const TSharedPtr<FJsonObject>& Obj, FCLSpawnerDef& Out)
	{
		Out = FCLSpawnerDef();
		if (!Obj.IsValid())
		{
			return;
		}
		Out.OriginMarker = JsonName(Obj, TEXT("originMarker"));
		if (Out.OriginMarker.IsNone())
		{
			Out.OriginMarker = JsonName(Obj, TEXT("originTag"));
		}
		if (Obj->HasField(TEXT("radiusCm")))
		{
			Out.RadiusCm = static_cast<float>(Obj->GetNumberField(TEXT("radiusCm")));
		}
		else if (Obj->HasField(TEXT("jitterCm")))
		{
			Out.RadiusCm = static_cast<float>(Obj->GetNumberField(TEXT("jitterCm")));
		}
		if (Obj->HasField(TEXT("clearRadiusCm")))
		{
			Out.ClearRadiusCm = static_cast<float>(Obj->GetNumberField(TEXT("clearRadiusCm")));
		}
		if (Obj->HasField(TEXT("clearTries")))
		{
			Out.ClearTries = static_cast<int32>(Obj->GetNumberField(TEXT("clearTries")));
		}
		const TArray<TSharedPtr<FJsonValue>>* Pool = nullptr;
		if (Obj->TryGetArrayField(TEXT("pool"), Pool) && Pool)
		{
			for (const TSharedPtr<FJsonValue>& V : *Pool)
			{
				const TSharedPtr<FJsonObject> Row = V.IsValid() ? V->AsObject() : nullptr;
				if (!Row.IsValid())
				{
					continue;
				}
				FCLSpawnerPoolEntry Entry;
				Entry.Bot = JsonName(Row, TEXT("bot"));
				if (Row->HasField(TEXT("weight")))
				{
					Entry.Weight = static_cast<float>(Row->GetNumberField(TEXT("weight")));
				}
				if (!Entry.Bot.IsNone())
				{
					Out.Pool.Add(Entry);
				}
			}
		}
		const TSharedPtr<FJsonObject>* Count = nullptr;
		if (Obj->TryGetObjectField(TEXT("count"), Count) && Count && Count->IsValid())
		{
			if ((*Count)->HasField(TEXT("min")))
			{
				Out.CountMin = static_cast<int32>((*Count)->GetNumberField(TEXT("min")));
			}
			if ((*Count)->HasField(TEXT("max")))
			{
				Out.CountMax = static_cast<int32>((*Count)->GetNumberField(TEXT("max")));
			}
		}
		if (Obj->HasField(TEXT("intervalSeconds")))
		{
			Out.IntervalSeconds = static_cast<float>(Obj->GetNumberField(TEXT("intervalSeconds")));
		}
		if (Obj->HasField(TEXT("waves")))
		{
			Out.Waves = static_cast<int32>(Obj->GetNumberField(TEXT("waves")));
		}
	}

	TSharedPtr<FCLWaveHoldEncounter> ParseWaveHold(const TSharedPtr<FJsonObject>& Obj)
	{
		TSharedPtr<FCLWaveHoldEncounter> E = MakeShared<FCLWaveHoldEncounter>();
		E->Id = JsonName(Obj, TEXT("id"));
		E->OpensMarker = JsonName(Obj, TEXT("opensMarker"));
		const TArray<TSharedPtr<FJsonValue>>* Phases = nullptr;
		if (!Obj->TryGetArrayField(TEXT("phases"), Phases) || !Phases)
		{
			return E;
		}
		for (const TSharedPtr<FJsonValue>& V : *Phases)
		{
			const TSharedPtr<FJsonObject> PhaseObj = V.IsValid() ? V->AsObject() : nullptr;
			if (!PhaseObj.IsValid())
			{
				continue;
			}
			FCLWaveHoldPhase Phase;
			Phase.Id = JsonName(PhaseObj, TEXT("id"));
			const TSharedPtr<FJsonObject>* Occupy = nullptr;
			if (PhaseObj->TryGetObjectField(TEXT("occupy"), Occupy) && Occupy && Occupy->IsValid())
			{
				Phase.OccupyMarker = JsonName(*Occupy, TEXT("marker"));
				ParseOffVolume(*Occupy, Phase.OffVolume);
			}
			const TSharedPtr<FJsonObject>* Spawner = nullptr;
			if (PhaseObj->TryGetObjectField(TEXT("spawner"), Spawner) && Spawner && Spawner->IsValid())
			{
				ParseSpawner(*Spawner, Phase.Spawner);
			}
			E->Phases.Add(MoveTemp(Phase));
		}
		return E;
	}
}

TSharedPtr<ICLEncounterRules> CLParseEncounter(const TSharedPtr<FJsonObject>& Obj, FString& OutError)
{
	if (!Obj.IsValid())
	{
		OutError = TEXT("encounter_null");
		return nullptr;
	}
	const FName Type = JsonName(Obj, TEXT("type"));
	if (Type == FName(TEXT("shrineClash")))
	{
		return ParseShrineClash(Obj);
	}
	if (Type == FName(TEXT("waveHold")))
	{
		return ParseWaveHold(Obj);
	}
	OutError = FString::Printf(TEXT("encounter_unknown_type %s"), *Type.ToString());
	return nullptr;
}
