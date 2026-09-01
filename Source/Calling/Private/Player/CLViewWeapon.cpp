#include "Player/CLViewWeapon.h"
#include "Loot/CLLootRulesService.h"
#include "Game/CLGameInstance.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
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

	USceneComponent* BuildFamilyFromFrame(
		USceneComponent* Root,
		const FCLWeaponFrameDef& Frame,
		const TCHAR* PartPrefix,
		UStaticMesh* Cube,
		UStaticMesh* Cyl,
		UMaterialInterface* Black,
		UMaterialInterface* Dark,
		UMaterialInterface* Red,
		UMaterialInterface* Glass,
		const FWeaponSee& See)
	{
		const FString FamilyName = FString::Printf(TEXT("Family_%s"), *Frame.ClassId.ToString());
		USceneComponent* Family = NewObject<USceneComponent>(Root->GetOwner(), FName(*FamilyName));
		Family->SetupAttachment(Root);
		Family->RegisterComponent();

		auto PartName = [PartPrefix](const TCHAR* Leaf) -> FString
		{
			return FString::Printf(TEXT("%s%s"), PartPrefix, Leaf);
		};

		for (const FCLWeaponSocketDef& Sock : Frame.Visuals)
		{
			UStaticMesh* Mesh = (Sock.Mesh == FName(TEXT("cylinder"))) ? Cyl : Cube;
			const FString Leaf = Sock.Socket.ToString();
			AddPart(Family, *PartName(*Leaf), Mesh, Sock.Loc, Sock.Scale, Sock.Rot, Dark, See);
		}

		AddSocket(Family, *PartName(TEXT("Muzzle")), Frame.Muzzle);
		AddSocket(Family, *PartName(TEXT("Ejector")), Frame.Ejector);

		const bool bGrenade = Frame.ClassId.ToString().Contains(TEXT("grenade"));
		const FVector OpticAt = bGrenade ? FVector(-2.9f, 0.f, 5.35f) : FVector(-2.8f, 0.f, 4.45f);
		AddPart(Family, *PartName(TEXT("OpticHoodL")), Cube, OpticAt + FVector(0.f, -1.35f, 0.f), FVector(0.01f, 0.005f, 0.034f), FRotator::ZeroRotator, Black, See);
		AddPart(Family, *PartName(TEXT("OpticHoodR")), Cube, OpticAt + FVector(0.f, 1.35f, 0.f), FVector(0.01f, 0.005f, 0.034f), FRotator::ZeroRotator, Black, See);
		AddPart(Family, *PartName(TEXT("OpticHoodB")), Cube, OpticAt + FVector(0.f, 0.f, -0.93f), FVector(0.01f, 0.03f, 0.005f), FRotator::ZeroRotator, Black, See);
		if (Glass)
		{
			AddPart(Family, *PartName(TEXT("OpticGlass")), Cube, OpticAt + FVector(0.08f, 0.f, 0.f), FVector(0.0015f, 0.022f, 0.026f), FRotator::ZeroRotator, Glass, See);
		}
		AddPart(Family, *PartName(TEXT("OpticLed")), Cube, OpticAt + FVector(0.12f, 0.f, 0.f), FVector(0.003f, 0.003f, 0.003f), FRotator::ZeroRotator, Red, See);
		AddPart(Family, *PartName(TEXT("ScopeTube")), Cyl, FVector(6.f, 0.f, 3.8f), FVector(0.048f, 0.048f, 0.14f), FRotator(90.f, 0.f, 0.f), Black, See);
		AddPart(Family, *PartName(TEXT("GlSight")), Cube, FVector(-3.4f, 0.f, 5.2f), FVector(0.012f, 0.02f, 0.028f), FRotator::ZeroRotator, Dark, See);
		return Family;
	}

	UCLLootRulesService* LootFrom(USceneComponent* Parent)
	{
		if (!Parent)
		{
			return nullptr;
		}
		AActor* Owner = Parent->GetOwner();
		if (!Owner)
		{
			return nullptr;
		}
		if (UGameInstance* GI = Owner->GetGameInstance())
		{
			if (UCLGameInstance* CLGI = Cast<UCLGameInstance>(GI))
			{
				return CLGI->GetLootRulesService();
			}
		}
		return nullptr;
	}

	USceneComponent* BuildOnParent(
		USceneComponent* Parent,
		FName RootName,
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

		bool bBuilt = false;
		if (const UCLLootRulesService* Loot = LootFrom(Parent))
		{
			for (const FCLWeaponFrameDef& Frame : Loot->GetWeaponFrames())
			{
				BuildFamilyFromFrame(Root, Frame, PartPrefix, Cube, Cyl, Black, Dark, Red, Glass, See);
				bBuilt = true;
			}
		}
		if (!bBuilt)
		{
			FCLWeaponFrameDef Pistol;
			Pistol.ClassId = FName(TEXT("pistol"));
			Pistol.Muzzle = FVector(16.8f, 0.f, 2.3f);
			Pistol.Ejector = FVector(4.2f, 2.8f, 2.6f);
			FCLWeaponSocketDef FrameSock;
			FrameSock.Socket = FName(TEXT("frame"));
			FrameSock.Mesh = FName(TEXT("cube"));
			FrameSock.Loc = FVector(-1.f, 0.f, -1.6f);
			FrameSock.Scale = FVector(0.16f, 0.032f, 0.07f);
			Pistol.Visuals.Add(FrameSock);
			BuildFamilyFromFrame(Root, Pistol, PartPrefix, Cube, Cyl, Black, Dark, Red, Glass, See);

			FCLWeaponFrameDef Grenade;
			Grenade.ClassId = FName(TEXT("grenade_rifle"));
			Grenade.Muzzle = FVector(19.6f, 0.f, 1.8f);
			Grenade.Ejector = FVector(2.2f, 3.4f, 2.4f);
			FCLWeaponSocketDef Tube;
			Tube.Socket = FName(TEXT("barrel"));
			Tube.Mesh = FName(TEXT("cylinder"));
			Tube.Loc = FVector(8.5f, 0.f, 1.8f);
			Tube.Scale = FVector(0.055f, 0.055f, 0.22f);
			Tube.Rot = FRotator(90.f, 0.f, 0.f);
			Grenade.Visuals.Add(Tube);
			BuildFamilyFromFrame(Root, Grenade, PartPrefix, Cube, Cyl, Black, Dark, Red, Glass, See);
		}

		CLViewWeapon::ShowFamily(Root, FName(TEXT("pistol")), ECLWeaponStock::None, false);
		return Root;
	}
}

