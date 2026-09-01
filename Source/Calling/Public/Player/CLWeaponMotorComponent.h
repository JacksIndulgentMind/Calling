#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Loot/CLItemInstance.h"
#include "Loot/CLLootRulesService.h"
#include "Core/CLTunes.h"
#include "Weapon/CLWeaponFireMode.h"
#include "CLWeaponMotorComponent.generated.h"

class UCLWeaponBehaviorComponent;
class UCameraComponent;
class ACLWeaponProjectile;

struct FCLRecoilImpulse
{
	FVector2D Degrees = FVector2D::ZeroVector;
	float Age = 0.f;
	float Duration = 0.09f;
};

USTRUCT()
struct FCLWeaponRuntimeOverlay
{
	GENERATED_BODY()

	UPROPERTY()
	FName SightId = NAME_None;

	UPROPERTY()
	int32 AmmoInMag = 30;

	UPROPERTY()
	int32 SpecialReserve = 0;

	UPROPERTY()
	int32 StoredPrimaryAmmo = -1;

	UPROPERTY()
	int32 StoredSpecialAmmo = -1;

	UPROPERTY()
	float ChargeHoldSeconds = 0.f;

	UPROPERTY()
	int32 BurstRemaining = 0;

	UPROPERTY()
	bool bBurstActive = false;
};

/**
 * Gun motor: ready/stow/ADS timing from Handling, fire cadence, FOV blend, flinch consume.
 * Day-one feel stays inside Min/Max ADS/Ready bands from config.
 */
UCLASS(ClassGroup = (Calling), meta = (BlueprintSpawnableComponent))
class CALLING_API UCLWeaponMotorComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCLWeaponMotorComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** C++ only — UHT forbids Blueprint-exposed pointers to USTRUCTs. */
	void EquipItem(const FCLItemInstance& Item, const FCLWeaponClassDef* ClassDef);

	UFUNCTION(BlueprintCallable, Category = "Calling|Weapon")
	void EquipItemByClassId(const FCLItemInstance& Item, FName WeaponClassId);

	UFUNCTION(BlueprintCallable, Category = "Calling|Weapon")
	void SetWantsADS(bool bADS);

	UFUNCTION(BlueprintCallable, Category = "Calling|Weapon")
	void SetWantsFire(bool bFire);

	/** Authority hitscan using an already-spread view. Guest FireHitscan RPCs here. */
	FRotator AuthorityFireHitscan(const FVector& Start, FRotator ViewRot, bool bIsAds);

	void AuthoritySpawnGrenade(const FVector& Start, const FVector& Direction);
	void AuthorityDetonateGrenade();
	void PlayHitscanFX(const FVector& Start, const FVector& Direction);
	void NotePredictedLiveGrenade(bool bLive);

	UFUNCTION(BlueprintCallable, Category = "Calling|Weapon")
	void StartReload();

	UFUNCTION(BlueprintCallable, Category = "Calling|Weapon")
	void SwapToSlot(ECLWeaponSlot Slot);

	UFUNCTION(BlueprintPure, Category = "Calling|Weapon")
	bool IsADS() const { return AdsAlpha >= 0.95f; }

	UFUNCTION(BlueprintPure, Category = "Calling|Weapon")
	float GetAdsAlpha() const { return AdsAlpha; }

	UFUNCTION(BlueprintPure, Category = "Calling|Weapon")
	float GetAdsEase() const { return FMath::SmoothStep(0.f, 1.f, AdsAlpha); }

	UFUNCTION(BlueprintPure, Category = "Calling|Weapon")
	float GetCurrentFOV() const;

	UFUNCTION(BlueprintPure, Category = "Calling|Weapon")
	float GetAdsMovePenalty() const { return EquippedClass.AdsMovePenalty; }

	bool IsRadarHot() const;

	UFUNCTION(BlueprintPure, Category = "Calling|Weapon")
	const FCLItemInstance& GetActiveItem() const { return ActiveItem; }

	UFUNCTION(BlueprintPure, Category = "Calling|Weapon")
	int32 GetAmmoInMag() const { return Overlay.AmmoInMag; }

	UFUNCTION(BlueprintPure, Category = "Calling|Weapon")
	int32 GetMagazineSize() const;

	UFUNCTION(BlueprintPure, Category = "Calling|Weapon")
	int32 GetSpecialReserve() const { return Overlay.SpecialReserve; }

	UFUNCTION(BlueprintPure, Category = "Calling|Weapon")
	bool IsSpecialEquipped() const { return ActiveItem.Weapon.Slot == ECLWeaponSlot::Special; }

	UFUNCTION(BlueprintCallable, Category = "Calling|Weapon")
	void SetSight(FName SightId);

	UFUNCTION(BlueprintPure, Category = "Calling|Weapon")
	FName GetSightId() const;

	UFUNCTION(BlueprintPure, Category = "Calling|Weapon")
	FName GetClassBandId() const;

	UFUNCTION(BlueprintPure, Category = "Calling|Weapon")
	ECLWeaponStock GetEquippedStock() const { return EquippedClass.Stock; }

	UFUNCTION(BlueprintPure, Category = "Calling|Weapon")
	FName GetProjectileId() const { return EquippedClass.ProjectileId; }

	UFUNCTION(BlueprintCallable, Category = "Calling|Weapon")
	FVector2D ConsumeHipRecoil();

	UFUNCTION(BlueprintPure, Category = "Calling|Weapon")
	FVector2D GetAdsRecoilPunch() const { return AdsRecoilPunch; }

	/** Camera-forward plus ADS punch/flinch so HUD pip, hitscan, and tracer share one ray. */
	FVector GetMuzzleAim(FVector& OutStart) const;

	/** World location of the visible muzzle; falls back to aim start. */
	FVector GetBarrelLocation() const;

	UFUNCTION(BlueprintPure, Category = "Calling|Weapon")
	float GetViewKickPitch() const { return ViewKickPitch; }

	UFUNCTION(BlueprintPure, Category = "Calling|Weapon")
	bool UsesGrenadeProjectile() const { return EquippedClass.Fire.Mode == ECLWeaponFireMode::Grenade; }

	UFUNCTION(BlueprintPure, Category = "Calling|Weapon")
	bool HasLiveGrenade() const;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Weapon")
	TObjectPtr<UCLWeaponBehaviorComponent> Behavior = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Weapon")
	TObjectPtr<UCameraComponent> Camera = nullptr;

