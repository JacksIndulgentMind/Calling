#include "Player/CLWeaponMotorComponent.h"
#include "Weapon/CLWeaponFireMode.h"
#include "Combat/CLHitscanService.h"
#include "Player/CLWeaponBehaviorComponent.h"
#include "Player/CLPlayerCharacter.h"
#include "Player/CLViewWeapon.h"
#include "Weapon/CLWeaponProjectile.h"
#include "Game/CLGameInstance.h"
#include "Loot/CLLootRulesService.h"
#include "Camera/CameraComponent.h"
#include "Misc/ConfigCacheIni.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameFramework/PlayerController.h"
#include "DrawDebugHelpers.h"

UCLWeaponMotorComponent::UCLWeaponMotorComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UCLWeaponMotorComponent::BeginPlay()
{
	Super::BeginPlay();
	ReloadSettings();
}

void UCLWeaponMotorComponent::ReloadSettings()
{
	Tune.LoadFromIni();
}

bool UCLWeaponMotorComponent::IsOwnerSliding() const
{
	if (const ACLPlayerCharacter* Char = Cast<ACLPlayerCharacter>(GetOwner()))
	{
		return Char->IsSliding();
	}
	return false;
}

float UCLWeaponMotorComponent::SlideHandlingScale() const
{
	return IsOwnerSliding() ? FMath::Max(0.5f, 1.f - Tune.SlideHandlingBonus) : 1.f;
}

const FCLWeaponClassDef* UCLWeaponMotorComponent::ResolveClassDef(const FCLItemInstance& Item) const
{
	if (const UWorld* World = GetWorld())
	{
		if (const UGameInstance* GI = World->GetGameInstance())
		{
			if (const UCLGameInstance* CLGI = Cast<UCLGameInstance>(GI))
			{
				if (const UCLLootRulesService* Loot = CLGI->GetLootRulesService())
				{
					return Loot->FindWeaponClass(Item.DefinitionId);
				}
			}
		}
	}
	return nullptr;
}

void UCLWeaponMotorComponent::StoreActiveAmmo()
{
	if (!ActiveItem.InstanceId.IsValid())
	{
		return;
	}
	if (ActiveItem.Weapon.Slot == ECLWeaponSlot::Primary)
	{
		Overlay.StoredPrimaryAmmo = Overlay.AmmoInMag;
	}
	else
	{
		Overlay.StoredSpecialAmmo = Overlay.AmmoInMag;
	}
}

void UCLWeaponMotorComponent::RestoreSlotAmmo(ECLWeaponSlot Slot)
{
	const int32 Stored = (Slot == ECLWeaponSlot::Primary) ? Overlay.StoredPrimaryAmmo : Overlay.StoredSpecialAmmo;
	if (Stored >= 0)
	{
		Overlay.AmmoInMag = Stored;
		return;
	}
	Overlay.AmmoInMag = GetMagazineSize();
	if (Slot == ECLWeaponSlot::Primary)
	{
		Overlay.StoredPrimaryAmmo = Overlay.AmmoInMag;
	}
	else
	{
		Overlay.StoredSpecialAmmo = Overlay.AmmoInMag;
	}
}

void UCLWeaponMotorComponent::EquipItemByClassId(const FCLItemInstance& Item, FName WeaponClassId)
{
	const FCLWeaponClassDef* ClassDef = ResolveClassDef(Item);
	if (!WeaponClassId.IsNone())
	{
		if (const UWorld* World = GetWorld())
		{
			if (const UGameInstance* GI = World->GetGameInstance())
			{
				if (const UCLGameInstance* CLGI = Cast<UCLGameInstance>(GI))
				{
					if (const UCLLootRulesService* Loot = CLGI->GetLootRulesService())
					{
						if (const FCLWeaponClassDef* Forced = Loot->FindWeaponClass(WeaponClassId))
						{
							ClassDef = Forced;
						}
					}
				}
			}
		}
	}
	EquipItem(Item, ClassDef);
}

