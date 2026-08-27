#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Core/DLError.h"
#include "DLErrorBoundary.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDLOnReportedError, const FDLError&, Error);

/**
 * Global error boundary. User errors surface; system errors get an incident id
 * and a local log. NonDeterministic I/O maps here when the call site cannot recover.
 */
UCLASS()
class DESTINYLIKE_API UDLErrorBoundary : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	UFUNCTION(BlueprintCallable, Category = "DestinyLike|Error")
	FDLError Report(FDLError Error);

	static FDLError ReportStatic(UObject* WorldContext, FDLError Error);

	UPROPERTY(BlueprintAssignable, Category = "DestinyLike|Error")
	FDLOnReportedError OnReportedError;

protected:
	void ReloadSettings();
	void PurgeOldIncidents() const;
	FString WriteIncident(const FDLError& Error) const;
	FString ErrorsDir() const;

	UPROPERTY()
	FString SupportContact = TEXT("See Saved/DestinyLike/Errors and contact support.");

	UPROPERTY()
	int32 ErrorLogRetentionDays = 14;
};
