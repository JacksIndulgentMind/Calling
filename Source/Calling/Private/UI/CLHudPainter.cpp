#include "UI/CLHudPainter.h"
#include "UI/CLHudRadarController.h"
#include "Player/CLPlayerCharacter.h"
#include "Player/CLWeaponMotorComponent.h"
#include "Loot/CLLootRulesService.h"
#include "Brushes/SlateColorBrush.h"
#include "Rendering/DrawElements.h"

void UCLHudPainter::PaintSightCrosshair(const FGeometry& AllottedGeometry, FSlateWindowElementList& OutDrawElements, int32 LayerId,
	const ACLPlayerCharacter* Char) const
{
	if (!Char || !Char->GetWeaponMotor() || Char->GetThirdPersonAlpha() > 0.55f)
	{
		return;
	}

	const UCLWeaponMotorComponent* Gun = Char->GetWeaponMotor();
	const ECLSightViewKind SightKind = UCLLootRulesService::SightViewKind(Gun->GetSightId());
	const float Ads = Gun->GetAdsEase();
	const float Tighten = FMath::Lerp(1.f, 0.62f, Ads);
	FVector2D Center = AllottedGeometry.GetLocalSize() * 0.5f;
	if (Ads > 0.4f)
	{
		const FVector2D Punch = Char->GetAdsReticlePunch();
		const float PxPerDeg = AllottedGeometry.GetLocalSize().X / 90.f;
		Center += FVector2D(Punch.X, -Punch.Y) * PxPerDeg;
	}
	const FLinearColor Color(1.f, 1.f, 1.f, 0.92f);
	const FPaintGeometry Paint = AllottedGeometry.ToPaintGeometry();

	auto Line = [&](const FVector2D& A, const FVector2D& B, float Thickness, const FLinearColor& Col)
	{
		TArray<FVector2D> Pts;
		Pts.Add(A);
		Pts.Add(B);
		FSlateDrawElement::MakeLines(OutDrawElements, LayerId, Paint, Pts, ESlateDrawEffect::None, Col, false, Thickness);
	};

	auto Ring = [&](float Radius, float Thickness, const FLinearColor& Col)
	{
		TArray<FVector2D> Pts;
		const int32 Segs = 28;
		for (int32 i = 0; i <= Segs; ++i)
		{
			const float A = (2.f * PI * i) / Segs;
			Pts.Add(Center + FVector2D(FMath::Cos(A), FMath::Sin(A)) * Radius);
		}
		FSlateDrawElement::MakeLines(OutDrawElements, LayerId, Paint, Pts, ESlateDrawEffect::None, Col, false, Thickness);
	};

	if (SightKind == ECLSightViewKind::Iron)
	{
		const float Arm = 11.f * Tighten;
		const float Gap = 3.f * Tighten;
		Line(Center + FVector2D(-Arm, Arm * 0.45f), Center + FVector2D(-Gap, Gap * 0.2f), 1.8f, Color);
		Line(Center + FVector2D(Arm, Arm * 0.45f), Center + FVector2D(Gap, Gap * 0.2f), 1.8f, Color);
		Line(Center + FVector2D(-Arm * 0.55f, Arm * 0.7f), Center + FVector2D(Arm * 0.55f, Arm * 0.7f), 1.6f, Color);
		return;
	}

	if (SightKind == ECLSightViewKind::Scope)
	{
		if (Ads > 0.02f)
		{
			const FVector2D Size = AllottedGeometry.GetLocalSize();
			const float Outer = FMath::Min(Size.X, Size.Y) * 0.48f;
			const float Inner = Outer * 0.36f;
			const FLinearColor Rim(0.02f, 0.02f, 0.025f, 0.92f * Ads);
			for (float R = Inner; R <= Outer; R += 3.5f)
			{
				Ring(R, 3.2f, Rim);
			}
			static const FSlateColorBrush DarkBrush(FLinearColor::White);
			const FLinearColor Vignette(0.01f, 0.01f, 0.015f, 0.72f * Ads);
			auto Box = [&](const FVector2D& Pos, const FVector2D& BoxSize)
			{
				FSlateDrawElement::MakeBox(
					OutDrawElements,
					LayerId,
					AllottedGeometry.ToPaintGeometry(BoxSize, FSlateLayoutTransform(1.f, FVector2f(Pos))),
					&DarkBrush,
					ESlateDrawEffect::None,
					Vignette);
			};
			const float Hole = Inner * 1.05f;
			Box(FVector2D(0.f, 0.f), FVector2D(Size.X, FMath::Max(0.f, Center.Y - Hole)));
			Box(FVector2D(0.f, Center.Y + Hole), FVector2D(Size.X, FMath::Max(0.f, Size.Y - (Center.Y + Hole))));
			Box(FVector2D(0.f, Center.Y - Hole), FVector2D(FMath::Max(0.f, Center.X - Hole), Hole * 2.f));
			Box(FVector2D(Center.X + Hole, Center.Y - Hole), FVector2D(FMath::Max(0.f, Size.X - (Center.X + Hole)), Hole * 2.f));
			const float Gap = 4.f;
			Line(Center + FVector2D(-18.f, 0.f), Center + FVector2D(-Gap, 0.f), 1.1f, Color);
			Line(Center + FVector2D(Gap, 0.f), Center + FVector2D(18.f, 0.f), 1.1f, Color);
			Line(Center + FVector2D(0.f, -18.f), Center + FVector2D(0.f, -Gap), 1.1f, Color);
			Line(Center + FVector2D(0.f, Gap), Center + FVector2D(0.f, 18.f), 1.1f, Color);
		}
		return;
	}

	if (Ads < 0.45f)
	{
		const float Radius = 6.f * Tighten;
		Ring(Radius, 1.2f, FLinearColor(1.f, 1.f, 1.f, 0.45f));
		const float Pip = 1.2f;
		Line(Center + FVector2D(-Pip, 0.f), Center + FVector2D(Pip, 0.f), 1.2f, Color);
		Line(Center + FVector2D(0.f, -Pip), Center + FVector2D(0.f, Pip), 1.2f, Color);
		return;
	}

	const FLinearColor Led(1.f, 0.12f, 0.08f, 0.95f);
	const float Pip = 1.6f;
	Line(Center + FVector2D(-Pip, 0.f), Center + FVector2D(Pip, 0.f), 1.8f, Led);
	Line(Center + FVector2D(0.f, -Pip), Center + FVector2D(0.f, Pip), 1.8f, Led);
}