USceneComponent* CLViewWeapon::BuildOnCamera(USceneComponent* Camera)
{
	const FWeaponSee See{ true, false };
	return BuildOnParent(Camera, TEXT("ViewWeaponRoot"), TEXT("View"), HipOffset, 1.f, See);
}

USceneComponent* CLViewWeapon::BuildOnBody(USceneComponent* Body)
{
	const FWeaponSee See{ false, true };
	return BuildOnParent(Body, TEXT("WorldWeaponRoot"), TEXT("World"), WorldHipOffset, WorldScale, See);
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

void CLViewWeapon::ShowFamily(USceneComponent* WeaponRoot, FName ClassId, ECLWeaponStock Stock, bool bCompensator)
{
	if (!WeaponRoot)
	{
		return;
	}

	const FName Band = UCLLootRulesService::CanonicalWeaponClassId(ClassId);
	const FString Want = FString::Printf(TEXT("Family_%s"), *Band.ToString());

	TArray<USceneComponent*> Children;
	WeaponRoot->GetChildrenComponents(false, Children);
	USceneComponent* Shown = nullptr;
	for (USceneComponent* Child : Children)
	{
		if (!Child)
		{
			continue;
		}
		const FString Name = Child->GetName();
		if (!Name.Contains(TEXT("Family_")))
		{
			continue;
		}
		const bool bMatch = Name.Contains(Want);
		Child->SetVisibility(bMatch, true);
		if (bMatch)
		{
			Shown = Child;
		}
	}

	if (!Shown)
	{
		return;
	}

	TArray<USceneComponent*> Parts;
	Shown->GetChildrenComponents(true, Parts);
	for (USceneComponent* Part : Parts)
	{
		if (!Part)
		{
			continue;
		}
		const FString Name = Part->GetName();
		if (Name.Contains(TEXT("stock"), ESearchCase::IgnoreCase) && !Name.Contains(TEXT("brace"), ESearchCase::IgnoreCase))
		{
			Part->SetVisibility(Stock == ECLWeaponStock::Stock, true);
		}
		else if (Name.Contains(TEXT("brace"), ESearchCase::IgnoreCase))
		{
			Part->SetVisibility(Stock == ECLWeaponStock::Brace, true);
		}
		else if (Name.Contains(TEXT("muzzle"), ESearchCase::IgnoreCase) && Part->IsA(UStaticMeshComponent::StaticClass()))
		{
			Part->SetVisibility(bCompensator, true);
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
	FName ClassId, ECLWeaponStock Stock, FName SightId, bool bCompensator)
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
			ShowFamily(ViewRoot, ClassId, Stock, bCompensator);
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
