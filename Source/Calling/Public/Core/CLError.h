#pragma once

#include "CoreMinimal.h"
#include "CLError.generated.h"

UENUM(BlueprintType)
enum class ECLErrorKind : uint8
{
	Logic UMETA(DisplayName = "Logic"),
	NonDeterministic UMETA(DisplayName = "NonDeterministic"),
	User UMETA(DisplayName = "User")
};

USTRUCT(BlueprintType)
struct CALLING_API FCLError
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Error")
	ECLErrorKind Kind = ECLErrorKind::Logic;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Error")
	FString Code;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Error")
	FString Message;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Error")
	FString IncidentId;

	bool IsOk() const { return Code.IsEmpty() && Message.IsEmpty(); }

	static FCLError Ok();
	static FCLError Make(ECLErrorKind Kind, const FString& Code, const FString& Message);
};

struct CALLING_API FCLStatus
{
	FCLError Error;

	bool IsOk() const { return Error.IsOk(); }
	static FCLStatus Ok();
	static FCLStatus Fail(ECLErrorKind Kind, const FString& Code, const FString& Message);
	static FCLStatus Fail(const FCLError& InError);
};

template<typename T>
struct TCLResult
{
	TOptional<T> Value;
	FCLError Error;

	bool IsOk() const { return Value.IsSet(); }
	const T& Get() const { return Value.GetValue(); }

	static TCLResult Ok(T InValue)
	{
		TCLResult R;
		R.Value = MoveTemp(InValue);
		return R;
	}

	static TCLResult Fail(ECLErrorKind Kind, const FString& Code, const FString& Message)
	{
		TCLResult R;
		R.Error = FCLError::Make(Kind, Code, Message);
		return R;
	}

	static TCLResult Fail(const FCLError& InError)
	{
		TCLResult R;
		R.Error = InError;
		return R;
	}
};
