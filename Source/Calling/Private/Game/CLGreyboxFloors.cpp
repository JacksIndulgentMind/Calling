#include "Game/CLGreyboxFloors.h"
#include "Game/CLGreyboxLayouts.h"
#include "Game/CLGreyboxRescue.h"
#include "AI/CLTaskMarker.h"
#include "Nav/CLNavAbilityEnvelope.h"
#include "Nav/CLNavTune.h"
#include "Nav/CLNavPathUtil.h"
#include "Core/CLTunes.h"
#include "Core/CLLog.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/PostProcessComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerStart.h"
#include "Engine/PlayerStartPIE.h"
#include "GameFramework/WorldSettings.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"
#include "Game/CLGreyboxFloorPads.inl"
#include "Game/CLGameModeBase.h"
#include "NavigationSystem.h"
#include "NavigationPath.h"
#include "NavMesh/RecastNavMesh.h"
#include "NavMesh/NavMeshBoundsVolume.h"
#include "NavMesh/LinkGenerationConfig.h"
#include "Nav/CLNavLinkPolicy.h"
#include "Nav/CLNavArea_AirDive.h"
#include "Navigation/NavLinkProxy.h"
#include "AI/NavigationSystemBase.h"
#include "Components/BrushComponent.h"
#include "TimerManager.h"
#include "HAL/PlatformTime.h"

namespace
{
	float EdgePadDropFromApexCm()
	{
		return CLNavAbility::AirDivePadDropFromApexCm();
	}

	float EdgePadDropFromLipCm()
	{
		FCLMovementTune Move;
		Move.LoadFromIni();
		return CLNavAbility::AirDivePadDropFromLipCm(Move);
	}

	float EdgeAirDiveChordCm()
	{
		FCLMovementTune Move;
		Move.LoadFromIni();
		return CLNavAbility::AirDivePadPlaceChordCm(Move);
	}

	float EdgeAirDiveEdgeCm()
	{
		for (const FCLNavLinkTune& L : CLNavTune::Get().Links)
		{
			if (L.Name.ToString().Equals(TEXT("AirDiveDown"), ESearchCase::IgnoreCase)
				|| CLNavTune::IsAirDiveLink(L.Name))
			{
				return L.JumpDistanceFromEdge;
			}
		}
		return 40.f;
	}

	float EdgeBakeJumpLengthCm()
	{
		FCLMovementTune Move;
		Move.LoadFromIni();
		return CLNavAbility::AirDiveBakeJumpLengthCm(Move, EdgeAirDiveEdgeCm());
	}

	float EdgeJumpLengthCm()
	{
		for (const FCLNavLinkTune& L : CLNavTune::Get().Links)
		{
			if (L.Name.ToString().Equals(TEXT("AirDiveDown"), ESearchCase::IgnoreCase)
				|| CLNavTune::IsAirDiveLink(L.Name))
			{
				return L.JumpLength;
			}
		}
		return 1508.f;
	}

	float EdgePadPlaceChordCm()
	{
		FCLMovementTune Move;
		Move.LoadFromIni();
		return CLNavAbility::AirDivePadPlaceChordCm(Move);
	}

	void DestroyGreyboxAirDiveLinks(UWorld& World)
	{
		TArray<ANavLinkProxy*> Dead;
		for (TActorIterator<ANavLinkProxy> It(&World); It; ++It)
		{
			if (*It && It->ActorHasTag(FName(TEXT("CLGreyboxAirDiveLink"))))
			{
				Dead.Add(*It);
			}
		}
		for (ANavLinkProxy* Link : Dead)
		{
			World.DestroyActor(Link);
		}
	}
}

ACLGreyboxFloors::ACLGreyboxFloors()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	bAlwaysRelevant = true;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Root->SetMobility(EComponentMobility::Static);
	SetRootComponent(Root);

	Rescue = CreateDefaultSubobject<UCLGreyboxRescue>(TEXT("Rescue"));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinderOptional<UMaterialInterface> GridFinder(
		TEXT("/Engine/EngineMaterials/WorldGridMaterial.WorldGridMaterial"),
		LOAD_Quiet | LOAD_NoWarn);
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> ShapeFinder(
		TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (CubeFinder.Succeeded())
	{
		CubeMesh = CubeFinder.Object;
	}
	if (UMaterialInterface* Grid = GridFinder.Get())
	{
		ShapeMat = Grid;
	}
	else if (ShapeFinder.Succeeded())
	{
		ShapeMat = ShapeFinder.Object;
	}

	Sun = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("Sun"));
	Sun->SetupAttachment(Root);
	Sun->SetMobility(EComponentMobility::Movable);
	Sun->SetRelativeRotation(FRotator(-48.f, 40.f, 0.f));
	Sun->SetIntensity(8.f);
	Sun->SetAtmosphereSunLight(true);
	Sun->SetCastShadows(false);

	Sky = CreateDefaultSubobject<USkyLightComponent>(TEXT("Sky"));
	Sky->SetupAttachment(Root);
	Sky->SetMobility(EComponentMobility::Movable);
	Sky->SetIntensity(1.f);
	Sky->bRealTimeCapture = true;

	Atmosphere = CreateDefaultSubobject<USkyAtmosphereComponent>(TEXT("Atmosphere"));
	Atmosphere->SetupAttachment(Root);

	Fog = CreateDefaultSubobject<UExponentialHeightFogComponent>(TEXT("Fog"));
	Fog->SetupAttachment(Root);
	Fog->SetFogDensity(0.015f);
	Fog->SetFogHeightFalloff(0.2f);

	PostProcess = CreateDefaultSubobject<UPostProcessComponent>(TEXT("PostProcess"));
	PostProcess->SetupAttachment(Root);
	PostProcess->bUnbound = true;
	PostProcess->Priority = 100.f;
	PostProcess->BlendWeight = 1.f;
	PostProcess->Settings.bOverride_AutoExposureMinBrightness = true;
	PostProcess->Settings.AutoExposureMinBrightness = 1.f;
	PostProcess->Settings.bOverride_AutoExposureMaxBrightness = true;
	PostProcess->Settings.AutoExposureMaxBrightness = 1.f;
	PostProcess->Settings.bOverride_AutoExposureBias = true;
	PostProcess->Settings.AutoExposureBias = 0.f;
}

void ACLGreyboxFloors::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	EnsureBuilt();
}

void ACLGreyboxFloors::BeginPlay()
{
	Super::BeginPlay();
	EnsureBuilt();
	ScheduleNavRebuild();
	if (Sky)
	{
		Sky->RecaptureSky();
	}
	RescueFallenPawns();
}

void ACLGreyboxFloors::ScheduleNavRebuild()
{
	UWorld* World = GetWorld();
	if (!World || !World->HasBegunPlay())
	{
		return;
	}
	UE_LOG(LogCalling, Display, TEXT("Greybox nav scheduled pads=%d net=%d"), Platforms.Num(), static_cast<int32>(World->GetNetMode()));
	World->GetTimerManager().SetTimer(NavRebuildTimer, this, &ACLGreyboxFloors::OnNavRebuildTimer, 0.15f, false);
}

void ACLGreyboxFloors::OnNavRebuildTimer()
{
	RebuildNavigation();
}

void ACLGreyboxFloors::OnRep_Layout()
{
	EnsureBuilt();
	ScheduleNavRebuild();
}

