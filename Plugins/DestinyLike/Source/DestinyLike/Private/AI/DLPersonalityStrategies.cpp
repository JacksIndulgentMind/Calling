#include "AI/DLPersonalityStrategies.h"
#include "AI/DLNavPersonalityComponent.h"
#include "AI/DLEngagementPersonalityComponent.h"

namespace
{
	class FNavWanderer final : public IDLNavStrategy
	{
	public:
		virtual void Tick(UDLNavPersonalityComponent& Owner, float DeltaTime) override { Owner.TickWanderer(DeltaTime); }
	};
	class FNavCoverCycler final : public IDLNavStrategy
	{
	public:
		virtual void Tick(UDLNavPersonalityComponent& Owner, float DeltaTime) override { Owner.TickCoverCycler(DeltaTime); }
	};
	class FNavFlanker final : public IDLNavStrategy
	{
	public:
		virtual void Tick(UDLNavPersonalityComponent& Owner, float DeltaTime) override { Owner.TickFlanker(DeltaTime); }
	};
	class FNavHoldGround final : public IDLNavStrategy
	{
	public:
		virtual void Tick(UDLNavPersonalityComponent& Owner, float DeltaTime) override { Owner.TickHoldGround(DeltaTime); }
	};
	class FNavAggressivePush final : public IDLNavStrategy
	{
	public:
		virtual void Tick(UDLNavPersonalityComponent& Owner, float DeltaTime) override { Owner.TickAggressivePush(DeltaTime); }
	};
	class FNavCircleConfused final : public IDLNavStrategy
	{
	public:
		virtual void Tick(UDLNavPersonalityComponent& Owner, float DeltaTime) override { Owner.TickCircleConfused(DeltaTime); }
	};

	class FEngagePusher final : public IDLEngageStrategy
	{
	public:
		virtual float Tick(UDLEngagementPersonalityComponent& Owner) override { Owner.EngagePusher(); return 0.35f; }
	};
	class FEngageFlanker final : public IDLEngageStrategy
	{
	public:
		virtual float Tick(UDLEngagementPersonalityComponent& Owner) override { Owner.EngageFlanker(); return 0.4f; }
	};
	class FEngageSniper final : public IDLEngageStrategy
	{
	public:
		virtual float Tick(UDLEngagementPersonalityComponent& Owner) override { Owner.EngageSniper(); return 1.1f; }
	};
	class FEngageGrenadier final : public IDLEngageStrategy
	{
	public:
		virtual float Tick(UDLEngagementPersonalityComponent& Owner) override { Owner.EngageGrenadier(); return 1.6f; }
	};
	class FEngageAmbusher final : public IDLEngageStrategy
	{
	public:
		virtual float Tick(UDLEngagementPersonalityComponent& Owner) override { Owner.EngageAmbusher(); return 0.5f; }
	};
	class FEngageCeilingShooter final : public IDLEngageStrategy
	{
	public:
		virtual float Tick(UDLEngagementPersonalityComponent& Owner) override { Owner.EngageCeilingShooter(); return 0.7f; }
	};
	class FEngageWeaponThrower final : public IDLEngageStrategy
	{
	public:
		virtual float Tick(UDLEngagementPersonalityComponent& Owner) override { Owner.EngageWeaponThrower(); return 2.5f; }
	};
	class FEngageIdleTroll final : public IDLEngageStrategy
	{
	public:
		virtual float Tick(UDLEngagementPersonalityComponent& Owner) override { Owner.EngageIdleTroll(); return 1.5f; }
	};
}

TSharedPtr<IDLNavStrategy> DLMakeNavStrategy(EDLNavPersonality Kind)
{
	switch (Kind)
	{
	case EDLNavPersonality::Wanderer: return MakeShared<FNavWanderer>();
	case EDLNavPersonality::CoverCycler: return MakeShared<FNavCoverCycler>();
	case EDLNavPersonality::Flanker: return MakeShared<FNavFlanker>();
	case EDLNavPersonality::HoldGround: return MakeShared<FNavHoldGround>();
	case EDLNavPersonality::AggressivePush: return MakeShared<FNavAggressivePush>();
	case EDLNavPersonality::CircleConfused: return MakeShared<FNavCircleConfused>();
	default: return MakeShared<FNavCoverCycler>();
	}
}

TSharedPtr<IDLEngageStrategy> DLMakeEngageStrategy(EDLEngagementPersonality Kind)
{
	switch (Kind)
	{
	case EDLEngagementPersonality::Pusher: return MakeShared<FEngagePusher>();
	case EDLEngagementPersonality::Flanker: return MakeShared<FEngageFlanker>();
	case EDLEngagementPersonality::Sniper: return MakeShared<FEngageSniper>();
	case EDLEngagementPersonality::Grenadier: return MakeShared<FEngageGrenadier>();
	case EDLEngagementPersonality::Ambusher: return MakeShared<FEngageAmbusher>();
	case EDLEngagementPersonality::CeilingShooter: return MakeShared<FEngageCeilingShooter>();
	case EDLEngagementPersonality::WeaponThrower: return MakeShared<FEngageWeaponThrower>();
	case EDLEngagementPersonality::IdleTroll: return MakeShared<FEngageIdleTroll>();
	default: return MakeShared<FEngagePusher>();
	}
}
