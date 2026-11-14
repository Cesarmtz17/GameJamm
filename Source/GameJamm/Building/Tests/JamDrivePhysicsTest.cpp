#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "AssetCompilingManager.h"
#include "Building/JamAssembler.h"
#include "Building/JamBuildPart.h"
#include "Building/JamDrivePhysics.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJamDriveLimitsTest, "Jam.Building.DriveLimits", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FJamDriveLimitsTest::RunTest(const FString& Parameters)
{
	const float Torque = JamDrivePhysics::WheelTorque(30.f, 25.f, 4, 70.f);
	TestTrue(TEXT("cuatro motores no superan 0.35g de traccion"), Torque * 4 / 25 <= 30.f * 980.f * 0.35f + 1.f);
	const float RearHeavy = JamDrivePhysics::WheelTorque(30.f, 25.f, 4, 5.f);
	TestTrue(TEXT("carga al borde reduce el par para no levantar un eje"), RearHeavy < Torque && RearHeavy * 4 <= 30.f * 980.f * 5.f * 0.65f + 1.f);
	TestEqual(TEXT("mas ruedas reparten el mismo par total"), JamDrivePhysics::WheelTorque(30.f, 25.f, 8, 70.f) * 8, Torque * 4);
	TestEqual(TEXT("mas peso conserva la aceleracion"), JamDrivePhysics::WheelTorque(60.f, 25.f, 4, 70.f), Torque * 2);
	TestTrue(TEXT("el turbo limita aceleracion lateral a 0.45g"), JamDrivePhysics::YawRate(3000.f, 45.f, 30.f) * 3000.f <= 0.45f * 980.f + 1.f);
	TestTrue(TEXT("vehiculo alto gira mas despacio"), JamDrivePhysics::YawRate(1000.f, 20.f, 100.f) < JamDrivePhysics::YawRate(1000.f, 45.f, 30.f));
	TestTrue(TEXT("varios cohetes comparten el limite de empuje terrestre"), JamDrivePhysics::GroundBoostScale(30, 240000) * 240000 <= 30 * 980 * 0.65 + 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJamDriveSimulationTest, "Jam.Building.DriveSimulation", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FJamDriveSimulationTest::RunTest(const FString& Parameters)
{
	// Mundo propio y suelo plano: no depende del mapa, del modelo ni del inventario del jugador.
	UWorld::InitializationValues IV;
	IV.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(true);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, FName(TEXT("JamDriveTest")), nullptr, true, ERHIFeatureLevel::Num, &IV);
	if (!TestNotNull(TEXT("mundo fisico"), World)) { return false; }
	for (const TCHAR* Shape : { TEXT("Cube"), TEXT("Cylinder"), TEXT("Cone"), TEXT("Sphere") })
	{
		LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("/Engine/BasicShapes/%s.%s"), Shape, Shape));
	}
	FAssetCompilingManager::Get().FinishAllCompilation();
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	World->InitializeActorsForPlay(FURL());
	World->SetBegunPlay(true); // mundo aislado sin GameMode que dispare StartPlay
	World->BeginPlay();
	AStaticMeshActor* Floor = World->SpawnActor<AStaticMeshActor>(FVector(0, 0, -50), FRotator::ZeroRotator);
	Floor->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
	Floor->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
	Floor->SetActorScale3D(FVector(200, 200, 1));
	Floor->GetStaticMeshComponent()->SetCollisionProfileName(TEXT("BlockAll"));

	for (const FName Device : { FName("Rocket"), FName("Fan"), FName("Drone") })
	{
		TArray<FJamPieceCount> Bag;
		auto Add = [&Bag](FName Type, int32 Count) { FJamPieceCount P; P.Type = Type; P.Count = Count; Bag.Add(P); };
		const bool bDrone = Device == "Drone";
		Add("Plank", 1);
		if (bDrone) { Add("Fan", 4); Bag.Last().Mode = "lift"; Add("Stabilizer", 1); }
		else { Add("Wheel", 4); Add(Device, 1); }
		FBuildRecipe Recipe;
		FString Note, Error;
		TArray<AJamBuildPart*> Parts;
		if (!TestTrue(TEXT("compone vehiculo"), UJamBuildGenerator::ComposeFromBag(Bag, Recipe, Note)) ||
			!TestTrue(TEXT("ensambla vehiculo"), UJamAssembler::Assemble(World, Recipe, FTransform(FVector(0, 0, 50)), Parts, Error))) { continue; }
		AJamBuildPart* Root = Parts[0];
		auto Step = [&]() { ++GFrameCounter; World->Tick(LEVELTICK_All, 1.f / 60.f); };
		for (int32 Frame = 0; Frame < 90; ++Frame) { Step(); }
		const FVector Start = Root->GetActorLocation();
		FVector DriveStart = Start;
		double MinUp = 1;
		for (AJamBuildPart* P : Parts) { if (P->Kind == EJamPartKind::Wheel || (bDrone && (P->Kind == EJamPartKind::Fan || P->Kind == EJamPartKind::Stabilizer))) { P->SetActive(true); } }
		for (int32 Frame = 0; Frame < 240; ++Frame)
		{
			for (AJamBuildPart* P : Parts)
			{
				if (P->Kind == EJamPartKind::Wheel) { P->SetDrive(1.f); }
				if (Frame == 90 && P->PartType == Device) { P->SetActive(true); }
			}
			if (bDrone && Frame == 90) { DriveStart = Root->GetActorLocation(); }
			if (bDrone && Frame >= 90)
			{
				Root->Build->PilotUp = FVector(0.3, 0, 1).GetSafeNormal();
				Root->Build->PilotTiltUntil = World->GetTimeSeconds() + 0.2f;
			}
			Step();
			if (Frame % 60 == 0) { UE_LOG(LogTemp, Display, TEXT("JamDrive frame=%d device=%s up=%.3f speed=%.1f upX=%.2f x=%.1f z=%.1f"), Frame, *Device.ToString(), Root->GetActorUpVector().Z, Root->Mesh->GetPhysicsLinearVelocity().Size(), Root->GetActorUpVector().X, Root->GetActorLocation().X, Root->GetActorLocation().Z); }
			MinUp = FMath::Min(MinUp, Root->GetActorUpVector().Z);
		}
		const double Forward = Root->GetActorLocation().X - DriveStart.X;
		UE_LOG(LogTemp, Display, TEXT("JamDriveSimulation %s: avance=%.1f cm minUp=%.3f"), *Device.ToString(), Forward, MinUp);
		TestTrue(*FString::Printf(TEXT("%s avanza al menos 1 m"), *Device.ToString()), Forward > 100);
		if (bDrone) { TestTrue(TEXT("el dron despega con su propio empuje"), Root->GetActorLocation().Z - Start.Z > 100); }
		TestTrue(*FString::Printf(TEXT("%s no vuelca al acelerar y encender propulsor"), *Device.ToString()), MinUp > 0.5);
		UJamAssembler::DestroyBuild(Parts);
	}
	World->BeginTearingDown();
	GEngine->DestroyWorldContext(World);
	World->DestroyWorld(false);
	return true;
}
#endif
