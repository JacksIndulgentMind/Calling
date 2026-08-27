#include "Player/DLPlayerCharacter.h"
#include "Player/DLPlayerActionRouter.h"
#include "Player/DLPossessionComponent.h"
#include "Input/DLInputTypes.h"
#include "Player/DLCombatMovementComponent.h"
#include "Player/DLHealthShieldComponent.h"
#include "Combat/DLDamageableComponent.h"
#include "Combat/DLEffectStackComponent.h"
#include "Player/DLWeaponMotorComponent.h"
#include "Player/DLWeaponBehaviorComponent.h"
#include "Player/DLAbilityLoadoutComponent.h"
#include "Player/DLViewWeapon.h"
#include "Player/DLCombatPawn.h"
#include "Player/DLPathfinderCharacter.h"
#include "Player/DLWardenCharacter.h"
#include "Game/DLProfileSubsystem.h"
#include "Game/DLGameInstance.h"
#include "Game/DLGameModeBase.h"
#include "Game/DLLobbySubsystem.h"
#include "Game/DLGameStateBase.h"
#include "Loot/DLLootRulesService.h"
#include "Core/DLTypes.h"
#include "Core/DLTunes.h"
#include "Core/DLTickClock.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/DamageType.h"
#include "Components/CapsuleComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "TimerManager.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "UObject/ConstructorHelpers.h"

ADLPlayerCharacter::ADLPlayerCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UDLCombatMovementComponent>(ACharacter::CharacterMovementComponentName))
{
	PrimaryActorTick.bCanEverTick = true;
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = true;
	bUseControllerRotationRoll = false;

	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.f);
	JumpMaxCount = 1;

	CombatMovement = Cast<UDLCombatMovementComponent>(GetCharacterMovement());
	if (CombatMovement)
	{
		CombatMovement->bOrientRotationToMovement = false;
		CombatMovement->bUseControllerDesiredRotation = true;
	}

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 0.f;
	CameraBoom->bUsePawnControlRotation = true;
	CameraBoom->bDoCollisionTest = false;

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;
	FollowCamera->FieldOfView = 90.f;

	BodyMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BodyMesh"));
	BodyMesh->SetupAttachment(RootComponent);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> BodyFinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (BodyFinder.Succeeded())
	{
		BodyMesh->SetStaticMesh(BodyFinder.Object);
	}
	BodyMesh->SetRelativeLocation(FVector(0.f, 0.f, -96.f));
	BodyMesh->SetRelativeScale3D(FVector(0.75f, 0.75f, 1.92f));
	BodyMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BodyMesh->SetCastShadow(false);

	HandR = CreateDefaultSubobject<USceneComponent>(TEXT("hand_r"));
	HandR->SetupAttachment(BodyMesh);
	HandR->SetRelativeLocation(FVector(28.f, 22.f, 110.f));
	HandL = CreateDefaultSubobject<USceneComponent>(TEXT("hand_l"));
	HandL->SetupAttachment(BodyMesh);
	HandL->SetRelativeLocation(FVector(28.f, -22.f, 110.f));

	ViewKnife = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ViewKnife"));
	ViewKnife->SetupAttachment(FollowCamera);
	ViewKnife->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ViewKnife->SetCastShadow(false);
	ViewKnife->SetHiddenInGame(true);
	ViewKnife->SetRelativeLocation(FVector(22.f, 12.f, -8.f));
	ViewKnife->SetRelativeScale3D(FVector(0.04f, 0.012f, 0.18f));
	WorldKnife = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WorldKnife"));
	WorldKnife->SetupAttachment(HandR);
	WorldKnife->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	WorldKnife->SetCastShadow(false);
	WorldKnife->SetHiddenInGame(true);
	WorldKnife->SetRelativeLocation(FVector(8.f, 0.f, 6.f));
	WorldKnife->SetRelativeScale3D(FVector(0.05f, 0.016f, 0.22f));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> KnifeFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (KnifeFinder.Succeeded())
	{
		ViewKnife->SetStaticMesh(KnifeFinder.Object);
		WorldKnife->SetStaticMesh(KnifeFinder.Object);
	}

	HealthShield = CreateDefaultSubobject<UDLHealthShieldComponent>(TEXT("HealthShield"));
	Damageable = CreateDefaultSubobject<UDLDamageableComponent>(TEXT("Damageable"));
	EffectStack = CreateDefaultSubobject<UDLEffectStackComponent>(TEXT("EffectStack"));
	WeaponBehavior = CreateDefaultSubobject<UDLWeaponBehaviorComponent>(TEXT("WeaponBehavior"));
	WeaponMotor = CreateDefaultSubobject<UDLWeaponMotorComponent>(TEXT("WeaponMotor"));
	Abilities = CreateDefaultSubobject<UDLAbilityLoadoutComponent>(TEXT("Abilities"));
	Possession = CreateDefaultSubobject<UDLPossessionComponent>(TEXT("Possession"));
}

