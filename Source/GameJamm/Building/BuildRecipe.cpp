#include "Building/BuildRecipe.h"
#include "Core/InventoryComponent.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

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

	Root->TryGetStringField(TEXT("name"), OutRecipe.Name);

	const TArray<TSharedPtr<FJsonValue>>* PartsArray = nullptr;
	if (!Root->TryGetArrayField(TEXT("parts"), PartsArray) || !PartsArray)
	{
		OutError = TEXT("Missing 'parts' array");
		return false;
	}

	for (const TSharedPtr<FJsonValue>& Value : *PartsArray)
	{
		const TSharedPtr<FJsonObject>* PartObject = nullptr;
		if (!Value.IsValid() || !Value->TryGetObject(PartObject) || !PartObject)
		{
			OutError = TEXT("Each part must be an object");
			return false;
		}

		FString Id, Type, AttachTo, Socket;
		(*PartObject)->TryGetStringField(TEXT("id"), Id);
		(*PartObject)->TryGetStringField(TEXT("type"), Type);
		(*PartObject)->TryGetStringField(TEXT("attach_to"), AttachTo);
		(*PartObject)->TryGetStringField(TEXT("socket"), Socket);

		FBuildRecipePart Part;
		Part.Id = FName(*Id);
		Part.Type = FName(*Type);
		Part.AttachTo = AttachTo.IsEmpty() ? NAME_None : FName(*AttachTo);
		Part.Socket = Socket.IsEmpty() ? NAME_None : FName(*Socket);
		OutRecipe.Parts.Add(Part);
	}

	const TArray<TSharedPtr<FJsonValue>>* ActivateArray = nullptr;
	if (Root->TryGetArrayField(TEXT("activate"), ActivateArray) && ActivateArray)
	{
		for (const TSharedPtr<FJsonValue>& Value : *ActivateArray)
		{
			FString Id;
			if (Value.IsValid() && Value->TryGetString(Id) && !Id.IsEmpty())
			{
				OutRecipe.Activate.Add(FName(*Id));
			}
		}
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
		if (Part.Type != "Seat") { Required.FindOrAdd(FName(*FString::Printf(TEXT("Part.%s"), *Part.Type.ToString())))++; }
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
