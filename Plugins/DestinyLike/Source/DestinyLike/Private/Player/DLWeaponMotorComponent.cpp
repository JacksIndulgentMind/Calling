#include "Player/DLWeaponMotorComponent.h"
#include "Combat/DLHitscanService.h"
#include "Player/DLWeaponBehaviorComponent.h"
#include "Player/DLPlayerCharacter.h"
#include "Player/DLViewWeapon.h"
#include "Weapon/DLWeaponProjectile.h"
#include "Game/DLGameInstance.h"
#include "Loot/DLLootRulesService.h"
#include "Camera/CameraComponent.h"
#include "Misc/ConfigCacheIni.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameFramework/PlayerController.h"
#include "DrawDebugHelpers.h"

UDLWeaponMotorComponent::UDLWeaponMotorComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UDLWeaponMotorComponent::BeginPlay()
{
	Super::BeginPlay();
	ReloadSettings();
}

void UDLWeaponMotorComponent::ReloadSettings()
{
	Tune.LoadFromIni();
}

bool UDLWeaponMotorComponent::IsOwnerSliding() const
{
	if (const ADLPlayerCharacter* Char = Cast<ADLPlayerCharacter>(GetOwner()))
	{
		return Char->IsSliding();
	}
	return false;
}

float UDLWeaponMotorComponent::SlideHandlingScale() const
{
	return IsOwnerSliding() ? FMath::Max(0.5f, 1.f - Tune.SlideHandlingBonus) : 1.f;
}

const FDLWeaponClassDef* UDLWeaponMotorComponent::ResolveClassDef(const FDLItemInstance& Item) const
{
	if (const UWorld* World = GetWorld())
	{
		if (const UGameInstance* GI = World->GetGameInstance())
		{
			if (const UDLGameInstance* DLGI = Cast<UDLGameInstance>(GI))
			{
				if (const UDLLootRulesService* Loot = DLGI->GetLootRulesService())
				{
					return Loot->FindWeaponClass(Item.DefinitionId);
				}
			}
		}
	}
	return nullptr;
}

void UDLWeaponMotorComponent::StoreActiveAmmo()
{
	if (!ActiveItem.InstanceId.IsValid())
	{
		return;
	}
	if (ActiveItem.Weapon.Slot == EDLWeaponSlot::Primary)
	{
		StoredPrimaryAmmo = AmmoInMag;
	}
	else
	{
		StoredSpecialAmmo = AmmoInMag;
	}
}

void UDLWeaponMotorComponent::RestoreSlotAmmo(EDLWeaponSlot Slot)
{
	const int32 Stored = (Slot == EDLWeaponSlot::Primary) ? StoredPrimaryAmmo : StoredSpecialAmmo;
	if (Stored >= 0)
	{
		AmmoInMag = Stored;
		return;
	}
	AmmoInMag = GetMagazineSize();
	if (Slot == EDLWeaponSlot::Primary)
	{
		StoredPrimaryAmmo = AmmoInMag;
	}
	else
	{
		StoredSpecialAmmo = AmmoInMag;
	}
}