void ADLPlayerCharacter::BeginPlay()
{
	Super::BeginPlay();

	if (CombatMovement)
	{
		JumpMaxCount = FMath::Max(1, CombatMovement->GetTune().MaxJumps);
	}

	if (WeaponMotor)
	{
		WeaponMotor->Behavior = WeaponBehavior;
		WeaponMotor->Camera = FollowCamera;
	}

	ViewWeaponRoot = DLViewWeapon::BuildOnCamera(FollowCamera);
	WorldWeaponRoot = DLViewWeapon::BuildOnBody(GetCapsuleComponent());

	ApplyProfileLoadout();

	if (APlayerController* PC = Cast<APlayerController>(Controller))
	{
		FRotator Control = PC->GetControlRotation();
		if (FMath::Abs(Control.Pitch) < 1.f)
		{
			Control.Pitch = -12.f;
			PC->SetControlRotation(Control);
		}
		PC->SetInputMode(FInputModeGameOnly());
		PC->bShowMouseCursor = false;
	}

	if (Damageable)
	{
		Damageable->OnDamaged.AddDynamic(this, &ADLPlayerCharacter::HandleDamaged);
	}
	if (HealthShield)
	{
		HealthShield->OnDeath.AddDynamic(this, &ADLPlayerCharacter::HandleDeath);
	}

	if (UGameInstance* GI = GetGameInstance())
	{
		if (UDLTickSubsystem* Tick = GI->GetSubsystem<UDLTickSubsystem>())
		{
			FixedTickHandle = Tick->OnFixedGameTick().AddUObject(this, &ADLPlayerCharacter::HandleFixedGameTick);
		}
		if (UDLLobbySubsystem* Lobby = GI->GetSubsystem<UDLLobbySubsystem>())
		{
			if (Possession && IsLocallyControlled() && !IsA<ADLCombatPawn>())
			{
				Possession->PossessOwn(this);
			}
		}
	}
}

void ADLPlayerCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	HurtAge += DeltaSeconds;
	BodyFlinchRot = FMath::RInterpTo(BodyFlinchRot, FRotator::ZeroRotator, DeltaSeconds, 12.f);
	if (BodyMesh)
	{
		FRotator Loco = FRotator::ZeroRotator;
		if (CombatMovement)
		{
			if (CombatMovement->IsDodging())
			{
				Loco.Yaw = 360.f * CombatMovement->GetDodgeAlpha();
			}
			if (CombatMovement->IsDiving())
			{
				Loco.Pitch = CombatMovement->GetDiveFlipDegrees();
			}
			const float Knee = CombatMovement->GetKneeLandAlpha();
			if (Knee > 0.f)
			{
				Loco.Pitch += 38.f * Knee;
				Loco.Roll += 8.f * Knee;
			}
		}
		BodyMesh->SetRelativeRotation(Loco + BodyFlinchRot);
	}

	if (UGameInstance* GI = GetGameInstance())
	{
		if (UDLLobbySubsystem* Lobby = GI->GetSubsystem<UDLLobbySubsystem>())
		{
			if (!Lobby->IsGameplayUnlocked())
			{
				AccumulatedLook = FVector2D::ZeroVector;
				AgentLook = FVector2D::ZeroVector;
				return;
			}
		}
	}
	// Render tick: look is applied immediately for responsiveness; move/fire consumed on fixed sim.
	if (Controller && (!AccumulatedLook.IsNearlyZero() || !AgentLook.IsNearlyZero()))
	{
		FDLAgentLookTune LookTune;
		LookTune.LoadFromIni();
		const float MaxYaw = LookTune.MaxYawRateDegPerSec * DeltaSeconds;
		const float MaxPitch = LookTune.MaxPitchRateDegPerSec * DeltaSeconds;
		AgentLook.X = FMath::Clamp(AgentLook.X, -MaxYaw, MaxYaw);
		AgentLook.Y = FMath::Clamp(AgentLook.Y, -MaxPitch, MaxPitch);
		AddControllerYawInput(AccumulatedLook.X + AgentLook.X);
		AddControllerPitchInput(AccumulatedLook.Y + AgentLook.Y);
		AgentLook = FVector2D::ZeroVector;
		AccumulatedLook = FVector2D::ZeroVector;
	}
	TickAgentLook(DeltaSeconds);

	if (HealthShield && Controller)
	{
		const bool bAds = WeaponMotor && WeaponMotor->GetAdsAlpha() > 0.5f;
		FVector2D Kick = HealthShield->ConsumeHipKick();
		if (WeaponMotor)
		{
			Kick += WeaponMotor->ConsumeHipRecoil();
		}
		if (!bAds && !Kick.IsNearlyZero())
		{
			FRotator Control = Controller->GetControlRotation();
			Control.Yaw += Kick.X;
			const float Pitch = FRotator::NormalizeAxis(Control.Pitch);
			Control.Pitch = FMath::Clamp(Pitch + Kick.Y, -89.f, 89.f);
			Controller->SetControlRotation(Control);
			FDLAgentLookTune RecoilTune;
			RecoilTune.LoadFromIni();
			if (RecoilCorrectRemaining <= 0.f)
			{
				RecoilCorrectRemaining = RecoilTune.RecoilCorrectDelay;
			}
			bRecoilPitchSlow = true;
		}
	}

	if (WeaponMotor)
	{
		const bool bGrenade = WeaponMotor->UsesGrenadeProjectile();
		const FName Sight = WeaponMotor->GetSightId();
		if (ViewWeaponRoot)
		{
			DLViewWeapon::UpdateAdsPose(ViewWeaponRoot, WeaponMotor->GetAdsEase(), Sight, WeaponMotor->GetViewKickPitch());
			DLViewWeapon::ShowFamily(ViewWeaponRoot, bGrenade);
			DLViewWeapon::ShowSight(ViewWeaponRoot, Sight);
		}
		if (WorldWeaponRoot)
		{
			DLViewWeapon::ShowFamily(WorldWeaponRoot, bGrenade);
			DLViewWeapon::ShowSight(WorldWeaponRoot, Sight);
		}
	}
	UpdateThirdPersonPeek(DeltaSeconds);
	TickKnifeSlash(DeltaSeconds);
}

