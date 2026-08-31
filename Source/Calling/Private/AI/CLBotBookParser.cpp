#include "AI/CLBotBookParser.h"

namespace
{
	FString Trim(const FString& S)
	{
		return S.TrimStartAndEnd();
	}

	bool StartsIgnore(const FString& Line, const TCHAR* Prefix)
	{
		return Line.StartsWith(Prefix, ESearchCase::IgnoreCase);
	}

	FCLBotPredicate ParsePredicate(FString Text)
	{
		FCLBotPredicate P;
		Text = Trim(Text);
		if (StartsIgnore(Text, TEXT("qualify ")))
		{
			Text = Trim(Text.Mid(8));
		}
		if (StartsIgnore(Text, TEXT("output is ")))
		{
			P.Name = TEXT("output");
			P.Op = TEXT("in");
			FString Rest = Trim(Text.Mid(10));
			Rest.ReplaceInline(TEXT(" or "), TEXT(","));
			Rest.ParseIntoArray(P.OrValues, TEXT(","), true);
			for (FString& V : P.OrValues)
			{
				V = Trim(V);
			}
			return P;
		}

		const TCHAR* Ops[] = { TEXT(">="), TEXT("<="), TEXT("=="), TEXT("!="), TEXT(">"), TEXT("<") };
		for (const TCHAR* Op : Ops)
		{
			int32 At = INDEX_NONE;
			if (Text.FindChar(*Op, At) || Text.Contains(Op))
			{
				At = Text.Find(Op);
				if (At != INDEX_NONE)
				{
					P.Name = Trim(Text.Left(At));
					P.Op = Op;
					P.Value = Trim(Text.Mid(At + FCString::Strlen(Op)));
					return P;
				}
			}
		}
		int32 Space = INDEX_NONE;
		if (Text.FindChar(TEXT(' '), Space))
		{
			P.Name = Trim(Text.Left(Space));
			P.Value = Trim(Text.Mid(Space + 1));
			P.Op = TEXT("==");
		}
		else
		{
			P.Name = Text;
			P.Op = TEXT("==");
			P.Value = TEXT("true");
		}
		return P;
	}

	void ParseKvLine(const FString& Line, FCLBotLeaf& Leaf)
	{
		FString L = Trim(Line);
		int32 Colon = INDEX_NONE;
		if (!L.FindChar(TEXT(':'), Colon))
		{
			return;
		}
		const FString Key = Trim(L.Left(Colon)).ToLower();
		FString Val = Trim(L.Mid(Colon + 1));
		if (Key == TEXT("while"))
		{
			TArray<FString> Parts;
			Val.ParseIntoArray(Parts, TEXT(","), true);
			for (FString& Part : Parts)
			{
				Part = Trim(Part);
				if (!Part.IsEmpty())
				{
					Leaf.WhileVerbs.Add(FName(*Part));
				}
			}
		}
		else if (Key == TEXT("success"))
		{
			Leaf.Success = ParsePredicate(Val);
		}
		else if (Key == TEXT("goodenough"))
		{
			Leaf.GoodEnough = ParsePredicate(Val);
		}
		else if (Key == TEXT("trysuccessfor"))
		{
			Leaf.TrySuccessFor = FCString::Atof(*Val);
		}
		else if (Key == TEXT("fail.timeout") || Key == TEXT("timeout"))
		{
			Leaf.FailTimeout = FCString::Atof(*Val);
		}
		else if (Key == TEXT("pulses"))
		{
			Leaf.Pulses = FMath::Max(1, FCString::Atoi(*Val));
		}
		else if (Key == TEXT("pulsegap"))
		{
			Leaf.PulseGap = FCString::Atof(*Val);
		}
		else if (Key == TEXT("move"))
		{
			Leaf.Move = Val.ToLower();
		}
		else if (Key == TEXT("id"))
		{
			(void)Val;
		}
	}

	bool ParseActivity(const FString& Line, FCLBotStmt& Out, FString& Error)
	{
		FString Inner = Trim(Line);
		if (!Inner.StartsWith(TEXT(":")))
		{
			Error = TEXT("expected_activity");
			return false;
		}
		Inner.RemoveFromStart(TEXT(":"));
		Inner.RemoveFromEnd(TEXT(";"));
		Inner = Trim(Inner);
		if (StartsIgnore(Inner, TEXT("ref ")))
		{
			Out.Kind = ECLBotStmtKind::Ref;
			Out.RefName = FName(*Trim(Inner.Mid(4)));
			Out.Id = FString::Printf(TEXT("ref_%s"), *Out.RefName.ToString());
			return true;
		}
		Out.Kind = ECLBotStmtKind::Leaf;
		TArray<FString> Tokens;
		Inner.ParseIntoArrayWS(Tokens);
		if (Tokens.Num() == 0)
		{
			Error = TEXT("empty_activity");
			return false;
		}
		Out.Leaf.Verb = FName(*Tokens[0]);
		Out.Id = Tokens[0];
		for (int32 i = 1; i < Tokens.Num(); ++i)
		{
			FString Key, Val;
			if (Tokens[i].Split(TEXT("="), &Key, &Val))
			{
				Out.Leaf.Params.Add(Key.ToLower(), Val);
				if (Key.Equals(TEXT("marker"), ESearchCase::IgnoreCase))
				{
					Out.Id = FString::Printf(TEXT("%s_%s"), *Tokens[0], *Val);
				}
			}
		}
		return true;
	}

