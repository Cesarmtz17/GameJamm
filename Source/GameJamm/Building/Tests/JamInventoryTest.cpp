#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Building/JamInventorySolver.h"
#include "Building/JamAssembler.h"
#include "Building/JamBuildSpawner.h"
#include "Core/InventoryComponent.h"
#include "Engine/World.h"
#include "Engine/Engine.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJamInventoryTest, "Jam.Building.Inventory", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FJamInventoryTest::RunTest(const FString& Parameters)
{
	FBuildRecipe Wanted, Fitted; FString Note;
	TArray<FJamPieceCount> Bag;
	auto Add = [&Bag](FName Type,int32 N){FJamPieceCount P; P.Type=Type; P.Count=N; Bag.Add(P);};
	Add("Plank",1); Add("Wheel",4); Add("Rocket",1);
	TestTrue(TEXT("receta de carro"), UJamBuildGenerator::ComposeFromBag(Bag,Wanted,Note));
	const TMap<FName,int32> Full = UBuildRecipeLibrary::GetRequiredItems(Wanted);
	TestFalse(TEXT("asiento gratuito"), Full.Contains("Part.Seat"));
	TestTrue(TEXT("inventario completo"), JamInventorySolver::Fit(Wanted, Full, Fitted, Note));
	TestFalse(TEXT("inventario vacio no inventa piezas"), JamInventorySolver::Fit(Wanted, {}, Fitted, Note));
	TMap<FName,int32> Limited = Full;
	Limited.Remove("Part.Rocket");
	TestTrue(TEXT("version sin turbo cuando falta cohete"), JamInventorySolver::Fit(Wanted, Limited, Fitted, Note));
	for (const auto& Cost : UBuildRecipeLibrary::GetRequiredItems(Fitted)) { TestTrue(TEXT("version reducida respeta cada cantidad"), Cost.Value <= Limited.FindRef(Cost.Key)); }
	Limited = Full; Limited.Remove("Part.Wheel"); Limited.Add("Part.BigWheel",4);
	TestTrue(TEXT("sustituye rueda con datos del catalogo"), JamInventorySolver::Fit(Wanted,Limited,Fitted,Note));
	TestEqual(TEXT("sustitucion usa cuatro ruedas grandes"), UBuildRecipeLibrary::GetRequiredItems(Fitted).FindRef("Part.BigWheel"),4);
	Limited = Full; Limited["Part.Wheel"]=25;
	Bag[1].Count=25;
	UJamBuildGenerator::ComposeFromBag(Bag,Wanted,Note);
	if (JamInventorySolver::Fit(Wanted,Limited,Fitted,Note))
	{
		for (const auto& Cost : UBuildRecipeLibrary::GetRequiredItems(Fitted)) { TestTrue(TEXT("no crea tablas gratis para extender chasis"), Cost.Value <= Limited.FindRef(Cost.Key)); }
	}

	UWorld* World=UWorld::CreateWorld(EWorldType::Game,false);
	if (!TestNotNull(TEXT("mundo"),World)) { return false; }
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	AActor* Owner=World->SpawnActor<AActor>();
	UInventoryComponent* Inventory=NewObject<UInventoryComponent>(Owner);
	Owner->AddInstanceComponent(Inventory); Inventory->RegisterComponent();
	Inventory->AddItem("Part.Block",1);
	AJamBuildSpawner* S=World->SpawnActor<AJamBuildSpawner>();
	S->SetOwner(Owner); S->bUseInventory=true; S->bUseDecisionModel=false; S->bAutoActivate=false;
	S->RecipeJsonOverride=TEXT(R"({"parts":[{"id":"body","type":"Block"}]})");
	S->BuildNow();
	TestEqual(TEXT("construccion consume"),Inventory->GetCount("Part.Block"),0);
	TestEqual(TEXT("construccion existe"),S->GetParts().Num(),1);
	S->BuildNow();
	TestEqual(TEXT("reconstruir reutiliza lo pagado"),S->GetParts().Num(),1);
	TestEqual(TEXT("reconstruir no duplica inventario"),Inventory->GetCount("Part.Block"),0);
	S->RecipeJsonOverride=TEXT(R"({"parts":[{"id":"body","type":"Rocket"}]})");
	S->BuildNow();
	TestEqual(TEXT("pedido imposible conserva lo anterior"),S->GetParts().Num(),1);
	S->ClearBuild(); S->ClearBuild();
	TestEqual(TEXT("desarmar devuelve una sola vez"),Inventory->GetCount("Part.Block"),1);
	World->BeginTearingDown(); GEngine->DestroyWorldContext(World); World->DestroyWorld(false);
	return true;
}
#endif
