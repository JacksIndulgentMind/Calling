#include "Player/CLViewWeapon.h"
#include "Loot/CLLootRulesService.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

namespace
{
	const FVector HipOffset(26.f, 11.f, -13.f);
	const FVector AdsIron(38.f, 2.f, -6.f);
	// Optic window at Glock (-2.6, 0, 4.2) sits on camera center when ADS.
	const FVector AdsRedDot(22.f, 0.f, -4.2f);
	const FVector AdsScope(8.f, 0.f, -3.8f);
	// Outside the combat cylinder (scale 1.15 → radius ~57.5) so the gun reads in third person.
	const FVector WorldHipOffset(40.f, 72.f, 10.f);
	const float WorldScale = 2.f;

	struct FWeaponSee
	{
		bool bOnlyOwnerSee = false;
		bool bOwnerNoSee = false;
	};

	UStaticMesh* CubeMesh()
	{
		return LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	}

	UStaticMesh* CylinderMesh()
	{
		return LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	}

	UMaterialInterface* ShapeMat()
	{
		if (UMaterialInterface* Grid = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")))
		{
			return Grid;
		}
		return LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/EngineMaterials/DefaultMaterial.DefaultMaterial"));
	}

	UMaterialInstanceDynamic* MakeColor(UObject* Outer, UMaterialInterface* Base, const FLinearColor& Color)
	{
		if (!Base)
		{
			return nullptr;
		}
		UMaterialInstanceDynamic* Mid = UMaterialInstanceDynamic::Create(Base, Outer);
		if (Mid)
		{
			Mid->SetVectorParameterValue(TEXT("Color"), Color);
			Mid->SetVectorParameterValue(TEXT("BaseColor"), Color);
		}
		return Mid;
	}

	UStaticMeshComponent* AddPart(
		USceneComponent* Parent,
		const TCHAR* Name,
		UStaticMesh* Mesh,
		const FVector& RelLoc,
		const FVector& RelScale,
		const FRotator& RelRot,
		UMaterialInterface* Mat,
		const FWeaponSee& See)
	{
		UStaticMeshComponent* Comp = NewObject<UStaticMeshComponent>(Parent->GetOwner(), FName(Name));
		Comp->SetupAttachment(Parent);
		Comp->SetRelativeLocation(RelLoc);
		Comp->SetRelativeRotation(RelRot);
		Comp->SetRelativeScale3D(RelScale);
		if (Mesh)
		{
			Comp->SetStaticMesh(Mesh);
		}
		if (Mat)
		{
			Comp->SetMaterial(0, Mat);
		}
		Comp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Comp->SetCastShadow(false);
		Comp->bVisibleInReflectionCaptures = false;
		Comp->SetOnlyOwnerSee(See.bOnlyOwnerSee);
		Comp->SetOwnerNoSee(See.bOwnerNoSee);
		Comp->RegisterComponent();
		return Comp;
	}

	USceneComponent* AddSocket(USceneComponent* Parent, const TCHAR* Name, const FVector& RelLoc)
	{
		USceneComponent* Sock = NewObject<USceneComponent>(Parent->GetOwner(), FName(Name));
		Sock->SetupAttachment(Parent);
		Sock->SetRelativeLocation(RelLoc);
		Sock->RegisterComponent();
		return Sock;
	}

	USceneComponent* BuildOnParent(
		USceneComponent* Parent,
		FName RootName,
		FName GlockName,
		FName GrenadeName,
		const TCHAR* PartPrefix,
		const FVector& RelLoc,
		float UniformScale,
		const FWeaponSee& See)
	{
		if (!Parent)
		{
			return nullptr;
		}

		USceneComponent* Root = NewObject<USceneComponent>(Parent->GetOwner(), RootName);
		Root->SetupAttachment(Parent);
		Root->SetRelativeLocation(RelLoc);
		Root->SetRelativeScale3D(FVector(UniformScale));
		Root->RegisterComponent();

		USceneComponent* GlockRoot = NewObject<USceneComponent>(Parent->GetOwner(), GlockName);
		GlockRoot->SetupAttachment(Root);
		GlockRoot->RegisterComponent();

		USceneComponent* GrenadeRoot = NewObject<USceneComponent>(Parent->GetOwner(), GrenadeName);
		GrenadeRoot->SetupAttachment(Root);
		GrenadeRoot->RegisterComponent();

		UStaticMesh* Cube = CubeMesh();
		UStaticMesh* Cyl = CylinderMesh();
		UMaterialInterface* Base = ShapeMat();
		UMaterialInstanceDynamic* Black = MakeColor(Parent->GetOwner(), Base, FLinearColor(0.02f, 0.02f, 0.025f, 1.f));
		UMaterialInstanceDynamic* Dark = MakeColor(Parent->GetOwner(), Base, FLinearColor(0.04f, 0.04f, 0.045f, 1.f));
		UMaterialInstanceDynamic* Red = MakeColor(Parent->GetOwner(), Base, FLinearColor(0.95f, 0.12f, 0.08f, 1.f));
		UMaterialInterface* GlassBase = LoadObject<UMaterialInterface>(nullptr,
			TEXT("/Engine/EngineMaterials/Widget3DPassThrough_Translucent_OneSided.Widget3DPassThrough_Translucent_OneSided"));
		if (!GlassBase)
		{
			GlassBase = LoadObject<UMaterialInterface>(nullptr,
				TEXT("/Engine/EngineMaterials/Widget3DPassThrough.Widget3DPassThrough"));
		}
		UMaterialInstanceDynamic* Glass = GlassBase
			? MakeColor(Parent->GetOwner(), GlassBase, FLinearColor(0.55f, 0.62f, 0.68f, 0.22f))
			: nullptr;

		auto PartName = [PartPrefix](const TCHAR* Leaf) -> FString
		{
			return FString::Printf(TEXT("%s%s"), PartPrefix, Leaf);
		};

		AddPart(GlockRoot, *PartName(TEXT("Frame")), Cube, FVector(-1.f, 0.f, -1.6f), FVector(0.16f, 0.032f, 0.07f), FRotator::ZeroRotator, Black, See);
		AddPart(GlockRoot, *PartName(TEXT("Slide")), Cube, FVector(1.6f, 0.f, 2.5f), FVector(0.20f, 0.028f, 0.024f), FRotator::ZeroRotator, Dark, See);
		AddPart(GlockRoot, *PartName(TEXT("DustCover")), Cube, FVector(5.4f, 0.f, 0.2f), FVector(0.12f, 0.032f, 0.022f), FRotator::ZeroRotator, Black, See);
		AddPart(GlockRoot, *PartName(TEXT("Barrel")), Cyl, FVector(10.2f, 0.f, 2.3f), FVector(0.012f, 0.012f, 0.13f), FRotator(90.f, 0.f, 0.f), Dark, See);
		AddSocket(GlockRoot, *PartName(TEXT("Muzzle")), FVector(16.8f, 0.f, 2.3f));
		AddSocket(GlockRoot, *PartName(TEXT("Ejector")), FVector(4.2f, 2.8f, 2.6f));
		AddPart(GlockRoot, *PartName(TEXT("Mag")), Cube, FVector(-3.2f, 0.f, -5.4f), FVector(0.03f, 0.024f, 0.07f), FRotator(8.f, 0.f, 0.f), Black, See);
		AddPart(GlockRoot, *PartName(TEXT("TriggerGuard")), Cube, FVector(-0.2f, 0.f, -4.2f), FVector(0.04f, 0.024f, 0.032f), FRotator::ZeroRotator, Black, See);
		AddPart(GlockRoot, *PartName(TEXT("ThumbRest")), Cube, FVector(-1.4f, -2.0f, 0.6f), FVector(0.028f, 0.012f, 0.018f), FRotator(0.f, 12.f, 0.f), Dark, See);
		// Open C/U red-dot hood: thin frame, air in the window, tiny LED. Not a closed tube.
		AddPart(GlockRoot, *PartName(TEXT("OpticHoodL")), Cube, FVector(-2.8f, -1.35f, 4.45f), FVector(0.01f, 0.005f, 0.034f), FRotator::ZeroRotator, Black, See);
		AddPart(GlockRoot, *PartName(TEXT("OpticHoodR")), Cube, FVector(-2.8f, 1.35f, 4.45f), FVector(0.01f, 0.005f, 0.034f), FRotator::ZeroRotator, Black, See);
		AddPart(GlockRoot, *PartName(TEXT("OpticHoodB")), Cube, FVector(-2.8f, 0.f, 3.52f), FVector(0.01f, 0.03f, 0.005f), FRotator::ZeroRotator, Black, See);
		if (Glass)
		{
			AddPart(GlockRoot, *PartName(TEXT("OpticGlass")), Cube, FVector(-2.72f, 0.f, 4.45f), FVector(0.0015f, 0.022f, 0.026f), FRotator::ZeroRotator, Glass, See);
		}
		AddPart(GlockRoot, *PartName(TEXT("OpticLed")), Cube, FVector(-2.68f, 0.f, 4.45f), FVector(0.003f, 0.003f, 0.003f), FRotator::ZeroRotator, Red, See);
		AddPart(GlockRoot, *PartName(TEXT("ScopeTube")), Cyl, FVector(6.f, 0.f, 3.8f), FVector(0.048f, 0.048f, 0.14f), FRotator(90.f, 0.f, 0.f), Black, See);

		const float VentY[4] = { -0.6f, -0.2f, 0.2f, 0.6f };
		for (int32 i = 0; i < 4; ++i)
		{
			AddPart(GlockRoot, *PartName(*FString::Printf(TEXT("Vent%d"), i)), Cube,
				FVector(6.4f + i * 0.9f, VentY[i] * 0.15f, 3.55f),
				FVector(0.012f, 0.01f, 0.008f),
				FRotator::ZeroRotator, Black, See);
		}

		AddPart(GrenadeRoot, *PartName(TEXT("GlTube")), Cyl, FVector(8.5f, 0.f, 1.8f), FVector(0.055f, 0.055f, 0.22f), FRotator(90.f, 0.f, 0.f), Dark, See);
		AddSocket(GrenadeRoot, *PartName(TEXT("GlMuzzle")), FVector(19.6f, 0.f, 1.8f));
		AddSocket(GrenadeRoot, *PartName(TEXT("GlEjector")), FVector(2.2f, 3.4f, 2.4f));
		AddPart(GrenadeRoot, *PartName(TEXT("GlBody")), Cube, FVector(0.4f, 0.f, 0.6f), FVector(0.14f, 0.05f, 0.07f), FRotator::ZeroRotator, Black, See);
		AddPart(GrenadeRoot, *PartName(TEXT("GlBreech")), Cube, FVector(-4.2f, 0.f, 1.4f), FVector(0.06f, 0.055f, 0.08f), FRotator::ZeroRotator, Dark, See);
		AddPart(GrenadeRoot, *PartName(TEXT("GlMag")), Cube, FVector(-1.6f, 0.f, -5.8f), FVector(0.045f, 0.038f, 0.09f), FRotator(10.f, 0.f, 0.f), Black, See);
		AddPart(GrenadeRoot, *PartName(TEXT("GlGuard")), Cube, FVector(0.6f, 0.f, -3.6f), FVector(0.05f, 0.03f, 0.03f), FRotator::ZeroRotator, Black, See);
		AddPart(GrenadeRoot, *PartName(TEXT("GlSight")), Cube, FVector(-3.4f, 0.f, 5.2f), FVector(0.012f, 0.02f, 0.028f), FRotator::ZeroRotator, Dark, See);
		AddPart(GrenadeRoot, *PartName(TEXT("OpticHoodL")), Cube, FVector(-2.9f, -1.35f, 5.35f), FVector(0.01f, 0.005f, 0.034f), FRotator::ZeroRotator, Black, See);
		AddPart(GrenadeRoot, *PartName(TEXT("OpticHoodR")), Cube, FVector(-2.9f, 1.35f, 5.35f), FVector(0.01f, 0.005f, 0.034f), FRotator::ZeroRotator, Black, See);
		AddPart(GrenadeRoot, *PartName(TEXT("OpticHoodB")), Cube, FVector(-2.9f, 0.f, 4.42f), FVector(0.01f, 0.03f, 0.005f), FRotator::ZeroRotator, Black, See);
		AddPart(GrenadeRoot, *PartName(TEXT("OpticLed")), Cube, FVector(-2.78f, 0.f, 5.35f), FVector(0.003f, 0.003f, 0.003f), FRotator::ZeroRotator, Red, See);

		CLViewWeapon::ShowFamily(Root, false);
		return Root;
	}
}

USceneComponent* CLViewWeapon::BuildOnCamera(USceneComponent* Camera)
{
	const FWeaponSee See{ true, false };
	return BuildOnParent(Camera, TEXT("ViewWeaponRoot"), TEXT("ViewGlock"), TEXT("ViewGrenadeRifle"), TEXT("View"), HipOffset, 1.f, See);
}

USceneComponent* CLViewWeapon::BuildOnBody(USceneComponent* Body)
{
	const FWeaponSee See{ false, true };
	return BuildOnParent(Body, TEXT("WorldWeaponRoot"), TEXT("WorldGlock"), TEXT("WorldGrenadeRifle"), TEXT("World"), WorldHipOffset, WorldScale, See);
}

void CLViewWeapon::UpdateAdsPose(USceneComponent* WeaponRoot, float AdsAlpha, FName SightId, float KickPitch)
{
	if (!WeaponRoot)
	{
		return;
	}
	FVector Ads = AdsIron;
	const ECLSightViewKind Kind = UCLLootRulesService::SightViewKind(SightId);
	if (Kind == ECLSightViewKind::Scope)
	{
		Ads = AdsScope;
	}
	else if (Kind != ECLSightViewKind::Iron)
	{
		Ads = AdsRedDot;
	}
	const float T = FMath::Clamp(AdsAlpha, 0.f, 1.f);
	WeaponRoot->SetRelativeLocation(FMath::Lerp(HipOffset, Ads, T));
	WeaponRoot->SetRelativeRotation(FRotator(KickPitch, 0.f, 0.f));
}

void CLViewWeapon::ShowFamily(USceneComponent* WeaponRoot, bool bGrenadeRifle)
{
	if (!WeaponRoot)
	{
		return;
	}

	TArray<USceneComponent*> Children;
	WeaponRoot->GetChildrenComponents(false, Children);
	for (USceneComponent* Child : Children)
	{
		if (!Child)
		{
			continue;
		}
		const FString Name = Child->GetName();
		const bool bGrenade = Name.Contains(TEXT("GrenadeRifle"));
		const bool bGlock = Name.Contains(TEXT("Glock"));
		if (bGrenade || bGlock)
		{
			Child->SetVisibility(bGrenade ? bGrenadeRifle : !bGrenadeRifle, true);
		}
	}
}

namespace
{
	void SetNameVisible(USceneComponent* Root, const TCHAR* Needle, bool bVisible)
	{
		if (!Root)
		{
			return;
		}
		TArray<USceneComponent*> Kids;
		Root->GetChildrenComponents(true, Kids);
		for (USceneComponent* Child : Kids)
		{
			if (!Child)
			{
				continue;
			}
			if (Child->GetName().Contains(Needle))
			{
				Child->SetVisibility(bVisible, true);
			}
		}
	}

