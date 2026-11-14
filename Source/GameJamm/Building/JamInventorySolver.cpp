#include "Building/JamInventorySolver.h"
#include "Building/JamAssembler.h"
#include "Building/JamBuildPart.h"
#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

bool JamInventorySolver::Fit(const FBuildRecipe& Wanted, const TMap<FName, int32>& Available, FBuildRecipe& Out, FString& Note)
{
	Out = FBuildRecipe(); Note.Reset();
	FString Error;
	if (!UBuildRecipeLibrary::ValidateRecipe(Wanted, nullptr, Error)) { Note = Error; return false; }
	auto Fits = [&Available](const FBuildRecipe& R)
	{
		for (const auto& Cost : UBuildRecipeLibrary::GetRequiredItems(R)) { if (Available.FindRef(Cost.Key) < Cost.Value) { return false; } }
		return true;
	};
	if (Fits(Wanted)) { Out = Wanted; return true; }
	TMap<FName, TArray<FName>> Substitutes;
	FString Json;
	TSharedPtr<FJsonObject> Root;
	if (FFileHelper::LoadFileToString(Json, *(FPaths::ProjectContentDir() / TEXT("Game/Building/Data/BuildKnowledge.json"))) && FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Root) && Root)
	{
		const TArray<TSharedPtr<FJsonValue>>* Elements = nullptr;
		if (Root->TryGetArrayField(TEXT("elements"), Elements))
		for (const auto& Value : *Elements)
		{
			const TSharedPtr<FJsonObject> E = Value->AsObject();
			TArray<FString> Names;
			if (!E || !E->TryGetStringArrayField(TEXT("substitutes"), Names)) { continue; }
			for (const FString& Name : Names) { Substitutes.FindOrAdd(FName(*E->GetStringField(TEXT("type")))).Add(FName(*Name)); }
		}
	}
	TArray<FJamPieceCount> Asked, Bag;
	UJamBuildGenerator::BagOfRecipe(Wanted, Asked);
	TMap<FName, int32> Remaining = Available;
	TArray<FString> Changes;
	TSet<FName> Short; // tipos pedidos que no alcanzaron
	for (const FJamPieceCount& I : Asked)
	{
		int32 Left = I.Count;
		auto Take = [&](FName Type)
		{
			FJamPartDef D, Original;
			if (!UJamPartCatalog::Find(Type, D) || !UJamPartCatalog::Find(I.Type, Original) || D.Kind != Original.Kind) { return; }
			if (Type != I.Type && !I.Mode.IsNone())
			{
				const FJamPieceRule* Rule = UJamBuildGenerator::PieceTable().FindByPredicate([Type](const FJamPieceRule& R){return R.Type == Type;});
				if (!Rule || !Rule->Modes.Contains(I.Mode)) { return; }
			}
			const FName Key(*(TEXT("Part.") + Type.ToString()));
			const int32 N = Type == "Seat" ? Left : FMath::Min(Left, FMath::Max(Remaining.FindRef(Key), 0));
			if (N <= 0) { return; }
			FJamPieceCount* Existing = Bag.FindByPredicate([Type](const FJamPieceCount& P){return P.Type == Type;});
			if (!Existing) { Existing = &Bag.AddDefaulted_GetRef(); Existing->Type = Type; Existing->Count = 0; Existing->Mode = I.Mode; }
			Existing->Count += N;
			if (Type != "Seat") { Remaining.FindOrAdd(Key) -= N; }
			Left -= N;
			if (Type != I.Type) { Changes.Add(FString::Printf(TEXT("use %s x%d en vez de %s"), *Type.ToString(), N, *I.Type.ToString())); }
		};
		Take(I.Type);
		for (FName Substitute : Substitutes.FindRef(I.Type)) { if (Left > 0) { Take(Substitute); } }
		if (Left > 0) { Changes.Add(FString::Printf(TEXT("faltan %s x%d"), *I.Type.ToString(), Left)); Short.Add(I.Type); }
	}
	Note = FString::Join(Changes, TEXT("; "));
	auto TryBag = [&](const TArray<FJamPieceCount>& B, FBuildRecipe& Candidate, FString& Why)
	{
		if (!B.ContainsByPredicate([](const FJamPieceCount& P){return P.Type != "Seat" && P.Count > 0;})) { Why = TEXT("nada que armar"); return false; }
		FString ComposeNote;
		if (!UJamBuildGenerator::ComposeFromBag(B, Candidate, ComposeNote, &Available) || !Fits(Candidate)) { Why = ComposeNote; return false; }
		const FJamCoherence C = UJamAssembler::Coherence(Candidate);
		if (C.Score < UJamAssembler::MinCoherence()) { Why = C.ToText(); return false; }
		Why = ComposeNote;
		return true;
	};
	FBuildRecipe Candidate;
	FString ComposeNote;
	if (!TryBag(Bag, Candidate, ComposeNote))
	{
		// Pocas piezas de un tipo (1 llanta para un carro) pueden dar algo que se cae: se intenta sin ese tipo antes de rendirse.
		TArray<FJamPieceCount> Without = Bag;
		Without.RemoveAll([&Short](const FJamPieceCount& P){ return Short.Contains(P.Type); });
		FString Why2;
		if (Short.Num() == 0 || Without.Num() == Bag.Num() || !TryBag(Without, Candidate, Why2)) { Note += TEXT("; ") + ComposeNote; return false; }
		TArray<FString> Dropped;
		for (FName T : Short) { Dropped.Add(T.ToString()); }
		ComposeNote = TEXT("sin ") + FString::Join(Dropped, TEXT(", ")) + TEXT(" (no alcanzan para algo estable); ") + Why2;
	}
	Candidate.Name = Wanted.Name;
	Out = MoveTemp(Candidate);
	Note += TEXT("; ") + ComposeNote;
	return true;
}
