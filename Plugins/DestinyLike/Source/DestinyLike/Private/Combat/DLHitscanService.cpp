#include "Combat/DLHitscanService.h"
#include "Combat/DLDamageableComponent.h"
#include "Player/DLHealthShieldComponent.h"
#include "Player/DLWeaponBehaviorComponent.h"
#include "Player/DLWeaponMotorComponent.h"
#include "Player/DLCombatMovementComponent.h"
#include "Player/DLPlayerCharacter.h"
#include "Camera/CameraComponent.h"
#include "Core/DLTunes.h"
#include "Engine/World.h"
#include "CollisionQueryParams.h"
#include "GameFramework/Pawn.h"
#include "EngineUtils.h"

namespace
{
	FRotator ApplyDiskSpread(FRotator View, float SpreadDegrees)
	{
		if (SpreadDegrees <= 0.f)
		{
			return View;
		}
		const float Theta = FMath::FRandRange(0.f, 2.f * PI);
		const float Radius = SpreadDegrees * FMath::Sqrt(FMath::FRand());
		View.Yaw += Radius * FMath::Cos(Theta);
		View.Pitch += Radius * FMath::Sin(Theta);
		return View;
	}

	FRotator ApplyBodyAimAssist(UWorld* World, AActor* Instigator, const FVector& Start, FRotator View,
		float ConeDeg, float Magnetism)
	{
		if (!World || !Instigator || ConeDeg <= 0.f || Magnetism <= 0.f)
		{
			return View;
		}
		const FVector Aim = View.Vector();
		float BestAng = ConeDeg;
		FVector BestTo = FVector::ZeroVector;
		bool bFound = false;
		for (TActorIterator<APawn> It(World); It; ++It)
		{
			APawn* Pawn = *It;
			if (!Pawn || Pawn == Instigator)
			{
				continue;
			}
			const UDLDamageableComponent* Dmg = Pawn->FindComponentByClass<UDLDamageableComponent>();
			if (Dmg && !Dmg->IsAlive())
			{
				continue;
			}
			const UDLHealthShieldComponent* HS = Pawn->FindComponentByClass<UDLHealthShieldComponent>();
			if (!Dmg && !HS)
			{
				continue;
			}
			if (HS && !HS->IsAlive())
			{
				continue;
			}
			const FVector Body = Pawn->GetActorLocation();
			const FVector To = Body - Start;
			if (To.SizeSquared() < 100.f)
			{
				continue;
			}
			const float Ang = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(Aim, To.GetSafeNormal()), -1.f, 1.f)));
			if (Ang < BestAng)
			{
				BestAng = Ang;
				BestTo = To;
				bFound = true;
			}
		}
		if (!bFound)
		{
			return View;
		}
		const float Edge = 1.f - FMath::Square(BestAng / FMath::Max(0.1f, ConeDeg));
		const float Blend = FMath::Clamp(Magnetism * Edge, 0.f, 1.f);
		const FVector Pulled = FMath::Lerp(Aim, BestTo.GetSafeNormal(), Blend).GetSafeNormal();
		return Pulled.Rotation();
	}

	float DamageFalloff(float DistCm, float RangeStat, const FDLCombatTune& Tune)
	{
		const float T = FMath::Clamp(RangeStat, 0.f, 1.f);
		const float Optimal = FMath::Lerp(Tune.RangeOptimalMinCm, Tune.RangeOptimalMaxCm, T);
		const float Zero = Optimal * FMath::Max(1.2f, Tune.RangeFalloffMul);
		if (DistCm <= Optimal)
		{
			return 1.f;
		}
		if (DistCm >= Zero)
		{
			return 0.f;
		}
		return (Zero - DistCm) / (Zero - Optimal);
	}
}

