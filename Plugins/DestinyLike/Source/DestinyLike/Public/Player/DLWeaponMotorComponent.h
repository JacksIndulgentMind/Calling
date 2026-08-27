#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Loot/DLItemInstance.h"
#include "Loot/DLLootRulesService.h"
#include "Core/DLTunes.h"
#include "DLWeaponMotorComponent.generated.h"

class UDLWeaponBehaviorComponent;
class UCameraComponent;
class ADLWeaponProjectile;

struct FDLRecoilImpulse
{
	FVector2D Degrees = FVector2D::ZeroVector;
	float Age = 0.f;
	float Duration = 0.09f;
};

/**
 * Gun motor: ready/stow/ADS timing from Handling, fire cadence, FOV blend, flinch consume.
 * Day-one feel stays inside Min/Max ADS/Ready bands from config.
 */
UCLASS(ClassGroup = (DestinyLike), meta = (BlueprintSpawnableComponent))
class DESTINYLIKE_API UDLWeaponMotorComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDLWeaponMotorComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** C++ only — UHT forbids Blueprint-exposed pointers to USTRUCTs. */
	void EquipItem(const FDLItemInstance& Item, const FDLWeaponClassDef* ClassDef);

	UFUNCTION(BlueprintCallable, Category = "DestinyLike|Weapon")
	void EquipItemByClassId(const FDLItemInstance& Item, FName WeaponClassId);

	UFUNCTION(BlueprintCallable, Category = "DestinyLike|Weapon")
	void SetWantsADS(bool bADS);

	UFUNCTION(BlueprintCallable, Category = "DestinyLike|Weapon")
	void SetWantsFire(bool bFire);

	UFUNCTION(BlueprintCallable, Category = "DestinyLike|Weapon")
	void StartReload();

	UFUNCTION(BlueprintCallable, Category = "DestinyLike|Weapon")
	void SwapToSlot(EDLWeaponSlot Slot);

	UFUNCTION(BlueprintPure, Category = "DestinyLike|Weapon")
	bool IsADS() const { return AdsAlpha >= 0.95f; }

	UFUNCTION(BlueprintPure, Category = "DestinyLike|Weapon")
	float GetAdsAlpha() const { return AdsAlpha; }

	UFUNCTION(BlueprintPure, Category = "DestinyLike|Weapon")
	float GetAdsEase() const { return FMath::SmoothStep(0.f, 1.f, AdsAlpha); }

	UFUNCTION(BlueprintPure, Category = "DestinyLike|Weapon")
	float GetCurrentFOV() const;

	UFUNCTION(BlueprintPure, Category = "DestinyLike|Weapon")
	float GetAdsMovePenalty() const { return ActiveClass.AdsMovePenalty; }

	bool IsRadarHot() const;

	UFUNCTION(BlueprintPure, Category = "DestinyLike|Weapon")
	const FDLItemInstance& GetActiveItem() const { return ActiveItem; }

	UFUNCTION(BlueprintPure, Category = "DestinyLike|Weapon")
	int32 GetAmmoInMag() const { return AmmoInMag; }

	UFUNCTION(BlueprintPure, Category = "DestinyLike|Weapon")
	int32 GetMagazineSize() const;

	UFUNCTION(BlueprintPure, Category = "DestinyLike|Weapon")
	int32 GetSpecialReserve() const { return SpecialReserve; }

	UFUNCTION(BlueprintPure, Category = "DestinyLike|Weapon")
	bool IsSpecialEquipped() const { return ActiveItem.Weapon.Slot == EDLWeaponSlot::Special; }

	UFUNCTION(BlueprintCallable, Category = "DestinyLike|Weapon")
	void SetSight(FName SightId);

	UFUNCTION(BlueprintPure, Category = "DestinyLike|Weapon")
	FName GetSightId() const;

	UFUNCTION(BlueprintPure, Category = "DestinyLike|Weapon")
	FName GetProjectileId() const { return ActiveClass.ProjectileId; }

	UFUNCTION(BlueprintCallable, Category = "DestinyLike|Weapon")
	FVector2D ConsumeHipRecoil();

	UFUNCTION(BlueprintPure, Category = "DestinyLike|Weapon")
	FVector2D GetAdsRecoilPunch() const { return AdsRecoilPunch; }

	/** Camera-forward plus ADS punch/flinch so HUD pip, hitscan, and tracer share one ray. */
	FVector GetMuzzleAim(FVector& OutStart) const;

	/** World location of the visible muzzle; falls back to aim start. */
	FVector GetBarrelLocation() const;

	UFUNCTION(BlueprintPure, Category = "DestinyLike|Weapon")
	float GetViewKickPitch() const { return ViewKickPitch; }

	UFUNCTION(BlueprintPure, Category = "DestinyLike|Weapon")
	bool UsesGrenadeProjectile() const { return ActiveClass.Fire.Mode == EDLWeaponFireMode::Grenade; }

	UFUNCTION(BlueprintPure, Category = "DestinyLike|Weapon")
	bool HasLiveGrenade() const;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DestinyLike|Weapon")
	TObjectPtr<UDLWeaponBehaviorComponent> Behavior = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DestinyLike|Weapon")
	TObjectPtr<UCameraComponent> Camera = nullptr;

protected:
	void ReloadSettings();
	void TryFire();
	FRotator FireHitscan(const FVector& Start, FRotator ViewRot);
	void SpawnTracer(const FVector& Start, const FVector& Direction);
	void SpawnCasing();
	FVector ResolveTracerStart(const FVector& AimStart) const;
	void SpawnGrenade(const FVector& Start, const FVector& Direction);
	bool DetonateLiveGrenade();
	bool HasProxDetonate() const;
	const FDLWeaponClassDef* ResolveClassDef(const FDLItemInstance& Item) const;
	void StoreActiveAmmo();
	void RestoreSlotAmmo(EDLWeaponSlot Slot);
	FVector ComputeMuzzleAim(FVector& OutStart) const;
	float ComputeAdsSeconds() const;
	float ComputeReadySeconds() const;
	float ComputeStowSeconds() const;
	float ComputeRangeDivergence() const;
	void ApplyShotRecoil();
	void QueueRecoilImpulse(TArray<FDLRecoilImpulse>& Into, FVector2D Degrees);
	FVector2D DrainRecoilImpulses(TArray<FDLRecoilImpulse>& Impulses, float DeltaTime);
	float ResolveSightAdsFov() const;
	bool IsOwnerSliding() const;
	float SlideHandlingScale() const;

	UPROPERTY()
	FDLItemInstance ActiveItem;

	UPROPERTY()
	FDLItemInstance PrimaryItem;

	UPROPERTY()
	FDLItemInstance SpecialItem;

	UPROPERTY()
	FDLWeaponClassDef ActiveClass;

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
	int32 AmmoInMag = 30;
	int32 SpecialReserve = 0;
	int32 StoredPrimaryAmmo = -1;
	int32 StoredSpecialAmmo = -1;
	FVector2D PendingHipRecoil = FVector2D::ZeroVector;
	FVector2D AdsRecoilPunch = FVector2D::ZeroVector;
	float ViewKickPitch = 0.f;
	TArray<FDLRecoilImpulse> HipImpulses;
	TArray<FDLRecoilImpulse> AdsImpulses;
	TArray<FDLRecoilImpulse> ViewKickImpulses;

	UPROPERTY()
	TWeakObjectPtr<ADLWeaponProjectile> LiveGrenade;

	UPROPERTY()
	FDLWeaponMotorTune Tune;
};
