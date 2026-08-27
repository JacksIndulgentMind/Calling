#pragma once

#include "CoreMinimal.h"
#include "AI/DLAIPersonalityData.h"

class UDLNavPersonalityComponent;
class UDLEngagementPersonalityComponent;

class IDLNavStrategy
{
public:
	virtual ~IDLNavStrategy() = default;
	virtual void Tick(UDLNavPersonalityComponent& Owner, float DeltaTime) = 0;
};

class IDLEngageStrategy
{
public:
	virtual ~IDLEngageStrategy() = default;
	virtual float Tick(UDLEngagementPersonalityComponent& Owner) = 0;
};

TSharedPtr<IDLNavStrategy> DLMakeNavStrategy(EDLNavPersonality Kind);
TSharedPtr<IDLEngageStrategy> DLMakeEngageStrategy(EDLEngagementPersonality Kind);
