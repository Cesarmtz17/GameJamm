#include "Core/JamLoot.h"
#include "Core/InventoryComponent.h"
#include "Building/JamBuildPart.h"
#include "GameJamm.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace JamLoot
{
	TSharedPtr<FJsonObject> Config()
	{
		static TSharedPtr<FJsonObject> Cached;
		if (!Cached)
		{
			FString Text;
			const FString Path = FPaths::ProjectContentDir() / TEXT("Game/Core/Data/Loot.json");
			if (!FFileHelper::LoadFileToString(Text, *Path) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Cached) || !Cached)
			{
				UE_LOG(LogJam, Warning, TEXT("Loot: no pude leer %s"), *Path);
				Cached = MakeShared<FJsonObject>();
			}
		}
		return Cached;
	}

	namespace
	{
		const TSharedPtr<FJsonObject> Caps()
		{
			const TSharedPtr<FJsonObject>* C = nullptr;
			return Config()->TryGetObjectField(TEXT("caps"), C) ? *C : MakeShared<FJsonObject>();
		}

		int32 CapInt(const TCHAR* Field, int32 Default)
		{
			int32 V = Default;
			Caps()->TryGetNumberField(Field, V);
			return V;
		}

		bool InList(const TCHAR* Field, const FString& Type)
		{
			TArray<FString> L;
			return Caps()->TryGetStringArrayField(Field, L) && L.Contains(Type);
		}
	}

	int32 Room(const UInventoryComponent* Inventory, FName ItemId)
	{
		if (!Inventory) { return 0; }
		const FString Id = ItemId.ToString();
		const int32 Have = Inventory->GetCount(ItemId);
		if (Id.StartsWith(TEXT("Mat.")))
		{
			return FMath::Max(0, CapInt(TEXT("material"), 10) - Have);
		}
		if (!Id.StartsWith(TEXT("Part.")))
		{
			return MAX_int32;
		}
		const FString Type = Id.RightChop(5);
		const int32 PerType = InList(TEXT("common_types"), Type) ? CapInt(TEXT("common"), 8) : InList(TEXT("rare_types"), Type) ? CapInt(TEXT("rare"), 2) : CapInt(TEXT("device"), 4);
		int32 Total = 0;
		for (const auto& It : Inventory->Items) { if (It.Key.ToString().StartsWith(TEXT("Part."))) { Total += FMath::Max(0, It.Value); } }
		return FMath::Max(0, FMath::Min(PerType - Have, CapInt(TEXT("total_parts"), 40) - Total));
	}

	int32 Give(UInventoryComponent* Inventory, FName ItemId, int32 Count)
	{
		const int32 N = FMath::Min(Count, Room(Inventory, ItemId));
		if (N > 0) { Inventory->AddItem(ItemId, N); }
		return FMath::Max(N, 0);
	}

	TMap<FName, int32> Roll(const FString& Table)
	{
		TMap<FName, int32> Out;
		const TSharedPtr<FJsonObject>* Tables = nullptr;
		const TSharedPtr<FJsonObject>* T = nullptr;
		if (!Config()->TryGetObjectField(TEXT("tables"), Tables) || !(*Tables)->TryGetObjectField(Table, T)) { return Out; }
		const TArray<TSharedPtr<FJsonValue>>* Items = nullptr;
		if (!(*T)->TryGetArrayField(TEXT("items"), Items) || Items->Num() == 0) { return Out; }
		int32 Rolls = 1;
		(*T)->TryGetNumberField(TEXT("rolls"), Rolls);
		float TotalW = 0.f;
		for (const auto& V : *Items) { TotalW += V->AsObject()->GetNumberField(TEXT("weight")); }
		for (int32 r = 0; r < Rolls; ++r)
		{
			float Pick = FMath::FRand() * TotalW;
			for (const auto& V : *Items)
			{
				const TSharedPtr<FJsonObject> I = V->AsObject();
				Pick -= I->GetNumberField(TEXT("weight"));
				if (Pick <= 0.f)
				{
					const int32 Min = I->GetIntegerField(TEXT("min")), Max = I->GetIntegerField(TEXT("max"));
					Out.FindOrAdd(FName(*I->GetStringField(TEXT("id")))) += FMath::RandRange(Min, FMath::Max(Min, Max));
					break;
				}
			}
		}
		return Out;
	}

	FString GiveBundle(AActor* Taker, const TMap<FName, int32>& Bundle, TMap<FName, int32>* OutLeftover)
	{
		UInventoryComponent* Inv = Taker ? Taker->FindComponentByClass<UInventoryComponent>() : nullptr;
		const APawn* Pawn = Cast<APawn>(Taker);
		TMap<FName, int32> Got;
		bool bFull = false;
		for (const auto& It : Bundle)
		{
			if (It.Key == TEXT("Energy"))
			{
				if (Pawn && Pawn->IsPlayerControlled())
				{
					UWorld* W = Taker->GetWorld();
					AJamBuildPart::SetPlayerEnergy(W, FMath::Min(EnergyMax(), AJamBuildPart::GetPlayerEnergy(W) + It.Value));
					Got.Add(It.Key, It.Value);
				}
				continue;
			}
			const int32 N = Inv ? Give(Inv, It.Key, It.Value) : 0;
			if (N > 0) { Got.Add(It.Key, N); }
			if (N < It.Value)
			{
				bFull = true;
				if (OutLeftover) { OutLeftover->Add(It.Key, It.Value - N); }
			}
		}
		// Lo que no cupo va en OutLeftover: quien llama avisa "Mochila llena" (el texto es solo de lo que entro).
		(void)bFull;
		return Describe(Got);
	}

	FString PartName(const FString& Type)
	{
		static TMap<FString, FString> Names;
		static bool bLoaded = false;
		if (!bLoaded)
		{
			bLoaded = true;
			FString Json;
			TSharedPtr<FJsonObject> Root;
			const TArray<TSharedPtr<FJsonValue>>* Elements = nullptr;
			if (FFileHelper::LoadFileToString(Json, *(FPaths::ProjectContentDir() / TEXT("Game/Building/Data/BuildKnowledge.json")))
				&& FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Root) && Root && Root->TryGetArrayField(TEXT("elements"), Elements))
			{
				for (const auto& V : *Elements)
				{
					const TSharedPtr<FJsonObject> E = V->AsObject();
					FString T;
					TArray<FString> Es;
					if (E && E->TryGetStringField(TEXT("type"), T) && E->TryGetStringArrayField(TEXT("names_es"), Es) && Es.Num() > 0 && !Es[0].IsEmpty() && !Names.Contains(T))
					{
						FString N = Es[0];
						N[0] = FChar::ToUpper(N[0]);
						Names.Add(T, N);
					}
				}
			}
		}
		const FString* Found = Names.Find(Type);
		return Found ? *Found : Type;
	}

	FString ItemName(FName ItemId)
	{
		const FString Id = ItemId.ToString();
		if (Id == TEXT("Energy")) { return TEXT("Energía"); }
		if (Id == TEXT("Backpack")) { return TEXT("Mochila"); }
		if (Id == TEXT("Mat.Fire")) { return TEXT("Fuego"); }
		if (Id == TEXT("Mat.Ice")) { return TEXT("Hielo"); }
		if (Id == TEXT("Mat.Shock")) { return TEXT("Rayo"); }
		if (Id == TEXT("Mat.Rock")) { return TEXT("Roca"); }
		if (Id == TEXT("Mat.Bomb")) { return TEXT("Bomba"); }
		return Id.StartsWith(TEXT("Part.")) ? PartName(Id.RightChop(5)) : Id;
	}

	FString IconName(FName ItemId)
	{
		FString Id = ItemId.ToString();
		Id.RemoveFromStart(TEXT("Part."));
		return TEXT("T_Icon_") + Id.Replace(TEXT("."), TEXT("_"));
	}

	FString ItemTier(FName ItemId)
	{
		const FString Id = ItemId.ToString();
		if (Id == TEXT("Energy") || Id == TEXT("Mat.Shock") || Id == TEXT("Mat.Bomb")) { return TEXT("device"); }
		if (!Id.StartsWith(TEXT("Part."))) { return TEXT("common"); }
		const FString Type = Id.RightChop(5);
		return InList(TEXT("rare_types"), Type) ? TEXT("rare") : InList(TEXT("common_types"), Type) ? TEXT("common") : TEXT("device");
	}

	FLinearColor TierColor(const FString& Tier)
	{
		return FLinearColor(FColor::FromHex(Tier == TEXT("rare") ? TEXT("E7B65A") : Tier == TEXT("device") ? TEXT("8FE3FF") : TEXT("F6F4ED")));
	}

	FLinearColor ItemColor(FName ItemId)
	{
		if (ItemId == TEXT("Mat.Fire")) { return FLinearColor(FColor::FromHex(TEXT("FF7A1A"))); }
		if (ItemId == TEXT("Mat.Ice")) { return FLinearColor(FColor::FromHex(TEXT("8FE3FF"))); }
		if (ItemId == TEXT("Mat.Shock")) { return FLinearColor(FColor::FromHex(TEXT("F5F06A"))); }
		if (ItemId == TEXT("Mat.Rock")) { return FLinearColor(FColor::FromHex(TEXT("C9A06B"))); }
		if (ItemId == TEXT("Mat.Bomb")) { return FLinearColor(FColor::FromHex(TEXT("D8B989"))); }
		if (ItemId == TEXT("Energy")) { return FLinearColor(FColor::FromHex(TEXT("408F94"))); }
		if (ItemId == TEXT("Backpack")) { return FLinearColor(FColor::FromHex(TEXT("F2C230"))); }
		return TierColor(ItemTier(ItemId));
	}

	FString Describe(const TMap<FName, int32>& Bundle)
	{
		TArray<FString> Parts;
		for (const auto& It : Bundle)
		{
			FString Id = It.Key.ToString();
			if (Id == TEXT("Energy")) { Parts.Add(FString::Printf(TEXT("+%d energia"), It.Value)); continue; }
			if (Id.RemoveFromStart(TEXT("Mat.")))
			{
				const FString Low = Id.ToLower();
				const TCHAR* Es = Low == TEXT("rock") ? TEXT("piedra") : Low == TEXT("fire") ? TEXT("fuego") : Low == TEXT("ice") ? TEXT("hielo") : Low == TEXT("shock") ? TEXT("rayo") : Low == TEXT("bomb") ? TEXT("bomba") : nullptr;
				Parts.Add(FString::Printf(TEXT("+%d material %s"), It.Value, Es ? Es : *Low));
				continue;
			}
			Id.RemoveFromStart(TEXT("Part."));
			Parts.Add(FString::Printf(TEXT("+%d %s"), It.Value, *PartName(Id)));
		}
		return FString::Join(Parts, TEXT(", "));
	}

	bool GroundAt(UWorld* World, float X, float Y, FVector& Out, const AActor* Ignore)
	{
		if (!World) { return false; }
		FHitResult Hit;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(JamLootGround), true, Ignore);
		if (World->LineTraceSingleByChannel(Hit, FVector(X, Y, 6000.f), FVector(X, Y, -6000.f), ECC_Visibility, Params))
		{
			Out = Hit.ImpactPoint;
			return true;
		}
		return false;
	}

	float EnergyMin()
	{
		const TSharedPtr<FJsonObject>* E = nullptr;
		double V = 50.0;
		if (Config()->TryGetObjectField(TEXT("energy"), E)) { (*E)->TryGetNumberField(TEXT("min"), V); }
		return float(V);
	}

	float EnergyMax()
	{
		const TSharedPtr<FJsonObject>* E = nullptr;
		double V = 150.0;
		if (Config()->TryGetObjectField(TEXT("energy"), E)) { (*E)->TryGetNumberField(TEXT("max"), V); }
		return float(V);
	}
}
