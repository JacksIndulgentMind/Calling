#include "UI/CLHudRadarController.h"
#include "Player/CLPlayerCharacter.h"
#include "Combat/CLHitscanService.h"
#include "Game/CLLobbySubsystem.h"
#include "Game/CLParticipantSeat.h"
#include "Game/CLLobbyTypes.h"
#include "Camera/CameraComponent.h"
#include "Engine/GameInstance.h"
#include "Blueprint/UserWidget.h"

void UCLHudRadarController::Refresh(const ACLPlayerCharacter* Viewer, float DeltaTime)
{
	RadarBlips.Reset();
	if (!Viewer)
	{
		RadarTracks.Reset();
		RadarScatterClock = 0.f;
		RadarWedge[0] = RadarWedge[1] = RadarWedge[2] = 0.f;
		return;
	}

	constexpr float FadeIn = 0.18f;
	constexpr float RippleDecay = 0.22f;
	constexpr float RangeCm = 3500.f;
	constexpr int32 TrailMax = 6;
	TArray<FCLRadarContact> Contacts;
	CLHitscanService::QueryRadarContacts(Viewer, Contacts, RangeCm, true);

	RadarTracks.RemoveAll([&](const FCLRadarTrack& Track)
	{
		if (!Track.Actor.IsValid())
		{
			return true;
		}
		for (const FCLRadarContact& Contact : Contacts)
		{
			if (Contact.Actor.Get() == Track.Actor.Get())
			{
				return false;
			}
		}
		return true;
	});

	auto FindTrack = [this](const AActor* Actor) -> FCLRadarTrack*
	{
		for (FCLRadarTrack& Track : RadarTracks)
		{
			if (Track.Actor.Get() == Actor)
			{
				return &Track;
			}
		}
		return nullptr;
	};

	auto Resample = [](FCLRadarTrack& Track, const FCLRadarContact& Contact)
	{
		const float DistT = FMath::Clamp(Contact.DistXY / RangeCm, 0.f, 1.f);
		const float MaxAng = Contact.bLowProfile ? FMath::Lerp(36.f, 99.f, DistT) : FMath::Lerp(4.5f, 24.f, DistT);
		const float RadialFrac = Contact.bLowProfile ? FMath::Lerp(0.12f, 0.54f, DistT) : FMath::Lerp(0.045f, 0.18f, DistT);
		Track.ScatterYawDeg = FMath::FRandRange(-MaxAng, MaxAng);
		Track.ScatterRadial = Contact.DistXY * RadialFrac * FMath::FRandRange(-1.f, 1.f);
	};

	RadarScatterClock += DeltaTime;
	const bool bResample = RadarScatterClock >= (1.f / 15.f);
	if (bResample)
	{
		RadarScatterClock = 0.f;
	}

	for (const FCLRadarContact& Contact : Contacts)
	{
		AActor* Actor = Contact.Actor.Get();
		if (!Actor)
		{
			continue;
		}
		FCLRadarTrack* Track = FindTrack(Actor);
		const bool bNew = Track == nullptr;
		if (bNew)
		{
			FCLRadarTrack Fresh;
			Fresh.Actor = Actor;
			Fresh.Alpha = 0.f;
			Resample(Fresh, Contact);
			RadarTracks.Add(Fresh);
			Track = &RadarTracks.Last();
		}
		Track->Alpha = FMath::Clamp(Track->Alpha + DeltaTime / FadeIn, 0.f, 1.f);
		if (bResample && !bNew)
		{
			if (Track->bHasOffset)
			{
				Track->Trail.Add(Track->LastOffset);
				while (Track->Trail.Num() > TrailMax)
				{
					Track->Trail.RemoveAt(0);
				}
			}
			Resample(*Track, Contact);
		}
	}

	FVector Eye = Viewer->GetActorLocation();
	FVector Fwd = Viewer->GetActorForwardVector();
	if (const UCameraComponent* Cam = Viewer->GetFollowCamera())
	{
		Eye = Cam->GetComponentLocation();
		Fwd = Cam->GetForwardVector();
	}
	Fwd.Z = 0.f;
	if (!Fwd.Normalize())
	{
		Fwd = FVector::ForwardVector;
	}
	const FVector Right(-Fwd.Y, Fwd.X, 0.f);

	const UCLLobbySubsystem* Lobby = nullptr;
	if (const UUserWidget* Host = Cast<UUserWidget>(GetOuter()))
	{
		if (const UGameInstance* GI = Host->GetGameInstance())
		{
			Lobby = GI->GetSubsystem<UCLLobbySubsystem>();
		}
	}

	for (const FCLRadarContact& Contact : Contacts)
	{
		FCLRadarTrack* Track = FindTrack(Contact.Actor.Get());
		if (!Track)
		{
			continue;
		}
		const float Forward = (Contact.Location.X - Eye.X) * Fwd.X + (Contact.Location.Y - Eye.Y) * Fwd.Y;
		const float RightAmt = (Contact.Location.X - Eye.X) * Right.X + (Contact.Location.Y - Eye.Y) * Right.Y;
		const float TrueAng = FMath::Atan2(RightAmt, Forward);
		const float TrueR = FVector::Dist2D(Contact.Location, Eye);
		const float PaintAng = TrueAng + FMath::DegreesToRadians(Track->ScatterYawDeg);
		const float PaintR = FMath::Clamp(TrueR + Track->ScatterRadial, 0.f, RangeCm);
		const float U = (PaintR / RangeCm) * FMath::Sin(PaintAng);
		const float V = (PaintR / RangeCm) * FMath::Cos(PaintAng);
		const FVector2D Offset(U, -V);
		Track->LastOffset = Offset;
		Track->bHasOffset = true;

		ECLPvpTeam Team = ECLPvpTeam::Unassigned;
		if (Lobby)
		{
			for (const UCLParticipantSeat* Seat : Lobby->GetSeats())
			{
				if (Seat && Seat->GetDrivenPawn() == Contact.Actor.Get())
				{
					Team = Seat->GetTeam();
					break;
				}
			}
		}
		FLinearColor Col(0.88f, 0.86f, 0.72f, 1.f);
		if (Team == ECLPvpTeam::Red)
		{
			Col = FLinearColor(0.92f, 0.16f, 0.12f, 1.f);
		}
		else if (Team == ECLPvpTeam::Blue)
		{
			Col = FLinearColor(0.18f, 0.48f, 1.f, 1.f);
		}

		const float DistT = FMath::Clamp(Contact.DistXY / RangeCm, 0.f, 1.f);
		FCLRadarPaintBlip Blip;
		Blip.Offset = Offset;
		Blip.Trail = Track->Trail;
		Blip.Color = Col;
		Blip.Alpha = Track->Alpha * FMath::Lerp(1.f, 0.22f, DistT) * (Contact.bLowProfile ? 0.55f : 1.f);
		Blip.Size = FMath::Lerp(11.f, 5.2f, DistT);
		RadarBlips.Add(Blip);
	}

	TArray<FCLRadarContact> Footsteps;
	CLHitscanService::QueryRadarContacts(Viewer, Footsteps, RangeCm, false);
	bool bWedgeOn[3] = {};
	for (const FCLRadarContact& Contact : Footsteps)
	{
		const float Forward = (Contact.Location.X - Eye.X) * Fwd.X + (Contact.Location.Y - Eye.Y) * Fwd.Y;
		const float RightAmt = (Contact.Location.X - Eye.X) * Right.X + (Contact.Location.Y - Eye.Y) * Right.Y;
		const float Deg = FMath::RadiansToDegrees(FMath::Atan2(RightAmt, Forward));
		int32 Wedge = 2;
		if (Deg >= -60.f && Deg < 60.f)
		{
			Wedge = 0;
		}
		else if (Deg >= 60.f)
		{
			Wedge = 1;
		}
		bWedgeOn[Wedge] = true;
	}
	for (int32 i = 0; i < 3; ++i)
	{
		if (bWedgeOn[i])
		{
			RadarWedge[i] = FMath::Clamp(RadarWedge[i] + DeltaTime / FadeIn, 0.f, 1.f);
		}
		else
		{
			RadarWedge[i] = FMath::Max(0.f, RadarWedge[i] - DeltaTime / RippleDecay);
		}
	}
}
