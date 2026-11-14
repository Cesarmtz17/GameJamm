#include "Building/BuildRecipe.h"
#include "Building/JamAssembler.h"
#include "Building/JamBuildPart.h"
#include "Building/JamLayout.h"
#include "Core/InventoryComponent.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

DEFINE_LOG_CATEGORY_STATIC(LogJamRecipe, Log, All);

namespace
{
	/** Primer campo string no vacío entre varios nombres posibles ("attach_to", "attachTo", "parent"...). Números cuentan como string. */
	FString GetStr(const TSharedPtr<FJsonObject>& O, std::initializer_list<const TCHAR*> Keys)
	{
		for (const TCHAR* K : Keys)
		{
			FString S;
			if (O->TryGetStringField(K, S))
			{
				S.TrimStartAndEndInline();
				if (!S.IsEmpty())
				{
					return S;
				}
			}
		}
		return FString();
	}

	bool AutoActivates(EJamPartKind Kind)
	{
		return Kind != EJamPartKind::Structural && Kind != EJamPartKind::Seat && Kind != EJamPartKind::Battery;
	}

	/**
	 * Arregla lo que un LLM suele escribir mal en vez de fallar (el asistente nunca bloquea el juego):
	 * tipos con alias o desconocidos, ids faltantes/duplicados, padres inexistentes, varias raíces, ciclos,
	 * sockets con sinónimos, más de MaxParts piezas y "activate" vacío. Cada arreglo queda en Warnings.
	 */
	void RepairRecipe(FBuildRecipe& R, TArray<FString>& Warnings)
	{
		// 1) Tipos: alias -> nombre canónico; desconocidos fuera (sus hijos suben al abuelo).
		TMap<FName, FName> RemovedParentOf;
		TArray<FBuildRecipePart> Kept;
		for (FBuildRecipePart& P : R.Parts)
		{
			FJamPartDef Def;
			if (P.Type.IsNone() || !UJamPartCatalog::Find(P.Type, Def))
			{
				Warnings.Add(FString::Printf(TEXT("dropped part '%s' with unknown type '%s'"), *P.Id.ToString(), *P.Type.ToString()));
				if (!P.Id.IsNone())
				{
					RemovedParentOf.Add(P.Id, P.AttachTo);
				}
				continue;
			}
			P.Type = Def.Type;
			Kept.Add(P);
		}
		R.Parts = MoveTemp(Kept);
		if (R.Parts.Num() == 0)
		{
			return;
		}

		// 2) Ids faltantes o duplicados.
		TSet<FName> Ids;
		for (int32 i = 0; i < R.Parts.Num(); ++i)
		{
			FBuildRecipePart& P = R.Parts[i];
			if (P.Id.IsNone() || Ids.Contains(P.Id))
			{
				FName NewId;
				int32 N = i;
				do { NewId = FName(*FString::Printf(TEXT("p%d"), N++)); } while (Ids.Contains(NewId));
				Warnings.Add(FString::Printf(TEXT("part #%d: id '%s' missing/duplicated -> '%s'"), i, *P.Id.ToString(), *NewId.ToString()));
				P.Id = NewId;
			}
			Ids.Add(P.Id);
		}

		// 3) Padres que se quitaron en (1) -> su abuelo.
		for (FBuildRecipePart& P : R.Parts)
		{
			int32 Guard = 0;
			while (!P.AttachTo.IsNone() && RemovedParentOf.Contains(P.AttachTo) && Guard++ < 64)
			{
				P.AttachTo = RemovedParentOf[P.AttachTo];
			}
			if (P.AttachTo == P.Id)
			{
				P.AttachTo = NAME_None;
			}
		}

		// 4) Exactamente una raíz: la primera sin padre (o la primera pieza). Lo demás sin padre válido cuelga de ella.
		int32 RootIdx = R.Parts.IndexOfByPredicate([](const FBuildRecipePart& P) { return P.AttachTo.IsNone(); });
		if (RootIdx == INDEX_NONE)
		{
			RootIdx = 0;
		}
		R.Parts[RootIdx].AttachTo = NAME_None;
		const FName RootId = R.Parts[RootIdx].Id;
		for (int32 i = 0; i < R.Parts.Num(); ++i)
		{
			FBuildRecipePart& P = R.Parts[i];
			if (i != RootIdx && (P.AttachTo.IsNone() || !Ids.Contains(P.AttachTo)))
			{
				Warnings.Add(FString::Printf(TEXT("part '%s': parent '%s' missing -> attached to root '%s'"), *P.Id.ToString(), *P.AttachTo.ToString(), *RootId.ToString()));
				P.AttachTo = RootId;
			}
		}

		// 5) Ciclos: si subiendo no se llega a la raíz, la pieza cuelga de la raíz.
		TMap<FName, FName> ParentOf;
		for (const FBuildRecipePart& P : R.Parts)
		{
			ParentOf.Add(P.Id, P.AttachTo);
		}
		for (FBuildRecipePart& P : R.Parts)
		{
			FName Current = P.Id;
			int32 Steps = 0;
			while (!Current.IsNone() && Steps <= R.Parts.Num())
			{
				Current = ParentOf.FindRef(Current);
				++Steps;
			}
			if (!Current.IsNone())
			{
				Warnings.Add(FString::Printf(TEXT("part '%s': attachment cycle -> attached to root"), *P.Id.ToString()));
				P.AttachTo = RootId;
				ParentOf.Add(P.Id, RootId);
			}
		}

		// 6) Sockets: sinónimos -> forma canónica ("left side" -> "Left", "Attach_Rear" -> "Back").
		for (FBuildRecipePart& P : R.Parts)
		{
			if (P.AttachTo.IsNone() || P.Socket.IsNone())
			{
				continue;
			}
			FVector Dir, Shift;
			bool bShift = false;
			if (JamLayout::ParseSocket(P.Socket.ToString(), Dir, Shift, bShift))
			{
				P.Socket = FName(*JamLayout::SocketToString(Dir, Shift, bShift));
			}
			else
			{
				Warnings.Add(FString::Printf(TEXT("part '%s': unknown socket '%s' -> Top"), *P.Id.ToString(), *P.Socket.ToString()));
				P.Socket = TEXT("Top");
			}
		}

		// 7) Límite de piezas: se queda lo más cercano a la raíz (BFS), así ningún hijo pierde a su padre.
		if (R.Parts.Num() > UJamAssembler::MaxParts)
		{
			TSet<FName> Keep;
			TArray<FName> Queue = { RootId };
			for (int32 q = 0; q < Queue.Num() && Keep.Num() < UJamAssembler::MaxParts; ++q)
			{
				Keep.Add(Queue[q]);
				for (const FBuildRecipePart& P : R.Parts)
				{
					if (P.AttachTo == Queue[q])
					{
						Queue.Add(P.Id);
					}
				}
			}
			Warnings.Add(FString::Printf(TEXT("%d parts > limit %d: extra parts dropped"), R.Parts.Num(), UJamAssembler::MaxParts));
			R.Parts.RemoveAll([&Keep](const FBuildRecipePart& P) { return !Keep.Contains(P.Id); });
		}

		// 8) Activate: solo ids que existen; si queda vacío, se enciende todo lo que tiene motor/arma.
		TSet<FName> Final;
		for (const FBuildRecipePart& P : R.Parts)
		{
			Final.Add(P.Id);
		}
		TArray<FName> Activate;
		for (const FName& Id : R.Activate)
		{
			if (Final.Contains(Id))
			{
				Activate.AddUnique(Id);
			}
		}
		if (Activate.Num() == 0)
		{
			for (const FBuildRecipePart& P : R.Parts)
			{
				FJamPartDef Def;
				if (UJamPartCatalog::Find(P.Type, Def) && AutoActivates(Def.Kind))
				{
					Activate.Add(P.Id);
				}
			}
		}
		R.Activate = MoveTemp(Activate);
	}
}

