#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Core/CLError.h"
#include "CLErrorBoundary.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCLOnReportedError, const FCLError&, Error);

/**
 * Global error boundary. User errors surface; system errors get an incident id
 * and a local log. NonDeterministic I/O maps here when the call site cannot recover.
 */
UCLASS()
class CALLING_API UCLErrorBoundary : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	UFUNCTION(BlueprintCallable, Category = "Calling|Error")
	FCLError Report(FCLError Error);

	static FCLError ReportStatic(UObject* WorldContext, FCLError Error);

	/** Crash / system-error path. Shares Saved/Calling/Errors and retention. */
	static FString WriteSystemIncident();

	UPROPERTY(BlueprintAssignable, Category = "Calling|Error")
	FCLOnReportedError OnReportedError;

protected:
	void ReloadSettings();
	void PurgeOldIncidents() const;
	FString WriteIncident(const FCLError& Error) const;
	FString ErrorsDir() const;

	static FString ErrorsDirPath();
	static int32 RetentionDaysFromIni();
	static void PurgeOldIncidentFiles();
	static FString WriteIncidentFile(const FCLError& Error, const TCHAR* FileSuffix);

	UPROPERTY()
	FString SupportContact = TEXT("See Saved/Calling/Errors and contact support.");

	UPROPERTY()
	int32 ErrorLogRetentionDays = 14;
};