bool DLHitscanService::Fire(UWorld* World, AActor* InstigatorActor, AController* InstigatorController,
	const FDLHitscanRequest& Request, FRotator* OutShotRot)
{
	if (!World || !InstigatorActor)
	{
		return false;
	}
	FRotator View = ApplyDiskSpread(Request.View, Request.SpreadDegrees);
	View = ApplyBodyAimAssist(World, InstigatorActor, Request.Start, View,
		Request.AimAssistConeDegrees, Request.AimAssistMagnetism);
	if (OutShotRot)
	{
		*OutShotRot = View;
	}
	const FVector End = Request.Start + View.Vector() * 50000.f;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(DLHitscan), false, InstigatorActor);
	FHitResult HitVis;
	FHitResult HitPawn;
	const bool bVis = World->LineTraceSingleByChannel(HitVis, Request.Start, End, ECC_Visibility, Params);
	const bool bPawn = World->LineTraceSingleByChannel(HitPawn, Request.Start, End, ECC_Pawn, Params);
	if (!bVis && !bPawn)
	{
		return false;
	}
	FHitResult Hit;
	if (bVis && bPawn)
	{
		Hit = (HitPawn.Distance <= HitVis.Distance) ? HitPawn : HitVis;
	}
	else
	{
		Hit = bPawn ? HitPawn : HitVis;
	}
	AActor* HitActor = Hit.GetActor();
	if (!HitActor)
	{
		return false;
	}
	UDLDamageableComponent* Damageable = HitActor->FindComponentByClass<UDLDamageableComponent>();
	UDLHealthShieldComponent* TargetHS = HitActor->FindComponentByClass<UDLHealthShieldComponent>();
	if (!Damageable && !TargetHS)
	{
		return false;
	}

	FDLCombatTune CombatTune;
	CombatTune.LoadFromIni();
	float Damage = Request.Damage * DamageFalloff(Hit.Distance, Request.RangeStat, CombatTune);
	if (Damage <= 0.f)
	{
		return false;
	}
	const bool bHead = Hit.BoneName.ToString().Contains(TEXT("head"));
	const bool bAdsPrecision = Request.bInstantKillOnPrecision && Request.bIsAds;
	const bool bPrecision = bHead || bAdsPrecision;
	if (bPrecision)
	{
		Damage *= Request.CritMultiplier;
	}
	if (UDLWeaponBehaviorComponent* Behavior = Request.Behavior.Get())
	{
		Damage *= Behavior->GetDamageMultiplier();
	}
	if (Request.bInstantKillOnPrecision && bPrecision && (!Request.bRequireAdsForInstakill || Request.bIsAds))
	{
		Damage = 9999.f;
	}
	if (Damageable)
	{
		Damageable->ApplyDamage(Damage, InstigatorController, bPrecision);
		if (!Damageable->IsAlive())
		{
			if (UDLWeaponBehaviorComponent* Behavior = Request.Behavior.Get())
			{
				if (bPrecision) { Behavior->NotifyPrecisionKill(); }
				else { Behavior->NotifyKill(); }
			}
		}
	}
	else
	{
		TargetHS->ApplyDamage(Damage, InstigatorController, bPrecision);
		if (!TargetHS->IsAlive())
		{
			if (UDLWeaponBehaviorComponent* Behavior = Request.Behavior.Get())
			{
				if (bPrecision) { Behavior->NotifyPrecisionKill(); }
				else { Behavior->NotifyKill(); }
			}
		}
	}
	return true;
}

bool DLHitscanService::QuerySightedFromPawn(const AActor* Viewer, FDLSightedTarget& OutTarget, float RangeCm)
{
	OutTarget = FDLSightedTarget();
	const ADLPlayerCharacter* Char = Cast<ADLPlayerCharacter>(Viewer);
	if (!Char || !Char->GetWorld() || !Char->GetWeaponMotor() || RangeCm <= 0.f)
	{
		return false;
	}
	FVector Start = FVector::ZeroVector;
	const FVector Dir = Char->GetWeaponMotor()->GetMuzzleAim(Start);
	Start = Char->GetWeaponMotor()->GetBarrelLocation();
	if (Dir.IsNearlyZero())
	{
		return false;
	}

	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(DLHudSights), false, Viewer);
	if (!Char->GetWorld()->LineTraceSingleByChannel(Hit, Start, Start + Dir * RangeCm, ECC_Pawn, Params))
	{
		return false;
	}
	AActor* HitActor = Hit.GetActor();
	if (!HitActor || HitActor == Viewer)
	{
		return false;
	}
	UDLDamageableComponent* Dmg = HitActor->FindComponentByClass<UDLDamageableComponent>();
	UDLHealthShieldComponent* HS = HitActor->FindComponentByClass<UDLHealthShieldComponent>();
	if ((!Dmg || !Dmg->IsAlive()) && (!HS || !HS->IsAlive()))
	{
		return false;
	}
	OutTarget.Actor = HitActor;
	if (Dmg)
	{
		OutTarget.Health = Dmg->GetHealth();
		OutTarget.Shield = Dmg->GetShield();
		OutTarget.MaxHealth = FMath::Max(1.f, Dmg->GetMaxHealth());
		OutTarget.MaxShield = Dmg->GetMaxShield();
	}
	else
	{
		OutTarget.Health = HS->GetHealth();
		OutTarget.Shield = HS->GetShield();
		OutTarget.MaxHealth = FMath::Max(1.f, HS->GetTune().MaxHealth);
		OutTarget.MaxShield = HS->GetTune().MaxShield;
	}
	return true;
}

namespace
{
	constexpr float RadarMotionFloor = 80.f;