void UDLWeaponMotorComponent::EquipItemByClassId(const FDLItemInstance& Item, FName WeaponClassId)
{
	const FDLWeaponClassDef* ClassDef = ResolveClassDef(Item);
	if (!WeaponClassId.IsNone())
	{
		if (const UWorld* World = GetWorld())
		{
			if (const UGameInstance* GI = World->GetGameInstance())
			{
				if (const UDLGameInstance* DLGI = Cast<UDLGameInstance>(GI))
				{
					if (const UDLLootRulesService* Loot = DLGI->GetLootRulesService())
					{
						if (const FDLWeaponClassDef* Forced = Loot->FindWeaponClass(WeaponClassId))
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

void UDLWeaponMotorComponent::EquipItem(const FDLItemInstance& Item, const FDLWeaponClassDef* ClassDef)
{
	StoreActiveAmmo();

	ActiveItem = Item;
	if (Item.Weapon.Slot == EDLWeaponSlot::Primary)
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
		ActiveClass = *ClassDef;
	}
	else
	{
		ActiveClass = FDLWeaponClassDef();
		ActiveClass.Id = Item.DefinitionId;
		ActiveClass.DisplayName = Item.DisplayName;
		ActiveClass.Slot = Item.Weapon.Slot;
		ActiveClass.BaseStats = Item.BaseStats;
		ActiveClass.HipFov = Tune.DefaultHipFOV;
		ActiveClass.AdsFov = 70.f;
	}
	if (!Item.SightId.IsNone())
	{
		ActiveClass.SightId = Item.SightId;
	}

	RestoreSlotAmmo(Item.Weapon.Slot);
	if (Item.Weapon.Slot == EDLWeaponSlot::Special && SpecialReserve <= 0)
	{
		SpecialReserve = FMath::Max(GetMagazineSize() * 3, 12);
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

float UDLWeaponMotorComponent::ComputeAdsSeconds() const
{
	const float Handling = FMath::Clamp(ActiveItem.FinalStats.Handling + ActiveItem.FinalStats.AdsSpeed, 0.f, 1.5f);
	float T = FMath::GetMappedRangeValueClamped(FVector2D(0.f, 1.f), FVector2D(Tune.MaxADSSeconds, Tune.MinADSSeconds), Handling);
	if (const UDLGameInstance* GI = Cast<UDLGameInstance>(GetWorld() ? GetWorld()->GetGameInstance() : nullptr))
	{
		if (const UDLLootRulesService* Loot = GI->GetLootRulesService())
		{
			if (const FDLSightDef* Sight = Loot->FindSight(GetSightId()))
			{
				T += FMath::Max(0.f, Sight->AdsZoomSeconds);
			}
		}
	}
	return T * SlideHandlingScale();
}

float UDLWeaponMotorComponent::ComputeReadySeconds() const
{
	float Draw = ActiveItem.FinalStats.DrawSeconds;
	if (Draw <= 0.f)
	{
		const float Handling = FMath::Clamp(ActiveItem.FinalStats.Handling, 0.f, 1.5f);
		Draw = FMath::GetMappedRangeValueClamped(FVector2D(0.f, 1.f), FVector2D(Tune.MaxReadySeconds, Tune.MinReadySeconds), Handling);
	}
	if (ActiveClass.Stock == EDLWeaponStock::Brace)
	{
		Draw *= 1.25f;
	}
	else if (ActiveClass.Stock == EDLWeaponStock::Stock)
	{
		Draw *= 1.45f;
	}
	const float HandlingNudge = FMath::Lerp(1.08f, 0.92f, FMath::Clamp(ActiveItem.FinalStats.Handling, 0.f, 1.f));
	return FMath::Clamp(Draw * HandlingNudge * SlideHandlingScale(), Tune.MinReadySeconds, Tune.MaxReadySeconds);
}

float UDLWeaponMotorComponent::ComputeStowSeconds() const
{
	float Stow = ActiveItem.FinalStats.StowSeconds;
	if (Stow <= 0.f)
	{
		Stow = ComputeReadySeconds() * 0.85f;
	}
	if (ActiveClass.Stock == EDLWeaponStock::Brace)
	{
		Stow *= 1.2f;
	}
	else if (ActiveClass.Stock == EDLWeaponStock::Stock)
	{
		Stow *= 1.35f;
	}
	return FMath::Clamp(Stow * SlideHandlingScale(), Tune.MinStowSeconds, Tune.MaxStowSeconds);
}

float UDLWeaponMotorComponent::ComputeRangeDivergence() const
{
	const float Grip = FMath::Clamp(ActiveItem.FinalStats.Grip, 0.05f, 1.f);
	const float BarrelNorm = FMath::Clamp(ActiveItem.FinalStats.BarrelLengthCm / 50.f, 0.15f, 1.4f);
	return FMath::Lerp(1.35f, 0.72f, Grip) * FMath::Lerp(1.22f, 0.82f, BarrelNorm);
}

FVector2D UDLWeaponMotorComponent::ConsumeHipRecoil()
{
	const FVector2D Kick = PendingHipRecoil;
	PendingHipRecoil = FVector2D::ZeroVector;
	return Kick;
}

void UDLWeaponMotorComponent::QueueRecoilImpulse(TArray<FDLRecoilImpulse>& Into, FVector2D Degrees)
{
	if (Degrees.IsNearlyZero())
	{
		return;
	}
	FDLRecoilImpulse Pulse;
	Pulse.Degrees = Degrees;
	Pulse.Duration = FMath::Max(0.02f, Tune.RecoilImpulseSeconds);
	Into.Add(Pulse);
	if (Into.Num() > 12)
	{
		Into.RemoveAt(0, Into.Num() - 12);
	}
}

FVector2D UDLWeaponMotorComponent::DrainRecoilImpulses(TArray<FDLRecoilImpulse>& Impulses, float DeltaTime)
{
	FVector2D Out = FVector2D::ZeroVector;
	if (DeltaTime <= 0.f)
	{
		return Out;
	}
	for (int32 i = Impulses.Num() - 1; i >= 0; --i)
	{
		FDLRecoilImpulse& Pulse = Impulses[i];
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

void UDLWeaponMotorComponent::ApplyShotRecoil()
{
	float Grip = FMath::Clamp(ActiveItem.FinalStats.Grip, 0.05f, 1.f);
	float FlipScale = 1.f;
	if (ActiveClass.Stock == EDLWeaponStock::Brace)
	{
		Grip = FMath::Clamp(Grip + 0.15f, 0.05f, 1.f);
		FlipScale *= 0.75f;
	}
	else if (ActiveClass.Stock == EDLWeaponStock::Stock)
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
	const float Rpm = ActiveItem.FinalStats.Rpm > 0.f ? ActiveItem.FinalStats.Rpm : ActiveClass.BaseStats.Rpm;
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

int32 UDLWeaponMotorComponent::GetMagazineSize() const
{
	if (ActiveItem.FinalStats.Magazine > 0)
	{
		return ActiveItem.FinalStats.Magazine;
	}
	return ActiveClass.BaseStats.Magazine > 0 ? ActiveClass.BaseStats.Magazine : 30;
}

void UDLWeaponMotorComponent::SetWantsADS(bool bADS) { bWantsADS = bADS; }
void UDLWeaponMotorComponent::SetWantsFire(bool bFire) { bWantsFire = bFire; }

bool UDLWeaponMotorComponent::IsRadarHot() const
{
	const UWorld* World = GetWorld();
	return World && (World->GetTimeSeconds() - LastRadarFireTime) < 0.35f;
}

void UDLWeaponMotorComponent::StartReload()
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

void UDLWeaponMotorComponent::SwapToSlot(EDLWeaponSlot Slot)
{
	const FDLItemInstance& Target = (Slot == EDLWeaponSlot::Primary) ? PrimaryItem : SpecialItem;
	if (!Target.InstanceId.IsValid())
	{
		return;
	}
	EquipItem(Target, ResolveClassDef(Target));
}

bool UDLWeaponMotorComponent::HasLiveGrenade() const
{
	const ADLWeaponProjectile* Nade = LiveGrenade.Get();
	return Nade && Nade->IsLiveGrenade();
}

bool UDLWeaponMotorComponent::HasProxDetonate() const
{
	for (const FDLModifierRoll& Mod : ActiveItem.Modifiers)
	{
		if (Mod.BehaviorId == FName(TEXT("prox_detonate")))
		{
			return true;
		}
	}
	return false;
}

bool UDLWeaponMotorComponent::DetonateLiveGrenade()
{
	if (ADLWeaponProjectile* Nade = LiveGrenade.Get())
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

FVector UDLWeaponMotorComponent::GetMuzzleAim(FVector& OutStart) const
{
	return ComputeMuzzleAim(OutStart);
}

FVector UDLWeaponMotorComponent::GetBarrelLocation() const
{
	FVector AimStart = FVector::ZeroVector;
	ComputeMuzzleAim(AimStart);
	return ResolveTracerStart(AimStart);
}

FVector UDLWeaponMotorComponent::ComputeMuzzleAim(FVector& OutStart) const
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
	if (const ADLPlayerCharacter* Char = Cast<ADLPlayerCharacter>(GetOwner()))
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

void UDLWeaponMotorComponent::SetSight(FName SightId)
{
	if (SightId != FName(TEXT("iron")) && SightId != FName(TEXT("red_dot")) && SightId != FName(TEXT("scope")))
	{
		return;
	}
	ActiveItem.SightId = SightId;
	ActiveClass.SightId = SightId;
	if (ActiveItem.Weapon.Slot == EDLWeaponSlot::Primary)
	{
		PrimaryItem.SightId = SightId;
	}
	else
	{
		SpecialItem.SightId = SightId;
	}
}

FName UDLWeaponMotorComponent::GetSightId() const
{
	if (!ActiveItem.SightId.IsNone())
	{
		return ActiveItem.SightId;
	}
	return ActiveClass.SightId.IsNone() ? FName(TEXT("red_dot")) : ActiveClass.SightId;
}

float UDLWeaponMotorComponent::ResolveSightAdsFov() const
{
	float Ads = ActiveClass.AdsFov > 0.f ? ActiveClass.AdsFov : 70.f;
	if (const UDLGameInstance* GI = Cast<UDLGameInstance>(GetWorld() ? GetWorld()->GetGameInstance() : nullptr))
	{
		if (const UDLLootRulesService* Loot = GI->GetLootRulesService())
		{
			if (const FDLSightDef* Sight = Loot->FindSight(GetSightId()))
			{
				Ads = Sight->AdsFov;
			}
		}
	}
	return Ads;
}

float UDLWeaponMotorComponent::GetCurrentFOV() const
{
	const float Hip = ActiveClass.HipFov > 0.f ? ActiveClass.HipFov : Tune.DefaultHipFOV;
	return FMath::Lerp(Hip, DisplayedAdsFov, GetAdsEase());
}

FRotator UDLWeaponMotorComponent::FireHitscan(const FVector& Start, FRotator ViewRot)
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
	if (const ADLPlayerCharacter* Char = Cast<ADLPlayerCharacter>(Owner))
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

	if (!IsADS() && ActiveClass.NoScopeReliability < 1.f)
	{
		if (FMath::FRand() > ActiveClass.NoScopeReliability)
		{
			ViewRot.Pitch += FMath::FRandRange(-4.f, 4.f);
			ViewRot.Yaw += FMath::FRandRange(-4.f, 4.f);
		}
	}

	const float BaseSpread = IsADS() ? ActiveClass.AdsSpreadDeg : ActiveClass.HipSpreadDeg;
	const float Stab = FMath::Clamp(ActiveItem.FinalStats.Stability, 0.f, 1.f);
	const float Spread = FMath::Max(0.02f, BaseSpread * ComputeRangeDivergence() * (1.f - Stab * 0.55f) / FMath::Max(0.35f, AccMul));
	const float Theta = FMath::FRandRange(0.f, 2.f * PI);
	const float Radius = Spread * FMath::Sqrt(FMath::FRand());
	ViewRot.Yaw += Radius * FMath::Cos(Theta);
	ViewRot.Pitch += Radius * FMath::Sin(Theta);

	FDLHitscanRequest Req;
	Req.Start = Start;
	Req.View = ViewRot;
	Req.Damage = ActiveItem.FinalStats.Impact;
	Req.RangeStat = ActiveItem.FinalStats.Range;
	Req.CritMultiplier = ActiveClass.CritMultiplier;
	Req.SpreadDegrees = 0.f;
	Req.AimAssistConeDegrees = Tune.AimAssistConeDegrees;
	Req.AimAssistMagnetism = Tune.AimAssistMagnetism;
	Req.bInstantKillOnPrecision = ActiveClass.bInstantKillOnPrecision;
	Req.bRequireAdsForInstakill = true;
	Req.bIsAds = IsADS();
	Req.Behavior = Behavior;
	AController* Instigator = nullptr;
	if (APawn* PawnOwner = Cast<APawn>(Owner))
	{
		Instigator = PawnOwner->GetController();
	}
	FRotator ShotRot = ViewRot;
	DLHitscanService::Fire(GetWorld(), Owner, Instigator, Req, &ShotRot);
	return ShotRot;
}

void UDLWeaponMotorComponent::SpawnTracer(const FVector& Start, const FVector& Direction)
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
	if (ADLWeaponProjectile* Tracer = World->SpawnActor<ADLWeaponProjectile>(SpawnAt, Dir.Rotation(), Params))
	{
		Tracer->InitTracer(Cast<APawn>(GetOwner()), Dir, 42000.f, 0.18f);
	}
}

void UDLWeaponMotorComponent::SpawnCasing()
{
	UWorld* World = GetWorld();
	ADLPlayerCharacter* Char = Cast<ADLPlayerCharacter>(GetOwner());
	if (!World || !Char)
	{
		return;
	}
	USceneComponent* Root = Char->UsesViewWeapon() ? Char->GetViewWeaponRoot() : Char->GetWorldWeaponRoot();
	USceneComponent* Ejector = DLViewWeapon::FindEjector(Root);
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
	if (ADLWeaponProjectile* Casing = World->SpawnActor<ADLWeaponProjectile>(Loc, Rot, Params))
	{
		Casing->InitCasing(Char, Impulse);
	}
}

FVector UDLWeaponMotorComponent::ResolveTracerStart(const FVector& AimStart) const
{
	const ADLPlayerCharacter* Char = Cast<ADLPlayerCharacter>(GetOwner());
	if (!Char)
	{
		return AimStart;
	}
	USceneComponent* Root = Char->UsesViewWeapon() ? Char->GetViewWeaponRoot() : Char->GetWorldWeaponRoot();
	if (USceneComponent* Muzzle = DLViewWeapon::FindMuzzle(Root))
	{
		return Muzzle->GetComponentLocation();
	}
	return AimStart;
}

void UDLWeaponMotorComponent::SpawnGrenade(const FVector& Start, const FVector& Direction)
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
	if (ADLWeaponProjectile* Nade = World->SpawnActor<ADLWeaponProjectile>(SpawnAt, Direction.Rotation(), Params))
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

void UDLWeaponMotorComponent::TryFire()
{
	if (const APawn* OwnerPawn = Cast<APawn>(GetOwner()))
	{
		if (const ADLPlayerCharacter* Char = Cast<ADLPlayerCharacter>(OwnerPawn))
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

	if (!bReady || bReloading || FireCooldown > 0.f || AmmoInMag <= 0)
	{
		if (AmmoInMag <= 0)
		{
			StartReload();
		}
		return;
	}

	if (Behavior)
	{
		Behavior->NotifyFired(true);
	}

	--AmmoInMag;
	if (UWorld* World = GetWorld())
	{
		LastRadarFireTime = World->GetTimeSeconds();
	}
	if (ActiveItem.Weapon.Slot == EDLWeaponSlot::Primary)
	{
		StoredPrimaryAmmo = AmmoInMag;
	}
	else
	{
		StoredSpecialAmmo = AmmoInMag;
	}

	const float Rpm = ActiveItem.FinalStats.Rpm > 0.f ? ActiveItem.FinalStats.Rpm : ActiveClass.BaseStats.Rpm;
	FireCooldown = Rpm > 0.f ? (60.f / Rpm) : ActiveClass.Fire.ChargeSeconds;

	FVector Start = FVector::ZeroVector;
	const FVector Aim = ComputeMuzzleAim(Start);
	FRotator ViewRot = Aim.Rotation();

	if (UsesGrenadeProjectile())
	{
		SpawnGrenade(Start, Aim);
		bAwaitingFireRelease = true;
		return;
	}

	const int32 Pellets = ActiveClass.Fire.Mode == EDLWeaponFireMode::Pellet
		? FMath::Max(1, ActiveClass.Fire.PelletCount) : 1;
	FRotator ShotRot = ViewRot;
	for (int32 i = 0; i < Pellets; ++i)
	{
		ShotRot = FireHitscan(Start, ViewRot);
	}
	if (ActiveClass.ProjectileId == FName(TEXT("tracer")) || ActiveClass.ProjectileId.IsNone())
	{
		SpawnTracer(Start, ShotRot.Vector());
	}
	SpawnCasing();
	ApplyShotRecoil();
}

void UDLWeaponMotorComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
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
				const int32 Need = FMath::Max(0, Mag - AmmoInMag);
				const int32 Take = FMath::Min(Need, SpecialReserve);
				SpecialReserve -= Take;
				AmmoInMag += Take;
			}
			else
			{
				AmmoInMag = Mag;
			}
			if (IsSpecialEquipped())
			{
				StoredSpecialAmmo = AmmoInMag;
			}
			else
			{
				StoredPrimaryAmmo = AmmoInMag;
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
	}
	if (bWantsFire)
	{
		TryFire();
	}
	PendingHipRecoil += DrainRecoilImpulses(HipImpulses, DeltaTime);
	AdsRecoilPunch += DrainRecoilImpulses(AdsImpulses, DeltaTime);
	ViewKickPitch += DrainRecoilImpulses(ViewKickImpulses, DeltaTime).Y;
	const float AdsRecover = FMath::Max(1.f, Tune.RecoilAdsRecoverPerSecond);
	AdsRecoilPunch.X = FMath::FInterpTo(AdsRecoilPunch.X, 0.f, DeltaTime, AdsRecover);
	AdsRecoilPunch.Y = FMath::FInterpTo(AdsRecoilPunch.Y, 0.f, DeltaTime, AdsRecover);
	ViewKickPitch = FMath::FInterpTo(ViewKickPitch, 0.f, DeltaTime, FMath::Max(1.f, Tune.RecoilViewKickRecover));
}
