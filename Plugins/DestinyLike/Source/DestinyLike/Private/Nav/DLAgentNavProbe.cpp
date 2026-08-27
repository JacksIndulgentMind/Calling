#include "Nav/DLAgentNavProbe.h"
#include "Nav/DLNavTune.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Components/CapsuleComponent.h"
#include "CollisionQueryParams.h"
#include "NavigationSystem.h"
#include "NavMesh/RecastNavMesh.h"
#include "Math/RotationMatrix.h"

namespace
{
	float SlopeAlongIngress(const FVector& Walk, const FVector& Normal)
	{
		const FVector WalkXY = FVector(Walk.X, Walk.Y, 0.f).GetSafeNormal();
		if (WalkXY.IsNearlyZero() || Normal.Z <= KINDA_SMALL_NUMBER)
		{
			return 0.f;
		}
		return -FVector::DotProduct(WalkXY, FVector(Normal.X, Normal.Y, 0.f)) / Normal.Z;
	}

	bool FloorZAt(UWorld* World, const AActor* Ignore, const FVector& Origin, float DownCm, float& OutZ)
	{
		FHitResult Hit;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(DLAgentFloorZ), false, Ignore);
		if (!World->LineTraceSingleByChannel(Hit, Origin, Origin - FVector(0.f, 0.f, DownCm), ECC_WorldStatic, Params))
		{
			return false;
		}
		OutZ = Hit.ImpactPoint.Z;
		return true;
	}
}

const TCHAR* DLAgentNavProbe::FwdKindName(EDLFwdKind Kind)
{
	switch (Kind)
	{
	case EDLFwdKind::Walk: return TEXT("walk");
	case EDLFwdKind::Drop: return TEXT("drop");
	case EDLFwdKind::Cover: return TEXT("cover");
	case EDLFwdKind::JumpUp: return TEXT("jumpUp");
	case EDLFwdKind::JumpDown: return TEXT("jumpDown");
	case EDLFwdKind::Wall: return TEXT("wall");
	default: return TEXT("open");
	}
}

int32 DLAgentNavProbe::NavTileCount(UWorld* World)
{
	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
	if (!NavSys)
	{
		return -1;
	}
	const ARecastNavMesh* Recast = Cast<ARecastNavMesh>(
		NavSys->GetDefaultNavDataInstance(FNavigationSystem::ECreateIfMissing::DontCreate));
	return Recast ? Recast->GetNumActiveTiles() : 0;
}

FDLAgentBlockHit DLAgentNavProbe::ProbeBlock(UWorld* World, const AActor* Ignore, const FVector& Start, const FVector& Dir, float FeetZ)
{
	const FDLNavProbeTune& P = DLNavTune::Get().Probe;
	const float JumpApexCm = DLNavTune::Get().JumpApexCm;
	const float MaxStepHeightCm = DLNavTune::Get().MaxStepHeightCm;
	FDLAgentBlockHit Out;
	Out.Dist = P.MaxCm;
	if (!World || Dir.IsNearlyZero())
	{
		return Out;
	}
	const FVector Walk = Dir.GetSafeNormal();
	bool bSawAir = false;
	float AirStart = P.MaxCm;
	float LastFloorZ = FeetZ;
	bool bHadFloor = true;

	for (float Ahead = 50.f; Ahead <= P.MaxCm + 0.1f; Ahead += P.SampleStepCm)
	{
		float FloorZ = 0.f;
		const FVector Origin = Start + Walk * Ahead + FVector(0.f, 0.f, 80.f);
		if (!FloorZAt(World, Ignore, Origin, P.FloorProbeMaxCm, FloorZ))
		{
			if (!bSawAir)
			{
				bSawAir = true;
				AirStart = Ahead;
			}
			bHadFloor = false;
			continue;
		}

		const float Rel = FeetZ - FloorZ;
		const bool bContiguous = bHadFloor && FMath::Abs(FloorZ - LastFloorZ) <= MaxStepHeightCm;
		if (!bSawAir && bContiguous)
		{
			LastFloorZ = FloorZ;
			continue;
		}

		Out.Dist = bSawAir ? AirStart : FMath::Max(50.f, Ahead - P.SampleStepCm);
		if (Rel > P.LipDropMinCm)
		{
			const float Gap = bSawAir ? (Ahead - AirStart) : 0.f;
			Out.Kind = (Gap > P.WalkOffGapMaxCm) ? EDLFwdKind::JumpDown : EDLFwdKind::Drop;
			return Out;
		}
		if (Rel < -P.LipDropMinCm)
		{
			if ((-Rel) > JumpApexCm)
			{
				Out.Kind = EDLFwdKind::Wall;
				return Out;
			}
			bool bHopOver = false;
			for (float Peek = Ahead + P.SampleStepCm; Peek <= Ahead + P.CoverDepthCm + 0.1f; Peek += P.SampleStepCm)
			{
				float PeekZ = 0.f;
				const FVector PeekOrigin = Start + Walk * Peek + FVector(0.f, 0.f, 80.f);
				if (FloorZAt(World, Ignore, PeekOrigin, P.FloorProbeMaxCm, PeekZ)
					&& FMath::Abs(FeetZ - PeekZ) <= P.LipDropMinCm)
				{
					bHopOver = true;
					break;
				}
			}
			Out.Kind = bHopOver ? EDLFwdKind::Cover : EDLFwdKind::JumpUp;
			return Out;
		}
		Out.Kind = EDLFwdKind::Cover;
		return Out;
	}

	if (bSawAir)
	{
		Out.Dist = AirStart;
		Out.Kind = EDLFwdKind::Wall;
		return Out;
	}

	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(DLAgentWall), false, Ignore);
	if (World->LineTraceSingleByChannel(Hit, Start, Start + Walk * P.MaxCm, ECC_WorldStatic, Params)
		&& Hit.bBlockingHit
		&& Hit.ImpactNormal.Z > P.WalkableNormalZ
		&& SlopeAlongIngress(Walk, Hit.ImpactNormal) > P.UpSlopeMin)
	{
		Out.Dist = Hit.Distance;
		Out.Kind = EDLFwdKind::Walk;
	}
	return Out;
}

