#include "Game/CLAgentStateSerializer.h"
#include "Game/CLInstanceIdentity.h"
#include "Combat/CLHitscanService.h"
#include "Combat/CLDamageableComponent.h"
#include "Core/CLTypes.h"
#include "Game/CLSeatMotor.h"
#include "AI/CLBotBookManager.h"
#include "Game/CLGameModeBase.h"
#include "Game/CLGameStateBase.h"
#include "Game/CLLobbySubsystem.h"
#include "Game/CLSceneRouter.h"
#include "Game/CLSessionHub.h"
#include "Nav/CLAgentNavProbe.h"
#include "Player/CLCombatMovementComponent.h"
#include "Player/CLHealthShieldComponent.h"
#include "Player/CLIntentReceiver.h"
#include "Player/CLLookController.h"
#include "Player/CLPlayerCharacter.h"
#include "Player/CLPlayerController.h"
#include "Player/CLWeaponMotorComponent.h"
#include "UI/CLMainMenuOverlay.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Dom/JsonValue.h"
#include "GameFramework/Controller.h"
#include "GameFramework/PlayerController.h"

namespace
{
	const TCHAR* SceneName(ECLSceneId Scene)
	{
		switch (Scene)
		{
		case ECLSceneId::Social: return TEXT("social");
		case ECLSceneId::Composer: return TEXT("composer");
		case ECLSceneId::Pvp: return TEXT("pvp");
		case ECLSceneId::Raid: return TEXT("raid");
		case ECLSceneId::Practice: return TEXT("practice");
		default: return TEXT("boot");
		}
	}

	void FillMatchMode(TSharedRef<FJsonObject> Root, UWorld* World)
	{
		if (!World)
		{
			return;
		}
		const ACLGameStateBase* GS = World->GetGameState<ACLGameStateBase>();
		if (!GS)
		{
			return;
		}
		Root->SetNumberField(TEXT("teamAScore"), GS->GetTeamAScore());
		Root->SetNumberField(TEXT("teamBScore"), GS->GetTeamBScore());
		Root->SetNumberField(TEXT("teamAKills"), GS->GetTeamAKills());
		Root->SetNumberField(TEXT("teamBKills"), GS->GetTeamBKills());
		Root->SetStringField(TEXT("scoreLine"), GS->GetScoreLine());
		Root->SetStringField(TEXT("liveShrine"), GS->GetLiveShrine().ToString());
		Root->SetBoolField(TEXT("shrineHeldRed"), GS->GetShrineHeldRed());
		Root->SetBoolField(TEXT("shrineHeldBlue"), GS->GetShrineHeldBlue());
		Root->SetStringField(TEXT("modeResult"), GS->GetModeResult());
		Root->SetStringField(TEXT("winningTeam"), GS->GetWinningTeam());
		Root->SetStringField(TEXT("modeFailReason"), GS->GetModeFailReason());
		TArray<TSharedPtr<FJsonValue>> EvArr;
		for (const FCLMatchEvent& E : GS->GetMatchEvents())
		{
			TSharedRef<FJsonObject> Ev = MakeShared<FJsonObject>();
			Ev->SetStringField(TEXT("code"), E.Code);
			Ev->SetStringField(TEXT("seat"), E.Seat);
			Ev->SetStringField(TEXT("book"), E.Book);
			Ev->SetStringField(TEXT("detail"), E.Detail);
			Ev->SetNumberField(TEXT("x"), E.X);
			Ev->SetNumberField(TEXT("y"), E.Y);
			Ev->SetNumberField(TEXT("t"), E.Time);
			EvArr.Add(MakeShared<FJsonValueObject>(Ev));
		}
		Root->SetArrayField(TEXT("events"), EvArr);
	}

