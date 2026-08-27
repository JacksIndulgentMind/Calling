#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Game/CLLobbyTypes.h"
#include "Templates/Function.h"
#include "CLGateCountdown.generated.h"

class UCLGateBox;
class UCLInvoiceService;
class UCLSeatRegistry;
class ACLGameModeBase;

UCLASS()
class CALLING_API UCLGateCountdown : public UObject
{
	GENERATED_BODY()

public:
	void ResetOpen();
	void ResetLocked();
	void InstallFromConfig();
	void LoadLaunchSecondsFromConfig();
	void ClearGate();

	bool HasGate() const { return Gate != nullptr; }
	const FCLLobbyGate* GetGate() const;
	bool IsUnlocked() const { return bGameplayUnlocked; }
	void SetUnlocked(bool bUnlocked) { bGameplayUnlocked = bUnlocked; }
	bool IsMatchStartQueued() const { return bMatchStartQueued; }
	bool IsCountdownRunning() const { return bCountdownRunning; }
	float GetCountdownRemaining() const { return CountdownRemaining; }
	bool IsReadyLocked() const { return bMatchStartQueued || (Gate != nullptr && bGameplayUnlocked); }

	void StartCountdownIfReady(const FCLLobbyInvoice* Invoice, int32 ReadyCount);
	bool RequestGo(const FCLLobbyInvoice* Invoice, int32 ReadyCount, bool bHostOk, TFunction<void()> OnFinishGo);
	void TickCountdown(float DeltaSeconds, TFunction<void()> OnFinishGo);
	void CancelCountdownIfUnready();
	void FinishGo(TFunction<void()> OnUnlocked);

protected:
	UPROPERTY()
	TObjectPtr<UCLGateBox> Gate;

	UPROPERTY()
	bool bGameplayUnlocked = true;

	UPROPERTY()
	float CountdownRemaining = 0.f;

	UPROPERTY()
	bool bCountdownRunning = false;

	UPROPERTY()
	bool bMatchStartQueued = false;

	UPROPERTY()
	float LaunchCountdownSeconds = 1.f;
};