float DLAgentNavProbe::FloorDropCm(UWorld* World, const AActor* Ignore, const FVector& Loc, float HalfHeight, const FVector& Dir, float AheadCm)
{
	const float FloorProbeMaxCm = DLNavTune::Get().Probe.FloorProbeMaxCm;
	if (!World || Dir.IsNearlyZero())
	{
		return 0.f;
	}
	const float FeetZ = Loc.Z - HalfHeight;
	float FloorZ = 0.f;
	const FVector Origin = Loc + Dir.GetSafeNormal() * AheadCm + FVector(0.f, 0.f, 40.f);
	if (!FloorZAt(World, Ignore, Origin, FloorProbeMaxCm, FloorZ))
	{
		return FloorProbeMaxCm;
	}
	return FeetZ - FloorZ;
}

void DLAgentNavProbe::FillStateJson(const TSharedRef<FJsonObject>& Root, UWorld* World, const ACharacter* Char)
{
	Root->SetNumberField(TEXT("navTiles"), NavTileCount(World));
	if (!Char || !World)
	{
		return;
	}

	const FDLNavProbeTune& P = DLNavTune::Get().Probe;
	FRotator YawRot = FRotator::ZeroRotator;
	if (const APlayerController* PC = Cast<APlayerController>(Char->GetController()))
	{
		YawRot.Yaw = PC->GetControlRotation().Yaw;
	}
	else
	{
		YawRot.Yaw = Char->GetActorRotation().Yaw;
	}
	const FVector Forward = FRotationMatrix(YawRot).GetUnitAxis(EAxis::X);
	const FVector Right = FRotationMatrix(YawRot).GetUnitAxis(EAxis::Y);
	const float HalfH = Char->GetCapsuleComponent() ? Char->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 96.f;
	const float FeetZ = Char->GetActorLocation().Z - HalfH;
	const FVector Start = Char->GetActorLocation() - Forward * P.StartBackupCm;
	const FDLAgentBlockHit Fwd = ProbeBlock(World, Char, Start, Forward, FeetZ);
	Root->SetNumberField(TEXT("fwdDist"), Fwd.Dist);
	Root->SetStringField(TEXT("fwdKind"), FwdKindName(Fwd.Kind));
	Root->SetNumberField(TEXT("fwdWalk"), Fwd.Kind == EDLFwdKind::Walk ? 1 : 0);
	Root->SetNumberField(TEXT("leftDist"), ProbeBlock(World, Char, Start, -Right, FeetZ).Dist);
	Root->SetNumberField(TEXT("rightDist"), ProbeBlock(World, Char, Start, Right, FeetZ).Dist);
	const FVector HeadStart = Char->GetActorLocation() + FVector(0.f, 0.f, P.HeadLiftCm) - Forward * P.StartBackupCm;
	Root->SetNumberField(TEXT("headDist"), ProbeBlock(World, Char, HeadStart, Forward, FeetZ).Dist);
}