void UCLWeaponMotorComponent::EquipItem(const FCLItemInstance& Item, const FCLWeaponClassDef* ClassDef)
{
	StoreActiveAmmo();

	ActiveItem = Item;
	if (Item.Weapon.Slot == ECLWeaponSlot::Primary)
	{
		PrimaryItem = Item;
	}
	else
	{
		SpecialItem = Item;
	}

	if (!ClassDef)
	{
		ClassDef = ResolveClassDef(Item);
	}
	if (ClassDef)
	{
		EquippedClass = *ClassDef;
	}
	else
	{
		EquippedClass = FCLWeaponClassDef();
		EquippedClass.Id = Item.DefinitionId;
		EquippedClass.DisplayName = Item.DisplayName;
		EquippedClass.Slot = Item.Weapon.Slot;
		EquippedClass.BaseStats = Item.BaseStats;
		EquippedClass.HipFov = Tune.DefaultHipFOV;
		EquippedClass.AdsFov = 70.f;
	}

	Overlay.SightId = !Item.SightId.IsNone() ? Item.SightId : EquippedClass.SightId;
	Overlay.ChargeHoldSeconds = 0.f;
	Overlay.BurstRemaining = 0;
	Overlay.bBurstActive = false;
	FireMode = CLMakeWeaponFireMode(EquippedClass.Fire.Mode);

	RestoreSlotAmmo(Item.Weapon.Slot);
	if (Item.Weapon.Slot == ECLWeaponSlot::Special && Overlay.SpecialReserve <= 0)
	{
		Overlay.SpecialReserve = FMath::Max(GetMagazineSize() * 3, 12);
	}
	bReady = false;
	ReadyTimeRemaining = ComputeReadySeconds() + ComputeStowSeconds() * 0.15f;
	bReloading = false;
	DisplayedAdsFov = ResolveSightAdsFov();

	if (Behavior)
	{
		Behavior->SetModifiers(ActiveItem.Modifiers);
	}
}

float UCLWeaponMotorComponent::ComputeAdsSeconds() const
{
	const float Handling = FMath::Clamp(ActiveItem.FinalStats.Handling + ActiveItem.FinalStats.AdsSpeed, 0.f, 1.5f);
	float T = FMath::GetMappedRangeValueClamped(FVector2D(0.f, 1.f), FVector2D(Tune.MaxADSSeconds, Tune.MinADSSeconds), Handling);
	if (const UCLGameInstance* GI = Cast<UCLGameInstance>(GetWorld() ? GetWorld()->GetGameInstance() : nullptr))
	{
		if (const UCLLootRulesService* Loot = GI->GetLootRulesService())
		{
			if (const FCLSightDef* Sight = Loot->FindSight(GetSightId()))
			{
				T += FMath::Max(0.f, Sight->AdsZoomSeconds);
			}
		}
	}
	return T * SlideHandlingScale();
}

float UCLWeaponMotorComponent::ComputeReadySeconds() const
{
	float Draw = ActiveItem.FinalStats.DrawSeconds;
	if (Draw <= 0.f)
	{
		const float Handling = FMath::Clamp(ActiveItem.FinalStats.Handling, 0.f, 1.5f);
		Draw = FMath::GetMappedRangeValueClamped(FVector2D(0.f, 1.f), FVector2D(Tune.MaxReadySeconds, Tune.MinReadySeconds), Handling);
	}
	if (EquippedClass.Stock == ECLWeaponStock::Brace)
	{
		Draw *= 1.25f;
	}
	else if (EquippedClass.Stock == ECLWeaponStock::Stock)
	{
		Draw *= 1.45f;
	}
	const float HandlingNudge = FMath::Lerp(1.08f, 0.92f, FMath::Clamp(ActiveItem.FinalStats.Handling, 0.f, 1.f));
	return FMath::Clamp(Draw * HandlingNudge * SlideHandlingScale(), Tune.MinReadySeconds, Tune.MaxReadySeconds);
}

