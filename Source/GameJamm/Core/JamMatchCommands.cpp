// Comandos de consola de la partida (Core), para probar sin recorrer el mapa:
//   Jam.Goto <X> <Y> [Z]   teletransporta al jugador (p. ej. Jam.Goto 3200 0 = al alcance de la torre azul 1)
//   Jam.Pos                escribe la posicion del jugador en el log y en pantalla
//   Jam.Time <segundos>    adelanta el reloj de la partida (720 = muerte subita, 899 = fin por vida)
//   Jam.Hurt <dano>        se hace dano a uno mismo (probar muerte y respawn)
#include "Core/JamGameState.h"
#include "Core/HealthComponent.h"
#include "Engine/DamageEvents.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "GameJamm.h"

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

	FAutoConsoleCommandWithWorldAndArgs CmdGoto(TEXT("Jam.Goto"), TEXT("Jam.Goto <X> <Y> [Z]: teletransporta al jugador."), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&JamGoto));
	FAutoConsoleCommandWithWorldAndArgs CmdPos(TEXT("Jam.Pos"), TEXT("Posicion del jugador."), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&JamPos));
	FAutoConsoleCommandWithWorldAndArgs CmdTime(TEXT("Jam.Time"), TEXT("Jam.Time <segundos>: adelanta el reloj de la partida."), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&JamTime));
	FAutoConsoleCommandWithWorldAndArgs CmdHurt(TEXT("Jam.Hurt"), TEXT("Jam.Hurt <dano>: dano a uno mismo (muerte/respawn)."), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&JamHurt));
}