void ADLPlayerCharacter::UpdateThirdPersonPeek(float DeltaSeconds)
{
	if (!CameraBoom || !FollowCamera || !FollowCamera->IsActive() || IsA(ADLCombatPawn::StaticClass()))
	{
		return;
	}

	FDLWeaponMotorTune Cam;
	Cam.LoadFromIni();
	const bool bBurst = CombatMovement &&
		(CombatMovement->IsDodging() || CombatMovement->IsDashing() || CombatMovement->IsDiving()
			|| CombatMovement->IsKneeLanding());
	if (bBurst)
	{
		ThirdPersonHold = Cam.ThirdPersonMinHold;
	}
	else
	{
		ThirdPersonHold = FMath::Max(0.f, ThirdPersonHold - DeltaSeconds);
	}

	const float Target = (bBurst || ThirdPersonHold > 0.f) ? 1.f : 0.f;
	const float Dur = Target > ThirdPersonAlpha
		? FMath::Max(0.05f, Cam.ThirdPersonBlendIn)
		: FMath::Max(0.05f, Cam.ThirdPersonBlendOut);
	ThirdPersonAlpha = FMath::FInterpConstantTo(ThirdPersonAlpha, Target, DeltaSeconds, 1.f / Dur);
	const float Ease = FMath::SmoothStep(0.f, 1.f, ThirdPersonAlpha);
	CameraBoom->TargetArmLength = Ease * Cam.ThirdPersonArmLength;
	CameraBoom->SocketOffset = FVector(0.f, 0.f, 28.f * Ease);

	const bool bGrenade = WeaponMotor && WeaponMotor->UsesGrenadeProjectile();
	const FName Sight = WeaponMotor ? WeaponMotor->GetSightId() : NAME_None;
	DLViewWeapon::SetThirdPersonPeek(ViewWeaponRoot, WorldWeaponRoot, Ease > 0.05f, bGrenade, Sight);
	if (ViewKnife)
	{
		const bool bSlash = KnifeSlashRemaining > 0.f;
		ViewKnife->SetVisibility(bSlash && Ease <= 0.05f, true);
	}
	if (WorldKnife)
	{
		WorldKnife->SetOwnerNoSee(Ease <= 0.05f);
	}
}

void ADLPlayerCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (Damageable)
	{
		Damageable->OnDamaged.RemoveDynamic(this, &ADLPlayerCharacter::HandleDamaged);
	}
	if (HealthShield)
	{
		HealthShield->OnDeath.RemoveDynamic(this, &ADLPlayerCharacter::HandleDeath);
	}
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UDLTickSubsystem* Tick = GI->GetSubsystem<UDLTickSubsystem>())
		{
			Tick->OnFixedGameTick().Remove(FixedTickHandle);
		}
	}
	Super::EndPlay(EndPlayReason);
}

void ADLPlayerCharacter::HandleDamaged(float RemainingHealth, float Applied)
{
	(void)RemainingHealth;
	if (Applied <= 0.f)
	{
		return;
	}
	HurtAge = 0.f;
	const float Mag = FMath::FRandRange(8.f, 14.f);
	BodyFlinchRot = FRotator(
		FMath::FRandRange(-Mag, Mag),
		FMath::FRandRange(-Mag, Mag),
		FMath::FRandRange(-Mag * 0.25f, Mag * 0.25f));
}

bool ADLPlayerCharacter::IsCombatAlive() const
{
	if (bTakenOut)
	{
		return false;
	}
	if (const UDLHealthShieldComponent* HS = HealthShield)
	{
		return HS->IsAlive();
	}
	if (const UDLDamageableComponent* Dmg = Damageable)
	{
		return Dmg->IsAlive();
	}
	return true;
}