protected:
	void ReloadSettings();
	void TryFire(float DeltaTime);
	void FireShot();
	FRotator FireHitscan(const FVector& Start, FRotator ViewRot);
	void SpawnTracer(const FVector& Start, const FVector& Direction);
	void SpawnCasing();
	FVector ResolveTracerStart(const FVector& AimStart) const;
	void SpawnGrenade(const FVector& Start, const FVector& Direction);
	bool DetonateLiveGrenade();
	bool HasProxDetonate() const;
	const FCLWeaponClassDef* ResolveClassDef(const FCLItemInstance& Item) const;
	void StoreActiveAmmo();
	void RestoreSlotAmmo(ECLWeaponSlot Slot);
	FVector ComputeMuzzleAim(FVector& OutStart) const;
	float ComputeAdsSeconds() const;
	float ComputeReadySeconds() const;
	float ComputeStowSeconds() const;
	float ComputeRangeDivergence() const;
	void ApplyShotRecoil();
	void QueueRecoilImpulse(TArray<FCLRecoilImpulse>& Into, FVector2D Degrees);
	FVector2D DrainRecoilImpulses(TArray<FCLRecoilImpulse>& Impulses, float DeltaTime);
	float ResolveSightAdsFov() const;
	bool IsOwnerSliding() const;
	float SlideHandlingScale() const;

	UPROPERTY()
	FCLItemInstance ActiveItem;

	UPROPERTY()
	FCLItemInstance PrimaryItem;

	UPROPERTY()
	FCLItemInstance SpecialItem;

	UPROPERTY()
	FCLWeaponClassDef EquippedClass;

	UPROPERTY()
	FCLWeaponRuntimeOverlay Overlay;

	TSharedPtr<ICLWeaponFireMode> FireMode;

	mutable FCLWeaponClassDef ResolveScratch;

	bool bWantsADS = false;
	bool bWantsFire = false;
	bool bAwaitingFireRelease = false;
	bool bReloading = false;
	bool bReady = true;
	float AdsAlpha = 0.f;
	float DisplayedAdsFov = 70.f;
	float FireCooldown = 0.f;
	float LastRadarFireTime = -1000.f;
	float ReloadTimeRemaining = 0.f;
	float ReadyTimeRemaining = 0.f;
	FVector2D PendingHipRecoil = FVector2D::ZeroVector;
	FVector2D AdsRecoilPunch = FVector2D::ZeroVector;
	float ViewKickPitch = 0.f;
	TArray<FCLRecoilImpulse> HipImpulses;
	TArray<FCLRecoilImpulse> AdsImpulses;
	TArray<FCLRecoilImpulse> ViewKickImpulses;

	UPROPERTY()
	TWeakObjectPtr<ACLWeaponProjectile> LiveGrenade;

	bool bPredictLiveGrenade = false;

	UPROPERTY()
	FCLWeaponMotorTune Tune;
};
