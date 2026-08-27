#include "Nav/CLNavTune.h"
#include "Core/CLLog.h"
#include "Core/CLError.h"
#include "Game/CLErrorBoundary.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Dom/JsonObject.h"

namespace
{
	FString ConfigPath()
	{
		return FPaths::Combine(FPaths::ProjectConfigDir(), TEXT("Nav/NavTune.json"));
	}

	void AddDefaultLinks(FCLNavTune& Tune)
	{
		auto Link = [](const TCHAR* Name, float Len, float Edge, const TCHAR* Depth, const TCHAR* Height, const TCHAR* Ends,
			float Filter, const TCHAR* Down, const TCHAR* Up) -> FCLNavLinkTune
		{
			FCLNavLinkTune L;
			L.Name = FName(Name);
			L.JumpLength = Len;
			L.JumpDistanceFromEdge = Edge;
			L.JumpMaxDepth = Depth;
			L.JumpHeight = Height;
			L.JumpEndsHeightTolerance = Ends;
			L.SamplingSeparationFactor = 2.f;
			L.FilterDistanceThreshold = Filter;
			L.DownArea = Down;
			L.UpArea = Up;
			return L;
		};
		Tune.Links.Reset();
		Tune.Links.Add(Link(TEXT("CoverOver"), 200.f, 20.f, TEXT("8"), TEXT("coverHeight"), TEXT("12"), 120.f, TEXT("default"), TEXT("default")));
		Tune.Links.Add(Link(TEXT("DropDown"), 280.f, 25.f, TEXT("survivingDrop"), TEXT("10"), TEXT("survivingDrop"), 180.f, TEXT("default"), TEXT("null")));
		Tune.Links.Add(Link(TEXT("JumpUp"), 280.f, 20.f, TEXT("-jumpApex"), TEXT("jumpApex"), TEXT("jumpApex"), 120.f, TEXT("null"), TEXT("default")));
		Tune.Links.Add(Link(TEXT("JumpDown"), 400.f, 25.f, TEXT("survivingDrop"), TEXT("coverHeight"), TEXT("survivingDrop"), 180.f, TEXT("default"), TEXT("null")));
		Tune.Links.Add(Link(TEXT("JumpOver"), 280.f, 20.f, TEXT("12"), TEXT("jumpApex"), TEXT("20"), 160.f, TEXT("longJump"), TEXT("longJump")));
	}

	FString JsonScalarString(const TSharedPtr<FJsonObject>& Obj, const TCHAR* Key, const FString& Default)
	{
		if (!Obj.IsValid() || !Obj->HasField(Key))
		{
			return Default;
		}
		const TSharedPtr<FJsonValue> V = Obj->TryGetField(Key);
		if (!V.IsValid())
		{
			return Default;
		}
		if (V->Type == EJson::Number)
		{
			return FString::Printf(TEXT("%g"), V->AsNumber());
		}
		if (V->Type == EJson::String)
		{
			return V->AsString();
		}
		return Default;
	}

	float JsonNum(const TSharedPtr<FJsonObject>& Obj, const TCHAR* Key, float Default)
	{
		return Obj.IsValid() && Obj->HasField(Key) ? static_cast<float>(Obj->GetNumberField(Key)) : Default;
	}

	FCLNavTune LoadOrDefault()
	{
		FCLNavTune Tune;
		AddDefaultLinks(Tune);

		FString Content;
		if (!FFileHelper::LoadFileToString(Content, *ConfigPath()))
		{
			UE_LOG(LogCalling, Warning, TEXT("Calling: missing NavTune.json; using compiled defaults"));
			UCLErrorBoundary::ReportStatic(nullptr, FCLError::Make(
				ECLErrorKind::NonDeterministic,
				TEXT("nav_tune_missing"),
				TEXT("NavTune.json missing; using compiled defaults")));
			return Tune;
		}

		TSharedPtr<FJsonObject> Root;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Content);
		if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
		{
			UE_LOG(LogCalling, Warning, TEXT("Calling: NavTune.json failed to parse; using compiled defaults"));
			UCLErrorBoundary::ReportStatic(nullptr, FCLError::Make(
				ECLErrorKind::NonDeterministic,
				TEXT("nav_tune_parse"),
				TEXT("NavTune.json failed to parse; using compiled defaults")));
			return Tune;
		}

		Tune.JumpApexCm = JsonNum(Root, TEXT("jumpApexCm"), Tune.JumpApexCm);
		Tune.CoverHeightCm = JsonNum(Root, TEXT("coverHeightCm"), Tune.CoverHeightCm);
		Tune.MaxStepHeightCm = JsonNum(Root, TEXT("maxStepHeightCm"), Tune.MaxStepHeightCm);
		Tune.AgentRadiusCm = JsonNum(Root, TEXT("agentRadiusCm"), Tune.AgentRadiusCm);
		Tune.AgentHeightCm = JsonNum(Root, TEXT("agentHeightCm"), Tune.AgentHeightCm);
		Tune.AgentMaxSlopeDeg = JsonNum(Root, TEXT("agentMaxSlopeDeg"), Tune.AgentMaxSlopeDeg);

