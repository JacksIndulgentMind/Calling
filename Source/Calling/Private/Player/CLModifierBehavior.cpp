#include "Player/CLModifierBehavior.h"
#include "Player/CLWeaponBehaviorComponent.h"

namespace
{
	class FOpeningShotRange final : public ICLModifierBehavior
	{
	public:
		explicit FOpeningShotRange(float InScale) : Scale(InScale) {}
		virtual float AccuracyBonus(const UCLWeaponBehaviorComponent& Owner) const override
		{
			return Owner.IsOpeningShotActive() ? Scale : 0.f;
		}
	private:
		float Scale = 0.f;
	};

	class FFirstShotAccuracy final : public ICLModifierBehavior
	{
	public:
		explicit FFirstShotAccuracy(float InScale) : Scale(InScale) {}
		virtual float AccuracyBonus(const UCLWeaponBehaviorComponent& Owner) const override
		{
			return Owner.IsOpeningShotActive() ? Scale : 0.f;
		}
	private:
		float Scale = 0.f;
	};

	class FKillDamageWindow final : public ICLModifierBehavior
	{
	public:
		explicit FKillDamageWindow(float InScale) : Scale(InScale) {}
		virtual void OnKill(UCLWeaponBehaviorComponent& Owner) override
		{
			Owner.StartKillClipWindow(1.5f);
		}
		virtual float DamageBonus(const UCLWeaponBehaviorComponent& Owner) const override
		{
			return Owner.IsKillClipActive() ? Scale : 0.f;
		}
	private:
		float Scale = 0.f;
	};

	class FPrecisionReloadSpeed final : public ICLModifierBehavior
	{
	public:
		explicit FPrecisionReloadSpeed(float InScale) : Scale(InScale) {}
		virtual void OnPrecisionKill(UCLWeaponBehaviorComponent& Owner) override
		{
			Owner.StartOutlawWindow(2.0f);
		}
		virtual float ReloadSpeedBonus(const UCLWeaponBehaviorComponent& Owner) const override
		{
			return Owner.IsOutlawActive() ? Scale : 0.f;
		}
	private:
		float Scale = 0.f;
	};

	class FAdsWhileSlideBonus final : public ICLModifierBehavior
	{
	public:
		explicit FAdsWhileSlideBonus(float InScale) : Scale(InScale) {}
		virtual float SlideAdsAccuracyBonus(const UCLWeaponBehaviorComponent& Owner) const override
		{
			return Scale;
		}
	private:
		float Scale = 0.f;
	};

	class FProxDetonate final : public ICLModifierBehavior
	{
	public:
		explicit FProxDetonate(float /*InScale*/) {}
		virtual bool HasProxDetonate() const override { return true; }
	};

	using FCLModifierFactory = TFunction<TSharedPtr<ICLModifierBehavior>(float)>;

	TMap<FName, FCLModifierFactory>& Registry()
	{
		static TMap<FName, FCLModifierFactory> Map;
		static bool bReady = false;
		if (!bReady)
		{
			bReady = true;
			Map.Add(FName(TEXT("opening_shot_range")), [](float S) -> TSharedPtr<ICLModifierBehavior> { return MakeShared<FOpeningShotRange>(S); });
			Map.Add(FName(TEXT("first_shot_accuracy")), [](float S) -> TSharedPtr<ICLModifierBehavior> { return MakeShared<FFirstShotAccuracy>(S); });
			Map.Add(FName(TEXT("kill_damage_window")), [](float S) -> TSharedPtr<ICLModifierBehavior> { return MakeShared<FKillDamageWindow>(S); });
			Map.Add(FName(TEXT("precision_reload_speed")), [](float S) -> TSharedPtr<ICLModifierBehavior> { return MakeShared<FPrecisionReloadSpeed>(S); });
			Map.Add(FName(TEXT("ads_while_slide_bonus")), [](float S) -> TSharedPtr<ICLModifierBehavior> { return MakeShared<FAdsWhileSlideBonus>(S); });
			Map.Add(FName(TEXT("prox_detonate")), [](float S) -> TSharedPtr<ICLModifierBehavior> { return MakeShared<FProxDetonate>(S); });
		}
		return Map;
	}
}

TSharedPtr<ICLModifierBehavior> CLMakeModifierBehavior(FName BehaviorId, float Scale)
{
	if (BehaviorId.IsNone())
	{
		return nullptr;
	}
	if (FCLModifierFactory* Factory = Registry().Find(BehaviorId))
	{
		return (*Factory)(Scale);
	}
	return nullptr;
}
