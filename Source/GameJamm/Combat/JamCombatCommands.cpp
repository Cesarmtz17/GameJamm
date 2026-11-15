// Consola de Combate (pruebas sin teclado):
//   Jam.Dummy [red|blue]     pone un muneco de practica enemigo 3 m frente al jugador
//   Jam.Weapon [sword|bow]   cambia de arma (sin argumento alterna, como la rueda o Q)
//   Jam.Attack [veces]       ataca con el arma en mano (repite cada intervalo)
//   Jam.Fuse [Material]      fusiona un material del inventario al arma (como F)
//   Jam.Grip P Y R [GX GY GZ [hueso]] agarre en vivo: giro y punto del arma que va en la mano, para afinar Weapon de Characters.json
//   Jam.Later <seg> <cmd...> corre un comando de consola despues (pruebas con -ExecCmds, que corre todo en el frame 0)
#include "Combat/JamTrainingDummy.h"
#include "Combat/JamWeaponComponent.h"
#include "GameFramework/Character.h"
#include "Components/SkeletalMeshComponent.h"
#include "TimerManager.h"
#include "GameJamm.h"
#include "Core/HealthComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "TimerManager.h"
#include "GameJamm.h"

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
		const float Step = W->GetInterval() * 1.05f;
		TWeakObjectPtr<UJamWeaponComponent> Weak(W);
		for (int32 i = 0; i < Times; ++i)
		{
			FTimerHandle H;
			World->GetTimerManager().SetTimer(H, FTimerDelegate::CreateLambda([Weak]() { if (Weak.IsValid()) { Weak->Attack(); } }), 0.05f + i * Step, false);
		}
	}

	void JamFuse(const TArray<FString>& Args, UWorld* World)
	{
		APawn* Pawn = LocalPawn(World);
		UJamWeaponComponent* W = Pawn ? Pawn->FindComponentByClass<UJamWeaponComponent>() : nullptr;
		if (!W) { return; }
		if (Args.Num() == 0) { W->FuseNext(); }
		else { W->Fuse(FName(*Args[0])); }
	}
	FAutoConsoleCommandWithWorldAndArgs CmdFuse(TEXT("Jam.Fuse"), TEXT("Jam.Fuse [Fire|Ice|Shock|Rock|Bomb]: fusiona al arma en mano un material del inventario (sin argumento: el siguiente, como F)."), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&JamFuse));

	FAutoConsoleCommandWithWorldAndArgs CmdDummy(TEXT("Jam.Dummy"), TEXT("Jam.Dummy [red|blue] [distancia cm]: muneco de practica enemigo al frente."), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&JamDummy));
	FAutoConsoleCommandWithWorldAndArgs CmdWeapon(TEXT("Jam.Weapon"), TEXT("Jam.Weapon [sword|bow]: cambia de arma (sin argumento alterna)."), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&JamWeapon));
	void JamGrip(const TArray<FString>& Args, UWorld* World)
	{
		ACharacter* Char = Cast<ACharacter>(LocalPawn(World));
		UJamWeaponComponent* W = Char ? Char->FindComponentByClass<UJamWeaponComponent>() : nullptr;
		if (!W || Args.Num() < 3) { UE_LOG(LogJam, Log, TEXT("Jam.Grip P Y R [X Y Z]")); return; }
		auto F = [&Args](int32 I) { return Args.Num() > I ? FCString::Atof(*Args[I]) : 0.f; };
		// Los 3 ultimos: punto del arma (en su espacio local) que queda en la mano, p. ej. el mango.
		const FRotator R(F(0), F(1), F(2));
		const FTransform T(R, -R.RotateVector(FVector(F(3), F(4), F(5))));
		const FName Socket = Args.Num() > 6 ? FName(*Args[6]) : FName(TEXT("hand_r"));
		W->AttachToHand(Char->GetMesh(), Socket, T, Socket, T);
		UE_LOG(LogJam, Log, TEXT("Jam.Grip rot (%s) pos (%s)"), *T.Rotator().ToCompactString(), *T.GetLocation().ToCompactString());
	}

	void JamLater(const TArray<FString>& Args, UWorld* World)
	{
		if (!World || Args.Num() < 2) { return; }
		const float Delay = FMath::Max(0.05f, FCString::Atof(*Args[0]));
		const FString Cmd = FString::Join(TArrayView<const FString>(Args).RightChop(1), TEXT(" "));
		TWeakObjectPtr<UWorld> WeakWorld(World);
		FTimerHandle H;
		World->GetTimerManager().SetTimer(H, FTimerDelegate::CreateLambda([WeakWorld, Cmd]()
		{
			APlayerController* PC = WeakWorld.IsValid() ? WeakWorld->GetFirstPlayerController() : nullptr;
			if (PC) { PC->ConsoleCommand(Cmd); }
		}), Delay, false);
	}

	FAutoConsoleCommandWithWorldAndArgs CmdGrip(TEXT("Jam.Grip"), TEXT("Jam.Grip P Y R [X Y Z]: agarre del arma en la mano."), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&JamGrip));
	FAutoConsoleCommandWithWorldAndArgs CmdLater(TEXT("Jam.Later"), TEXT("Jam.Later <seg> <comando>: corre un comando despues."), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&JamLater));
	FAutoConsoleCommandWithWorldAndArgs CmdAttack(TEXT("Jam.Attack"), TEXT("Jam.Attack [veces]: ataca con el arma en mano."), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&JamAttack));
}