float UCLWeaponMotorComponent::ComputeStowSeconds() const
{
	float Stow = ActiveItem.FinalStats.StowSeconds;
	if (Stow <= 0.f)
	{
		Stow = ComputeReadySeconds() * 0.85f;
	}
	if (EquippedClass.Stock == ECLWeaponStock::Brace)
	{
		Stow *= 1.2f;
	}
	else if (EquippedClass.Stock == ECLWeaponStock::Stock)
	{
		Stow *= 1.35f;
	}
	return FMath::Clamp(Stow * SlideHandlingScale(), Tune.MinStowSeconds, Tune.MaxStowSeconds);
}

float UCLWeaponMotorComponent::ComputeRangeDivergence() const
{
	const float Grip = FMath::Clamp(ActiveItem.FinalStats.Grip, 0.05f, 1.f);
	const float BarrelNorm = FMath::Clamp(ActiveItem.FinalStats.BarrelLengthCm / 50.f, 0.15f, 1.4f);
	return FMath::Lerp(1.35f, 0.72f, Grip) * FMath::Lerp(1.22f, 0.82f, BarrelNorm);
}

FVector2D UCLWeaponMotorComponent::ConsumeHipRecoil()
{
	const FVector2D Kick = PendingHipRecoil;
	PendingHipRecoil = FVector2D::ZeroVector;
	return Kick;
}

void UCLWeaponMotorComponent::QueueRecoilImpulse(TArray<FCLRecoilImpulse>& Into, FVector2D Degrees)
{
	if (Degrees.IsNearlyZero())
	{
		return;
	}
	FCLRecoilImpulse Pulse;
	Pulse.Degrees = Degrees;
	Pulse.Duration = FMath::Max(0.02f, Tune.RecoilImpulseSeconds);
	Into.Add(Pulse);
	if (Into.Num() > 12)
	{
		Into.RemoveAt(0, Into.Num() - 12);
	}
}

FVector2D UCLWeaponMotorComponent::DrainRecoilImpulses(TArray<FCLRecoilImpulse>& Impulses, float DeltaTime)
{
	FVector2D Out = FVector2D::ZeroVector;
	if (DeltaTime <= 0.f)
	{
		return Out;
	}
	for (int32 i = Impulses.Num() - 1; i >= 0; --i)
	{
		FCLRecoilImpulse& Pulse = Impulses[i];
		const float Dur = FMath::Max(0.02f, Pulse.Duration);
		const float Prev = Pulse.Age;
		Pulse.Age += DeltaTime;
		const float A0 = 1.f - FMath::Square(1.f - FMath::Clamp(Prev / Dur, 0.f, 1.f));
		const float A1 = 1.f - FMath::Square(1.f - FMath::Clamp(Pulse.Age / Dur, 0.f, 1.f));
		Out += Pulse.Degrees * (A1 - A0);
		if (Pulse.Age >= Dur)
		{
			Impulses.RemoveAtSwap(i);
		}
	}
	return Out;
}

void UCLWeaponMotorComponent::ApplyShotRecoil()
{
	float Grip = FMath::Clamp(ActiveItem.FinalStats.Grip, 0.05f, 1.f);
	float FlipScale = 1.f;
	if (EquippedClass.Stock == ECLWeaponStock::Brace)
	{
		Grip = FMath::Clamp(Grip + 0.15f, 0.05f, 1.f);
		FlipScale *= 0.75f;
	}
	else if (EquippedClass.Stock == ECLWeaponStock::Stock)
	{
		Grip = FMath::Clamp(Grip + 0.25f, 0.05f, 1.f);
		FlipScale *= 0.55f;
	}

	const float Mass = FMath::Max(0.25f, ActiveItem.FinalStats.MassKg);
	const float Grains = FMath::Max(0.f, ActiveItem.FinalStats.AmmoGrains);
	const float Vel = FMath::Max(0.f, ActiveItem.FinalStats.MuzzleVelocityMps);
	const float Momentum = (Grains * 6.479891e-5f) * Vel;
	float Flip = (Momentum > 0.01f) ? (4.2f * Momentum / Mass) : 1.1f;
	Flip *= (1.f - Grip * 0.35f);
	Flip *= (1.f - FMath::Clamp(ActiveItem.FinalStats.Compensator, 0.f, 1.f) * 0.4f);
	Flip *= FlipScale;
	Flip = FMath::Clamp(Flip, 0.05f, 18.f);
	const float Rpm = ActiveItem.FinalStats.Rpm > 0.f ? ActiveItem.FinalStats.Rpm : EquippedClass.BaseStats.Rpm;
	if (Rpm >= Tune.RecoilRpmKnee && Tune.RecoilRpmKnee > 0.f)
	{
		Flip *= Tune.RecoilRpmKnee / Rpm;
	}

	float Yaw = 0.f;
	if (Grip < 0.92f)
	{
		const float Sign = (FMath::FRand() < 0.5f) ? -1.f : 1.f;
		Yaw = Flip * (1.f - Grip) * Sign;
	}

	QueueRecoilImpulse(ViewKickImpulses, FVector2D(0.f, Flip));
	if (AdsAlpha > 0.4f)
	{
		QueueRecoilImpulse(AdsImpulses, FVector2D(Yaw * Tune.AdsRecoilScale, Flip * Tune.AdsRecoilScale));
	}
	else
	{
		QueueRecoilImpulse(HipImpulses, FVector2D(Yaw * Tune.HipRecoilScale, Flip * Tune.HipRecoilScale));
	}
}

