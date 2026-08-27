#include "Player/CLPlayerCharacter.h"
#include "Player/CLPossessionComponent.h"
#include "Player/CLLookController.h"
#include "Player/CLIntentReceiver.h"
#include "Player/CLCombatMovementComponent.h"
#include "Player/CLHealthShieldComponent.h"
#include "Combat/CLDamageableComponent.h"
#include "Combat/CLEffectStackComponent.h"
#include "Player/CLWeaponMotorComponent.h"
#include "Player/CLWeaponBehaviorComponent.h"
#include "Player/CLAbilityLoadoutComponent.h"
#include "Player/CLViewWeapon.h"
#include "Player/CLCombatPawn.h"
#include "Player/CLPathfinderCharacter.h"
#include "Player/CLWardenCharacter.h"
#include "Game/CLProfileSubsystem.h"
#include "Game/CLGameInstance.h"
#include "Game/CLGameModeBase.h"
#include "Game/CLLobbySubsystem.h"
#include "Game/CLGameStateBase.h"
#include "Loot/CLLootRulesService.h"
#include "Core/CLTypes.h"
#include "Core/CLTunes.h"
#include "Core/CLTickClock.h"
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

ACLPlayerCharacter::ACLPlayerCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UCLCombatMovementComponent>(ACharacter::CharacterMovementComponentName))
{
	PrimaryActorTick.bCanEverTick = true;
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = true;
	bUseControllerRotationRoll = false;

	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.f);
	JumpMaxCount = 1;

	CombatMovement = Cast<UCLCombatMovementComponent>(GetCharacterMovement());
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

	HealthShield = CreateDefaultSubobject<UCLHealthShieldComponent>(TEXT("HealthShield"));
	Damageable = CreateDefaultSubobject<UCLDamageableComponent>(TEXT("Damageable"));
	EffectStack = CreateDefaultSubobject<UCLEffectStackComponent>(TEXT("EffectStack"));
	WeaponBehavior = CreateDefaultSubobject<UCLWeaponBehaviorComponent>(TEXT("WeaponBehavior"));
	WeaponMotor = CreateDefaultSubobject<UCLWeaponMotorComponent>(TEXT("WeaponMotor"));
	Abilities = CreateDefaultSubobject<UCLAbilityLoadoutComponent>(TEXT("Abilities"));
	Possession = CreateDefaultSubobject<UCLPossessionComponent>(TEXT("Possession"));
	LookController = CreateDefaultSubobject<UCLLookController>(TEXT("LookController"));
	IntentReceiver = CreateDefaultSubobject<UCLIntentReceiver>(TEXT("IntentReceiver"));
}

void ACLPlayerCharacter::BeginPlay()
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

	ViewWeaponRoot = CLViewWeapon::BuildOnCamera(FollowCamera);
	WorldWeaponRoot = CLViewWeapon::BuildOnBody(GetCapsuleComponent());

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
		Damageable->OnDamaged.AddDynamic(this, &ACLPlayerCharacter::HandleDamaged);
	}
	if (HealthShield)
	{
		HealthShield->OnDeath.AddDynamic(this, &ACLPlayerCharacter::HandleDeath);
	}

	if (UGameInstance* GI = GetGameInstance())
	{
		if (UCLTickSubsystem* Tick = GI->GetSubsystem<UCLTickSubsystem>())
		{
			FixedTickHandle = Tick->OnFixedGameTick().AddUObject(this, &ACLPlayerCharacter::HandleFixedGameTick);
		}
		if (UCLLobbySubsystem* Lobby = GI->GetSubsystem<UCLLobbySubsystem>())
		{
			if (Possession && IsLocallyControlled() && !IsA<ACLCombatPawn>())
			{
				Possession->PossessOwn(this);
			}
		}
	}
}