void ADLPlayerCharacter::NoteIncomingDamage(AController* InstigatorController, float Applied)
{
	if (Applied <= 0.f || !InstigatorController || InstigatorController == GetController())
	{
		return;
	}
	const UWorld* World = GetWorld();
	const float Now = World ? World->GetTimeSeconds() : 0.f;
	DamageTimes.FindOrAdd(InstigatorController) = Now;
	LastDamageInstigator = InstigatorController;
}

void ADLPlayerCharacter::HandleDeath()
{
	if (bTakenOut)
	{
		return;
	}
	if (const UWorld* World = GetWorld())
	{
		if (World->GetNetMode() == NM_Client)
		{
			bTakenOut = true;
			return;
		}
	}
	bTakenOut = true;
	ClearAgentIntent();
	if (WeaponMotor)
	{
		WeaponMotor->SetWantsFire(false);
		WeaponMotor->SetWantsADS(false);
	}
	if (CombatMovement)
	{
		CombatMovement->SetMoveInput(FVector2D::ZeroVector);
		CombatMovement->SetWantsSprint(false);
	}

	TArray<AController*> Assists;
	const UWorld* World = GetWorld();
	const float Now = World ? World->GetTimeSeconds() : 0.f;
	AController* Killer = LastDamageInstigator.Get();
	for (const TPair<TWeakObjectPtr<AController>, float>& Pair : DamageTimes)
	{
		AController* Other = Pair.Key.Get();
		if (!Other || Other == Killer)
		{
			continue;
		}
		if (Now - Pair.Value <= 4.f)
		{
			Assists.Add(Other);
		}
	}
	if (ADLGameStateBase* GS = World ? World->GetGameState<ADLGameStateBase>() : nullptr)
	{
		GS->RegisterTakeOut(Killer, this, Assists);
	}

	if (World)
	{
		World->GetTimerManager().SetTimer(RespawnTimer, this, &ADLPlayerCharacter::RequestTakeOutRespawn, 2.f, false);
	}
}

void ADLPlayerCharacter::RequestTakeOutRespawn()
{
	if (UWorld* World = GetWorld())
	{
		if (ADLGameModeBase* GM = World->GetAuthGameMode<ADLGameModeBase>())
		{
			GM->RequestRespawn(GetController());
		}
	}
}

void ADLPlayerCharacter::NotifyRespawned()
{
	bTakenOut = false;
	DamageTimes.Reset();
	LastDamageInstigator.Reset();
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RespawnTimer);
	}
}

void ADLPlayerCharacter::PlayKnifeSlash()
{
	KnifeSlashRemaining = 0.25f;
	if (ViewKnife)
	{
		ViewKnife->SetHiddenInGame(false);
		ViewKnife->SetVisibility(true, true);
	}
	if (WorldKnife)
	{
		WorldKnife->SetHiddenInGame(false);
		WorldKnife->SetVisibility(true, true);
	}
}

void ADLPlayerCharacter::TickKnifeSlash(float DeltaSeconds)
{
	if (KnifeSlashRemaining <= 0.f)
	{
		return;
	}
	KnifeSlashRemaining = FMath::Max(0.f, KnifeSlashRemaining - DeltaSeconds);
	const float Alpha = 1.f - (KnifeSlashRemaining / 0.25f);
	const FRotator Slash(FMath::Lerp(18.f, -42.f, Alpha), FMath::Lerp(-55.f, 70.f, Alpha), 0.f);
	if (ViewKnife)
	{
		ViewKnife->SetRelativeRotation(Slash);
	}
	if (WorldKnife)
	{
		WorldKnife->SetRelativeRotation(Slash);
	}
	if (KnifeSlashRemaining <= 0.f)
	{
		if (ViewKnife)
		{
			ViewKnife->SetHiddenInGame(true);
			ViewKnife->SetVisibility(false, true);
		}
		if (WorldKnife)
		{
			WorldKnife->SetHiddenInGame(true);
			WorldKnife->SetVisibility(false, true);
		}
	}
}

float ADLPlayerCharacter::GetHurtAlpha() const
{
	if (HurtAge < 0.05f)
	{
		return FMath::Clamp(HurtAge / 0.05f, 0.f, 1.f);
	}
	const float Out = (HurtAge - 0.05f) / 0.75f;
	return FMath::Clamp(1.f - Out, 0.f, 1.f);
}

FVector2D ADLPlayerCharacter::GetAdsReticlePunch() const
{
	if (!WeaponMotor || WeaponMotor->GetAdsAlpha() <= 0.4f)
	{
		return FVector2D::ZeroVector;
	}
	FVector2D Punch = FVector2D::ZeroVector;
	if (HealthShield)
	{
		Punch += HealthShield->GetReticlePunch();
	}
	Punch += WeaponMotor->GetAdsRecoilPunch();
	return Punch;
}

