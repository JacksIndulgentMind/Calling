#pragma once

#include "CoreMinimal.h"
#include "AI/CLAIPersonalityData.h"

class UCLNavPersonalityComponent;
class UCLEngagementPersonalityComponent;

class ICLNavStrategy
{
public:
	virtual ~ICLNavStrategy() = default;
	virtual void Tick(UCLNavPersonalityComponent& Owner, float DeltaTime) = 0;
};

class ICLEngageStrategy
{
public:
	virtual ~ICLEngageStrategy() = default;
	virtual float Tick(UCLEngagementPersonalityComponent& Owner) = 0;
};

TSharedPtr<ICLNavStrategy> CLMakeNavStrategy(ECLNavPersonality Kind);
TSharedPtr<ICLEngageStrategy> CLMakeEngageStrategy(ECLEngagementPersonality Kind);