		if (const TSharedPtr<FJsonObject> Probe = Root->HasField(TEXT("probe")) ? Root->GetObjectField(TEXT("probe")) : nullptr)
		{
			Tune.Probe.MaxCm = JsonNum(Probe, TEXT("maxCm"), Tune.Probe.MaxCm);
			Tune.Probe.JumpableHeadClearCm = JsonNum(Probe, TEXT("jumpableHeadClearCm"), Tune.Probe.JumpableHeadClearCm);
			Tune.Probe.JumpFaceCm = JsonNum(Probe, TEXT("jumpFaceCm"), Tune.Probe.JumpFaceCm);
			Tune.Probe.HeadLiftCm = JsonNum(Probe, TEXT("headLiftCm"), Tune.Probe.HeadLiftCm);
			Tune.Probe.WalkableNormalZ = JsonNum(Probe, TEXT("walkableNormalZ"), Tune.Probe.WalkableNormalZ);
			Tune.Probe.UpSlopeMin = JsonNum(Probe, TEXT("upSlopeMin"), Tune.Probe.UpSlopeMin);
			Tune.Probe.LipDropMinCm = JsonNum(Probe, TEXT("lipDropMinCm"), Tune.Probe.LipDropMinCm);
			Tune.Probe.FloorProbeMaxCm = JsonNum(Probe, TEXT("floorProbeMaxCm"), Tune.Probe.FloorProbeMaxCm);
			Tune.Probe.SampleStepCm = JsonNum(Probe, TEXT("sampleStepCm"), Tune.Probe.SampleStepCm);
			Tune.Probe.WalkOffGapMaxCm = JsonNum(Probe, TEXT("walkOffGapMaxCm"), Tune.Probe.WalkOffGapMaxCm);
			Tune.Probe.CoverDepthCm = JsonNum(Probe, TEXT("coverDepthCm"), Tune.Probe.CoverDepthCm);
			Tune.Probe.StartBackupCm = JsonNum(Probe, TEXT("startBackupCm"), Tune.Probe.StartBackupCm);
		}

		const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
		if (Root->TryGetArrayField(TEXT("links"), Arr) && Arr && Arr->Num() > 0)
		{
			Tune.Links.Reset();
			for (const TSharedPtr<FJsonValue>& V : *Arr)
			{
				const TSharedPtr<FJsonObject> O = V->AsObject();
				if (!O.IsValid())
				{
					continue;
				}
				FCLNavLinkTune L;
				L.Name = FName(*O->GetStringField(TEXT("name")));
				L.JumpLength = JsonNum(O, TEXT("jumpLength"), 200.f);
				L.JumpDistanceFromEdge = JsonNum(O, TEXT("jumpDistanceFromEdge"), 20.f);
				L.JumpMaxDepth = JsonScalarString(O, TEXT("jumpMaxDepth"), TEXT("8"));
				L.JumpHeight = JsonScalarString(O, TEXT("jumpHeight"), TEXT("coverHeight"));
				L.JumpEndsHeightTolerance = JsonScalarString(O, TEXT("jumpEndsHeightTolerance"), TEXT("12"));
				L.SamplingSeparationFactor = JsonNum(O, TEXT("samplingSeparationFactor"), 2.f);
				L.FilterDistanceThreshold = JsonNum(O, TEXT("filterDistanceThreshold"), 120.f);
				L.DownArea = O->HasField(TEXT("downArea")) ? O->GetStringField(TEXT("downArea")) : TEXT("default");
				L.UpArea = O->HasField(TEXT("upArea")) ? O->GetStringField(TEXT("upArea")) : TEXT("default");
				Tune.Links.Add(L);
			}
		}

		return Tune;
	}
}

const FCLNavTune& CLNavTune::Get()
{
	static const FCLNavTune Cached = LoadOrDefault();
	return Cached;
}

float CLNavTune::ResolveScalar(const FString& TokenOrNumber, float Fallback, const FCLNavTune& Tune, float SurvivingDropCm)
{
	if (TokenOrNumber.Equals(TEXT("survivingDrop"), ESearchCase::IgnoreCase))
	{
		return SurvivingDropCm;
	}
	if (TokenOrNumber.Equals(TEXT("jumpApex"), ESearchCase::IgnoreCase))
	{
		return Tune.JumpApexCm;
	}
	if (TokenOrNumber.Equals(TEXT("-jumpApex"), ESearchCase::IgnoreCase))
	{
		return -Tune.JumpApexCm;
	}
	if (TokenOrNumber.Equals(TEXT("coverHeight"), ESearchCase::IgnoreCase))
	{
		return Tune.CoverHeightCm;
	}
	if (TokenOrNumber.IsNumeric())
	{
		return FCString::Atof(*TokenOrNumber);
	}
	return Fallback;
}