int32 UCLWeaponMotorComponent::GetMagazineSize() const
{
	if (ActiveItem.FinalStats.Magazine > 0)
	{
		return ActiveItem.FinalStats.Magazine;
	}
	return EquippedClass.BaseStats.Magazine > 0 ? EquippedClass.BaseStats.Magazine : 30;
}

void UCLWeaponMotorComponent::SetWantsADS(bool bADS) { bWantsADS = bADS; }
void UCLWeaponMotorComponent::SetWantsFire(bool bFire) { bWantsFire = bFire; }

bool UCLWeaponMotorComponent::IsRadarHot() const
{
	const UWorld* World = GetWorld();
	return World && (World->GetTimeSeconds() - LastRadarFireTime) < 0.35f;
}

void UCLWeaponMotorComponent::StartReload()
{
	if (bReloading || !bReady)
	{
		return;
	}
	bReloading = true;
	const float ReloadStat = FMath::Clamp(ActiveItem.FinalStats.Reload, 0.f, 1.5f);
	float Seconds = FMath::GetMappedRangeValueClamped(FVector2D(0.f, 1.f), FVector2D(2.2f, 1.0f), ReloadStat);
	if (Behavior)
	{
		Seconds /= FMath::Max(0.5f, Behavior->GetReloadSpeedMultiplier());
	}
	ReloadTimeRemaining = Seconds;
}

void UCLWeaponMotorComponent::SwapToSlot(ECLWeaponSlot Slot)
{
	const FCLItemInstance& Target = (Slot == ECLWeaponSlot::Primary) ? PrimaryItem : SpecialItem;
	if (!Target.InstanceId.IsValid())
	{
		return;
	}
	EquipItem(Target, ResolveClassDef(Target));
}

bool UCLWeaponMotorComponent::HasLiveGrenade() const
{
	const ACLWeaponProjectile* Nade = LiveGrenade.Get();
	return Nade && Nade->IsLiveGrenade();
}

bool UCLWeaponMotorComponent::HasProxDetonate() const
{
	return Behavior && Behavior->HasProxDetonate();
}

bool UCLWeaponMotorComponent::DetonateLiveGrenade()
{
	if (ACLWeaponProjectile* Nade = LiveGrenade.Get())
	{
		if (Nade->IsLiveGrenade())
		{
			Nade->Detonate();
			LiveGrenade.Reset();
			return true;
		}
	}
	LiveGrenade.Reset();
	return false;
}

FVector UCLWeaponMotorComponent::GetMuzzleAim(FVector& OutStart) const
{
	return ComputeMuzzleAim(OutStart);
}

FVector UCLWeaponMotorComponent::GetBarrelLocation() const
{
	FVector AimStart = FVector::ZeroVector;
	ComputeMuzzleAim(AimStart);
	return ResolveTracerStart(AimStart);
}