bool UBuildRecipeLibrary::ParseRecipeJson(const FString& Json, FBuildRecipe& OutRecipe, FString& OutError)
{
	OutRecipe = FBuildRecipe();
	OutError.Reset();

	const int32 Start = Json.Find(TEXT("{"));
	int32 End = INDEX_NONE;
	Json.FindLastChar(TEXT('}'), End);
	if (Start == INDEX_NONE || End == INDEX_NONE || End <= Start)
	{
		OutError = TEXT("No JSON object found");
		return false;
	}

	const FString Body = Json.Mid(Start, End - Start + 1);
	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Body);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		OutError = TEXT("Invalid JSON");
		return false;
	}

	// Algunos LLM envuelven todo: {"recipe": {...}}.
	const TSharedPtr<FJsonObject>* Nested = nullptr;
	if (!Root->HasField(TEXT("parts")) && Root->TryGetObjectField(TEXT("recipe"), Nested) && Nested && Nested->IsValid())
	{
		Root = *Nested;
	}

	Root->TryGetStringField(TEXT("name"), OutRecipe.Name);

	const TArray<TSharedPtr<FJsonValue>>* PartsArray = nullptr;
	if (!Root->TryGetArrayField(TEXT("parts"), PartsArray) || !PartsArray)
	{
		OutError = TEXT("Missing 'parts' array");
		return false;
	}

	for (const TSharedPtr<FJsonValue>& Value : *PartsArray)
	{
		if (!Value.IsValid())
		{
			continue;
		}
		FBuildRecipePart Part;
		const TSharedPtr<FJsonObject>* PartObject = nullptr;
		FString AsString;
		if (Value->TryGetObject(PartObject) && PartObject && PartObject->IsValid())
		{
			const FString Id = GetStr(*PartObject, {TEXT("id")});
			const FString Type = GetStr(*PartObject, {TEXT("type"), TEXT("part"), TEXT("kind")});
			const FString AttachTo = GetStr(*PartObject, {TEXT("attach_to"), TEXT("attachTo"), TEXT("parent"), TEXT("attach")});
			const FString Socket = GetStr(*PartObject, {TEXT("socket"), TEXT("face"), TEXT("side")});
			Part.Id = Id.IsEmpty() ? NAME_None : FName(*Id);
			Part.Type = Type.IsEmpty() ? NAME_None : FName(*Type);
			Part.AttachTo = AttachTo.IsEmpty() ? NAME_None : FName(*AttachTo);
			Part.Socket = Socket.IsEmpty() ? NAME_None : FName(*Socket);
		}
		else if (Value->TryGetString(AsString) && !AsString.TrimStartAndEnd().IsEmpty())
		{
			Part.Type = FName(*AsString.TrimStartAndEnd()); // "parts": ["Plank", "Wheel"]
		}
		else
		{
			continue;
		}
		OutRecipe.Parts.Add(Part);
	}

	const TArray<TSharedPtr<FJsonValue>>* ActivateArray = nullptr;
	if (Root->TryGetArrayField(TEXT("activate"), ActivateArray) && ActivateArray)
	{
		for (const TSharedPtr<FJsonValue>& Value : *ActivateArray)
		{
			FString Id;
			if (Value.IsValid() && Value->TryGetString(Id) && !Id.TrimStartAndEnd().IsEmpty())
			{
				OutRecipe.Activate.Add(FName(*Id.TrimStartAndEnd()));
			}
		}
	}

	TArray<FString> Warnings;
	RepairRecipe(OutRecipe, Warnings);
	for (const FString& W : Warnings)
	{
		UE_LOG(LogJamRecipe, Warning, TEXT("recipe '%s': %s"), *OutRecipe.Name, *W);
	}
	if (OutRecipe.Name.IsEmpty())
	{
		OutRecipe.Name = TEXT("Toma sin título");
	}

	return ValidateRecipe(OutRecipe, nullptr, OutError);
}