void ACLPlayerCharacter::Tick(float DeltaSeconds)
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
		if (UCLLobbySubsystem* Lobby = GI->GetSubsystem<UCLLobbySubsystem>())
		{
			if (!Lobby->IsGameplayUnlocked())
			{
				AccumulatedLook = FVector2D::ZeroVector;
				if (IntentReceiver)
				{
					IntentReceiver->ClearLookDelta();
				}
				return;
			}
		}
	}
	// Render tick: look is applied immediately for responsiveness; move/fire consumed on fixed sim.
	if (Controller && LookController)
	{
		const FVector2D AgentLookDelta = IntentReceiver ? IntentReceiver->TakeLookDelta() : FVector2D::ZeroVector;
		LookController->ApplyDeltaLook(DeltaSeconds, AccumulatedLook, AgentLookDelta);
		AccumulatedLook = FVector2D::ZeroVector;
	}
	if (LookController)
	{
		LookController->TickAgentLook(DeltaSeconds);
	}

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
			if (LookController)
			{
				LookController->NoteHipRecoil();
			}
		}
	}

	if (WeaponMotor)
	{
		const bool bGrenade = WeaponMotor->UsesGrenadeProjectile();
		const FName Sight = WeaponMotor->GetSightId();
		if (ViewWeaponRoot)
		{
			CLViewWeapon::UpdateAdsPose(ViewWeaponRoot, WeaponMotor->GetAdsEase(), Sight, WeaponMotor->GetViewKickPitch());
			CLViewWeapon::ShowFamily(ViewWeaponRoot, bGrenade);
			CLViewWeapon::ShowSight(ViewWeaponRoot, Sight);
		}
		if (WorldWeaponRoot)
		{
			CLViewWeapon::ShowFamily(WorldWeaponRoot, bGrenade);
			CLViewWeapon::ShowSight(WorldWeaponRoot, Sight);
		}
	}
	UpdateThirdPersonPeek(DeltaSeconds);
	TickKnifeSlash(DeltaSeconds);
}

void ACLPlayerCharacter::UpdateThirdPersonPeek(float DeltaSeconds)
{
	if (!CameraBoom || !FollowCamera || !FollowCamera->IsActive() || IsA(ACLCombatPawn::StaticClass()))
	{
		return;
	}

	FCLWeaponMotorTune Cam;
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
	CLViewWeapon::SetThirdPersonPeek(ViewWeaponRoot, WorldWeaponRoot, Ease > 0.05f, bGrenade, Sight);
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

void ACLPlayerCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (Damageable)
	{
		Damageable->OnDamaged.RemoveDynamic(this, &ACLPlayerCharacter::HandleDamaged);
	}
	if (HealthShield)
	{
		HealthShield->OnDeath.RemoveDynamic(this, &ACLPlayerCharacter::HandleDeath);
	}
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UCLTickSubsystem* Tick = GI->GetSubsystem<UCLTickSubsystem>())
		{
			Tick->OnFixedGameTick().Remove(FixedTickHandle);
		}
	}
	Super::EndPlay(EndPlayReason);
}

void ACLPlayerCharacter::HandleDamaged(float RemainingHealth, float Applied)
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

bool ACLPlayerCharacter::IsCombatAlive() const
{
	if (bTakenOut)
	{
		return false;
	}
	if (const UCLHealthShieldComponent* HS = HealthShield)
	{
		return HS->IsAlive();
	}
	if (const UCLDamageableComponent* Dmg = Damageable)
	{
		return Dmg->IsAlive();
	}
	return true;
}

void ACLPlayerCharacter::NoteIncomingDamage(AController* InstigatorController, float Applied)
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

void ACLPlayerCharacter::HandleDeath()
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
	if (ACLGameStateBase* GS = World ? World->GetGameState<ACLGameStateBase>() : nullptr)
	{
		GS->RegisterTakeOut(Killer, this, Assists);
	}

	if (World)
	{
		World->GetTimerManager().SetTimer(RespawnTimer, this, &ACLPlayerCharacter::RequestTakeOutRespawn, 2.f, false);
	}
}

void ACLPlayerCharacter::RequestTakeOutRespawn()
{
	if (UWorld* World = GetWorld())
	{
		if (ACLGameModeBase* GM = World->GetAuthGameMode<ACLGameModeBase>())
		{
			GM->RequestRespawn(GetController());
		}
	}
}