	bool ParseBlock(const TArray<FString>& Lines, int32& Index, TArray<FCLBotStmt>& Out, FCLBotBook& Book, FString& Error);

	bool ParseIf(const TArray<FString>& Lines, int32& Index, FCLBotStmt& Out, FCLBotBook& Book, FString& Error)
	{
		const FString Line = Trim(Lines[Index]);
		int32 Open = Line.Find(TEXT("("));
		int32 Close = Line.Find(TEXT(")"));
		if (Open == INDEX_NONE || Close <= Open)
		{
			Error = TEXT("if_missing_predicate");
			return false;
		}
		Out.Kind = ECLBotStmtKind::If;
		Out.IfPred = ParsePredicate(Line.Mid(Open + 1, Close - Open - 1));
		Out.Id = FString::Printf(TEXT("if_%s"), *Out.IfPred.Name);
		++Index;
		while (Index < Lines.Num())
		{
			const FString Cur = Trim(Lines[Index]);
			if (StartsIgnore(Cur, TEXT("else")))
			{
				int32 POpen = Cur.Find(TEXT("("));
				int32 PClose = Cur.Find(TEXT(")"));
				if (POpen != INDEX_NONE && PClose > POpen)
				{
					Out.ElseTag = Trim(Cur.Mid(POpen + 1, PClose - POpen - 1)).ToLower();
				}
				++Index;
				if (!ParseBlock(Lines, Index, Out.ElseBody, Book, Error))
				{
					return false;
				}
				continue;
			}
			if (StartsIgnore(Cur, TEXT("endif")))
			{
				++Index;
				return true;
			}
			if (!ParseBlock(Lines, Index, Out.ThenBody, Book, Error))
			{
				return false;
			}
		}
		Error = TEXT("if_missing_endif");
		return false;
	}

	void AttachNote(FCLBotStmt& Stmt, const TArray<FString>& NoteLines)
	{
		for (const FString& Raw : NoteLines)
		{
			FString L = Trim(Raw);
			if (StartsIgnore(L, TEXT("id:")))
			{
				Stmt.Id = Trim(L.Mid(3));
				continue;
			}
			if (Stmt.Kind == ECLBotStmtKind::Leaf)
			{
				ParseKvLine(L, Stmt.Leaf);
			}
			if (StartsIgnore(L, TEXT("fallbacks:")))
			{
				(void)L;
			}
		}
	}

	bool ParseBlock(const TArray<FString>& Lines, int32& Index, TArray<FCLBotStmt>& Out, FCLBotBook& Book, FString& Error)
	{
		while (Index < Lines.Num())
		{
			const FString Line = Trim(Lines[Index]);
			if (Line.IsEmpty() || Line.StartsWith(TEXT("'")))
			{
				++Index;
				continue;
			}
			if (StartsIgnore(Line, TEXT("else")) || StartsIgnore(Line, TEXT("endif")) || StartsIgnore(Line, TEXT("stop"))
				|| StartsIgnore(Line, TEXT("@enduml")))
			{
				return true;
			}
			if (StartsIgnore(Line, TEXT("start")))
			{
				++Index;
				continue;
			}
			if (StartsIgnore(Line, TEXT("if ")))
			{
				FCLBotStmt Stmt;
				if (!ParseIf(Lines, Index, Stmt, Book, Error))
				{
					return false;
				}
				Out.Add(MoveTemp(Stmt));
				continue;
			}
			if (Line.StartsWith(TEXT(":")))
			{
				FCLBotStmt Stmt;
				if (!ParseActivity(Line, Stmt, Error))
				{
					return false;
				}
				++Index;
				if (Index < Lines.Num() && StartsIgnore(Trim(Lines[Index]), TEXT("note")))
				{
					++Index;
					TArray<FString> Note;
					while (Index < Lines.Num() && !StartsIgnore(Trim(Lines[Index]), TEXT("end note")))
					{
						Note.Add(Lines[Index]);
						++Index;
					}
					if (Index < Lines.Num())
					{
						++Index;
					}
					AttachNote(Stmt, Note);
				}
				if (Stmt.Kind == ECLBotStmtKind::Leaf && Stmt.Leaf.Verb == FName(TEXT("goto")))
				{
					const bool bHasMarker = Stmt.Leaf.Params.Contains(TEXT("marker"));
					const bool bHasXyz = Stmt.Leaf.Params.Contains(TEXT("x"));
					if (!bHasMarker && bHasXyz && !Book.bAllowXyzGoto)
					{
						Error = TEXT("catalog_xyz_goto");
						return false;
					}
					if (!bHasMarker && !bHasXyz)
					{
						Error = TEXT("goto_needs_marker_or_xyz");
						return false;
					}
				}
				Out.Add(MoveTemp(Stmt));
				continue;
			}
			Error = FString::Printf(TEXT("disallowed_syntax:%s"), *Line.Left(40));
			return false;
		}
		return true;
	}

