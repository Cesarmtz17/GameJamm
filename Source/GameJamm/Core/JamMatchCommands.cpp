// Comandos de consola de la partida (Core), para probar sin recorrer el mapa:
//   Jam.Goto <X> <Y> [Z]   teletransporta al jugador (p. ej. Jam.Goto 3200 0 = al alcance de la torre azul 1)
//   Jam.Pos                escribe la posicion del jugador en el log y en pantalla
//   Jam.Time <segundos>    adelanta el reloj de la partida (720 = muerte subita, 899 = fin por vida)
//   Jam.Hurt <dano>        se hace dano a uno mismo (probar muerte y respawn)
//   Jam.ShotIn <seg>       en N s guarda una captura en Saved/Screenshots (pruebas sin pantalla)
//   Jam.Ground X0 X1 Y0 Y1 [paso]  mapa de suelo en el log (# = hay piso, . = hueco) para buscar donde se caen los agentes
#include "Core/JamGameState.h"
#include "Core/HealthComponent.h"
#include "Engine/DamageEvents.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "GameJamm.h"
#include "TimerManager.h"

namespace
{
	APawn* JamPlayerPawn(UWorld* World)
	{
		APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
		return PC ? PC->GetPawn() : nullptr;
	}

	void JamSay(const FString& Msg)
	{
		UE_LOG(LogJam, Log, TEXT("%s"), *Msg);
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Cyan, Msg);
		}
	}

	void JamGoto(const TArray<FString>& Args, UWorld* World)
	{
		APawn* Pawn = JamPlayerPawn(World);
		if (!Pawn || Args.Num() < 2)
		{
			JamSay(TEXT("Jam.Goto <X> <Y> [Z]"));
			return;
		}
		const FVector To(FCString::Atof(*Args[0]), FCString::Atof(*Args[1]), Args.Num() > 2 ? FCString::Atof(*Args[2]) : 300.f);
		Pawn->TeleportTo(To, Pawn->GetActorRotation());
		JamSay(FString::Printf(TEXT("Jam.Goto -> %s"), *To.ToCompactString()));
	}

	void JamPos(const TArray<FString>& Args, UWorld* World)
	{
		if (APawn* Pawn = JamPlayerPawn(World))
		{
			JamSay(FString::Printf(TEXT("Jam.Pos %s yaw %.0f"), *Pawn->GetActorLocation().ToCompactString(), Pawn->GetActorRotation().Yaw));
		}
	}

	void JamTime(const TArray<FString>& Args, UWorld* World)
	{
		AJamGameState* GS = World ? World->GetGameState<AJamGameState>() : nullptr;
		if (GS && Args.Num() > 0 && World->GetNetMode() != NM_Client)
		{
			GS->MatchSeconds = FCString::Atof(*Args[0]);
			JamSay(FString::Printf(TEXT("Jam.Time -> %.0f s"), GS->MatchSeconds));
		}
	}

	void JamHurt(const TArray<FString>& Args, UWorld* World)
	{
		APawn* Pawn = JamPlayerPawn(World);
		UHealthComponent* Health = Pawn ? Pawn->FindComponentByClass<UHealthComponent>() : nullptr;
		if (Health)
		{
			const float Dmg = Args.Num() > 0 ? FCString::Atof(*Args[0]) : 50.f;
			Pawn->TakeDamage(Dmg, FDamageEvent(), nullptr, nullptr);
			JamSay(FString::Printf(TEXT("Jam.Hurt %.0f -> vida %.0f"), Dmg, Health->GetHealth()));
		}
	}

	void JamGround(const TArray<FString>& Args, UWorld* World)
	{
		if (!World || Args.Num() < 4) { JamSay(TEXT("Jam.Ground X0 X1 Y0 Y1 [paso]")); return; }
		const float X0 = FCString::Atof(*Args[0]), X1 = FCString::Atof(*Args[1]);
		const float Y0 = FCString::Atof(*Args[2]), Y1 = FCString::Atof(*Args[3]);
		const float Step = Args.Num() > 4 ? FMath::Max(10.f, FCString::Atof(*Args[4])) : 50.f;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(JamGround), true);
		UE_LOG(LogJam, Log, TEXT("Jam.Ground filas Y de %.0f a %.0f, columnas X de %.0f a %.0f cada %.0f"), Y0, Y1, X0, X1, Step);
		for (float Y = Y0; Y <= Y1 + 1.f; Y += Step)
		{
			FString Row;
			for (float X = X0; X <= X1 + 1.f; X += Step)
			{
				FHitResult Hit;
				const bool bHit = World->LineTraceSingleByChannel(Hit, FVector(X, Y, 1500.f), FVector(X, Y, -1500.f), ECC_Pawn, Params);
				Row += !bHit ? TEXT(".") : (Hit.ImpactPoint.Z > 250.f ? TEXT("^") : TEXT("#"));
			}
			UE_LOG(LogJam, Log, TEXT("Y=%6.0f %s"), Y, *Row);
		}
	}
	void JamShotIn(const TArray<FString>& Args, UWorld* World)
	{
		if (!World || Args.Num() < 1) { return; }
		const float Delay = FMath::Max(0.1f, FCString::Atof(*Args[0]));
		FTimerHandle H;
		World->GetTimerManager().SetTimer(H, FTimerDelegate::CreateLambda([World]()
		{
			APlayerController* PC = World->GetFirstPlayerController();
			if (PC) { PC->ConsoleCommand(TEXT("HighResShot 1")); }
			UE_LOG(LogJam, Log, TEXT("Jam.ShotIn: captura pedida"));
		}), Delay, false);
	}
	FAutoConsoleCommandWithWorldAndArgs CmdShotIn(TEXT("Jam.ShotIn"), TEXT("Jam.ShotIn <seg>: en N s guarda una captura en Saved/Screenshots."), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&JamShotIn));

	FAutoConsoleCommandWithWorldAndArgs CmdGround(TEXT("Jam.Ground"), TEXT("Jam.Ground X0 X1 Y0 Y1 [paso]: mapa de suelo en el log (# piso, ^ alto, . hueco)."), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&JamGround));

	FAutoConsoleCommandWithWorldAndArgs CmdGoto(TEXT("Jam.Goto"), TEXT("Jam.Goto <X> <Y> [Z]: teletransporta al jugador."), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&JamGoto));
	FAutoConsoleCommandWithWorldAndArgs CmdPos(TEXT("Jam.Pos"), TEXT("Posicion del jugador."), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&JamPos));
	FAutoConsoleCommandWithWorldAndArgs CmdTime(TEXT("Jam.Time"), TEXT("Jam.Time <segundos>: adelanta el reloj de la partida."), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&JamTime));
	FAutoConsoleCommandWithWorldAndArgs CmdHurt(TEXT("Jam.Hurt"), TEXT("Jam.Hurt <dano>: dano a uno mismo (muerte/respawn)."), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&JamHurt));
}