	void FillPawn(TSharedRef<FJsonObject> Root, ACLPlayerCharacter* Char, UWorld* World)
	{
		const FVector Loc = Char->GetActorLocation();
		FRotator Control = FRotator::ZeroRotator;
		if (AController* Ctrl = Char->GetController())
		{
			Control = Ctrl->GetControlRotation();
		}
		Root->SetBoolField(TEXT("ok"), true);
		Root->SetNumberField(TEXT("x"), Loc.X);
		Root->SetNumberField(TEXT("y"), Loc.Y);
		Root->SetNumberField(TEXT("z"), Loc.Z);
		Root->SetNumberField(TEXT("yaw"), Control.Yaw);
		Root->SetNumberField(TEXT("pitch"), FRotator::NormalizeAxis(Control.Pitch));
		Root->SetNumberField(TEXT("vz"), Char->GetVelocity().Z);
		Root->SetBoolField(TEXT("alive"), Char->IsCombatAlive());
		CLAgentNavProbe::FillStateJson(Root, World, Char);
		if (const UCLCombatMovementComponent* Move = Char->GetCombatMovement())
		{
			Root->SetBoolField(TEXT("sliding"), Move->IsSliding());
			Root->SetBoolField(TEXT("dodging"), Move->IsDodging());
			Root->SetBoolField(TEXT("dashing"), Move->IsDashing());
			Root->SetBoolField(TEXT("air"), !Move->IsMovingOnGround());
			Root->SetBoolField(TEXT("diving"), Move->IsDiveReported());
			Root->SetNumberField(TEXT("jumpsLeft"), Move->GetJumpsRemaining());
			Root->SetNumberField(TEXT("crouchAlpha"), Move->GetCrouchAlpha());
			Root->SetNumberField(TEXT("slideDuration"), Move->GetSlideDuration());
			Root->SetNumberField(TEXT("slideDistanceCm"), Move->EstimateSlideTravelCm());
			Root->SetNumberField(TEXT("dashDuration"), Move->GetDashDuration());
			Root->SetNumberField(TEXT("dashDistanceCm"), Move->GetDashDistance());
			Root->SetNumberField(TEXT("dodgeDuration"), Move->GetDodgeDuration());
		}
		if (const UCLDamageableComponent* Dmg = Char->GetDamageable())
		{
			Root->SetNumberField(TEXT("health"), Dmg->GetHealth());
			Root->SetNumberField(TEXT("shield"), Dmg->GetShield());
		}
		else if (const UCLHealthShieldComponent* HS = Char->GetHealthShield())
		{
			Root->SetNumberField(TEXT("health"), HS->GetHealth());
			Root->SetNumberField(TEXT("shield"), HS->GetShield());
		}
		if (const UCLWeaponMotorComponent* Gun = Char->GetWeaponMotor())
		{
			Root->SetStringField(TEXT("gun"), Gun->GetActiveItem().DisplayName);
			Root->SetStringField(TEXT("slot"), Gun->IsSpecialEquipped() ? TEXT("special") : TEXT("primary"));
			Root->SetNumberField(TEXT("ammo"), Gun->GetAmmoInMag());
			Root->SetNumberField(TEXT("reserve"), Gun->GetSpecialReserve());
			Root->SetNumberField(TEXT("adsAlpha"), Gun->GetAdsAlpha());
			Root->SetNumberField(TEXT("recoilPitch"), Gun->GetAdsRecoilPunch().Y);
			Root->SetNumberField(TEXT("recoilYaw"), Gun->GetAdsRecoilPunch().X);
		}
		FCLSightedTarget Sighted;
		if (CLHitscanService::QuerySightedFromPawn(Char, Sighted))
		{
			Root->SetNumberField(TEXT("sightedHealth"), Sighted.Health);
			Root->SetNumberField(TEXT("sightedShield"), Sighted.Shield);
		}
		TArray<FCLRadarContact> Radar;
		CLHitscanService::QueryRadarContacts(Char, Radar);
		Root->SetNumberField(TEXT("radarBlips"), Radar.Num());
		Root->SetNumberField(TEXT("radarRipple"), CLHitscanService::QueryRadarRippleMask(Char));
		const FVector2D AgentMove = Char->GetAgentMove();
		Root->SetNumberField(TEXT("agentMoveX"), AgentMove.X);
		Root->SetNumberField(TEXT("agentMoveY"), AgentMove.Y);
		if (const UCLIntentReceiver* Intent = Char->GetIntentReceiver())
		{
			Root->SetBoolField(TEXT("agentFire"), Intent->WantsFire());
			Root->SetBoolField(TEXT("agentAds"), Intent->WantsADS());
		}
		if (const UCLLookController* Look = Char->GetLookController())
		{
			Root->SetBoolField(TEXT("lookTrack"), Look->IsLookTracking());
			if (Look->GetLookTrackSeat().IsValid())
			{
				Root->SetStringField(TEXT("lookTrackSeat"), Look->GetLookTrackSeat().ToString(EGuidFormats::DigitsWithHyphens));
			}
		}
	}
}