	void SetMeshesOwnerNoSee(USceneComponent* Root, bool bOwnerNoSee)
	{
		if (!Root)
		{
			return;
		}
		TArray<USceneComponent*> Kids;
		Root->GetChildrenComponents(true, Kids);
		Kids.Add(Root);
		for (USceneComponent* Child : Kids)
		{
			if (UStaticMeshComponent* Mesh = Cast<UStaticMeshComponent>(Child))
			{
				Mesh->SetOwnerNoSee(bOwnerNoSee);
			}
		}
	}
}

void CLViewWeapon::ShowSight(USceneComponent* WeaponRoot, FName SightId)
{
	if (!WeaponRoot)
	{
		return;
	}
	const ECLSightViewKind Kind = UCLLootRulesService::SightViewKind(SightId);
	const bool bIron = Kind == ECLSightViewKind::Iron;
	const bool bScope = Kind == ECLSightViewKind::Scope;
	const bool bRed = !bIron && !bScope;
	SetNameVisible(WeaponRoot, TEXT("OpticHood"), bRed);
	SetNameVisible(WeaponRoot, TEXT("OpticGlass"), bRed);
	SetNameVisible(WeaponRoot, TEXT("OpticLed"), bRed);
	SetNameVisible(WeaponRoot, TEXT("ScopeTube"), bScope);
	SetNameVisible(WeaponRoot, TEXT("GlSight"), bIron);
}

void CLViewWeapon::SetThirdPersonPeek(USceneComponent* ViewRoot, USceneComponent* WorldRoot, bool bPeek,
	bool bGrenadeRifle, FName SightId)
{
	if (ViewRoot)
	{
		if (bPeek)
		{
			ViewRoot->SetVisibility(false, true);
		}
		else
		{
			ViewRoot->SetVisibility(true, false);
			ShowFamily(ViewRoot, bGrenadeRifle);
			ShowSight(ViewRoot, SightId);
		}
	}
	SetMeshesOwnerNoSee(WorldRoot, !bPeek);
}

namespace
{
	USceneComponent* FindNamedVisible(USceneComponent* Root, const TCHAR* Needle)
	{
		if (!Root)
		{
			return nullptr;
		}
		TArray<USceneComponent*> Kids;
		Root->GetChildrenComponents(true, Kids);
		for (USceneComponent* Child : Kids)
		{
			if (!Child || !Child->IsVisible())
			{
				continue;
			}
			if (Child->GetName().Contains(Needle))
			{
				return Child;
			}
		}
		return nullptr;
	}
}

USceneComponent* CLViewWeapon::FindMuzzle(USceneComponent* WeaponRoot)
{
	if (USceneComponent* Gl = FindNamedVisible(WeaponRoot, TEXT("GlMuzzle")))
	{
		return Gl;
	}
	return FindNamedVisible(WeaponRoot, TEXT("Muzzle"));
}

USceneComponent* CLViewWeapon::FindEjector(USceneComponent* WeaponRoot)
{
	if (USceneComponent* Gl = FindNamedVisible(WeaponRoot, TEXT("GlEjector")))
	{
		return Gl;
	}
	return FindNamedVisible(WeaponRoot, TEXT("Ejector"));
}
