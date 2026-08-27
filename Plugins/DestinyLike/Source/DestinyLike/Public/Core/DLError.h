#pragma once

#include "CoreMinimal.h"
#include "DLError.generated.h"

UENUM(BlueprintType)
enum class EDLErrorKind : uint8
{
	Logic UMETA(DisplayName = "Logic"),
	NonDeterministic UMETA(DisplayName = "NonDeterministic"),
	User UMETA(DisplayName = "User")
};

USTRUCT(BlueprintType)
struct DESTINYLIKE_API FDLError
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DestinyLike|Error")
	EDLErrorKind Kind = EDLErrorKind::Logic;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DestinyLike|Error")
	FString Code;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DestinyLike|Error")
	FString Message;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DestinyLike|Error")
	FString IncidentId;

	bool IsOk() const { return Code.IsEmpty() && Message.IsEmpty(); }

	static FDLError Ok();
	static FDLError Make(EDLErrorKind Kind, const FString& Code, const FString& Message);
};

struct DESTINYLIKE_API FDLStatus
{
	FDLError Error;

	bool IsOk() const { return Error.IsOk(); }
	static FDLStatus Ok();
	static FDLStatus Fail(EDLErrorKind Kind, const FString& Code, const FString& Message);
	static FDLStatus Fail(const FDLError& InError);
};

template<typename T>
struct TDLResult
{
	TOptional<T> Value;
	FDLError Error;

	bool IsOk() const { return Value.IsSet(); }
	const T& Get() const { return Value.GetValue(); }

	static TDLResult Ok(T InValue)
	{
		TDLResult R;
		R.Value = MoveTemp(InValue);
		return R;
	}

	static TDLResult Fail(EDLErrorKind Kind, const FString& Code, const FString& Message)
	{
		TDLResult R;
		R.Error = FDLError::Make(Kind, Code, Message);
		return R;
	}

	static TDLResult Fail(const FDLError& InError)
	{
		TDLResult R;
		R.Error = InError;
		return R;
	}
};
