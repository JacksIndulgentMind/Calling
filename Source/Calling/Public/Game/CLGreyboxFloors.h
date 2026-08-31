#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Game/CLGreyboxLayouts.h"
#include "CLGreyboxFloors.generated.h"

class UStaticMeshComponent;
class UDirectionalLightComponent;
class USkyLightComponent;
class USkyAtmosphereComponent;
class UStaticMesh;
class UMaterialInterface;
class APlayerStart;
class UCLGreyboxRescue;

UENUM(BlueprintType)
enum class ECLGreyboxLayout : uint8
{
	SocialExtracted UMETA(DisplayName = "Social (parked extracted social floors, unused)"),
	PvpExtracted UMETA(DisplayName = "PvP (parked extracted PvP lanes, unused)"),
	RaidCourt UMETA(DisplayName = "Raid 01 (court)"),
	RaidApproach UMETA(DisplayName = "Raid 02 (approach)"),
	RaidArena UMETA(DisplayName = "Raid 03 (arena)"),
	RaidPit UMETA(DisplayName = "Raid 04 (pit)"),
	SocialSquare UMETA(DisplayName = "Social (100 m square)"),
	PvpThreeLane UMETA(DisplayName = "PvP (3-lane ravine courtyard)"),
	PracticePillar UMETA(DisplayName = "Practice (pillar air-dive)")
};

/**
 * Original white greybox only. Social square, 3-lane PvP ravine, raid
 * raid chambers. No imported meshes.
 */
UCLASS()
class CALLING_API ACLGreyboxFloors : public AActor
{
	GENERATED_BODY()

public:
	ACLGreyboxFloors();

	virtual void PostInitializeComponents() override;
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, ReplicatedUsing = OnRep_Layout, Category = "Calling|Greybox")
	ECLGreyboxLayout Layout = ECLGreyboxLayout::SocialSquare;

	/** World-space spawn (cm). PvP returns Red (west). */
	FVector GetPlayerStartLocation() const;

	/** Blue (east) spawn. Valid for PvpThreeLane. */
	FVector GetBluePlayerStartLocation() const;

	/** Z below which RescueFallenPawns teleports. Pit maps sit lower than spawn. */
	float GetRescueMinZ() const;
	/** Court lip stand after an island dive. Empty if this layout has no edge pad. */
	FVector GetEdgeRecallLocation() const;
	bool IsOnEdgePad(const FVector& Loc) const;
	bool HasEdgePad() const { return bHasEdgePad; }

	/** Encounter director circle radius that stays on these floors. */
	float GetSuggestedArenaHalfExtent() const;

	static ACLGreyboxFloors* SpawnIfMissing(UWorld* World, ECLGreyboxLayout Layout);

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
	void AddBox(const FVector& CenterCm, const FVector& SizeCm, const FRotator& Rotation, bool bExcludeFromNav = false, bool bVaultableCover = false);
	/** 5 m × 5 m catalog: floor, ramp_low, ramp_mid, ramp_steep, rail, cover_half, cover_full. */
	void StampModule(FName Id, const FVector& CenterCm, const FRotator& Rotation = FRotator::ZeroRotator);
	void StampFillFloor(const FVector& CenterCm, float SizeXMeters, float SizeYMeters, float SlabZCm);
	void StampCornerShrines(const FCLPvpThreeLaneRecipe& Recipe, float PitZ);
	void BuildPvpThreeLane();
	void BuildPracticePillar();
	void StampTaskMarkers();
	/** Recast from NavTune.json (agent + jump links). Surviving drop is spawn Z minus rescue Z. */
	void RebuildNavigation();

	bool bFindPathMeshOk = false;
	bool bEdgePadLipOk = false;
	bool bEdgePadPadOk = false;
	bool bEdgePadPartial = false;
	int32 EdgePadPathPoints = 0;
	int32 EdgePadOffMesh = 0;
	int32 EdgePadValidEndsMax = 0;
	float AirDiveJumpLengthCm = 0.f;
	float AirDiveJumpMaxDepthCm = 0.f;
	float AirDiveJumpHeightCm = 0.f;
	float EdgePadBakeMs = 0.f;
	FVector CachedEdgeLip = FVector::ZeroVector;
	FVector CachedEdgePad = FVector::ZeroVector;
	bool bHasEdgePad = false;

protected:
	UFUNCTION()
	void OnRep_Layout();

	void EnsureBuilt();
	void BuildLayout();
	void ApplyVisibleShading();
	void ScheduleNavRebuild();
	UFUNCTION()
	void OnNavRebuildTimer();

	UPROPERTY(VisibleAnywhere, Category = "Calling|Greybox")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, Category = "Calling|Greybox")
	TObjectPtr<UDirectionalLightComponent> Sun;

	UPROPERTY(VisibleAnywhere, Category = "Calling|Greybox")
	TObjectPtr<USkyLightComponent> Sky;

	UPROPERTY(VisibleAnywhere, Category = "Calling|Greybox")
	TObjectPtr<USkyAtmosphereComponent> Atmosphere;

	UPROPERTY(VisibleAnywhere, Category = "Calling|Greybox")
	TObjectPtr<class UExponentialHeightFogComponent> Fog;

	UPROPERTY(VisibleAnywhere, Category = "Calling|Greybox")
	TObjectPtr<class UPostProcessComponent> PostProcess;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> Platforms;

	UPROPERTY()
	TObjectPtr<UStaticMesh> CubeMesh;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> ShapeMat;

	UPROPERTY()
	TObjectPtr<UCLGreyboxRescue> Rescue;

	ECLGreyboxLayout BuiltLayout = ECLGreyboxLayout::SocialSquare;
	bool bHasBuilt = false;
	FTimerHandle NavRebuildTimer;
};