bool ADLPlayerCharacter::UsesViewWeapon() const
{
	return IsLocallyControlled() && FollowCamera && FollowCamera->IsActive();
}

void ADLPlayerCharacter::SetDemoViewActive(bool bActive)
{
	(void)bActive;
}

void ADLPlayerCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	// Binding is done in ADLPlayerController via Enhanced Input.
}

void ADLPlayerCharacter::Jump()
{
	if (!IsCombatAlive())
	{
		return;
	}
	if (!CombatMovement)
	{
		Super::Jump();
		return;
	}
	if (CombatMovement->IsDiving())
	{
		return;
	}

	if (CombatMovement->IsMovingOnGround())
	{
		Super::Jump();
		CombatMovement->NotifyJumped();
		return;
	}

	// Air jump is owned by the Jump slot (rocket pulse). Inertia dampers do not pulse from Space.
	if (CombatMovement->GetJumpsRemaining() > 0)
	{
		CombatMovement->NotifyJumped();
	}
}

void ADLPlayerCharacter::Landed(const FHitResult& Hit)
{
	Super::Landed(Hit);
	if (CombatMovement)
	{
		CombatMovement->NotifyLanded();
	}
}

void ADLPlayerCharacter::FellOutOfWorld(const UDamageType& DmgType)
{
	if (UWorld* World = GetWorld())
	{
		if (ADLGameModeBase* GM = World->GetAuthGameMode<ADLGameModeBase>())
		{
			GM->RequestRespawn(GetController());
			return;
		}
	}
	Super::FellOutOfWorld(DmgType);
}

bool ADLPlayerCharacter::IsSliding() const
{
	return CombatMovement && CombatMovement->IsSliding();
}

void ADLPlayerCharacter::SetRippleCamo(bool bEnabled)
{
	if (!BodyMesh)
	{
		return;
	}
	BodyMesh->SetWorldScale3D(bEnabled ? FVector(0.35f, 0.35f, 1.92f) : FVector(0.75f, 0.75f, 1.92f));
	BodyMesh->SetCastShadow(!bEnabled);
}

void ADLPlayerCharacter::AccumulateInput(FVector2D MoveXY, FVector2D LookDelta, bool bSprint, bool bCrouch, bool bADS, bool bFire)
{
	AccumulatedMove = MoveXY;
	AccumulatedLook += LookDelta;
	bAccumSprint = bSprint;
	bAccumCrouch = bCrouch;
	bAccumADS = bADS;
	bAccumFire = bFire;
}

void ADLPlayerCharacter::HandleFixedGameTick(float DeltaSeconds)
{
	(void)DeltaSeconds;
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UDLLobbySubsystem* Lobby = GI->GetSubsystem<UDLLobbySubsystem>())
		{
			if (!Lobby->IsGameplayUnlocked())
			{
				ConsumeSimInput(FVector2D::ZeroVector, FVector2D::ZeroVector, false, false, false, false);
				return;
			}
		}
	}
	ConsumeSimInput(
		AccumulatedMove + AgentMove,
		FVector2D::ZeroVector,
		bAccumSprint || bAgentSprint,
		bAccumCrouch || bAgentCrouch,
		bAccumADS || bAgentADS,
		bAccumFire || bAgentFire);
	ConsumeAgentPulses();
}

void ADLPlayerCharacter::ApplyAgentIntent(const FDLAgentIntent& Intent)
{
	AgentMove = Intent.Move;
	AgentLook += Intent.Look;
	bAgentSprint = Intent.bSprint;
	bAgentCrouch = Intent.bCrouch;
	bAgentADS = Intent.bADS;
	bAgentFire = Intent.bFire;
	if (Intent.bJump) { bAgentJump = true; }
	if (Intent.bDodge) { bAgentDodge = true; }
	if (Intent.bDash) { bAgentDash = true; }
	if (Intent.bReload) { bAgentReload = true; }
	if (Intent.bSwap) { bAgentSwap = true; }
	if (Intent.bSlide) { bAgentSlide = true; }
	if (Intent.bAirDive) { bAgentAirDive = true; }
	if (Intent.bMelee) { bAgentMelee = true; }
	if (Intent.bWeaponPrimary) { bAgentWeaponPrimary = true; }
	if (Intent.bWeaponSpecial) { bAgentWeaponSpecial = true; }
	if (!Intent.SightId.IsNone()) { AgentSightId = Intent.SightId; }
}

void ADLPlayerCharacter::ApplyAgentIntent(FVector2D MoveXY, FVector2D LookDelta, bool bSprint, bool bCrouch, bool bADS, bool bFire,
	bool bJump, bool bDodge, bool bDash, bool bReload, bool bSwap)
{
	FDLAgentIntent Intent;
	Intent.Move = MoveXY;
	Intent.Look = LookDelta;
	Intent.bSprint = bSprint;
	Intent.bCrouch = bCrouch;
	Intent.bADS = bADS;
	Intent.bFire = bFire;
	Intent.bJump = bJump;
	Intent.bDodge = bDodge;
	Intent.bDash = bDash;
	Intent.bReload = bReload;
	Intent.bSwap = bSwap;
	ApplyAgentIntent(Intent);
}

