#include "AI/CLPersonalityStrategies.h"
#include "AI/CLNavPersonalityComponent.h"
#include "AI/CLEngagementPersonalityComponent.h"

namespace
{
	class FNavWanderer final : public ICLNavStrategy
	{
	public:
		virtual void Tick(UCLNavPersonalityComponent& Owner, float DeltaTime) override { Owner.TickWanderer(DeltaTime); }
	};
	class FNavCoverCycler final : public ICLNavStrategy
	{
	public:
		virtual void Tick(UCLNavPersonalityComponent& Owner, float DeltaTime) override { Owner.TickCoverCycler(DeltaTime); }
	};
	class FNavFlanker final : public ICLNavStrategy
	{
	public:
		virtual void Tick(UCLNavPersonalityComponent& Owner, float DeltaTime) override { Owner.TickFlanker(DeltaTime); }
	};
	class FNavHoldGround final : public ICLNavStrategy
	{
	public:
		virtual void Tick(UCLNavPersonalityComponent& Owner, float DeltaTime) override { Owner.TickHoldGround(DeltaTime); }
	};
	class FNavAggressivePush final : public ICLNavStrategy
	{
	public:
		virtual void Tick(UCLNavPersonalityComponent& Owner, float DeltaTime) override { Owner.TickAggressivePush(DeltaTime); }
	};
	class FNavCircleConfused final : public ICLNavStrategy
	{
	public:
		virtual void Tick(UCLNavPersonalityComponent& Owner, float DeltaTime) override { Owner.TickCircleConfused(DeltaTime); }
	};

	class FEngagePusher final : public ICLEngageStrategy
	{
	public:
		virtual float Tick(UCLEngagementPersonalityComponent& Owner) override { Owner.EngagePusher(); return 0.35f; }
	};
	class FEngageFlanker final : public ICLEngageStrategy
	{
	public:
		virtual float Tick(UCLEngagementPersonalityComponent& Owner) override { Owner.EngageFlanker(); return 0.4f; }
	};
	class FEngageSniper final : public ICLEngageStrategy
	{
	public:
		virtual float Tick(UCLEngagementPersonalityComponent& Owner) override { Owner.EngageSniper(); return 1.1f; }
	};
	class FEngageGrenadier final : public ICLEngageStrategy
	{
	public:
		virtual float Tick(UCLEngagementPersonalityComponent& Owner) override { Owner.EngageGrenadier(); return 1.6f; }
	};
	class FEngageAmbusher final : public ICLEngageStrategy
	{
	public:
		virtual float Tick(UCLEngagementPersonalityComponent& Owner) override { Owner.EngageAmbusher(); return 0.5f; }
	};
	class FEngageCeilingShooter final : public ICLEngageStrategy
	{
	public:
		virtual float Tick(UCLEngagementPersonalityComponent& Owner) override { Owner.EngageCeilingShooter(); return 0.7f; }
	};
	class FEngageWeaponThrower final : public ICLEngageStrategy
	{
	public:
		virtual float Tick(UCLEngagementPersonalityComponent& Owner) override { Owner.EngageWeaponThrower(); return 2.5f; }
	};
	class FEngageIdleTroll final : public ICLEngageStrategy
	{
	public:
		virtual float Tick(UCLEngagementPersonalityComponent& Owner) override { Owner.EngageIdleTroll(); return 1.5f; }
	};
}

TSharedPtr<ICLNavStrategy> CLMakeNavStrategy(ECLNavPersonality Kind)
{
	switch (Kind)
	{
	case ECLNavPersonality::Wanderer: return MakeShared<FNavWanderer>();
	case ECLNavPersonality::CoverCycler: return MakeShared<FNavCoverCycler>();
	case ECLNavPersonality::Flanker: return MakeShared<FNavFlanker>();
	case ECLNavPersonality::HoldGround: return MakeShared<FNavHoldGround>();
	case ECLNavPersonality::AggressivePush: return MakeShared<FNavAggressivePush>();
	case ECLNavPersonality::CircleConfused: return MakeShared<FNavCircleConfused>();
	default: return MakeShared<FNavCoverCycler>();
	}
}

TSharedPtr<ICLEngageStrategy> CLMakeEngageStrategy(ECLEngagementPersonality Kind)
{
	switch (Kind)
	{
	case ECLEngagementPersonality::Pusher: return MakeShared<FEngagePusher>();
	case ECLEngagementPersonality::Flanker: return MakeShared<FEngageFlanker>();
	case ECLEngagementPersonality::Sniper: return MakeShared<FEngageSniper>();
	case ECLEngagementPersonality::Grenadier: return MakeShared<FEngageGrenadier>();
	case ECLEngagementPersonality::Ambusher: return MakeShared<FEngageAmbusher>();
	case ECLEngagementPersonality::CeilingShooter: return MakeShared<FEngageCeilingShooter>();
	case ECLEngagementPersonality::WeaponThrower: return MakeShared<FEngageWeaponThrower>();
	case ECLEngagementPersonality::IdleTroll: return MakeShared<FEngageIdleTroll>();
	default: return MakeShared<FEngagePusher>();
	}
}