void ACLGreyboxFloors::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ACLGreyboxFloors, Layout);
}

ACLGreyboxFloors* ACLGreyboxFloors::SpawnIfMissing(UWorld* World, ECLGreyboxLayout Layout)
{
	if (!World)
	{
		return nullptr;
	}

	ACLGreyboxFloors::ApplyVoidWorldSettings(World);

	// Placed map cubes (SocialPad, or a leftover PvP floor) sit at one Z and hide the ravine.
	const bool bHideAllPlacedFloors = (Layout == ECLGreyboxLayout::PvpThreeLane || Layout == ECLGreyboxLayout::PracticePillar);
	for (TActorIterator<AStaticMeshActor> It(World); It; ++It)
	{
		AStaticMeshActor* MeshActor = *It;
		if (!MeshActor)
		{
			continue;
		}
		const FString Label = MeshActor->GetActorNameOrLabel();
		const bool bNamedFloor = Label.Contains(TEXT("Pad")) || Label.Contains(TEXT("Floor"))
			|| Label.Contains(TEXT("Ground"));
		if (bHideAllPlacedFloors || bNamedFloor || Label == TEXT("SocialPad"))
		{
			MeshActor->SetActorHiddenInGame(true);
			MeshActor->SetActorEnableCollision(false);
		}
	}

	for (TActorIterator<ACLGreyboxFloors> It(World); It; ++It)
	{
		ACLGreyboxFloors* Existing = *It;
		Existing->Layout = Layout;
		Existing->EnsureBuilt();
		if (Layout == ECLGreyboxLayout::PvpThreeLane)
		{
			EnsureTaggedPlayerStart(World, FName(TEXT("Red")), Existing->GetPlayerStartLocation(), FRotator(0.f, 0.f, 0.f));
			EnsureTaggedPlayerStart(World, FName(TEXT("Blue")), Existing->GetBluePlayerStartLocation(), FRotator(0.f, 180.f, 0.f));
		}
		else
		{
			EnsurePlayerStart(World, Existing->GetPlayerStartLocation());
		}
		return Existing;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ACLGreyboxFloors* Floors = World->SpawnActorDeferred<ACLGreyboxFloors>(
		ACLGreyboxFloors::StaticClass(), FTransform::Identity, nullptr, nullptr,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Floors)
	{
		return nullptr;
	}

	Floors->Layout = Layout;
	Floors->FinishSpawning(FTransform::Identity);
	Floors->EnsureBuilt();
	if (Layout == ECLGreyboxLayout::PvpThreeLane)
	{
		EnsureTaggedPlayerStart(World, FName(TEXT("Red")), Floors->GetPlayerStartLocation(), FRotator(0.f, 0.f, 0.f));
		EnsureTaggedPlayerStart(World, FName(TEXT("Blue")), Floors->GetBluePlayerStartLocation(), FRotator(0.f, 180.f, 0.f));
	}
	else
	{
		EnsurePlayerStart(World, Floors->GetPlayerStartLocation());
	}
	return Floors;
}

APlayerStart* ACLGreyboxFloors::EnsurePlayerStart(UWorld* World, const FVector& Location)
{
	if (!World)
	{
		return nullptr;
	}

	APlayerStart* Found = nullptr;
	for (TActorIterator<APlayerStart> It(World); It; ++It)
	{
		if (!It->IsA<APlayerStartPIE>())
		{
			Found = *It;
			break;
		}
	}

	if (Found)
	{
		Found->SetActorLocationAndRotation(Location, FRotator::ZeroRotator);
		return Found;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	return World->SpawnActor<APlayerStart>(APlayerStart::StaticClass(), Location, FRotator::ZeroRotator, SpawnParams);
}

APlayerStart* ACLGreyboxFloors::EnsureTaggedPlayerStart(UWorld* World, FName Tag, const FVector& Location, const FRotator& Rotation)
{
	if (!World)
	{
		return nullptr;
	}

	APlayerStart* Found = nullptr;
	for (TActorIterator<APlayerStart> It(World); It; ++It)
	{
		if (It->IsA<APlayerStartPIE>())
		{
			continue;
		}
		if (It->PlayerStartTag == Tag)
		{
			Found = *It;
			break;
		}
	}

	if (Found)
	{
		Found->SetActorLocationAndRotation(Location, Rotation);
		Found->PlayerStartTag = Tag;
		return Found;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	APlayerStart* Spawned = World->SpawnActor<APlayerStart>(APlayerStart::StaticClass(), Location, Rotation, SpawnParams);
	if (Spawned)
	{
		Spawned->PlayerStartTag = Tag;
	}
	return Spawned;
}

void ACLGreyboxFloors::ApplyVoidWorldSettings(UWorld* World)
{
	if (!World)
	{
		return;
	}
	if (AWorldSettings* WorldSettings = World->GetWorldSettings())
	{
		WorldSettings->KillZ = -80000.f;
		WorldSettings->bEnableWorldBoundsChecks = false;
	}
}

void ACLGreyboxFloors::RescueFallenPawns() const
{
	if (Rescue)
	{
		Rescue->RescueFallenPawns();
	}
}

FVector ACLGreyboxFloors::GetPlayerStartLocation() const
{
	switch (Layout)
	{
	case ECLGreyboxLayout::SocialExtracted:
	{
		// Capsule half-height is 96 cm; stand on the origin pad, not under it.
		for (const FCLGeneratedFloorPad& Pad : GSocialExtractedPads)
		{
			const float HalfX = Pad.SizeXMeters * 50.f;
			const float HalfY = Pad.SizeYMeters * 50.f;
			if (FMath::Abs(Pad.CenterX) <= HalfX + 1.f && FMath::Abs(Pad.CenterY) <= HalfY + 1.f)
			{
				return FVector(Pad.CenterX, Pad.CenterY, Pad.CenterZ + 130.f);
			}
		}
		return FVector(GSocialExtractedPlayerStart.X, GSocialExtractedPlayerStart.Y, GSocialExtractedPlayerStart.Z + 50.f);
	}
	case ECLGreyboxLayout::SocialSquare:
		return FVector(0.f, 0.f, 130.f);
	case ECLGreyboxLayout::PvpExtracted:
		return FVector(-5000.f, 0.f, 130.f);
	case ECLGreyboxLayout::PvpThreeLane:
	{
		FCLPvpThreeLaneRecipe R;
		R.Load();
		return FVector(-R.PadCenterCm(), 0.f, 130.f);
	}
	case ECLGreyboxLayout::PracticePillar:
		return FVector(0.f, 0.f, 130.f);
	case ECLGreyboxLayout::RaidApproach:
		return FVector(-3500.f, 0.f, 130.f);
	case ECLGreyboxLayout::RaidPit:
		return FVector(0.f, 0.f, 130.f);
	default:
		return FVector(0.f, 0.f, 200.f);
	}
}

FVector ACLGreyboxFloors::GetBluePlayerStartLocation() const
{
	if (Layout == ECLGreyboxLayout::PvpThreeLane)
	{
		FCLPvpThreeLaneRecipe R;
		R.Load();
		return FVector(R.PadCenterCm(), 0.f, 130.f);
	}
	return GetPlayerStartLocation();
}

float ACLGreyboxFloors::GetRescueMinZ() const
{
	if (Layout == ECLGreyboxLayout::PvpThreeLane)
	{
		if (bHasEdgePad)
		{
			return CachedEdgePad.Z - 500.f;
		}
		return GetPlayerStartLocation().Z - CLNavAbility::AirDiveRefDropCm() - 800.f;
	}
	if (Layout == ECLGreyboxLayout::PracticePillar)
	{
		FCLMovementTune Move;
		Move.LoadFromIni();
		return -CLNavAbility::AirDivePadDropFromLipCm(Move) - 500.f;
	}
	return GetPlayerStartLocation().Z - 150.f;
}

FVector ACLGreyboxFloors::GetEdgeRecallLocation() const
{
	if (bHasEdgePad)
	{
		return FVector(CachedEdgeLip.X, CachedEdgeLip.Y, CachedEdgeLip.Z + 98.f);
	}
	return GetPlayerStartLocation();
}

bool ACLGreyboxFloors::IsOnEdgePad(const FVector& Loc) const
{
	if (!bHasEdgePad)
	{
		return false;
	}
	return FVector::Dist2D(Loc, CachedEdgePad) < 420.f
		&& Loc.Z > CachedEdgePad.Z - 50.f
		&& Loc.Z < CachedEdgePad.Z + 260.f;
}

float ACLGreyboxFloors::GetSuggestedArenaHalfExtent() const
{
	switch (Layout)
	{
	case ECLGreyboxLayout::RaidCourt:
		return 2200.f;
	case ECLGreyboxLayout::RaidApproach:
		return 600.f;
	case ECLGreyboxLayout::RaidArena:
		return 1600.f;
	case ECLGreyboxLayout::RaidPit:
		return 700.f;
	default:
		return 2500.f;
	}
}

void ACLGreyboxFloors::EnsureBuilt()
{
	// Already stamped for this layout. PvP used to rebuild every call so a
	// stale slab could not survive a code change; that also destroyed the map
	// on ChoosePlayerStart / respawn and wiped Recast under live BotBooks.
	if (bHasBuilt && BuiltLayout == Layout && Platforms.Num() > 0)
	{
		return;
	}
	for (UStaticMeshComponent* Mesh : Platforms)
	{
		if (Mesh)
		{
			Mesh->DestroyComponent();
		}
	}
	Platforms.Empty();
	BuildLayout();
	ApplyVisibleShading();
	StampTaskMarkers();
	BuiltLayout = Layout;
	bHasBuilt = true;
	if (UWorld* World = GetWorld(); World && World->HasBegunPlay())
	{
		ScheduleNavRebuild();
	}
}

void ACLGreyboxFloors::AddPlatform(const FVector& CenterCm, float SizeXMeters, float SizeYMeters, float SizeZCm)
{
	AddBox(
		FVector(CenterCm.X, CenterCm.Y, CenterCm.Z - SizeZCm * 0.5f),
		FVector(SizeXMeters * 100.f, SizeYMeters * 100.f, SizeZCm),
		FRotator::ZeroRotator);
}

void ACLGreyboxFloors::AddBox(const FVector& CenterCm, const FVector& SizeCm, const FRotator& Rotation, bool bExcludeFromNav, bool bVaultableCover)
{
	const FName CompName(*FString::Printf(TEXT("Floor_%d"), Platforms.Num()));
	UStaticMeshComponent* Mesh = NewObject<UStaticMeshComponent>(this, CompName);
	Mesh->SetupAttachment(RootComponent);
	Mesh->SetMobility(EComponentMobility::Static);
	// Cover is collision + probe (vault or wall-slide), not a Recast floor. Baking
	// walkable on the lid puts goto waypoints where the pawn cannot stand.
	Mesh->SetCanEverAffectNavigation(!bExcludeFromNav);
	Mesh->bFillCollisionUnderneathForNavmesh = !bExcludeFromNav;
	if (bExcludeFromNav)
	{
		Mesh->ComponentTags.Add(FName(TEXT("CLCoverObstacle")));
	}
	if (bVaultableCover)
	{
		Mesh->ComponentTags.Add(FName(TEXT("CLVaultableCover")));
	}
	Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Mesh->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	Mesh->SetCastShadow(false);
	Mesh->SetReceivesDecals(false);
	if (CubeMesh)
	{
		Mesh->SetStaticMesh(CubeMesh);
	}
	if (ShapeMat)
	{
		Mesh->SetMaterial(0, ShapeMat);
	}
	const FVector Scale(SizeCm.X / 100.f, SizeCm.Y / 100.f, SizeCm.Z / 100.f);
	Mesh->SetRelativeTransform(FTransform(Rotation, CenterCm, Scale));
	Mesh->RegisterComponent();
	if (!bExcludeFromNav)
	{
		FNavigationSystem::UpdateComponentData(*Mesh);
	}
	Platforms.Add(Mesh);
}

void ACLGreyboxFloors::StampModule(FName Id, const FVector& CenterCm, const FRotator& Rotation)
{
	FVector Size(500.f, 500.f, 40.f);
	FRotator Rot = Rotation;
	const FString Key = Id.ToString().ToLower();
	if (Key == TEXT("floor"))
	{
		Size = FVector(500.f, 500.f, 40.f);
	}
	else if (Key == TEXT("ramp_low"))
	{
		Size = FVector(500.f, 500.f, 40.f);
		if (FMath::IsNearlyZero(Rot.Pitch))
		{
			Rot.Pitch = -15.f;
		}
	}
	else if (Key == TEXT("ramp_mid"))
	{
		Size = FVector(500.f, 500.f, 40.f);
		if (FMath::IsNearlyZero(Rot.Pitch))
		{
			Rot.Pitch = -26.6f;
		}
	}
	else if (Key == TEXT("ramp_steep"))
	{
		Size = FVector(500.f, 500.f, 40.f);
		if (FMath::IsNearlyZero(Rot.Pitch))
		{
			Rot.Pitch = -40.f;
		}
	}
	else if (Key == TEXT("rail"))
	{
		Size = FVector(500.f, 60.f, 220.f);
	}
	else if (Key == TEXT("cover_half"))
	{
		Size = FVector(80.f, 500.f, 85.f);
	}
	else if (Key == TEXT("cover_full"))
	{
		Size = FVector(80.f, 500.f, 220.f);
	}
	const bool bCoverObstacle = (Key == TEXT("cover_half") || Key == TEXT("cover_full"));
	const bool bVaultableCover = (Key == TEXT("cover_half"));
	AddBox(CenterCm, Size, Rot, bCoverObstacle, bVaultableCover);
}

void ACLGreyboxFloors::StampCornerShrines(const FCLPvpThreeLaneRecipe& Recipe, float PitZ)
{
	const float Rim = Recipe.RimM() * 100.f;
	const float Inset = 500.f;
	const float Corner = Rim - Inset;
	const FVector WellC(-Corner, Corner, PitZ);
	const FVector TreeC(Corner, Corner, PitZ);
	const FVector HeelC(-Corner, -Corner, PitZ);
	const FVector CairnC(Corner, -Corner, PitZ);

	const float WallH = 180.f;
	const float WallT = 40.f;
	const float Apothem = 270.f;
	const float Side = 2.f * Apothem * FMath::Tan(PI / 8.f);
	for (int32 i = 0; i < 8; ++i)
	{
		const float Deg = 22.5f + 45.f * static_cast<float>(i);
		const float Ang = FMath::DegreesToRadians(Deg);
		const FVector Off(FMath::Cos(Ang) * Apothem, FMath::Sin(Ang) * Apothem, WallH * 0.5f);
		AddBox(WellC + Off, FVector(Side, WallT, WallH), FRotator(0.f, Deg + 90.f, 0.f));
	}

	AddBox(TreeC + FVector(0.f, 0.f, 200.f), FVector(80.f, 80.f, 400.f), FRotator::ZeroRotator);
	AddBox(TreeC + FVector(0.f, 0.f, 380.f), FVector(280.f, 280.f, 80.f), FRotator::ZeroRotator);
	AddBox(TreeC + FVector(0.f, 0.f, 440.f), FVector(180.f, 180.f, 60.f), FRotator::ZeroRotator);
	AddBox(TreeC + FVector(-120.f, -80.f, 22.5f), FVector(180.f, 50.f, 45.f), FRotator(0.f, -45.f, 0.f));

	AddBox(HeelC + FVector(0.f, 0.f, 200.f), FVector(90.f, 90.f, 400.f), FRotator(0.f, 45.f, 0.f));

	AddBox(CairnC + FVector(0.f, 0.f, 40.f), FVector(220.f, 220.f, 80.f), FRotator::ZeroRotator);
	AddBox(CairnC + FVector(0.f, 0.f, 120.f), FVector(150.f, 150.f, 80.f), FRotator::ZeroRotator);
	AddBox(CairnC + FVector(0.f, 0.f, 200.f), FVector(80.f, 80.f, 80.f), FRotator::ZeroRotator);
}

void ACLGreyboxFloors::StampFillFloor(const FVector& CenterCm, float SizeXMeters, float SizeYMeters, float SlabZCm)
{
	const float Tile = 500.f;
	const float W = SizeXMeters * 100.f;
	const float D = SizeYMeters * 100.f;
	const int32 Nx = FMath::Max(1, FMath::CeilToInt(W / Tile));
	const int32 Ny = FMath::Max(1, FMath::CeilToInt(D / Tile));
	const float OriginX = CenterCm.X - (Nx - 1) * Tile * 0.5f;
	const float OriginY = CenterCm.Y - (Ny - 1) * Tile * 0.5f;
	const float Z = CenterCm.Z - SlabZCm * 0.5f;
	for (int32 ix = 0; ix < Nx; ++ix)
	{
		for (int32 iy = 0; iy < Ny; ++iy)
		{
			StampModule(FName(TEXT("floor")), FVector(OriginX + ix * Tile, OriginY + iy * Tile, Z), FRotator::ZeroRotator);
		}
	}
}

void ACLGreyboxFloors::BuildPvpThreeLane()
{
	FCLPvpThreeLaneRecipe R;
	R.Load();
	const float M = 100.f;
	const float PadDepthM = R.PadDepthM;
	const float CourtM = R.CourtM;
	const float RavineM = R.RavineM;
	const float WidthM = R.WidthM;
	const float SlabZ = R.SlabZ;
	const float LaneW = R.LaneW;
	const float PadInnerM = R.PadInnerM;
	const float RimM = R.RimM();
	const float SlopeM = PadInnerM - RimM;
	const float LaneY[3] = { R.LaneYMeters[0] * M, R.LaneYMeters[1] * M, R.LaneYMeters[2] * M };

	const float FloorZ = 0.f;
	const float PitZ = -RavineM * M;
	const float Run = SlopeM * M;
	const float Drop = RavineM * M;
	const float Hyp = FMath::Sqrt(Run * Run + Drop * Drop);
	const float PitchDeg = FMath::RadiansToDegrees(FMath::Atan2(Drop, Run));

	StampFillFloor(FVector(-R.PadCenterCm(), 0.f, FloorZ), PadDepthM, WidthM, SlabZ);
	StampFillFloor(FVector(R.PadCenterCm(), 0.f, FloorZ), PadDepthM, WidthM, SlabZ);
	StampFillFloor(FVector(0.f, 0.f, PitZ), CourtM, WidthM, SlabZ);

	const FVector WestUp = FVector(Drop, 0.f, Run).GetSafeNormal();
	const FVector EastUp = FVector(-Drop, 0.f, Run).GetSafeNormal();
	const int32 RampAlong = FMath::Max(1, FMath::CeilToInt(Hyp / 500.f));
	const int32 RampAcross = FMath::Max(1, FMath::CeilToInt(LaneW * M / 500.f));
	for (float Y : LaneY)
	{
		const FVector WestStart(-PadInnerM * M, Y, FloorZ);
		const FVector WestEnd(-RimM * M, Y, PitZ);
		const FVector EastStart(PadInnerM * M, Y, FloorZ);
		const FVector EastEnd(RimM * M, Y, PitZ);
		for (int32 i = 0; i < RampAlong; ++i)
		{
			const float T = (static_cast<float>(i) + 0.5f) / static_cast<float>(RampAlong);
			for (int32 j = 0; j < RampAcross; ++j)
			{
				const float YOff = (static_cast<float>(j) - (RampAcross - 1) * 0.5f) * 500.f;
				const FVector WestPos = FMath::Lerp(WestStart, WestEnd, T) + FVector(0.f, YOff, 0.f) - WestUp * (SlabZ * 0.5f);
				const FVector EastPos = FMath::Lerp(EastStart, EastEnd, T) + FVector(0.f, YOff, 0.f) - EastUp * (SlabZ * 0.5f);
				StampModule(FName(TEXT("ramp_mid")), WestPos, FRotator(-PitchDeg, 0.f, 0.f));
				StampModule(FName(TEXT("ramp_mid")), EastPos, FRotator(PitchDeg, 0.f, 0.f));
			}
		}
	}

	auto SlopeZAtX = [&](float Xcm) -> float
	{
		if (Xcm <= -PadInnerM * M || Xcm >= PadInnerM * M)
		{
			return FloorZ;
		}
		if (Xcm >= -RimM * M && Xcm <= RimM * M)
		{
			return PitZ;
		}
		if (Xcm < 0.f)
		{
			const float T = (Xcm + PadInnerM * M) / Run;
			return FloorZ + T * (PitZ - FloorZ);
		}
		const float T = (Xcm - RimM * M) / Run;
		return PitZ + T * (FloorZ - PitZ);
	};

	const float CrossXs[2] = {
		FMath::Lerp(-PadInnerM, -RimM, R.CrossFractions[0]) * M,
		FMath::Lerp(PadInnerM, RimM, R.CrossFractions[1]) * M
	};
	for (float CrossX : CrossXs)
	{
		if (FMath::Abs(CrossX) <= RimM * M + 50.f)
		{
			continue;
		}
		// Thin along the ramp (X): a wide terrace at SlopeZ overhangs the slope by ~1 m at the
		// downhill lip. Keep a short X so DropDown / step can land on the ramp below.
		const float CrossAlongM = FMath::Min(LaneW, 4.f);
		StampFillFloor(FVector(CrossX, 0.f, SlopeZAtX(CrossX)), CrossAlongM, WidthM, SlabZ);
	}

	for (float Y : LaneY)
	{
		StampModule(FName(TEXT("cover_full")), FVector(-R.CoverLipM() * M, Y, 120.f), FRotator::ZeroRotator);
		StampModule(FName(TEXT("cover_full")), FVector(R.CoverLipM() * M, Y, 120.f), FRotator::ZeroRotator);
	}

	AddBox(FVector(0.f, 0.f, PitZ + 175.f), FVector(80.f, 80.f, 350.f), FRotator::ZeroRotator);
	AddBox(FVector(-R.ControlPostM() * M, 0.f, 175.f), FVector(80.f, 80.f, 350.f), FRotator::ZeroRotator);
	AddBox(FVector(R.ControlPostM() * M, 0.f, 175.f), FVector(80.f, 80.f, 350.f), FRotator::ZeroRotator);

	auto NearCross = [&](float Xcm) -> bool
	{
		for (float CrossX : CrossXs)
		{
			if (FMath::Abs(Xcm - CrossX) < 4.f * M)
			{
				return true;
			}
		}
		return false;
	};

	auto OnSlope = [&](float Xcm) -> bool
	{
		return FMath::Abs(Xcm) > RimM * M + 100.f && FMath::Abs(Xcm) < PadInnerM * M - 50.f;
	};

	// Crossing cover: per-lane spacing/phase so the three routes do not line up.
	// Half-height ~85 cm — only a slide (or crouch; same capsule) is behind it.
	const float HalfCoverZ = R.HalfCoverZ;
	const float LaneStepM[3] = { R.LaneStepM[0], R.LaneStepM[1], R.LaneStepM[2] };
	const float LanePhaseM[3] = { R.LanePhaseM[0], R.LanePhaseM[1], R.LanePhaseM[2] };
	for (int32 Lane = 0; Lane < 3; ++Lane)
	{
		const float Step = LaneStepM[Lane] * M;
		const float Phase = LanePhaseM[Lane] * M;
		int32 CrossIdx = 0;
		for (float X = -PadInnerM * M + Step + Phase; X < PadInnerM * M; X += Step, ++CrossIdx)
		{
			if (!OnSlope(X) || NearCross(X))
			{
				continue;
			}
			const uint32 Seed = 0xB529u * static_cast<uint32>(Lane + 1)
				^ 0x1F83u * static_cast<uint32>(CrossIdx + 5);
			const uint32 Kind = Seed % 8u;
			if (Kind == 0u)
			{
				continue;
			}
			const float Height = (Kind <= 3u) ? HalfCoverZ : (220.f + static_cast<float>(Kind % 3u) * 40.f);
			const float GapDir = (Seed & 1u) ? 1.f : -1.f;
			const float Yc = LaneY[Lane] + GapDir * 200.f;
			const float FloorAt = SlopeZAtX(X);
			const FName CoverId = (Height <= HalfCoverZ + 1.f) ? FName(TEXT("cover_half")) : FName(TEXT("cover_full"));
			StampModule(CoverId, FVector(X, Yc, FloorAt + Height * 0.5f), FRotator::ZeroRotator);
		}
	}

	// Side rails: outer edges plus a few inner pieces. Less dense than crossing cover.
	const float RailStep = R.RailStepM * M;
	int32 RailIdx = 0;
	for (float X = -PadInnerM * M + RailStep * 0.5f; X < PadInnerM * M; X += RailStep, ++RailIdx)
	{
		if (!OnSlope(X) || NearCross(X))
		{
			continue;
		}
		const float FloorAt = SlopeZAtX(X);
		StampModule(FName(TEXT("rail")), FVector(X, -24.f * M, FloorAt + 110.f), FRotator::ZeroRotator);
		StampModule(FName(TEXT("rail")), FVector(X, 24.f * M, FloorAt + 110.f), FRotator::ZeroRotator);
		if ((RailIdx % 2) == 0)
		{
			const float InnerY = ((RailIdx / 2) % 2 == 0) ? -6.f * M : 6.f * M;
			StampModule(FName(TEXT("rail")), FVector(X, InnerY, FloorAt + 90.f), FRotator::ZeroRotator);
		}
	}

	// Inner-circle cover at 6 m. Outer mixed rings replaced by a Stonehenge menhir ring.
	auto NearLaneMouth = [&](float Xcm, float Ycm) -> bool
	{
		if (FMath::Abs(FMath::Abs(Xcm) - RimM * M) > 500.f)
		{
			return false;
		}
		for (float Y : LaneY)
		{
			if (FMath::Abs(Ycm - Y) < 700.f)
			{
				return true;
			}
		}
		return false;
	};

	const float InnerRad = R.CourtRadiiM[0] * M;
	const int32 InnerN = R.RingCount[0];
	for (int32 i = 0; i < InnerN; ++i)
	{
		const float Deg = (360.f / static_cast<float>(InnerN)) * static_cast<float>(i);
		const float Ang = FMath::DegreesToRadians(Deg);
		const float X = FMath::Cos(Ang) * InnerRad;
		const float Y = FMath::Sin(Ang) * InnerRad;
		if (NearLaneMouth(X, Y))
		{
			continue;
		}
		const float Height = ((i % 2) == 0) ? HalfCoverZ : 160.f;
		const bool bSpoke = (i % 2) == 0;
		const float Yaw = bSpoke ? Deg : Deg + 90.f;
		const FName CoverId = (Height <= HalfCoverZ + 1.f) ? FName(TEXT("cover_half")) : FName(TEXT("cover_full"));
		StampModule(CoverId, FVector(X, Y, PitZ + Height * 0.5f), FRotator(0.f, Yaw, 0.f));
	}

	const int32 MenhirN = FMath::Max(4, R.MenhirCount);
	const float MenhirRad = R.MenhirRadiusM * M;
	const float MenhirH = FMath::Max(180.f, R.MenhirHeightCm);
	const float PostW = FMath::Max(40.f, R.MenhirPostWidthCm);
	const float PostD = FMath::Max(40.f, R.MenhirPostDepthCm);
	const float Span = FMath::Max(PostW + 40.f, R.MenhirSpanCm);
	const float LintelH = FMath::Max(24.f, R.MenhirLintelHeightCm);
	for (int32 i = 0; i < MenhirN; ++i)
	{
		const float Deg = (360.f / static_cast<float>(MenhirN)) * static_cast<float>(i);
		const float Ang = FMath::DegreesToRadians(Deg);
		const float X = FMath::Cos(Ang) * MenhirRad;
		const float Y = FMath::Sin(Ang) * MenhirRad;
		if (NearLaneMouth(X, Y))
		{
			continue;
		}
		const FVector Tangent(-FMath::Sin(Ang), FMath::Cos(Ang), 0.f);
		const FRotator Yaw(0.f, Deg + 90.f, 0.f);
		const FVector Station(X, Y, 0.f);
		const float Half = Span * 0.5f;
		const FVector PostA = Station + Tangent * Half;
		const FVector PostB = Station - Tangent * Half;
		AddBox(FVector(PostA.X, PostA.Y, PitZ + MenhirH * 0.5f), FVector(PostW, PostD, MenhirH), Yaw);
		AddBox(FVector(PostB.X, PostB.Y, PitZ + MenhirH * 0.5f), FVector(PostW, PostD, MenhirH), Yaw);
		AddBox(
			FVector(X, Y, PitZ + MenhirH + LintelH * 0.5f),
			FVector(Span + PostW, PostD, LintelH),
			Yaw);
	}

	// Short inner hide for the hold pawn (not a 5 m cover_half slab).
	AddBox(FVector(0.f, 320.f, PitZ + HalfCoverZ * 0.5f), FVector(80.f, 180.f, HalfCoverZ), FRotator::ZeroRotator, true);

	StampCornerShrines(R, PitZ);

	FVector EdgeLip;
	FVector EdgePad;
	R.EdgeAirDiveEnds(EdgeLip, EdgePad, EdgePadPlaceChordCm(), EdgeAirDiveEdgeCm(), EdgePadDropFromLipCm());
	CachedEdgeLip = EdgeLip;
	CachedEdgePad = EdgePad;
	bHasEdgePad = true;
	AddPlatform(EdgePad, 8.f, 8.f, 400.f);
	{
		FCLMovementTune Move;
		Move.LoadFromIni();
		const float DistXY = FVector::Dist2D(EdgeLip, EdgePad);
		const float Dz = EdgePad.Z - EdgeLip.Z;
		const float Diag = FMath::Sqrt(DistXY * DistXY + Dz * Dz);
		const float JumpLen = EdgeJumpLengthCm();
		const float L = CLNavAbility::SamePlaneJumpLengthCm(Move);
		const float H = CLNavAbility::JumpApexUpCm(Move, FMath::Max(1, Move.MaxJumps));
		const float Depth = CLNavAbility::AirDiveRefDropCm();
		const float X0 = CLNavAbility::RecastJumpLaunchPlaneInterceptCm(L, H, Depth);
		UE_LOG(LogCalling, Display,
			TEXT("Greybox edgePad island lip=%s pad=%s distXY=%.0f dZ=%.0f diag=%.0f jumpLen=%.0f L=%.0f x0=%.0f chord=%.0f H-D=%.0f"),
			*EdgeLip.ToCompactString(), *EdgePad.ToCompactString(),
			DistXY, Dz, Diag, JumpLen, L, X0, EdgeAirDiveChordCm(), H - Depth);
		if (Diag > JumpLen)
		{
			UE_LOG(LogCalling, Warning,
				TEXT("Greybox edgePad canary 3D %.0f exceeds JumpLength %.0f (freeze JumpLength from max envelope, do not move the pad)"),
				Diag, JumpLen);
		}
	}
	UE_LOG(LogCalling, Display,
		TEXT("Greybox PvpThreeLane padZ=0 courtZ=%.0f courtM=%.0f laneW=%.0f rampPitch=%.1f deg westCut=%.0f eastCut=%.0f red=%s edgePad=%s"),
		PitZ, CourtM, LaneW, PitchDeg, CrossXs[0], CrossXs[1],
		*GetPlayerStartLocation().ToCompactString(),
		*EdgePad.ToCompactString());
}

void ACLGreyboxFloors::BuildPracticePillar()
{
	FCLMovementTune Move;
	Move.LoadFromIni();
	const float Drop = CLNavAbility::AirDivePadDropFromLipCm(Move);
	const float Chord = CLNavAbility::AirDivePadPlaceChordCm(Move);
	const float PadX = Chord - CLNavAbility::AirDivePadRimInsetCm(40.f);
	AddPlatform(FVector(0.f, 0.f, 0.f), 8.f, 8.f, 40.f);
	AddPlatform(FVector(PadX, 0.f, -Drop), 3.5f, 3.5f, 400.f);
	UE_LOG(LogCalling, Display, TEXT("Greybox PracticePillar drop=%.0f chordXY=%.0f padX=%.0f"), Drop, Chord, PadX);
}

void ACLGreyboxFloors::StampTaskMarkers()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	ACLTaskMarker::DestroyAllInWorld(World);
	if (Layout == ECLGreyboxLayout::PvpThreeLane)
	{
		FCLPvpThreeLaneRecipe R;
		R.Load();
		const float PitZ = R.PitZCm();
		const float LintelTop = PitZ + R.MenhirHeightCm + R.MenhirLintelHeightCm;
		const float MenhirRad = R.MenhirRadiusM * 100.f;
		const float CoverX = R.CoverLipM() * 100.f;
		const float Rim = R.RimM() * 100.f;
		const float Corner = Rim - 500.f;
		ACLTaskMarker::SpawnAt(World, FName(TEXT("spawn_red")), GetPlayerStartLocation());
		ACLTaskMarker::SpawnAt(World, FName(TEXT("spawn_blue")), GetBluePlayerStartLocation());
		ACLTaskMarker::SpawnAt(World, FName(TEXT("court_center")), FVector(0.f, 0.f, PitZ));
		ACLTaskMarker::SpawnAt(World, FName(TEXT("hide_center_lee")), FVector(0.f, 400.f, PitZ));
		ACLTaskMarker::SpawnAt(World, FName(TEXT("cover_west_cut")), FVector(-CoverX, 0.f, 120.f));
		ACLTaskMarker::SpawnAt(World, FName(TEXT("cover_east_cut")), FVector(CoverX, 0.f, 120.f));
		ACLTaskMarker::SpawnAt(World, FName(TEXT("shrine_well")), FVector(-Corner, Corner, PitZ), 280.f);
		ACLTaskMarker::SpawnAt(World, FName(TEXT("shrine_tree")), FVector(Corner - 120.f, Corner - 80.f, PitZ), 180.f);
		ACLTaskMarker::SpawnAt(World, FName(TEXT("shrine_heel")), FVector(-Corner, -Corner, PitZ), 150.f);
		ACLTaskMarker::SpawnAt(World, FName(TEXT("shrine_cairn")), FVector(Corner, -Corner, PitZ + 240.f), 150.f);
		for (int32 i = 0; i < 8; ++i)
		{
			const float Deg = 45.f * static_cast<float>(i);
			const float Ang = FMath::DegreesToRadians(Deg);
			const FVector Station(FMath::Cos(Ang) * MenhirRad, FMath::Sin(Ang) * MenhirRad, LintelTop);
			const FVector Approach(FMath::Cos(Ang) * 1250.f, FMath::Sin(Ang) * 1250.f, PitZ);
			ACLTaskMarker::SpawnAt(World, FName(*FString::Printf(TEXT("menhir_%d"), i)), Station);
			ACLTaskMarker::SpawnAt(World, FName(*FString::Printf(TEXT("menhir_%d_approach"), i)), Approach);
		}
		FVector EdgeLip;
		FVector EdgePad;
		R.EdgeAirDiveEnds(EdgeLip, EdgePad, EdgePadPlaceChordCm(), EdgeAirDiveEdgeCm(), EdgePadDropFromLipCm());
		ACLTaskMarker::SpawnAt(World, FName(TEXT("edge_lip")), EdgeLip);
		ACLTaskMarker::SpawnAt(World, FName(TEXT("edge_pad")), EdgePad);
		ACLTaskMarker::SpawnAt(World, FName(TEXT("slide_end")), FVector(900.f, 0.f, PitZ));
		ACLTaskMarker::SpawnAt(World, FName(TEXT("dash_end")), FVector(950.f, 0.f, PitZ));
		return;
	}
	if (Layout == ECLGreyboxLayout::PracticePillar)
	{
		FCLMovementTune Move;
		Move.LoadFromIni();
		const float Drop = CLNavAbility::AirDivePadDropFromLipCm(Move);
		const float PadX = CLNavAbility::AirDivePadPlaceChordCm(Move)
			- CLNavAbility::AirDivePadRimInsetCm(40.f);
		ACLTaskMarker::SpawnAt(World, FName(TEXT("pillar_top")), GetPlayerStartLocation());
		ACLTaskMarker::SpawnAt(World, FName(TEXT("pillar_pad")), FVector(PadX, 0.f, -Drop));
		ACLTaskMarker::SpawnAt(World, FName(TEXT("spawn_default")), GetPlayerStartLocation());
		return;
	}
	ACLTaskMarker::SpawnAt(World, FName(TEXT("spawn_default")), GetPlayerStartLocation());
}

void ACLGreyboxFloors::BuildLayout()
{
	if (TUniquePtr<ICLGreyboxLayout> Built = CLMakeGreyboxLayout(Layout))
	{
		Built->Build(*this);
	}
}

void ACLGreyboxFloors::ApplyVisibleShading()
{
	const FLinearColor White(0.95f, 0.95f, 0.92f);
	for (UStaticMeshComponent* Mesh : Platforms)
	{
		if (!Mesh)
		{
			continue;
		}
		UMaterialInterface* Base = Mesh->GetMaterial(0);
		if (!Base)
		{
			continue;
		}
		UMaterialInstanceDynamic* Mid = Mesh->CreateAndSetMaterialInstanceDynamic(0);
		if (!Mid)
		{
			continue;
		}
		Mid->SetVectorParameterValue(TEXT("Color"), White);
		Mid->SetVectorParameterValue(TEXT("BaseColor"), White);
		Mid->SetVectorParameterValue(TEXT("TintColor"), White);
	}
}

void ACLGreyboxFloors::RebuildNavigation()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// One NavMeshBoundsVolume for walkable mesh AND jump-gen sampling.
	// UE 5.8 has no separate link-bounds actor. Every platform must sit in this box.
	FBox Box(ForceInit);
	for (UStaticMeshComponent* Mesh : Platforms)
	{
		if (Mesh)
		{
			Box += Mesh->Bounds.GetBox();
		}
	}
	// Hop corridor Z must include the pad floor (pad Z can sit below lip−Drop).
	if (Layout == ECLGreyboxLayout::PvpThreeLane)
	{
		FCLPvpThreeLaneRecipe R;
		R.Load();
		FVector Lip;
		FVector Pad;
		R.EdgeAirDiveEnds(Lip, Pad, EdgePadPlaceChordCm(), EdgeAirDiveEdgeCm(), EdgePadDropFromLipCm());
		const float LookXY = FMath::Max3(EdgeJumpLengthCm(), EdgeBakeJumpLengthCm(), static_cast<float>(FVector::Dist2D(Lip, Pad))) + 400.f;
		const float LookZ = FMath::Max(CLNavAbility::AirDiveRefDropCm(), FMath::Abs(Lip.Z - Pad.Z)) + 800.f;
		FBox Hop(Lip, Lip);
		Hop += Pad;
		Hop = Hop.ExpandBy(FVector(LookXY, LookXY, LookZ));
		Box += Hop;
		Box.Min.Z = FMath::Min3(Box.Min.Z, Pad.Z - LookZ, Lip.Z - LookZ);
		Box.Max.Z = FMath::Max(Box.Max.Z, Lip.Z + 1800.f);
	}
	if (!Box.IsValid && Layout == ECLGreyboxLayout::PvpThreeLane)
	{
		FCLPvpThreeLaneRecipe R;
		R.Load();
		const float PadX = (R.PadInnerM + R.PadDepthM) * 100.f + 800.f;
		FVector Lip;
		FVector Pad;
		R.EdgeAirDiveEnds(Lip, Pad, EdgePadPlaceChordCm(), EdgeAirDiveEdgeCm(), EdgePadDropFromLipCm());
		const float MinZ = FMath::Min(Pad.Z, R.PitZCm()) - 800.f;
		Box = FBox(FVector(-PadX, -9000.f, MinZ), FVector(PadX, 9000.f, 800.f));
		Box += Lip;
		Box += Pad;
	}
	if (!Box.IsValid)
	{
		UE_LOG(LogCalling, Warning, TEXT("Greybox nav skipped: empty bounds pads=%d"), Platforms.Num());
		return;
	}
	Box = Box.ExpandBy(FVector(400.f, 400.f, 400.f));

	if (!FNavigationSystem::GetCurrent<UNavigationSystemV1>(World))
	{
		FNavigationSystem::AddNavigationSystemToWorld(*World, FNavigationSystemRunMode::GameMode);
	}
	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
	if (!NavSys)
	{
		UE_LOG(LogCalling, Warning, TEXT("Greybox: no navigation system after AddNavigationSystemToWorld"));
		return;
	}

	NavSys->bInitialBuildingLocked = false;
	NavSys->ReleaseInitialBuildingLock();
	// Empty runtime bounds volumes otherwise produce 0 tiles.
	// This volume is not a tile-size knob: JumpLength searches within it for landings.
	NavSys->bWholeWorldNavigable = true;

	ANavMeshBoundsVolume* Vol = nullptr;
	for (TActorIterator<ANavMeshBoundsVolume> It(World); It; ++It)
	{
		if (It->ActorHasTag(FName(TEXT("CLGreyboxNavBounds"))))
		{
			Vol = *It;
			break;
		}
	}
	if (!Vol)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Vol = World->SpawnActor<ANavMeshBoundsVolume>(Box.GetCenter(), FRotator::ZeroRotator, Params);
		if (Vol)
		{
			Vol->Tags.Add(FName(TEXT("CLGreyboxNavBounds")));
		}
	}
	if (Vol)
	{
		Vol->SetActorLocation(Box.GetCenter());
		const FVector Extent = Box.GetExtent();
		// Default cube brush is 200 uu on a side (extent 100).
		Vol->SetActorScale3D(FVector(
			FMath::Max(Extent.X / 100.f, 0.1f),
			FMath::Max(Extent.Y / 100.f, 0.1f),
			FMath::Max(Extent.Z / 100.f, 0.1f)));
		if (UBrushComponent* Brush = Vol->GetBrushComponent())
		{
			Brush->bHiddenInGame = true;
			Brush->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
		NavSys->OnNavigationBoundsUpdated(Vol);
	}

	ARecastNavMesh* Recast = Cast<ARecastNavMesh>(
		NavSys->GetDefaultNavDataInstance(FNavigationSystem::ECreateIfMissing::Create));
	if (Recast)
	{
		const float SurvivingDropCm = FMath::Max(100.f, GetPlayerStartLocation().Z - GetRescueMinZ());
		CLNavLinkPolicy::ApplyToRecast(*Recast, SurvivingDropCm);
		const TArray<FNavLinkGenerationJumpConfig>& JumpConfigs = Recast->GetNavLinkJumpConfigs();
		for (const FNavLinkGenerationJumpConfig& Cfg : JumpConfigs)
		{
			UE_LOG(LogCalling, Display,
				TEXT("Greybox jumpCfg %s en=%s len=%.0f edge=%.0f h=%.0f d=%.0f ends=%.0f"),
				*Cfg.Name.ToString(),
				Cfg.bEnabled ? TEXT("yes") : TEXT("no"),
				Cfg.JumpLength, Cfg.JumpDistanceFromEdge, Cfg.JumpHeight, Cfg.JumpMaxDepth,
				Cfg.JumpEndsHeightTolerance);
			if (Cfg.Name.ToString().Equals(TEXT("AirDiveDown"), ESearchCase::IgnoreCase)
				|| (AirDiveJumpLengthCm <= 0.f && CLNavTune::IsAirDiveLink(Cfg.Name)))
			{
				AirDiveJumpLengthCm = Cfg.JumpLength;
				AirDiveJumpMaxDepthCm = Cfg.JumpMaxDepth;
				AirDiveJumpHeightCm = Cfg.JumpHeight;
			}
		}
	}

	if (Layout == ECLGreyboxLayout::PvpThreeLane)
	{
		DestroyGreyboxAirDiveLinks(*World);
	}

	// Octree ignores geometry until Recast exists. Push cubes after Create.
	for (UStaticMeshComponent* Mesh : Platforms)
	{
		if (Mesh && !Mesh->ComponentHasTag(FName(TEXT("CLCoverObstacle")))
			&& !Mesh->ComponentHasTag(FName(TEXT("CLVaultableCover"))))
		{
			Mesh->SetCanEverAffectNavigation(true);
			FNavigationSystem::UpdateComponentData(*Mesh);
		}
	}

	const double BakeStart = FPlatformTime::Seconds();
	NavSys->Build();
	EdgePadBakeMs = static_cast<float>((FPlatformTime::Seconds() - BakeStart) * 1000.0);
	UE_LOG(LogCalling, Display, TEXT("Greybox nav bake ms=%.0f"), EdgePadBakeMs);
	EdgePadOffMesh = 0;
	EdgePadValidEndsMax = 0;
	if (Recast)
	{
		FRecastDebugGeometry Geo;
		Recast->GetDebugGeometryForTile(Geo, FNavTileRef());
		EdgePadOffMesh = Geo.OffMeshLinks.Num();
		TArray<FSupportedAreaData> Areas;
		Recast->GetSupportedAreas(Areas);
		const int32 AirDiveId = Recast->GetAreaID(UCLNavArea_AirDive::StaticClass());
		UE_LOG(LogCalling, Display, TEXT("Greybox AirDive areaId=%d supported=%d"),
			AirDiveId, Areas.Num());
		int32 NearPad = 0;
		for (const FRecastDebugGeometry::FOffMeshLink& Lnk : Geo.OffMeshLinks)
		{
			if (bHasEdgePad)
			{
				const float ToPad = FMath::Min(
					FVector::Dist(Lnk.Right, CachedEdgePad),
					FVector::Dist(Lnk.Left, CachedEdgePad));
				if (ToPad < 1000.f)
				{
					++NearPad;
					EdgePadValidEndsMax = FMath::Max(EdgePadValidEndsMax, static_cast<int32>(Lnk.ValidEnds));
					UE_LOG(LogCalling, Display,
						TEXT("Greybox offMesh nearPad toPad=%.0f validEnds=%u gen=%s L=%s R=%s"),
						ToPad, (unsigned)Lnk.ValidEnds,
						Lnk.bIsGenerated ? TEXT("yes") : TEXT("no"),
						*Lnk.Left.ToCompactString(), *Lnk.Right.ToCompactString());
				}
			}
		}
		UE_LOG(LogCalling, Display,
			TEXT("Greybox offMesh count=%d nearPad=%d validEndsMax=%d cellH=%.0f radius=%.0f tile=%.0f"),
			EdgePadOffMesh, NearPad, EdgePadValidEndsMax,
			Recast->GetCellHeight(ENavigationDataResolution::Default),
			Recast->AgentRadius,
			Recast->TileSizeUU);
	}
	bFindPathMeshOk = false;
	bEdgePadLipOk = false;
	bEdgePadPadOk = false;
	bEdgePadPartial = false;
	EdgePadPathPoints = 0;
	if (Layout == ECLGreyboxLayout::PvpThreeLane)
	{
		FCLPvpThreeLaneRecipe R;
		R.Load();
		FVector Lip;
		FVector Pad;
		R.EdgeAirDiveEnds(Lip, Pad, EdgePadPlaceChordCm(), EdgeAirDiveEdgeCm(), EdgePadDropFromLipCm());
		Lip.Z += 96.f;
		Pad.Z += 96.f;
		FNavLocation ProjLip;
		FNavLocation ProjPad;
		const FVector Extent(800.f, 800.f, 4000.f);
		bEdgePadLipOk = NavSys->ProjectPointToNavigation(Lip, ProjLip, Extent);
		bEdgePadPadOk = NavSys->ProjectPointToNavigation(Pad, ProjPad, Extent);
		if (bEdgePadLipOk && bEdgePadPadOk)
		{
			if (UNavigationPath* Path = UNavigationSystemV1::FindPathToLocationSynchronously(
				World, ProjLip.Location, ProjPad.Location, nullptr))
			{
				EdgePadPathPoints = Path->PathPoints.Num();
				const bool bValid = Path->IsValid();
				bEdgePadPartial = Path->IsPartial();
				if (EdgePadPathPoints >= 2)
				{
					UE_LOG(LogCalling, Display, TEXT("Greybox edgePad path valid=%s partial=%s p0=%s pN=%s"),
						bValid ? TEXT("yes") : TEXT("no"),
						bEdgePadPartial ? TEXT("yes") : TEXT("no"),
						*Path->PathPoints[0].ToCompactString(),
						*Path->PathPoints.Last().ToCompactString());
					bFindPathMeshOk = bValid && !bEdgePadPartial
						&& CLNavPathUtil::PathEndsOnFromAndDestNav(
							*NavSys, Path->PathPoints, ProjLip, ProjPad, Extent);
				}
			}
		}
		UE_LOG(LogCalling, Display,
			TEXT("Greybox edgePad FindPath meshOk=%s points=%d airDiveLen=%.0f height=%.0f maxDepth=%.0f lipOk=%s padOk=%s offMesh=%d validEndsMax=%d"),
			bFindPathMeshOk ? TEXT("yes") : TEXT("no"),
			EdgePadPathPoints,
			AirDiveJumpLengthCm,
			AirDiveJumpHeightCm,
			AirDiveJumpMaxDepthCm,
			bEdgePadLipOk ? TEXT("yes") : TEXT("no"),
			bEdgePadPadOk ? TEXT("yes") : TEXT("no"),
			EdgePadOffMesh,
			EdgePadValidEndsMax);
	}
	const FBox NavBox = Recast ? Recast->GetBounds() : FBox(ForceInit);
	const bool bPadInNav = bHasEdgePad && NavBox.IsValid
		&& CachedEdgePad.X >= NavBox.Min.X && CachedEdgePad.X <= NavBox.Max.X
		&& CachedEdgePad.Y >= NavBox.Min.Y && CachedEdgePad.Y <= NavBox.Max.Y
		&& CachedEdgePad.Z >= NavBox.Min.Z && CachedEdgePad.Z <= NavBox.Max.Z;
	UE_LOG(LogCalling, Display,
		TEXT("Greybox edgePad AABB pad=%s geoZ=[%.0f,%.0f] navZ=[%.0f,%.0f] padInNav=%s"),
		*CachedEdgePad.ToCompactString(),
		Box.Min.Z, Box.Max.Z,
		NavBox.Min.Z, NavBox.Max.Z,
		bPadInNav ? TEXT("yes") : TEXT("no"));
	UE_LOG(LogCalling, Display, TEXT("Greybox nav rebuilt geo=%s nav=%s tiles=%d pads=%d jumpLinks=%s"),
		*Box.ToString(),
		*NavBox.ToString(),
		Recast ? Recast->GetNumActiveTiles() : 0,
		Platforms.Num(),
		(Recast && Recast->bGenerateNavLinks) ? TEXT("on") : TEXT("off"));
}