void ACLPlayerCharacter::NotifyRespawned()
{
	bTakenOut = false;
	DamageTimes.Reset();
	LastDamageInstigator.Reset();
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RespawnTimer);
	}
}

void ACLPlayerCharacter::PlayKnifeSlash()
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

void ACLPlayerCharacter::TickKnifeSlash(float DeltaSeconds)
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

float ACLPlayerCharacter::GetHurtAlpha() const
{
	if (HurtAge < 0.05f)
	{
		return FMath::Clamp(HurtAge / 0.05f, 0.f, 1.f);
	}
	const float Out = (HurtAge - 0.05f) / 0.75f;
	return FMath::Clamp(1.f - Out, 0.f, 1.f);
}

FVector2D ACLPlayerCharacter::GetAdsReticlePunch() const
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

bool ACLPlayerCharacter::UsesViewWeapon() const
{
	return IsLocallyControlled() && FollowCamera && FollowCamera->IsActive();
}

void ACLPlayerCharacter::SetDemoViewActive(bool bActive)
{
	(void)bActive;
}

void ACLPlayerCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	// Binding is done in ACLPlayerController via Enhanced Input.
}

void ACLPlayerCharacter::Jump()
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

void ACLPlayerCharacter::Landed(const FHitResult& Hit)
{
	Super::Landed(Hit);
	if (CombatMovement)
	{
		CombatMovement->NotifyLanded();
	}
}

void ACLPlayerCharacter::FellOutOfWorld(const UDamageType& DmgType)
{
	if (UWorld* World = GetWorld())
	{
		if (ACLGameModeBase* GM = World->GetAuthGameMode<ACLGameModeBase>())
		{
			GM->RequestRespawn(GetController());
			return;
		}
	}
	Super::FellOutOfWorld(DmgType);
}

bool ACLPlayerCharacter::IsSliding() const
{
	return CombatMovement && CombatMovement->IsSliding();
}

void ACLPlayerCharacter::SetRippleCamo(bool bEnabled)
{
	if (!BodyMesh)
	{
		return;
	}
	BodyMesh->SetWorldScale3D(bEnabled ? FVector(0.35f, 0.35f, 1.92f) : FVector(0.75f, 0.75f, 1.92f));
	BodyMesh->SetCastShadow(!bEnabled);
}

void ACLPlayerCharacter::AccumulateInput(FVector2D MoveXY, FVector2D LookDelta, bool bSprint, bool bCrouch, bool bADS, bool bFire)
{
	AccumulatedMove = MoveXY;
	AccumulatedLook += LookDelta;
	bAccumSprint = bSprint;
	bAccumCrouch = bCrouch;
	bAccumADS = bADS;
	bAccumFire = bFire;
}

void ACLPlayerCharacter::HandleFixedGameTick(float DeltaSeconds)
{
	(void)DeltaSeconds;
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UCLLobbySubsystem* Lobby = GI->GetSubsystem<UCLLobbySubsystem>())
		{
			if (!Lobby->IsGameplayUnlocked())
			{
				ConsumeSimInput(FVector2D::ZeroVector, FVector2D::ZeroVector, false, false, false, false);
				return;
			}
		}
	}
	ConsumeSimInput(
		AccumulatedMove + (IntentReceiver ? IntentReceiver->GetMove() : FVector2D::ZeroVector),
		FVector2D::ZeroVector,
		bAccumSprint || (IntentReceiver && IntentReceiver->WantsSprint()),
		bAccumCrouch || (IntentReceiver && IntentReceiver->WantsCrouch()),
		bAccumADS || (IntentReceiver && IntentReceiver->WantsADS()),
		bAccumFire || (IntentReceiver && IntentReceiver->WantsFire()));
	if (IntentReceiver)
	{
		IntentReceiver->ConsumeAgentPulses();
	}
}

void ACLPlayerCharacter::ApplyAgentIntent(const FCLAgentIntent& Intent)
{
	if (IntentReceiver)
	{
		IntentReceiver->ApplyAgentIntent(Intent);
	}
}

