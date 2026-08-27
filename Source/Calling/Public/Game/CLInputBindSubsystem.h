#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Input/CLInputTypes.h"
#include "CLInputBindSubsystem.generated.h"

/**
 * Machine-wide primary/secondary/gamepad keybinds. Localhost file, not a shipping account.
 * Column uniqueness: a chord is Primary on at most one action, Secondary on at most one,
 * and Gamepad on at most one. Same chord across columns is the same-tick macro.
 */
UCLASS()
class CALLING_API UCLInputBindSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	const TMap<ECLBindableAction, FCLActionBinds>& GetBinds() const { return Binds; }

	FCLActionBinds GetBinds(ECLBindableAction Action) const;
	FCLKeyChord GetChord(ECLBindableAction Action, ECLBindColumn Column) const;
	FString GetChordDisplay(ECLBindableAction Action, ECLBindColumn Column) const;

	TArray<FCLBindUse> FindUses(const FCLKeyChord& Chord) const;
	FCLBindUse FindSameColumnUse(const FCLKeyChord& Chord, ECLBindColumn Column, ECLBindableAction ExceptAction) const;

	/** Assigns Chord to the slot. Steals the same column elsewhere. Returns true if a steal happened. */
	bool SetBind(ECLBindableAction Action, ECLBindColumn Column, const FCLKeyChord& Chord, FCLBindUse& OutStolen);
	void ClearBind(ECLBindableAction Action, ECLBindColumn Column);
	void ResetDefaults();

	bool SaveToDisk() const;
	void LoadFromDisk();
	void LoadQuirkySnapshot();

	static TMap<ECLBindableAction, FCLActionBinds> MakeDefaults();

private:
	FString GetSavePath() const;
	static FString GetQuirkyPath();
	bool ApplyBindsJson(const FString& Content);
	void EnsureComplete();

	UPROPERTY()
	TMap<ECLBindableAction, FCLActionBinds> Binds;
};