FString UBuildRecipeLibrary::RecipeToJson(const FBuildRecipe& Recipe)
{
	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("name"), Recipe.Name);

	TArray<TSharedPtr<FJsonValue>> PartsArray;
	for (const FBuildRecipePart& Part : Recipe.Parts)
	{
		TSharedRef<FJsonObject> PartObject = MakeShared<FJsonObject>();
		PartObject->SetStringField(TEXT("id"), Part.Id.ToString());
		PartObject->SetStringField(TEXT("type"), Part.Type.ToString());
		if (!Part.AttachTo.IsNone())
		{
			PartObject->SetStringField(TEXT("attach_to"), Part.AttachTo.ToString());
		}
		if (!Part.Socket.IsNone())
		{
			PartObject->SetStringField(TEXT("socket"), Part.Socket.ToString());
		}
		PartsArray.Add(MakeShared<FJsonValueObject>(PartObject));
	}
	Root->SetArrayField(TEXT("parts"), PartsArray);

	TArray<TSharedPtr<FJsonValue>> ActivateArray;
	for (const FName& Id : Recipe.Activate)
	{
		ActivateArray.Add(MakeShared<FJsonValueString>(Id.ToString()));
	}
	Root->SetArrayField(TEXT("activate"), ActivateArray);

	FString Out;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Out);
	FJsonSerializer::Serialize(Root, Writer);
	return Out;
}

