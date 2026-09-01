#include "AI/CLBotDefCatalog.h"
#include "AI/CLBotBookManager.h"
#include "AI/CLBotBookTypes.h"
#include "Core/CLLog.h"
#include "Game/CLErrorBoundary.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Dom/JsonObject.h"

void UCLBotDefCatalog::Initialize(FSubsystemCollectionBase& Collection)
{
	Collection.InitializeDependency(UCLBotBookManager::StaticClass());
	Super::Initialize(Collection);
	LoadFiles();
}

namespace
{
	FName JsonName(const TSharedPtr<FJsonObject>& Obj, const TCHAR* Key)
	{
		FString S;
		if (Obj.IsValid() && Obj->TryGetStringField(Key, S) && !S.IsEmpty())
		{
			return FName(*S);
		}
		return NAME_None;
	}

	bool KitExists(FName Kit)
	{
		const FString Path = FPaths::Combine(FPaths::ProjectConfigDir(), TEXT("Classes"), Kit.ToString() + TEXT(".json"));
		return FPaths::FileExists(Path);
	}

	void CollectUseAbilityTimeouts(const TArray<FCLBotStmt>& Body, TArray<float>& Out)
	{
		for (const FCLBotStmt& S : Body)
		{
			if (S.Kind == ECLBotStmtKind::Leaf)
			{
				const FString Verb = S.Leaf.Verb.ToString();
				if (Verb.Equals(TEXT("useAbilitySelf"), ESearchCase::IgnoreCase)
					|| Verb.Equals(TEXT("useAbilityFocus"), ESearchCase::IgnoreCase))
				{
					Out.Add(S.Leaf.FailTimeout);
				}
			}
			else if (S.Kind == ECLBotStmtKind::If)
			{
				CollectUseAbilityTimeouts(S.ThenBody, Out);
				CollectUseAbilityTimeouts(S.ElseBody, Out);
			}
			else if (S.Kind == ECLBotStmtKind::Ref)
			{
				(void)S;
			}
		}
	}
}

FCLStatus UCLBotDefCatalog::ValidateDef(const FCLBotDef& Def, UCLBotBookManager* Books) const
{
	if (Def.Id.IsNone())
	{
		return FCLStatus::Fail(ECLErrorKind::Logic, TEXT("bot_def_missing_id"), TEXT(""));
	}
	if (!Books || !Books->FindBook(Def.Intellect.BotBook))
	{
		return FCLStatus::Fail(ECLErrorKind::Logic, TEXT("bot_def_missing_book"),
			FString::Printf(TEXT("%s book=%s"), *Def.Id.ToString(), *Def.Intellect.BotBook.ToString()));
	}
	if (!KitExists(Def.AbilityStack.Kit))
	{
		return FCLStatus::Fail(ECLErrorKind::Logic, TEXT("bot_def_unknown_kit"),
			FString::Printf(TEXT("%s kit=%s"), *Def.Id.ToString(), *Def.AbilityStack.Kit.ToString()));
	}
	if (Def.AbilityStack.CooldownScale <= 0.f)
	{
		return FCLStatus::Fail(ECLErrorKind::Logic, TEXT("bot_def_cooldown_scale"),
			Def.Id.ToString());
	}
	const FCLBotBook* Book = Books->FindBook(Def.Intellect.BotBook);
	TArray<float> Timeouts;
	if (Book)
	{
		CollectUseAbilityTimeouts(Book->Body, Timeouts);
	}
	if (Timeouts.Num() > 0)
	{
		float MinTimeout = Timeouts[0];
		for (float T : Timeouts)
		{
			MinTimeout = FMath::Min(MinTimeout, T);
		}
		const float AbilityCd = 8.f * 0.15f * Def.AbilityStack.CooldownScale;
		if (AbilityCd > MinTimeout)
		{
			return FCLStatus::Fail(ECLErrorKind::Logic, TEXT("bot_def_cooldown_mismatch"),
				FString::Printf(TEXT("%s cd=%.2f timeout=%.2f"), *Def.Id.ToString(), AbilityCd, MinTimeout));
		}
	}
	return FCLStatus::Ok();
}

bool UCLBotDefCatalog::LoadFiles()
{
	Defs.Reset();
	UCLBotBookManager* Books = GetGameInstance() ? GetGameInstance()->GetSubsystem<UCLBotBookManager>() : nullptr;
	const FString Dir = FPaths::Combine(FPaths::ProjectConfigDir(), TEXT("Bots"));
	TArray<FString> Files;
	IFileManager::Get().FindFiles(Files, *FPaths::Combine(Dir, TEXT("*.json")), true, false);
	bool bOk = true;
	for (const FString& File : Files)
	{
		const FString Path = FPaths::Combine(Dir, File);
		FString Text;
		if (!FFileHelper::LoadFileToString(Text, *Path))
		{
			continue;
		}
		TSharedPtr<FJsonObject> Root;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
		if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
		{
			UCLErrorBoundary::ReportStatic(this, FCLError::Make(
				ECLErrorKind::NonDeterministic, TEXT("bot_def_parse"), Path));
			bOk = false;
			continue;
		}
		FCLBotDef Def;
		Def.Id = JsonName(Root, TEXT("id"));
		Def.Role = JsonName(Root, TEXT("role"));
		const TSharedPtr<FJsonObject>* Intellect = nullptr;
		if (Root->TryGetObjectField(TEXT("intellect"), Intellect) && Intellect && Intellect->IsValid())
		{
			if ((*Intellect)->HasField(TEXT("changeResponseSeconds")))
			{
				Def.Intellect.ChangeResponseSeconds = static_cast<float>((*Intellect)->GetNumberField(TEXT("changeResponseSeconds")));
			}
			Def.Intellect.BotBook = JsonName(*Intellect, TEXT("botBook"));
			if ((*Intellect)->HasField(TEXT("botBookSkill")))
			{
				Def.Intellect.BotBookSkill = static_cast<int32>((*Intellect)->GetNumberField(TEXT("botBookSkill")));
			}
			(*Intellect)->TryGetBoolField(TEXT("bulkApperception"), Def.Intellect.bBulkApperception);
		}
		const TSharedPtr<FJsonObject>* Stack = nullptr;
		if (Root->TryGetObjectField(TEXT("abilityStack"), Stack) && Stack && Stack->IsValid())
		{
			Def.AbilityStack.Kit = JsonName(*Stack, TEXT("kit"));
			if ((*Stack)->HasField(TEXT("cooldownScale")))
			{
				Def.AbilityStack.CooldownScale = static_cast<float>((*Stack)->GetNumberField(TEXT("cooldownScale")));
			}
		}
		const FCLStatus Status = ValidateDef(Def, Books);
		if (!Status.IsOk())
		{
			UCLErrorBoundary::ReportStatic(this, Status.Error);
			bOk = false;
			continue;
		}
		Defs.Add(Def.Id, MoveTemp(Def));
	}
	bLoaded = bOk;
	UE_LOG(LogCalling, Display, TEXT("BotDef catalog loaded %d"), Defs.Num());
	return Defs.Num() > 0 || Files.Num() == 0;
}

const FCLBotDef* UCLBotDefCatalog::Find(FName Id) const
{
	return Defs.Find(Id);
}