FVector UCLWeaponMotorComponent::ComputeMuzzleAim(FVector& OutStart) const
{
	OutStart = FVector::ZeroVector;
	FRotator ViewRot = FRotator::ZeroRotator;
	if (const AActor* Owner = GetOwner())
	{
		OutStart = Owner->GetActorLocation() + FVector(0.f, 0.f, 60.f);
		ViewRot = Owner->GetActorRotation();
		if (const APawn* Pawn = Cast<APawn>(Owner))
		{
			if (const AController* Ctrl = Pawn->GetController())
			{
				ViewRot = Ctrl->GetControlRotation();
			}
		}
	}
	if (const ACLPlayerCharacter* Char = Cast<ACLPlayerCharacter>(GetOwner()))
	{
		const FVector2D Punch = Char->GetAdsReticlePunch();
		ViewRot.Yaw += Punch.X;
		ViewRot.Pitch += Punch.Y;
	}
	else if (AdsAlpha > 0.4f)
	{
		ViewRot.Yaw += AdsRecoilPunch.X;
		ViewRot.Pitch += AdsRecoilPunch.Y;
	}
	return ViewRot.Vector();
}

void UCLWeaponMotorComponent::SetSight(FName SightId)
{
	if (SightId.IsNone())
	{
		return;
	}
	bool bKnown = UCLLootRulesService::FindLoadedSight(SightId) != nullptr;
	if (!bKnown)
	{
		if (const UWorld* World = GetWorld())
		{
			if (const UCLGameInstance* GI = Cast<UCLGameInstance>(World->GetGameInstance()))
			{
				if (const UCLLootRulesService* Loot = GI->GetLootRulesService())
				{
					bKnown = Loot->IsKnownSight(SightId);
				}
			}
		}
	}
	if (!bKnown && SightId != FName(TEXT("iron")) && SightId != FName(TEXT("red_dot")) && SightId != FName(TEXT("scope")))
	{
		return;
	}
	ActiveItem.SightId = SightId;
	Overlay.SightId = SightId;
	if (ActiveItem.Weapon.Slot == ECLWeaponSlot::Primary)
	{
		PrimaryItem.SightId = SightId;
	}
	else
	{
		SpecialItem.SightId = SightId;
	}
}

FName UCLWeaponMotorComponent::GetSightId() const
{
	if (!Overlay.SightId.IsNone())
	{
		return Overlay.SightId;
	}
	if (!ActiveItem.SightId.IsNone())
	{
		return ActiveItem.SightId;
	}
	return EquippedClass.SightId.IsNone() ? FName(TEXT("red_dot")) : EquippedClass.SightId;
}

float UCLWeaponMotorComponent::ResolveSightAdsFov() const
{
	float Ads = EquippedClass.AdsFov > 0.f ? EquippedClass.AdsFov : 70.f;
	if (const UCLGameInstance* GI = Cast<UCLGameInstance>(GetWorld() ? GetWorld()->GetGameInstance() : nullptr))
	{
		if (const UCLLootRulesService* Loot = GI->GetLootRulesService())
		{
			if (const FCLSightDef* Sight = Loot->FindSight(GetSightId()))
			{
				Ads = Sight->AdsFov;
			}
		}
	}
	return Ads;
}

float UCLWeaponMotorComponent::GetCurrentFOV() const
{
	const float Hip = EquippedClass.HipFov > 0.f ? EquippedClass.HipFov : Tune.DefaultHipFOV;
	return FMath::Lerp(Hip, DisplayedAdsFov, GetAdsEase());
}