void ADLPlayerCharacter::ClearAgentIntent()
{
	AgentMove = FVector2D::ZeroVector;
	AgentLook = FVector2D::ZeroVector;
	bAgentSprint = false;
	bAgentCrouch = false;
	bAgentADS = false;
	bAgentFire = false;
	bAgentJump = false;
	bAgentDodge = false;
	bAgentDash = false;
	bAgentReload = false;
	bAgentSwap = false;
	bAgentSlide = false;
	bAgentAirDive = false;
	bAgentMelee = false;
	bAgentWeaponPrimary = false;
	bAgentWeaponSpecial = false;
	AgentSightId = NAME_None;
	ClearAgentLook();
}

void ADLPlayerCharacter::ClearAgentLook()
{
	bLookGoalYaw = false;
	bLookGoalPitch = false;
	LookGoalYaw = 0.f;
	LookGoalPitch = 0.f;
	bLookTrack = false;
	LookTrackSeatId.Invalidate();
	bLookStickyValid = false;
	LookSticky = FVector::ZeroVector;
	LookTrackReactRemaining = 0.f;
}

void ADLPlayerCharacter::SetLookGoalYawPitch(bool bYaw, float Yaw, bool bPitch, float Pitch)
{
	bLookTrack = false;
	LookTrackSeatId.Invalidate();
	bLookStickyValid = false;
	LookTrackReactRemaining = 0.f;
	bLookGoalYaw = bYaw;
	bLookGoalPitch = bPitch;
	if (bYaw)
	{
		LookGoalYaw = Yaw;
	}
	if (bPitch)
	{
		LookGoalPitch = Pitch;
	}
}

void ADLPlayerCharacter::ApplyAgentLookCommand(const FDLLookCommand& Look)
{
	if (!Look.HasAbsolute())
	{
		return;
	}
	const bool bYaw = Look.Mode == EDLLookMode::AbsYaw || Look.Mode == EDLLookMode::AbsBoth;
	const bool bPitch = Look.Mode == EDLLookMode::AbsPitch || Look.Mode == EDLLookMode::AbsBoth;
	SetLookGoalYawPitch(bYaw, Look.Value.X, bPitch, Look.Value.Y);
}

void ADLPlayerCharacter::SetLookTrackSeat(const FGuid& SeatId)
{
	if (!SeatId.IsValid())
	{
		return;
	}
	bLookGoalYaw = false;
	bLookGoalPitch = false;
	if (bLookTrack && LookTrackSeatId == SeatId)
	{
		return;
	}
	bLookTrack = true;
	LookTrackSeatId = SeatId;
	bLookStickyValid = false;
	LookSticky = FVector::ZeroVector;
	LookTrackReactRemaining = 0.f;
}

void ADLPlayerCharacter::ApplyAgentLookFromStep(const FGuid& TrackSeatId, const FDLLookCommand& Look)
{
	if (TrackSeatId.IsValid())
	{
		SetLookTrackSeat(TrackSeatId);
		return;
	}
	ApplyAgentLookCommand(Look);
}

void ADLPlayerCharacter::SlewControlToward(const FRotator& Desired, float DeltaSeconds, bool bSlewPitch, float PitchRateDegPerSec)
{
	AController* Ctrl = GetController();
	if (!Ctrl)
	{
		return;
	}
	FDLAgentLookTune Tune;
	Tune.LoadFromIni();
	FRotator Cur = Ctrl->GetControlRotation();
	const float YawStep = Tune.MaxYawRateDegPerSec * DeltaSeconds;
	const float YawDelta = FRotator::NormalizeAxis(Desired.Yaw - Cur.Yaw);
	Cur.Yaw += FMath::Clamp(YawDelta, -YawStep, YawStep);
	if (bSlewPitch)
	{
		const float PitchStep = FMath::Max(1.f, PitchRateDegPerSec) * DeltaSeconds;
		const float PitchGoal = FMath::Clamp(FRotator::NormalizeAxis(Desired.Pitch), -89.f, 89.f);
		const float PitchNow = FMath::Clamp(FRotator::NormalizeAxis(Cur.Pitch), -89.f, 89.f);
		Cur.Pitch = FMath::Clamp(PitchNow + FMath::Clamp(PitchGoal - PitchNow, -PitchStep, PitchStep), -89.f, 89.f);
	}
	Ctrl->SetControlRotation(Cur);
}

