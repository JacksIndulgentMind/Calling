#include "Player/CLCombatPawn.h"
#include "AI/CLCombatAIController.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"

ACLCombatPawn::ACLCombatPawn(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bUseNpcLoadout = true;
	AIControllerClass = ACLCombatAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		Capsule->InitCapsuleSize(50.f, 96.f);
	}
}

void ACLCombatPawn::BeginPlay()
{
	Super::BeginPlay();
	if (FollowCamera)
	{
		FollowCamera->Deactivate();
		FollowCamera->SetActive(false);
	}
	if (CameraBoom)
	{
		CameraBoom->bUsePawnControlRotation = false;
	}
	if (BodyMesh)
	{
		BodyMesh->SetRelativeScale3D(FVector(1.15f, 1.15f, 2.1f));
	}
}

void ACLCombatPawn::SetDemoViewActive(bool bActive)
{
	if (!FollowCamera || !CameraBoom)
	{
		return;
	}
	if (bActive)
	{
		FollowCamera->Activate();
		FollowCamera->SetActive(true);
		CameraBoom->bUsePawnControlRotation = true;
		CameraBoom->TargetArmLength = 0.f;
		CameraBoom->SocketOffset = FVector::ZeroVector;
		if (BodyMesh)
		{
			BodyMesh->SetOwnerNoSee(true);
		}
	}
	else
	{
		FollowCamera->Deactivate();
		FollowCamera->SetActive(false);
		CameraBoom->bUsePawnControlRotation = false;
		CameraBoom->TargetArmLength = 0.f;
		CameraBoom->SocketOffset = FVector::ZeroVector;
		if (BodyMesh)
		{
			BodyMesh->SetOwnerNoSee(false);
		}
	}
}
