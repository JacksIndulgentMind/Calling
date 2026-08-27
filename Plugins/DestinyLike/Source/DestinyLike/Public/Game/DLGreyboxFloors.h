#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DLGreyboxFloors.generated.h"

class UStaticMeshComponent;
class UDirectionalLightComponent;
class USkyLightComponent;
class USkyAtmosphereComponent;
class UStaticMesh;
class UMaterialInterface;
class APlayerStart;

UENUM(BlueprintType)
enum class EDLGreyboxLayout : uint8
{
	SocialLeviathan UMETA(DisplayName = "Social (reduced Leviathan floors, unused)"),
	PvpBannerfall UMETA(DisplayName = "PvP (reduced Bannerfall lanes, unused)"),
	RaidKalli UMETA(DisplayName = "Raid 01 (Kalli court)"),
	RaidShuro UMETA(DisplayName = "Raid 02 (Shuro approach)"),
	RaidMorgeth UMETA(DisplayName = "Raid 03 (Morgeth arena)"),
	RaidVault UMETA(DisplayName = "Raid 04 (vault pit)"),
	SocialSquare UMETA(DisplayName = "Social (100 m square)"),
	PvpThreeLane UMETA(DisplayName = "PvP (3-lane ravine courtyard)")
};

/**
 * Original white greybox only. Social square, 3-lane PvP ravine, Last Wish
 * raid chambers. No imported meshes.
 */
UCLASS()
class DESTINYLIKE_API ADLGreyboxFloors : public AActor
{
	GENERATED_BODY()

public:
	ADLGreyboxFloors();

	virtual void PostInitializeComponents() override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DestinyLike|Greybox")
	EDLGreyboxLayout Layout = EDLGreyboxLayout::SocialSquare;

	/** World-space spawn (cm). PvP returns Red (west). */
	FVector GetPlayerStartLocation() const;

	/** Blue (east) spawn. Valid for PvpThreeLane. */
	FVector GetBluePlayerStartLocation() const;

	/** Z below which RescueFallenPawns teleports. Pit maps sit lower than spawn. */
	float GetRescueMinZ() const;

	/** Encounter director circle radius that stays on these floors. */
	float GetSuggestedArenaHalfExtent() const;

	static ADLGreyboxFloors* SpawnIfMissing(UWorld* World, EDLGreyboxLayout Layout);

	/** Spawn or move a PlayerStart so FindPlayerStart never falls back to WorldSettings (0,0,0). */
	static APlayerStart* EnsurePlayerStart(UWorld* World, const FVector& Location);

	/** Spawn or move a tagged start (Red / Blue). */
	static APlayerStart* EnsureTaggedPlayerStart(UWorld* World, FName Tag, const FVector& Location, const FRotator& Rotation);

	/** Pit maps sit below default KillZ. Engine bounds checks would destroy/recurse on fall. */
	static void ApplyVoidWorldSettings(UWorld* World);

	int32 NumPlatforms() const { return Platforms.Num(); }

	/** Teleport any pawn that already fell through the void back onto the pad. */
	void RescueFallenPawns() const;

	void AddPlatform(const FVector& CenterCm, float SizeXMeters, float SizeYMeters, float SizeZCm = 20.f);
	void AddBox(const FVector& CenterCm, const FVector& SizeCm, const FRotator& Rotation);
	/** 5 m × 5 m catalog: floor, ramp_low, ramp_mid, ramp_steep, rail, cover_half, cover_full. */
	void StampModule(FName Id, const FVector& CenterCm, const FRotator& Rotation = FRotator::ZeroRotator);
	void StampFillFloor(const FVector& CenterCm, float SizeXMeters, float SizeYMeters, float SlabZCm);
	void BuildPvpThreeLane();

protected:
	void EnsureBuilt();
	void BuildLayout();
	void ApplyVisibleShading();
	/** Recast from NavTune.json (agent + jump links). Surviving drop is spawn Z minus rescue Z. */
	void RebuildNavigation();

	UPROPERTY(VisibleAnywhere, Category = "DestinyLike|Greybox")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, Category = "DestinyLike|Greybox")
	TObjectPtr<UDirectionalLightComponent> Sun;

	UPROPERTY(VisibleAnywhere, Category = "DestinyLike|Greybox")
	TObjectPtr<USkyLightComponent> Sky;

	UPROPERTY(VisibleAnywhere, Category = "DestinyLike|Greybox")
	TObjectPtr<USkyAtmosphereComponent> Atmosphere;

	UPROPERTY(VisibleAnywhere, Category = "DestinyLike|Greybox")
	TObjectPtr<class UExponentialHeightFogComponent> Fog;

	UPROPERTY(VisibleAnywhere, Category = "DestinyLike|Greybox")
	TObjectPtr<class UPostProcessComponent> PostProcess;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> Platforms;

	UPROPERTY()
	TObjectPtr<UStaticMesh> CubeMesh;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> ShapeMat;

	EDLGreyboxLayout BuiltLayout = EDLGreyboxLayout::SocialSquare;
	bool bHasBuilt = false;
	float MissingPawnSeconds = 0.f;
	float NavRebuildDelay = -1.f;
};