FRotator UCLWeaponMotorComponent::FireHitscan(const FVector& Start, FRotator ViewRot)
{
	AActor* Owner = GetOwner();
	if (!Owner || !GetWorld())
	{
		return ViewRot;
	}

	float AccMul = 1.f;
	if (Behavior)
	{
		AccMul *= Behavior->GetAccuracyMultiplier();
	}
	if (const ACLPlayerCharacter* Char = Cast<ACLPlayerCharacter>(Owner))
	{
		if (Char->IsSliding())
		{
			AccMul *= (1.f + Tune.SlideHandlingBonus);
			if (Behavior && IsADS())
			{
				AccMul *= (1.f + Behavior->GetSlideAdsAccuracyBonus());
			}
		}
	}

	if (!IsADS() && EquippedClass.NoScopeReliability < 1.f)
	{
		if (FMath::FRand() > EquippedClass.NoScopeReliability)
		{
			ViewRot.Pitch += FMath::FRandRange(-4.f, 4.f);
			ViewRot.Yaw += FMath::FRandRange(-4.f, 4.f);
		}
	}

	const float BaseSpread = IsADS() ? EquippedClass.AdsSpreadDeg : EquippedClass.HipSpreadDeg;
	const float Stab = FMath::Clamp(ActiveItem.FinalStats.Stability, 0.f, 1.f);
	const float Spread = FMath::Max(0.02f, BaseSpread * ComputeRangeDivergence() * (1.f - Stab * 0.55f) / FMath::Max(0.35f, AccMul));
	const float Theta = FMath::FRandRange(0.f, 2.f * PI);
	const float Radius = Spread * FMath::Sqrt(FMath::FRand());
	ViewRot.Yaw += Radius * FMath::Cos(Theta);
	ViewRot.Pitch += Radius * FMath::Sin(Theta);

	FCLHitscanRequest Req;
	Req.Start = Start;
	Req.View = ViewRot;
	Req.Damage = ActiveItem.FinalStats.Impact;
	Req.RangeStat = ActiveItem.FinalStats.Range;
	Req.CritMultiplier = EquippedClass.CritMultiplier;
	Req.SpreadDegrees = 0.f;
	Req.AimAssistConeDegrees = Tune.AimAssistConeDegrees;
	Req.AimAssistMagnetism = Tune.AimAssistMagnetism;
	Req.bInstantKillOnPrecision = EquippedClass.bInstantKillOnPrecision;
	Req.bRequireAdsForInstakill = true;
	Req.bIsAds = IsADS();
	Req.Behavior = Behavior;
	AController* Instigator = nullptr;
	if (APawn* PawnOwner = Cast<APawn>(Owner))
	{
		Instigator = PawnOwner->GetController();
	}
	FRotator ShotRot = ViewRot;
	CLHitscanService::Fire(GetWorld(), Owner, Instigator, Req, &ShotRot);
	return ShotRot;
}

void UCLWeaponMotorComponent::SpawnTracer(const FVector& Start, const FVector& Direction)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	const FVector Origin = ResolveTracerStart(Start);
	const FVector Dir = Direction.GetSafeNormal();
	const FVector SpawnAt = Origin + Dir * 8.f;
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Params.Owner = GetOwner();
	Params.Instigator = Cast<APawn>(GetOwner());
	if (ACLWeaponProjectile* Tracer = World->SpawnActor<ACLWeaponProjectile>(SpawnAt, Dir.Rotation(), Params))
	{
		Tracer->InitTracer(Cast<APawn>(GetOwner()), Dir, 42000.f, 0.18f);
	}
}

void UCLWeaponMotorComponent::SpawnCasing()
{
	UWorld* World = GetWorld();
	ACLPlayerCharacter* Char = Cast<ACLPlayerCharacter>(GetOwner());
	if (!World || !Char)
	{
		return;
	}
	USceneComponent* Root = Char->UsesViewWeapon() ? Char->GetViewWeaponRoot() : Char->GetWorldWeaponRoot();
	USceneComponent* Ejector = CLViewWeapon::FindEjector(Root);
	const FVector Loc = Ejector ? Ejector->GetComponentLocation() : Char->GetActorLocation() + FVector(20.f, 20.f, 50.f);
	const FRotator Rot = Ejector ? Ejector->GetComponentRotation() : Char->GetActorRotation();
	const FVector Right = Rot.RotateVector(FVector::RightVector);
	const FVector Up = Rot.RotateVector(FVector::UpVector);
	const FVector Back = Rot.RotateVector(-FVector::ForwardVector);
	const FVector Impulse = Right * FMath::FRandRange(180.f, 280.f) + Up * FMath::FRandRange(90.f, 160.f) + Back * FMath::FRandRange(20.f, 60.f);
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Params.Owner = Char;
	Params.Instigator = Char;
	if (ACLWeaponProjectile* Casing = World->SpawnActor<ACLWeaponProjectile>(Loc, Rot, Params))
	{
		Casing->InitCasing(Char, Impulse);
	}
}

