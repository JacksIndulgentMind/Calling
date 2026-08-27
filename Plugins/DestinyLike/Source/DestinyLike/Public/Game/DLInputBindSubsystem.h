#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Input/DLInputTypes.h"
#include "DLInputBindSubsystem.generated.h"

/**
 * Machine-wide primary/secondary/gamepad keybinds. Localhost file, not a shipping account.
 * Column uniqueness: a chord is Primary on at most one action, Secondary on at most one,
 * and Gamepad on at most one. Same chord across columns is the same-tick macro.
 */
UCLASS()
class DESTINYLIKE_API UDLInputBindSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	const TMap<EDLBindableAction, FDLActionBinds>& GetBinds() const { return Binds; }

	FDLActionBinds GetBinds(EDLBindableAction Action) const;
	FDLKeyChord GetChord(EDLBindableAction Action, EDLBindColumn Column) const;
	FString GetChordDisplay(EDLBindableAction Action, EDLBindColumn Column) const;

	TArray<FDLBindUse> FindUses(const FDLKeyChord& Chord) const;
	FDLBindUse FindSameColumnUse(const FDLKeyChord& Chord, EDLBindColumn Column, EDLBindableAction ExceptAction) const;

	/** Assigns Chord to the slot. Steals the same column elsewhere. Returns true if a steal happened. */
	bool SetBind(EDLBindableAction Action, EDLBindColumn Column, const FDLKeyChord& Chord, FDLBindUse& OutStolen);
	void ClearBind(EDLBindableAction Action, EDLBindColumn Column);
	void ResetDefaults();

	bool SaveToDisk() const;
	void LoadFromDisk();
	void LoadQuirkySnapshot();

	static TMap<EDLBindableAction, FDLActionBinds> MakeDefaults();

private:
	FString GetSavePath() const;
	static FString GetQuirkyPath();
	bool ApplyBindsJson(const FString& Content);
	void EnsureComplete();

	UPROPERTY()
	TMap<EDLBindableAction, FDLActionBinds> Binds;
};