void UCLHudPainter::PaintHurtVignette(const FGeometry& AllottedGeometry, FSlateWindowElementList& OutDrawElements, int32 LayerId,
	const ACLPlayerCharacter* Char) const
{
	if (!Char)
	{
		return;
	}
	const float Alpha = Char->GetHurtAlpha();
	if (Alpha <= 0.01f)
	{
		return;
	}

	const FVector2D Size = AllottedGeometry.GetLocalSize();
	const float EdgeX = FMath::Max(48.f, Size.X * 0.11f);
	const float EdgeY = FMath::Max(36.f, Size.Y * 0.14f);
	const FLinearColor Col(0.62f, 0.02f, 0.05f, 0.58f * Alpha);
	static const FSlateColorBrush Brush(FLinearColor::White);

	auto Box = [&](const FVector2D& Pos, const FVector2D& BoxSize)
	{
		FSlateDrawElement::MakeBox(
			OutDrawElements,
			LayerId,
			AllottedGeometry.ToPaintGeometry(BoxSize, FSlateLayoutTransform(1.f, FVector2f(Pos))),
			&Brush,
			ESlateDrawEffect::None,
			Col);
	};

	Box(FVector2D(0.f, 0.f), FVector2D(Size.X, EdgeY));
	Box(FVector2D(0.f, Size.Y - EdgeY), FVector2D(Size.X, EdgeY));
	Box(FVector2D(0.f, EdgeY), FVector2D(EdgeX, Size.Y - EdgeY * 2.f));
	Box(FVector2D(Size.X - EdgeX, EdgeY), FVector2D(EdgeX, Size.Y - EdgeY * 2.f));
}