TMap<FName, int32> UBuildRecipeLibrary::GetRequiredItems(const FBuildRecipe& Recipe)
{
	TMap<FName, int32> Required;
	for (const FBuildRecipePart& Part : Recipe.Parts)
	{
		Required.FindOrAdd(FName(*FString::Printf(TEXT("Part.%s"), *Part.Type.ToString())))++;
	}
	return Required;
}

bool UBuildRecipeLibrary::ValidateRecipe(const FBuildRecipe& Recipe, const UInventoryComponent* Inventory, FString& OutError)
{
	if (Recipe.Parts.Num() == 0)
	{
		OutError = TEXT("Recipe has no parts");
		return false;
	}

	if (Recipe.Parts.Num() > UJamAssembler::MaxParts)
	{
		OutError = FString::Printf(TEXT("Too many parts (%d > %d)"), Recipe.Parts.Num(), UJamAssembler::MaxParts);
		return false;
	}

	TSet<FName> Ids;
	int32 RootCount = 0;
	for (const FBuildRecipePart& Part : Recipe.Parts)
	{
		if (Part.Id.IsNone() || Part.Type.IsNone())
		{
			OutError = TEXT("Every part needs 'id' and 'type'");
			return false;
		}
		if (Ids.Contains(Part.Id))
		{
			OutError = FString::Printf(TEXT("Duplicated part id '%s'"), *Part.Id.ToString());
			return false;
		}
		FJamPartDef Def;
		if (!UJamPartCatalog::Find(Part.Type, Def))
		{
			OutError = FString::Printf(TEXT("Unknown part type '%s'"), *Part.Type.ToString());
			return false;
		}
		Ids.Add(Part.Id);
		if (Part.AttachTo.IsNone())
		{
			++RootCount;
		}
	}

	if (RootCount != 1)
	{
		OutError = FString::Printf(TEXT("Recipe needs exactly one root part (found %d)"), RootCount);
		return false;
	}

	for (const FBuildRecipePart& Part : Recipe.Parts)
	{
		if (!Part.AttachTo.IsNone() && (!Ids.Contains(Part.AttachTo) || Part.AttachTo == Part.Id))
		{
			OutError = FString::Printf(TEXT("Part '%s' attaches to unknown part '%s'"), *Part.Id.ToString(), *Part.AttachTo.ToString());
			return false;
		}
	}

	// Sin ciclos: subiendo por AttachTo desde cualquier pieza hay que llegar a la raíz.
	TMap<FName, FName> ParentOf;
	for (const FBuildRecipePart& Part : Recipe.Parts)
	{
		ParentOf.Add(Part.Id, Part.AttachTo);
	}
	for (const FBuildRecipePart& Part : Recipe.Parts)
	{
		FName Current = Part.Id;
		int32 Steps = 0;
		while (!Current.IsNone())
		{
			if (++Steps > Recipe.Parts.Num())
			{
				OutError = FString::Printf(TEXT("Attachment cycle involving part '%s'"), *Part.Id.ToString());
				return false;
			}
			Current = ParentOf.FindRef(Current);
		}
	}

	for (const FName& Id : Recipe.Activate)
	{
		if (!Ids.Contains(Id))
		{
			OutError = FString::Printf(TEXT("Activate references unknown part '%s'"), *Id.ToString());
			return false;
		}
	}

	if (Inventory && !Inventory->HasItems(GetRequiredItems(Recipe)))
	{
		OutError = TEXT("Not enough parts in inventory");
		return false;
	}

	return true;
}
