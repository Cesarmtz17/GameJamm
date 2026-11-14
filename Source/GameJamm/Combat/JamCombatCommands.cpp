// Consola de Combate (pruebas sin teclado):
//   Jam.Dummy [red|blue]     pone un muneco de practica enemigo 3 m frente al jugador
//   Jam.Weapon [sword|bow]   cambia de arma (sin argumento alterna, como la rueda o Q)
//   Jam.Attack [veces]       ataca con el arma en mano (repite cada intervalo)
#include "Combat/JamTrainingDummy.h"
#include "Combat/JamWeaponComponent.h"
#include "Core/HealthComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "TimerManager.h"

namespace
{
	APawn* LocalPawn(UWorld* World)
	{
		APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
		return PC ? PC->GetPawn() : nullptr;
	}

	void JamDummy(const TArray<FString>& Args, UWorld* World)
	{
		APawn* Pawn = LocalPawn(World);
		if (!Pawn) { return; }
		const FRotator Yaw(0.f, Pawn->GetControlRotation().Yaw, 0.f);
		const float Dist = Args.Num() > 1 ? FCString::Atof(*Args[1]) : 300.f;
		const FTransform T(Yaw + FRotator(0, 180, 0), Pawn->GetActorLocation() + Yaw.Vector() * Dist);
		if (AJamTrainingDummy* D = World->SpawnActorDeferred<AJamTrainingDummy>(AJamTrainingDummy::StaticClass(), T, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn))
		{
			if (Args.Num() > 0 && Args[0].Equals(TEXT("red"), ESearchCase::IgnoreCase)) { D->Health->Team = EJamTeam::Red; }
			D->FinishSpawning(T);
		}
	}

	void JamWeapon(const TArray<FString>& Args, UWorld* World)
	{
		APawn* Pawn = LocalPawn(World);
		UJamWeaponComponent* W = Pawn ? Pawn->FindComponentByClass<UJamWeaponComponent>() : nullptr;
		if (!W) { return; }
		if (Args.Num() == 0) { W->NextWeapon(); }
		else { W->SetWeapon(Args[0].Equals(TEXT("bow"), ESearchCase::IgnoreCase) ? EJamWeapon::Bow : EJamWeapon::Sword); }
	}

	void JamAttack(const TArray<FString>& Args, UWorld* World)
	{
		APawn* Pawn = LocalPawn(World);
		UJamWeaponComponent* W = Pawn ? Pawn->FindComponentByClass<UJamWeaponComponent>() : nullptr;
		if (!W) { return; }
		const int32 Times = Args.Num() > 0 ? FMath::Clamp(FCString::Atoi(*Args[0]), 1, 50) : 1;
		const float Step = (W->Current == EJamWeapon::Sword ? W->SwordInterval : W->BowInterval) * 1.05f;
		TWeakObjectPtr<UJamWeaponComponent> Weak(W);
		for (int32 i = 0; i < Times; ++i)
		{
			FTimerHandle H;
			World->GetTimerManager().SetTimer(H, FTimerDelegate::CreateLambda([Weak]() { if (Weak.IsValid()) { Weak->Attack(); } }), 0.05f + i * Step, false);
		}
	}

	FAutoConsoleCommandWithWorldAndArgs CmdDummy(TEXT("Jam.Dummy"), TEXT("Jam.Dummy [red|blue] [distancia cm]: muneco de practica enemigo al frente."), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&JamDummy));
	FAutoConsoleCommandWithWorldAndArgs CmdWeapon(TEXT("Jam.Weapon"), TEXT("Jam.Weapon [sword|bow]: cambia de arma (sin argumento alterna)."), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&JamWeapon));
	FAutoConsoleCommandWithWorldAndArgs CmdAttack(TEXT("Jam.Attack"), TEXT("Jam.Attack [veces]: ataca con el arma en mano."), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&JamAttack));
}