	FVector RadarViewerEye(const ADLPlayerCharacter* Char)
	{
		return Char->GetActorLocation() + FVector(0.f, 0.f, 60.f);
	}

	bool IsRadarMotion(const ADLPlayerCharacter* Char)
	{
		if (Char->GetVelocity().Size2D() > RadarMotionFloor)
		{
			return true;
		}
		if (const UDLWeaponMotorComponent* Gun = Char->GetWeaponMotor())
		{
			if (Gun->IsRadarHot())
			{
				return true;
			}
		}
		const UDLCombatMovementComponent* Move = Char->GetCombatMovement();
		return Move && (Move->IsSliding() || Move->IsDodging() || Move->IsDashing());
	}

	bool HasRadarLos(const ADLPlayerCharacter* Viewer, const ADLPlayerCharacter* Target)
	{
		UWorld* World = Viewer->GetWorld();
		if (!World)
		{
			return false;
		}
		FCollisionQueryParams Params(SCENE_QUERY_STAT(DLRadarLos), false, Viewer);
		Params.AddIgnoredActor(Target);
		FHitResult Hit;
		const FVector Start = RadarViewerEye(Viewer);
		const FVector End = Target->GetActorLocation();
		return !World->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params);
	}

	void RadarHeadingBasis(const ADLPlayerCharacter* Char, FVector& OutFwd, FVector& OutRight)
	{
		FVector Fwd = Char->GetActorForwardVector();
		if (const UCameraComponent* Cam = Char->GetFollowCamera())
		{
			Fwd = Cam->GetForwardVector();
		}
		Fwd.Z = 0.f;
		if (!Fwd.Normalize())
		{
			Fwd = FVector::ForwardVector;
		}
		OutFwd = Fwd;
		OutRight = FVector(-Fwd.Y, Fwd.X, 0.f);
	}

	int32 RadarWedgeFromRel(const FVector& Rel, const FVector& Fwd, const FVector& Right)
	{
		const float Forward = Rel.X * Fwd.X + Rel.Y * Fwd.Y;
		const float RightAmt = Rel.X * Right.X + Rel.Y * Right.Y;
		const float Deg = FMath::RadiansToDegrees(FMath::Atan2(RightAmt, Forward));
		if (Deg >= -60.f && Deg < 60.f)
		{
			return 0;
		}
		if (Deg >= 60.f)
		{
			return 1;
		}
		return 2;
	}
}

void DLHitscanService::QueryRadarContacts(const AActor* ViewerActor, TArray<FDLRadarContact>& OutContacts, float RangeCm, bool bRequireLos)
{
	OutContacts.Reset();
	const ADLPlayerCharacter* Viewer = Cast<ADLPlayerCharacter>(ViewerActor);
	if (!Viewer || !Viewer->GetWorld() || RangeCm <= 0.f)
	{
		return;
	}
	for (TActorIterator<ADLPlayerCharacter> It(Viewer->GetWorld()); It; ++It)
	{
		ADLPlayerCharacter* Other = *It;
		if (!Other || Other == Viewer || !Other->IsCombatAlive())
		{
			continue;
		}
		const float Dist = FVector::Dist2D(Viewer->GetActorLocation(), Other->GetActorLocation());
		if (Dist > RangeCm)
		{
			continue;
		}
		if (!IsRadarMotion(Other) || (bRequireLos && !HasRadarLos(Viewer, Other)))
		{
			continue;
		}
		FDLRadarContact Contact;
		Contact.Actor = Other;
		Contact.Location = Other->GetActorLocation();
		Contact.DistXY = Dist;
		if (const UDLCombatMovementComponent* Move = Other->GetCombatMovement())
		{
			Contact.CrouchAlpha = Move->GetCrouchAlpha();
			Contact.bLowProfile = Contact.CrouchAlpha > 0.5f && !Move->IsSliding();
		}
		OutContacts.Add(Contact);
	}
}

int32 DLHitscanService::QueryRadarRippleMask(const AActor* ViewerActor, float RangeCm)
{
	TArray<FDLRadarContact> Contacts;
	QueryRadarContacts(ViewerActor, Contacts, RangeCm, false);
	const ADLPlayerCharacter* Viewer = Cast<ADLPlayerCharacter>(ViewerActor);
	if (!Viewer || Contacts.Num() == 0)
	{
		return 0;
	}
	FVector Fwd, Right;
	RadarHeadingBasis(Viewer, Fwd, Right);
	const FVector Eye = Viewer->GetActorLocation();
	int32 Mask = 0;
	for (const FDLRadarContact& Contact : Contacts)
	{
		const int32 Wedge = RadarWedgeFromRel(Contact.Location - Eye, Fwd, Right);
		Mask |= (1 << Wedge);
	}
	return Mask;
}
