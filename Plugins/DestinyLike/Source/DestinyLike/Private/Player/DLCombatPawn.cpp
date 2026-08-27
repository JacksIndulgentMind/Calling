#include "Player/DLCombatPawn.h"
#include "AI/DLCombatAIController.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"

ADLCombatPawn::ADLCombatPawn(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bUseNpcLoadout = true;
	AIControllerClass = ADLCombatAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		Capsule->InitCapsuleSize(50.f, 96.f);
	}
}

void ADLCombatPawn::BeginPlay()
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

void ADLCombatPawn::SetDemoViewActive(bool bActive)
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