	void ParseFloatingNote(const TArray<FString>& Lines, int32& Index, FCLBotBook& Book)
	{
		++Index;
		while (Index < Lines.Num() && !StartsIgnore(Trim(Lines[Index]), TEXT("end note")))
		{
			FString L = Trim(Lines[Index]);
			if (StartsIgnore(L, TEXT("fallbacks:")))
			{
				TArray<FString> Parts;
				Trim(L.Mid(10)).ParseIntoArray(Parts, TEXT(","), true);
				for (FString& P : Parts)
				{
					P = Trim(P);
					if (!P.IsEmpty())
					{
						Book.Fallbacks.Add(FName(*P));
					}
				}
			}
			else if (StartsIgnore(L, TEXT("onrespawn:")))
			{
				Book.OnRespawn = FName(*Trim(L.Mid(10)));
			}
			else if (StartsIgnore(L, TEXT("onstop:")))
			{
				Book.OnStop = FName(*Trim(L.Mid(7)));
			}
			else if (StartsIgnore(L, TEXT("trysuccessfor:")))
			{
				Book.DefaultTrySuccessFor = FCString::Atof(*Trim(L.Mid(14)));
			}
			++Index;
		}
		if (Index < Lines.Num())
		{
			++Index;
		}
	}
}

FCLStatus FCLBotBookParser::Parse(const FString& Text, bool bAllowXyzGoto, FCLBotBook& OutBook, FString& OutError)
{
	OutBook = FCLBotBook();
	OutBook.bAllowXyzGoto = bAllowXyzGoto;
	TArray<FString> Lines;
	Text.ParseIntoArrayLines(Lines, false);
	int32 Index = 0;
	bool bStarted = false;
	while (Index < Lines.Num())
	{
		const FString Line = Trim(Lines[Index]);
		if (Line.IsEmpty() || Line.StartsWith(TEXT("'")))
		{
			++Index;
			continue;
		}
		if (StartsIgnore(Line, TEXT("@startuml")))
		{
			FString Rest = Trim(Line.Mid(9));
			if (!Rest.IsEmpty())
			{
				OutBook.Name = FName(*Rest);
			}
			bStarted = true;
			++Index;
			continue;
		}
		if (!bStarted)
		{
			OutError = TEXT("missing_startuml");
			return FCLStatus::Fail(ECLErrorKind::User, TEXT("botbook_syntax"), OutError);
		}
		if (StartsIgnore(Line, TEXT("floating note")))
		{
			ParseFloatingNote(Lines, Index, OutBook);
			continue;
		}
		if (StartsIgnore(Line, TEXT("stop")))
		{
			FCLBotStmt Stop;
			Stop.Kind = ECLBotStmtKind::Stop;
			Stop.Id = TEXT("stop");
			OutBook.Body.Add(Stop);
			++Index;
			continue;
		}
		if (StartsIgnore(Line, TEXT("@enduml")))
		{
			break;
		}
		if (StartsIgnore(Line, TEXT("else")) || StartsIgnore(Line, TEXT("endif")))
		{
			OutError = TEXT("unexpected_else_endif");
			return FCLStatus::Fail(ECLErrorKind::User, TEXT("botbook_syntax"), OutError);
		}
		if (!ParseBlock(Lines, Index, OutBook.Body, OutBook, OutError))
		{
			return FCLStatus::Fail(ECLErrorKind::User, TEXT("botbook_syntax"), OutError);
		}
		if (Index < Lines.Num() && (StartsIgnore(Trim(Lines[Index]), TEXT("stop")) || StartsIgnore(Trim(Lines[Index]), TEXT("@enduml"))))
		{
			continue;
		}
		if (Index < Lines.Num() && Trim(Lines[Index]) == Line)
		{
			OutError = FString::Printf(TEXT("disallowed_syntax:%s"), *Line.Left(40));
			return FCLStatus::Fail(ECLErrorKind::User, TEXT("botbook_syntax"), OutError);
		}
	}
	if (OutBook.Name.IsNone())
	{
		OutBook.Name = FName(TEXT("unnamed"));
	}
	if (OutBook.Body.Num() == 0)
	{
		OutError = TEXT("empty_book");
		return FCLStatus::Fail(ECLErrorKind::User, TEXT("botbook_syntax"), OutError);
	}
	return FCLStatus::Ok();
}
