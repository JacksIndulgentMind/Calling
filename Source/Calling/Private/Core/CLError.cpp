#include "Core/CLError.h"

FCLError FCLError::Ok()
{
	return FCLError();
}

FCLError FCLError::Make(ECLErrorKind Kind, const FString& Code, const FString& Message)
{
	FCLError E;
	E.Kind = Kind;
	E.Code = Code;
	E.Message = Message;
	return E;
}

FCLStatus FCLStatus::Ok()
{
	return FCLStatus();
}

FCLStatus FCLStatus::Fail(ECLErrorKind Kind, const FString& Code, const FString& Message)
{
	FCLStatus S;
	S.Error = FCLError::Make(Kind, Code, Message);
	return S;
}

FCLStatus FCLStatus::Fail(const FCLError& InError)
{
	FCLStatus S;
	S.Error = InError;
	return S;
}