void ACLPlayerCharacter::ApplyAgentIntent(FVector2D MoveXY, FVector2D LookDelta, bool bSprint, bool bCrouch, bool bADS, bool bFire,
	bool bJump, bool bDodge, bool bDash, bool bReload, bool bSwap)
{
	if (IntentReceiver)
	{
		IntentReceiver->ApplyAgentIntent(MoveXY, LookDelta, bSprint, bCrouch, bADS, bFire, bJump, bDodge, bDash, bReload, bSwap);
	}
}

void ACLPlayerCharacter::ClearAgentIntent()
{
	if (IntentReceiver)
	{
		IntentReceiver->ClearAgentIntent();
	}
	ClearAgentLook();
}

void ACLPlayerCharacter::ClearAgentLook()
{
	if (LookController)
	{
		LookController->ClearAgentLook();
	}
}

void ACLPlayerCharacter::SetLookGoalYawPitch(bool bYaw, float Yaw, bool bPitch, float Pitch)
{
	if (LookController)
	{
		LookController->SetLookGoalYawPitch(bYaw, Yaw, bPitch, Pitch);
	}
}

void ACLPlayerCharacter::ApplyAgentLookCommand(const FCLLookCommand& Look)
{
	if (LookController)
	{
		LookController->ApplyAgentLookCommand(Look);
	}
}

void ACLPlayerCharacter::SetLookTrackSeat(const FGuid& SeatId)
{
	if (LookController)
	{
		LookController->SetLookTrackSeat(SeatId);
	}
}

void ACLPlayerCharacter::ApplyAgentLookFromStep(const FGuid& TrackSeatId, const FCLLookCommand& Look)
{
	if (LookController)
	{
		LookController->ApplyAgentLookFromStep(TrackSeatId, Look);
	}
}

void ACLPlayerCharacter::ConsumeSimInput(FVector2D MoveXY, FVector2D LookDelta, bool bSprint, bool bCrouch, bool bADS, bool bFire)
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

void ACLPlayerCharacter::ApplyProfileLoadout()
{
	FCLCharacterAppearance Appearance;
	if (IsA<ACLPathfinderCharacter>())
	{
		Appearance.ClassId = ECLClassId::Pathfinder;
	}
	else if (IsA<ACLWardenCharacter>())
	{
		Appearance.ClassId = ECLClassId::Warden;
	}

	UCLGameInstance* GI = Cast<UCLGameInstance>(GetGameInstance());
	UCLProfileSubsystem* Profiles = (!bUseNpcLoadout && GI) ? GI->GetProfileSubsystem() : nullptr;
	UCLLootRulesService* Loot = GI ? GI->GetLootRulesService() : nullptr;
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
	const FCLLocalProfile Profile = bHasProfile ? Profiles->GetActiveProfile() : FCLLocalProfile();
	FName RealmId = FName(TEXT("local"));
	if (UCLLobbySubsystem* Lobby = GI ? GI->GetSubsystem<UCLLobbySubsystem>() : nullptr)
	{
		RealmId = Lobby->GetLootRealmId();
	}

	auto EquipFromVault = [&](const FGuid& Id)
	{
		if (!bHasProfile || !Id.IsValid())
		{
			return false;
		}
		for (const FCLItemInstance& Item : Profile.VaultItems)
		{
			if (Item.InstanceId == Id && Item.Kind == ECLItemKind::Weapon)
			{
				const FName ItemRealm = Item.RealmId.IsNone() ? FName(TEXT("local")) : Item.RealmId;
				if (ItemRealm != RealmId)
				{
					return false;
				}
				const FCLWeaponClassDef* ClassDef = Loot->FindWeaponClass(Item.DefinitionId);
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
		FCLItemInstance Starter = Loot->MakeWeaponOfClass(ClassId, ECLItemRarity::Common, TEXT("starter"));
		const FCLWeaponClassDef* ClassDef = Loot->FindWeaponClass(Starter.DefinitionId);
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
	WeaponMotor->SwapToSlot(ECLWeaponSlot::Primary);
}