FVector UCLWeaponMotorComponent::ResolveTracerStart(const FVector& AimStart) const
{
	const ACLPlayerCharacter* Char = Cast<ACLPlayerCharacter>(GetOwner());
	if (!Char)
	{
		return AimStart;
	}
	USceneComponent* Root = Char->UsesViewWeapon() ? Char->GetViewWeaponRoot() : Char->GetWorldWeaponRoot();
	if (USceneComponent* Muzzle = CLViewWeapon::FindMuzzle(Root))
	{
		return Muzzle->GetComponentLocation();
	}
	return AimStart;
}

void UCLWeaponMotorComponent::SpawnGrenade(const FVector& Start, const FVector& Direction)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	const FVector SpawnAt = Start + Direction.GetSafeNormal() * 50.f;
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Params.Owner = GetOwner();
	Params.Instigator = Cast<APawn>(GetOwner());
	if (ACLWeaponProjectile* Nade = World->SpawnActor<ACLWeaponProjectile>(SpawnAt, Direction.Rotation(), Params))
	{
		float Damage = ActiveItem.FinalStats.Impact;
		if (Behavior)
		{
			Damage *= Behavior->GetDamageMultiplier();
		}
		Nade->InitGrenade(Cast<APawn>(GetOwner()), Direction, 2200.f, 6.f, Damage, 280.f, HasProxDetonate(), 180.f);
		LiveGrenade = Nade;
	}
}

void UCLWeaponMotorComponent::TryFire(float DeltaTime)
{
	if (const APawn* OwnerPawn = Cast<APawn>(GetOwner()))
	{
		if (const ACLPlayerCharacter* Char = Cast<ACLPlayerCharacter>(OwnerPawn))
		{
			if (!Char->IsCombatAlive())
			{
				return;
			}
		}
	}
	if (HasLiveGrenade())
	{
		if (bAwaitingFireRelease)
		{
			return;
		}
		DetonateLiveGrenade();
		bAwaitingFireRelease = true;
		if (UWorld* World = GetWorld())
		{
			LastRadarFireTime = World->GetTimeSeconds();
		}
		return;
	}

	if (Overlay.AmmoInMag <= 0)
	{
		Overlay.bBurstActive = false;
		StartReload();
		return;
	}

	FCLFireCadenceIn In;
	In.bWantsFire = bWantsFire;
	In.bReady = bReady;
	In.bReloading = bReloading;
	In.DeltaTime = DeltaTime;
	In.Rpm = ActiveItem.FinalStats.Rpm > 0.f ? ActiveItem.FinalStats.Rpm : EquippedClass.BaseStats.Rpm;
	In.Fire = EquippedClass.Fire;

	FCLFireCadenceIO Io;
	Io.bAwaitingFireRelease = bAwaitingFireRelease;
	Io.FireCooldown = FireCooldown;
	Io.ChargeHoldSeconds = Overlay.ChargeHoldSeconds;
	Io.BurstRemaining = Overlay.BurstRemaining;
	Io.bBurstActive = Overlay.bBurstActive;

	const int32 Shots = FireMode ? FireMode->ConsumeFire(In, Io) : 0;
	bAwaitingFireRelease = Io.bAwaitingFireRelease;
	FireCooldown = Io.FireCooldown;
	Overlay.ChargeHoldSeconds = Io.ChargeHoldSeconds;
	Overlay.BurstRemaining = Io.BurstRemaining;
	Overlay.bBurstActive = Io.bBurstActive;
	if (Shots <= 0)
	{
		return;
	}
	FireShot();
}

