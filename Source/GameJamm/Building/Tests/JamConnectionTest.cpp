#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "AssetCompilingManager.h"
#include "Building/JamAssembler.h"
#include "Building/JamBuildPart.h"
#include "Building/JamDecisionClient.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "PhysicsEngine/PhysicsConstraintComponent.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJamConnectionTest, "Jam.Building.Connections", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FJamConnectionTest::RunTest(const FString& Parameters)
{
	UWorld::InitializationValues IV;
	IV.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(true);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &IV);
	if (!TestNotNull(TEXT("mundo"), World)) { return false; }
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	for (const TCHAR* Shape : { TEXT("Cube"), TEXT("Cylinder"), TEXT("Cone"), TEXT("Sphere") })
	{
		LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("/Engine/BasicShapes/%s.%s"), Shape, Shape));
	}
	FAssetCompilingManager::Get().FinishAllCompilation();
	World->InitializeActorsForPlay(FURL());
	World->SetBegunPlay(true);
	World->BeginPlay();
	int32 Count = 0;
	for (FName Type : UJamPartCatalog::GetTypes())
	for (float Yaw : { 0.f, 90.f })
	{
		FBuildRecipe R;
		auto Add = [&R](FName Id, FName Kind, FName Parent, FName Socket) { FBuildRecipePart P; P.Id=Id; P.Type=Kind; P.AttachTo=Parent; P.Socket=Socket; R.Parts.Add(P); };
		Add("body", "Plank", NAME_None, NAME_None);
		Add("segment", "Plank", "body", "Front");
		Add("device", Type, "segment", "Top_Center_Back");
		FString Error;
		TArray<AJamBuildPart*> Parts;
		if (!TestTrue(*FString::Printf(TEXT("ensambla %s yaw %.0f"), *Type.ToString(), Yaw), UJamAssembler::Assemble(World, R, FTransform(FRotator(0,Yaw,0), FVector(0,0,1000)), Parts, Error))) { continue; }
		auto Find = [&Parts](FName Id) { return *Parts.FindByPredicate([Id](AJamBuildPart* P){return P->RecipeId == Id;}); };
		AJamBuildPart* Root = Find("body"), *Segment = Find("segment"), *Device = Find("device");
		TestTrue(TEXT("segmento soldado"), Segment->bWelded);
		const EJamPartKind K = Device->Kind;
		const bool Inert = K == EJamPartKind::Structural || K == EJamPartKind::Seat || K == EJamPartKind::Battery || K == EJamPartKind::Slide;
		TestEqual(*FString::Printf(TEXT("soldadura %s"), *Type.ToString()), Device->bWelded, Inert);
		if (!Inert)
		{
			UPhysicsConstraintComponent* C = Device->FindComponentByClass<UPhysicsConstraintComponent>();
			if (TestNotNull(TEXT("union fisica"), C))
			{
				UPrimitiveComponent* A=nullptr, *B=nullptr; FName BoneA, BoneB;
				C->GetConstrainedComponents(A, BoneA, B, BoneB);
				TestTrue(TEXT("union contra raiz soldada, no segmento"), A == Root->Mesh && B == Device->Mesh);
				TestEqual(TEXT("no se separa en X"), C->ConstraintInstance.GetLinearXMotion(), LCM_Locked);
				TestEqual(TEXT("no se separa en Y"), C->ConstraintInstance.GetLinearYMotion(), LCM_Locked);
				TestEqual(TEXT("no se separa en Z"), C->ConstraintInstance.GetLinearZMotion(), LCM_Locked);
				const bool Axle = K == EJamPartKind::Wheel || K == EJamPartKind::Head || K == EJamPartKind::Turntable;
				TestEqual(TEXT("libertad angular correcta"), C->ConstraintInstance.GetAngularTwistMotion(), K == EJamPartKind::Hinge ? ACM_Limited : Axle ? ACM_Free : ACM_Locked);
				FJamPartDef D; UJamPartCatalog::Find(Type, D);
				TestEqual(TEXT("motor solo en rueda motriz o plato"), Device->DriveConstraint != nullptr, (K == EJamPartKind::Wheel && D.Power > 0) || K == EJamPartKind::Turntable);
				if (Axle || K == EJamPartKind::Hinge)
				{
					const FVector Axis = FRotator(0,Yaw,0).RotateVector(K == EJamPartKind::Wheel || K == EJamPartKind::Hinge ? FVector::RightVector : FVector::UpVector);
					TestTrue(TEXT("eje gira con la construccion"), C->GetForwardVector().Equals(Axis, 0.001));
				}
			}
		}
		if (K == EJamPartKind::Fan || K == EJamPartKind::Rocket)
		{
			TestTrue(TEXT("salida atras empuja hacia delante incluso rotado"), Device->GetActorQuat().RotateVector(Device->LocalDirection).Equals(FRotator(0,Yaw,0).Vector(), 0.001));
		}
		++Count;
		UJamAssembler::DestroyBuild(Parts);
	}
	UE_LOG(LogTemp, Display, TEXT("JamConnections: %d combinaciones tipo/orientacion verificadas"), Count);
	World->BeginTearingDown(); GEngine->DestroyWorldContext(World); World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJamConnectionDecisionTest, "Jam.Building.ConnectionDecisions", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FJamConnectionDecisionTest::RunTest(const FString& Parameters)
{
	TSharedPtr<FJsonObject> Request;
	TestTrue(TEXT("peticion valida"), FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(FJamDecisionClient::BuildRequestBody(TEXT("dron con turbo"))), Request));
	if (!Request) { return false; }
	TestTrue(TEXT("modelo recibe contrato"), Request->GetObjectField(TEXT("state"))->GetStringField(TEXT("connection_rules")).Contains(TEXT("Y axle")));
	TestTrue(TEXT("modo por helice"), Request->GetObjectField(TEXT("questions"))->HasField(TEXT("mode_Fan")));
	FJamBuildChoice Choice; FString Note;
	const FString Response = TEXT(R"({"answers":{"use_Plank":{"noul":0.9},"use_Fan":{"noul":0.9},"use_Rocket":{"noul":0.9},"use_Stabilizer":{"noul":0.9},"mode_lift":{"noul":0.9},"mode_Fan":{"choice":"lift","confidence":0.9},"mode_Rocket":{"choice":"default","confidence":0.9}}})");
	TestTrue(TEXT("interpreta roles mixtos"), FJamDecisionClient::ParseResponse(Response, Choice, Note));
	const FJamPieceCount* Fan = Choice.Pieces.FindByPredicate([](const FJamPieceCount& P){return P.Type == "Fan";});
	const FJamPieceCount* Rocket = Choice.Pieces.FindByPredicate([](const FJamPieceCount& P){return P.Type == "Rocket";});
	TestTrue(TEXT("helices sustentan"), Fan && Fan->Mode == "lift");
	TestTrue(TEXT("cohete conserva avance"), Rocket && Rocket->Mode.IsNone());
	FBuildRecipe Recipe;
	TestTrue(TEXT("decision se puede componer"), UJamBuildGenerator::ComposeFromBag(Choice.Pieces, Recipe, Note));
	TestTrue(TEXT("resultado coherente"), UJamAssembler::Coherence(Recipe).Score >= 85);
	return true;
}
#endif