void FCLAgentStateSerializer::FillSceneMenu(TSharedRef<FJsonObject> Root, UGameInstance* GI, APlayerController* LocalPC)
{
	ECLSceneId Scene = ECLSceneId::Boot;
	if (GI)
	{
		if (const UCLSceneRouter* Scenes = GI->GetSubsystem<UCLSceneRouter>())
		{
			Scene = Scenes->GetCurrentScene();
		}
	}
	if (GI && GI->GetWorld())
	{
		if (const ACLGameModeBase* GM = GI->GetWorld()->GetAuthGameMode<ACLGameModeBase>())
		{
			Scene = GM->GetSceneId();
		}
		else if (const ACLGameStateBase* GS = GI->GetWorld()->GetGameState<ACLGameStateBase>())
		{
			Scene = GS->GetSceneId();
		}
	}
	Root->SetStringField(TEXT("scene"), SceneName(Scene));
	bool bMenu = false;
	if (const ACLPlayerController* PC = Cast<ACLPlayerController>(LocalPC))
	{
		if (const UCLMainMenuOverlay* Menu = PC->GetMainMenu())
		{
			bMenu = Menu->IsOverlayVisible();
		}
	}
	Root->SetBoolField(TEXT("menu"), bMenu);
}

TSharedRef<FJsonObject> FCLAgentStateSerializer::Build(
	UGameInstance* GI,
	ACLPlayerCharacter* Char,
	APlayerController* LocalPC,
	const UCLRemoteAgentSeatMotor* Remote,
	const FGuid& AgentSeatId,
	const FGuid& ProbeSeat)
{
	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	UWorld* World = GI ? GI->GetWorld() : nullptr;
	if (Char)
	{
		FillPawn(Root, Char, World);
	}
	else
	{
		Root->SetBoolField(TEXT("ok"), false);
		Root->SetStringField(TEXT("error"), TEXT("no_local_pawn"));
	}
	FillMatchMode(Root, World);

	if (const ACLPlayerCharacter* Viewed = LocalPC ? Cast<ACLPlayerCharacter>(LocalPC->GetViewTarget()) : nullptr)
	{
		if (const UCLDamageableComponent* Dmg = Viewed->GetDamageable())
		{
			Root->SetNumberField(TEXT("viewHealth"), Dmg->GetHealth());
			Root->SetNumberField(TEXT("viewShield"), Dmg->GetShield());
		}
		else if (const UCLHealthShieldComponent* HS = Viewed->GetHealthShield())
		{
			Root->SetNumberField(TEXT("viewHealth"), HS->GetHealth());
			Root->SetNumberField(TEXT("viewShield"), HS->GetShield());
		}
		TArray<FCLRadarContact> ViewRadar;
		CLHitscanService::QueryRadarContacts(Viewed, ViewRadar);
		Root->SetNumberField(TEXT("viewRadarBlips"), ViewRadar.Num());
	}

	if (GI)
	{
		if (const UCLBotBookManager* Books = GI->GetSubsystem<UCLBotBookManager>())
		{
			const FGuid BookSeat = ProbeSeat.IsValid() ? ProbeSeat : AgentSeatId;
			Books->FillStateJson(Root, BookSeat);
		}
	}

	if (Remote)
	{
		Root->SetNumberField(TEXT("seqRemaining"), Remote->RemainingSeconds());
		Root->SetBoolField(TEXT("goto"), Remote->IsGotoActive());
		if (Remote->IsGotoActive())
		{
			const FVector Goal = Remote->GetGotoGoal();
			Root->SetNumberField(TEXT("gotoX"), Goal.X);
			Root->SetNumberField(TEXT("gotoY"), Goal.Y);
			Root->SetNumberField(TEXT("gotoZ"), Goal.Z);
			const FCLAgentGotoDriver& G = Remote->GetGotoDriver();
			Root->SetNumberField(TEXT("gotoIdx"), G.Index);
			Root->SetNumberField(TEXT("gotoPts"), G.Path.Num());
			Root->SetNumberField(TEXT("gotoDive"), G.PathAirDive.IsValidIndex(G.Index) ? G.PathAirDive[G.Index] : 0);
			Root->SetStringField(TEXT("gotoSteer"), G.SteerReason.ToString());
			Root->SetBoolField(TEXT("gotoLaunch"), G.bLaunchOk);
			Root->SetNumberField(TEXT("gotoDistLip"), G.DistLip);
			Root->SetNumberField(TEXT("gotoDistXY"), G.DistXYToWp);
			Root->SetNumberField(TEXT("gotoDZ"), G.DeltaZToWp);
			Root->SetNumberField(TEXT("gotoSteerX"), G.SteerAt.X);
			Root->SetNumberField(TEXT("gotoSteerY"), G.SteerAt.Y);
			Root->SetNumberField(TEXT("gotoSteerZ"), G.SteerAt.Z);
			Root->SetBoolField(TEXT("gotoBlocked"), G.bMoveBlocked);
			Root->SetNumberField(TEXT("gotoMoveX"), G.LastMoveXY.X);
			Root->SetNumberField(TEXT("gotoMoveY"), G.LastMoveXY.Y);
			Root->SetNumberField(TEXT("gotoStuck"), G.StuckSeconds);
			Root->SetStringField(TEXT("gotoFwd"), G.FwdKind.ToString());
			Root->SetNumberField(TEXT("gotoFwdDist"), G.FwdDist);
		}
	}
	else
	{
		Root->SetNumberField(TEXT("seqRemaining"), 0);
		Root->SetBoolField(TEXT("goto"), false);
	}

	FCLAgentStateSerializer::FillSceneMenu(Root, GI, LocalPC);
	if (GI)
	{
		if (const UCLInstanceIdentitySubsystem* Id = GI->GetSubsystem<UCLInstanceIdentitySubsystem>())
		{
			Id->StampJson(Root);
		}
		if (const UCLLobbySubsystem* Lobby = GI->GetSubsystem<UCLLobbySubsystem>())
		{
			Lobby->FillStateJson(Root);
		}
		if (const UCLSessionHub* Hub = GI->GetSubsystem<UCLSessionHub>())
		{
			Root->SetNumberField(TEXT("hubPort"), Hub->GetPort());
			Root->SetBoolField(TEXT("hub"), Hub->IsListening());
		}
	}
	if (AgentSeatId.IsValid())
	{
		Root->SetStringField(TEXT("agentSeat"), AgentSeatId.ToString(EGuidFormats::DigitsWithHyphens));
	}
	if (ProbeSeat.IsValid())
	{
		Root->SetStringField(TEXT("probeSeat"), ProbeSeat.ToString(EGuidFormats::DigitsWithHyphens));
	}
	return Root;
}
