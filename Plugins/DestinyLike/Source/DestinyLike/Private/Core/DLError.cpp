#include "Core/DLError.h"

FDLError FDLError::Ok()
{
	return FDLError();
}

FDLError FDLError::Make(EDLErrorKind Kind, const FString& Code, const FString& Message)
{
	FDLError E;
	E.Kind = Kind;
	E.Code = Code;
	E.Message = Message;
	return E;
}

FDLStatus FDLStatus::Ok()
{
	return FDLStatus();
}

FDLStatus FDLStatus::Fail(EDLErrorKind Kind, const FString& Code, const FString& Message)
{
	FDLStatus S;
	S.Error = FDLError::Make(Kind, Code, Message);
	return S;
}

FDLStatus FDLStatus::Fail(const FDLError& InError)
{
	FDLStatus S;
	S.Error = InError;
	return S;
}