void ADLPlayerCharacter::TickAgentLook(float DeltaSeconds)
{
	AController* Ctrl = GetController();
	if (!Ctrl)
	{
		return;
	}

	if (RecoilCorrectRemaining > 0.f)
	{
		RecoilCorrectRemaining = FMath::Max(0.f, RecoilCorrectRemaining - DeltaSeconds);
	}

	FRotator Desired = Ctrl->GetControlRotation();
	bool bSlew = false;
	if (bLookTrack && LookTrackSeatId.IsValid())
	{
		AActor* Target = nullptr;
		if (UGameInstance* GI = GetGameInstance())
		{
			if (UDLLobbySubsystem* Lobby = GI->GetSubsystem<UDLLobbySubsystem>())
			{
				Target = Lobby->GetDrivenPawn(LookTrackSeatId);
			}
		}
		if (Target)
		{
			FDLAgentLookTune Tune;
			Tune.LoadFromIni();
			const FVector Loc = Target->GetActorLocation();
			if (!bLookStickyValid)
			{
				LookSticky = Loc;
				bLookStickyValid = true;
				LookTrackReactRemaining = 0.f;
			}
			else if (FVector::Dist(Loc, LookSticky) > Tune.TrackMoveEpsilonCm && LookTrackReactRemaining <= 0.f)
			{
				LookTrackReactRemaining = Tune.TrackReactSeconds;
			}
			if (LookTrackReactRemaining > 0.f)
			{
				LookTrackReactRemaining -= DeltaSeconds;
				if (LookTrackReactRemaining <= 0.f)
				{
					LookSticky = Loc;
					LookTrackReactRemaining = 0.f;
				}
			}
		}
		if (bLookStickyValid)
		{
			const FVector To = LookSticky - GetActorLocation();
			if (!To.IsNearlyZero())
			{
				Desired = To.Rotation();
				bSlew = true;
			}
		}
	}
	else if (bLookGoalYaw || bLookGoalPitch)
	{
		if (bLookGoalYaw)
		{
			Desired.Yaw = LookGoalYaw;
		}
		if (bLookGoalPitch)
		{
			Desired.Pitch = LookGoalPitch;
		}
		bSlew = true;
	}

	if (bSlew)
	{
		FDLAgentLookTune Tune;
		Tune.LoadFromIni();
		const bool bSlewPitch = RecoilCorrectRemaining <= 0.f;
		float PitchRate = Tune.MaxPitchRateDegPerSec;
		if (bSlewPitch && bRecoilPitchSlow)
		{
			PitchRate = Tune.RecoilCorrectPitchRate;
			const float PitchErr = FMath::Abs(FRotator::NormalizeAxis(Desired.Pitch) - FRotator::NormalizeAxis(Ctrl->GetControlRotation().Pitch));
			if (PitchErr < 1.f)
			{
				bRecoilPitchSlow = false;
			}
		}
		SlewControlToward(Desired, DeltaSeconds, bSlewPitch, PitchRate);
	}
}

void ADLPlayerCharacter::ConsumeAgentPulses()
{
	if (!IsCombatAlive())
	{
		bAgentJump = false;
		bAgentDodge = false;
		bAgentDash = false;
		bAgentReload = false;
		bAgentSwap = false;
		bAgentSlide = false;
		bAgentAirDive = false;
		bAgentMelee = false;
		bAgentWeaponPrimary = false;
		bAgentWeaponSpecial = false;
		return;
	}
	if (bAgentJump)
	{
		DLPlayerActionRouter::DispatchPulse(this, EDLBindableAction::Jump);
		bAgentJump = false;
	}
	if (bAgentDodge)
	{
		DLPlayerActionRouter::DispatchPulse(this, EDLBindableAction::Dodge);
		bAgentDodge = false;
	}
	if (bAgentDash)
	{
		DLPlayerActionRouter::DispatchPulse(this, EDLBindableAction::Dash);
		bAgentDash = false;
	}
	if (bAgentReload)
	{
		DLPlayerActionRouter::DispatchPulse(this, EDLBindableAction::Reload);
		bAgentReload = false;
	}
	if (bAgentSwap)
	{
		DLPlayerActionRouter::DispatchPulse(this, EDLBindableAction::Swap);
		bAgentSwap = false;
	}
	if (bAgentSlide)
	{
		DLPlayerActionRouter::DispatchPulse(this, EDLBindableAction::Slide);
		bAgentSlide = false;
	}
	if (bAgentAirDive)
	{
		DLPlayerActionRouter::DispatchPulse(this, EDLBindableAction::AirDive);
		bAgentAirDive = false;
	}
	if (bAgentMelee)
	{
		DLPlayerActionRouter::DispatchPulse(this, EDLBindableAction::Melee);
		bAgentMelee = false;
	}
	if (bAgentWeaponPrimary)
	{
		DLPlayerActionRouter::DispatchPulse(this, EDLBindableAction::WeaponPrimary);
		bAgentWeaponPrimary = false;
	}
	if (bAgentWeaponSpecial)
	{
		DLPlayerActionRouter::DispatchPulse(this, EDLBindableAction::WeaponSpecial);
		bAgentWeaponSpecial = false;
	}
	if (!AgentSightId.IsNone() && WeaponMotor)
	{
		WeaponMotor->SetSight(AgentSightId);
		AgentSightId = NAME_None;
	}
}