void UCLHudPainter::PaintRadar(const FGeometry& AllottedGeometry, FSlateWindowElementList& OutDrawElements, int32 LayerId,
	const TArray<FCLRadarPaintBlip>& Blips, const float Wedge[3], bool bHasCharacter) const
{
	if (!bHasCharacter)
	{
		return;
	}
	const FVector2D Size = AllottedGeometry.GetLocalSize();
	const float Short = FMath::Max(1.f, FMath::Min(Size.X, Size.Y));
	const float Dia = Short * 0.16f;
	const float Inset = Short * 0.02f;
	const FVector2D Center(Inset + Dia * 0.5f, Inset + Dia * 0.5f);
	const float Radius = Dia * 0.5f;
	const FPaintGeometry Paint = AllottedGeometry.ToPaintGeometry();
	const FLinearColor RingCol(1.f, 1.f, 1.f, 0.28f);
	const FLinearColor TickCol(1.f, 1.f, 1.f, 0.45f);
	const FLinearColor InnerFill(0.06f, 0.06f, 0.07f, 0.78f);
	const FLinearColor RippleCol(0.82f, 0.88f, 0.95f, 1.f);

	auto Ring = [&](float Rad, float Thickness, const FLinearColor& Col)
	{
		TArray<FVector2D> Pts;
		const int32 Segs = 32;
		for (int32 i = 0; i <= Segs; ++i)
		{
			const float A = (2.f * PI * i) / Segs;
			Pts.Add(Center + FVector2D(FMath::Cos(A), FMath::Sin(A)) * Rad);
		}
		FSlateDrawElement::MakeLines(OutDrawElements, LayerId, Paint, Pts, ESlateDrawEffect::None, Col, false, Thickness);
	};

	auto Line = [&](const FVector2D& A, const FVector2D& B, float Thickness, const FLinearColor& Col)
	{
		TArray<FVector2D> Pts;
		Pts.Add(A);
		Pts.Add(B);
		FSlateDrawElement::MakeLines(OutDrawElements, LayerId, Paint, Pts, ESlateDrawEffect::None, Col, false, Thickness);
	};

	auto Arc = [&](float Rad, float Deg0, float Deg1, float Thickness, const FLinearColor& Col)
	{
		TArray<FVector2D> Pts;
		const int32 Segs = 12;
		for (int32 i = 0; i <= Segs; ++i)
		{
			const float Deg = FMath::Lerp(Deg0, Deg1, i / static_cast<float>(Segs));
			const float A = FMath::DegreesToRadians(Deg);
			Pts.Add(Center + FVector2D(FMath::Sin(A), -FMath::Cos(A)) * Rad);
		}
		FSlateDrawElement::MakeLines(OutDrawElements, LayerId, Paint, Pts, ESlateDrawEffect::None, Col, false, Thickness);
	};

	for (int32 i = 1; i <= 8; ++i)
	{
		const float T = i / 8.f;
		Ring(Radius * 0.5f * T, FMath::Max(2.2f, Radius * 0.08f), InnerFill);
	}
	Ring(Radius, 6.f, FLinearColor(1.f, 1.f, 1.f, 0.12f));
	Ring(Radius, 3.2f, RingCol);

	static const float WedgeDeg[3][2] = { { -60.f, 60.f }, { 60.f, 180.f }, { -180.f, -60.f } };
	for (int32 w = 0; w < 3; ++w)
	{
		if (Wedge[w] < 0.02f)
		{
			continue;
		}
		for (int32 k = 0; k < 8; ++k)
		{
			const float RimT = (k + 1) / 8.f;
			FLinearColor Col = RippleCol;
			Col.A = Wedge[w] * FMath::Lerp(0.04f, 0.32f, RimT);
			Arc(Radius * RimT, WedgeDeg[w][0], WedgeDeg[w][1], FMath::Lerp(1.6f, 3.4f, RimT), Col);
		}
	}

	Line(Center + FVector2D(0.f, -Radius * 0.82f), Center + FVector2D(0.f, -Radius), 2.2f, TickCol);
	Line(Center + FVector2D(-2.2f, 0.f), Center + FVector2D(2.2f, 0.f), 1.2f, TickCol);
	Line(Center + FVector2D(0.f, -2.2f), Center + FVector2D(0.f, 2.2f), 1.2f, TickCol);

	static const FSlateColorBrush Brush(FLinearColor::White);
	auto BlipBox = [&](const FVector2D& Pos, float S, const FLinearColor& Col)
	{
		FSlateDrawElement::MakeBox(
			OutDrawElements,
			LayerId + 1,
			AllottedGeometry.ToPaintGeometry(FVector2D(S, S), FSlateLayoutTransform(1.f, FVector2f(Pos - FVector2D(S * 0.5f, S * 0.5f)))),
			&Brush,
			ESlateDrawEffect::None,
			Col);
	};
	auto BloomBlip = [&](const FVector2D& Pos, float S, const FLinearColor& Base, float Alpha)
	{
		FLinearColor Halo = Base;
		Halo.A = Alpha * 0.12f;
		BlipBox(Pos, S * 2.4f, Halo);
		FLinearColor Mid = Base;
		Mid.A = Alpha * 0.28f;
		BlipBox(Pos, S * 1.6f, Mid);
		FLinearColor Core = Base;
		Core.A = Alpha;
		BlipBox(Pos, S, Core);
	};

	for (const FCLRadarPaintBlip& Blip : Blips)
	{
		if (Blip.Alpha < 0.02f)
		{
			continue;
		}
		for (int32 i = 0; i < Blip.Trail.Num(); ++i)
		{
			const float TrailA = Blip.Alpha * (i + 1) / 6.f;
			if (TrailA < 0.02f)
			{
				continue;
			}
			const FVector2D Pos = Center + Blip.Trail[i] * Radius;
			BloomBlip(Pos, Blip.Size * 0.72f, Blip.Color, TrailA);
		}
		BloomBlip(Center + Blip.Offset * Radius, Blip.Size, Blip.Color, Blip.Alpha);
	}
}