void UCLWeaponMotorComponent::FireShot()
{
	if (Behavior)
	{
		Behavior->NotifyFired(true);
	}

	--Overlay.AmmoInMag;
	if (UWorld* World = GetWorld())
	{
		LastRadarFireTime = World->GetTimeSeconds();
	}
	if (ActiveItem.Weapon.Slot == ECLWeaponSlot::Primary)
	{
		Overlay.StoredPrimaryAmmo = Overlay.AmmoInMag;
	}
	else
	{
		Overlay.StoredSpecialAmmo = Overlay.AmmoInMag;
	}

	FVector Start = FVector::ZeroVector;
	const FVector Aim = ComputeMuzzleAim(Start);
	FRotator ViewRot = Aim.Rotation();

	if (UsesGrenadeProjectile())
	{
		SpawnGrenade(Start, Aim);
		return;
	}

	const int32 Pellets = EquippedClass.Fire.Mode == ECLWeaponFireMode::Pellet
		? FMath::Max(1, EquippedClass.Fire.PelletCount) : 1;
	FRotator ShotRot = ViewRot;
	for (int32 i = 0; i < Pellets; ++i)
	{
		ShotRot = FireHitscan(Start, ViewRot);
	}
	if (EquippedClass.ProjectileId == FName(TEXT("tracer")) || EquippedClass.ProjectileId.IsNone())
	{
		SpawnTracer(Start, ShotRot.Vector());
	}
	SpawnCasing();
	ApplyShotRecoil();
}

void UCLWeaponMotorComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bReady)
	{
		ReadyTimeRemaining -= DeltaTime;
		if (ReadyTimeRemaining <= 0.f)
		{
			bReady = true;
		}
	}

	if (bReloading)
	{
		ReloadTimeRemaining -= DeltaTime;
		if (ReloadTimeRemaining <= 0.f)
		{
			bReloading = false;
			const int32 Mag = GetMagazineSize();
			if (IsSpecialEquipped())
			{
				const int32 Need = FMath::Max(0, Mag - Overlay.AmmoInMag);
				const int32 Take = FMath::Min(Need, Overlay.SpecialReserve);
				Overlay.SpecialReserve -= Take;
				Overlay.AmmoInMag += Take;
			}
			else
			{
				Overlay.AmmoInMag = Mag;
			}
			if (IsSpecialEquipped())
			{
				Overlay.StoredSpecialAmmo = Overlay.AmmoInMag;
			}
			else
			{
				Overlay.StoredPrimaryAmmo = Overlay.AmmoInMag;
			}
		}
	}

	const float AdsSeconds = FMath::Max(0.05f, ComputeAdsSeconds());
	const float AdsRate = 1.f / AdsSeconds;
	AdsAlpha = FMath::Clamp(AdsAlpha + (bWantsADS ? AdsRate : -AdsRate) * DeltaTime, 0.f, 1.f);
	DisplayedAdsFov = FMath::FInterpTo(DisplayedAdsFov, ResolveSightAdsFov(), DeltaTime, 8.f);

	if (Camera)
	{
		Camera->SetFieldOfView(GetCurrentFOV());
	}

	FireCooldown = FMath::Max(0.f, FireCooldown - DeltaTime);
	if (!bWantsFire)
	{
		bAwaitingFireRelease = false;
		Overlay.ChargeHoldSeconds = 0.f;
	}
	if (bWantsFire || Overlay.bBurstActive)
	{
		TryFire(DeltaTime);
	}
	PendingHipRecoil += DrainRecoilImpulses(HipImpulses, DeltaTime);
	AdsRecoilPunch += DrainRecoilImpulses(AdsImpulses, DeltaTime);
	ViewKickPitch += DrainRecoilImpulses(ViewKickImpulses, DeltaTime).Y;
	const float AdsRecover = FMath::Max(1.f, Tune.RecoilAdsRecoverPerSecond);
	AdsRecoilPunch.X = FMath::FInterpTo(AdsRecoilPunch.X, 0.f, DeltaTime, AdsRecover);
	AdsRecoilPunch.Y = FMath::FInterpTo(AdsRecoilPunch.Y, 0.f, DeltaTime, AdsRecover);
	ViewKickPitch = FMath::FInterpTo(ViewKickPitch, 0.f, DeltaTime, FMath::Max(1.f, Tune.RecoilViewKickRecover));
}