void ADLPlayerCharacter::ConsumeSimInput(FVector2D MoveXY, FVector2D LookDelta, bool bSprint, bool bCrouch, bool bADS, bool bFire)
{
	if (!IsCombatAlive())
	{
		MoveXY = FVector2D::ZeroVector;
		bSprint = false;
		bCrouch = false;
		bADS = false;
		bFire = false;
	}
	if (CombatMovement)
	{
		CombatMovement->SetMoveInput(MoveXY);
		CombatMovement->SetWantsSprint(bSprint);
		CombatMovement->SetWantsCrouch(bCrouch);
		CombatMovement->SetIsADS(bADS);
		if (WeaponMotor)
		{
			CombatMovement->SetAdsMovePenaltyOverride(WeaponMotor->GetAdsMovePenalty());
		}
	}

	if (WeaponMotor)
	{
		WeaponMotor->SetWantsADS(bADS);
		WeaponMotor->SetWantsFire(bFire);
	}

	if (!LookDelta.IsNearlyZero() && Controller)
	{
		AddControllerYawInput(LookDelta.X);
		AddControllerPitchInput(LookDelta.Y);
	}
}

void ADLPlayerCharacter::ApplyProfileLoadout()
{
	FDLCharacterAppearance Appearance;
	if (IsA<ADLPathfinderCharacter>())
	{
		Appearance.ClassId = EDLClassId::Pathfinder;
	}
	else if (IsA<ADLWardenCharacter>())
	{
		Appearance.ClassId = EDLClassId::Warden;
	}

	UDLGameInstance* GI = Cast<UDLGameInstance>(GetGameInstance());
	UDLProfileSubsystem* Profiles = (!bUseNpcLoadout && GI) ? GI->GetProfileSubsystem() : nullptr;
	UDLLootRulesService* Loot = GI ? GI->GetLootRulesService() : nullptr;
	if (Profiles && Profiles->HasActiveProfile())
	{
		Appearance = Profiles->GetActiveProfile().Character;
	}
	if (Abilities)
	{
		Abilities->LoadFromCharacter(Appearance);
	}

	if (!Loot || !WeaponMotor)
	{
		return;
	}

	const bool bHasProfile = !bUseNpcLoadout && Profiles && Profiles->HasActiveProfile();
	const FDLLocalProfile Profile = bHasProfile ? Profiles->GetActiveProfile() : FDLLocalProfile();
	FName RealmId = FName(TEXT("local"));
	if (UDLLobbySubsystem* Lobby = GI ? GI->GetSubsystem<UDLLobbySubsystem>() : nullptr)
	{
		RealmId = Lobby->GetLootRealmId();
	}

	auto EquipFromVault = [&](const FGuid& Id)
	{
		if (!bHasProfile || !Id.IsValid())
		{
			return false;
		}
		for (const FDLItemInstance& Item : Profile.VaultItems)
		{
			if (Item.InstanceId == Id && Item.Kind == EDLItemKind::Weapon)
			{
				const FName ItemRealm = Item.RealmId.IsNone() ? FName(TEXT("local")) : Item.RealmId;
				if (ItemRealm != RealmId)
				{
					return false;
				}
				const FDLWeaponClassDef* ClassDef = Loot->FindWeaponClass(Item.DefinitionId);
				WeaponMotor->EquipItem(Item, ClassDef);
				if (HealthShield)
				{
					HealthShield->SetFlinchResist(Item.FinalStats.FlinchResist);
					HealthShield->SetFlinchStability(Item.FinalStats.Stability);
				}
				if (CombatMovement)
				{
					CombatMovement->SetMobilityBonus(Item.FinalStats.MobilityBonus);
				}
				return true;
			}
		}
		return false;
	};

	auto EquipStarter = [&](FName ClassId)
	{
		FDLItemInstance Starter = Loot->MakeWeaponOfClass(ClassId, EDLItemRarity::Common, TEXT("starter"));
		const FDLWeaponClassDef* ClassDef = Loot->FindWeaponClass(Starter.DefinitionId);
		WeaponMotor->EquipItem(Starter, ClassDef);
		if (HealthShield)
		{
			HealthShield->SetFlinchResist(Starter.FinalStats.FlinchResist);
			HealthShield->SetFlinchStability(Starter.FinalStats.Stability);
		}
	};

	if (!EquipFromVault(Profile.EquippedPrimaryId))
	{
		EquipStarter(FName(TEXT("nox_l9")));
	}
	if (!EquipFromVault(Profile.EquippedSpecialId))
	{
		EquipStarter(FName(TEXT("grenade_rifle")));
	}
	WeaponMotor->SwapToSlot(EDLWeaponSlot::Primary);
}
