#include "Building/JamPlayConsole.h"

#include "Building/JamBuildPart.h"
#include "Building/JamDrivePhysics.h"
#include "Building/JamBuildSpawner.h"
#include "Core/InventoryComponent.h"
#include "Building/JamVoice.h"
#include "Blueprint/WidgetTree.h"
#include "Camera/CameraComponent.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/MultiLineEditableTextBox.h"
#include "Engine/Canvas.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "GameFramework/Character.h"
#include "GameFramework/GameModeBase.h"
#include "Building/JamAssembler.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SpotLightComponent.h"
#include "Containers/Ticker.h"
#include "Core/JamStatics.h"
#include "Core/JamMessages.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Components/CapsuleComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "PhysicsEngine/PhysicsConstraintComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "InputCoreTypes.h"

namespace
{
	const FName SayTag(TEXT("JamSay"));
	TWeakObjectPtr<AJamBuildSpawner> GLastSay;   // ultima construccion pedida (para el HUD)
	bool GTyping = false;                        // caja de texto abierta: el personaje no lee teclas
	// Entrada simulada (Jam.Drive / Jam.Fire y la bateria Jam.Test): entra por el mismo camino que el teclado.
	float GDriveThrottle = 0.f, GDriveSteer = 0.f;
	double GDriveUntil = 0.0, GFireUntil = 0.0;
	bool GBoostOnce = false;
	float GLiftInput = 0.f;                      // montado: Espacio sube (+1), Ctrl baja (-1), nada = mantener altura
	double GLiftUntil = 0.0;                     // Jam.Test: Espacio simulado

	void Screen(const FString& Msg, FColor Color = FColor::Yellow)
	{
		// Va a la cola de avisos del jugador (JamMsg), que dibuja AJamHUD; la build Shipping no muestra el canal de depuracion.
		const FLinearColor& Col = Color == FColor::Red ? JamMsg::Bad : Color == FColor::Green ? JamMsg::Good
			: (Color == FColor::Orange || Color == FColor::Magenta) ? JamMsg::Warn : JamMsg::Info;
		JamMsg::Post(Msg, Col, Color == FColor::Red ? 6.f : 5.f);
	}

	/** ~6 m delante del jugador, sobre el suelo, de perfil (su frente apunta a la derecha del jugador: no le dispara ni lo atropella). */
	FTransform SayTransform(UWorld* World)
	{
		FVector Loc = FVector::ZeroVector;
		float Yaw = 0.f;
		if (const APlayerController* PC = World->GetFirstPlayerController())
		{
			// Desde el personaje (no desde la camara, que en tercera persona va 4 m atras), en la direccion a la que mira.
			FRotator ViewRot;
			PC->GetPlayerViewPoint(Loc, ViewRot);
			Yaw = ViewRot.Yaw;
			if (const APawn* Pawn = PC->GetPawn()) { Loc = Pawn->GetActorLocation(); }
			Loc += FRotator(0, Yaw, 0).Vector() * 600.f;
			FHitResult Hit;
			FCollisionQueryParams Q(NAME_None, false, PC->GetPawn());
			if (World->LineTraceSingleByChannel(Hit, Loc + FVector(0, 0, 300), Loc - FVector(0, 0, 3000), ECC_Visibility, Q))
			{
				Loc = Hit.ImpactPoint;
			}
		}
		return FTransform(FRotator(0, Yaw + 90.f, 0), Loc + FVector(0, 0, 10));
	}

	AJamBuildSpawner* SpawnSay(UWorld* World, const FString& Text, const FTransform& At, const TCHAR* RecipeJson = nullptr, bool bAutoActivate = true, bool bInventory = true)
	{
		if (bInventory && GLastSay.IsValid() && GLastSay->bUseInventory)
		{
			GLastSay->bAutoActivate = bAutoActivate;
			GLastSay->SetActorTransform(At);
			GLastSay->SetPromptAndBuild(Text);
			return GLastSay.Get();
		}
		AJamBuildSpawner* S = World->SpawnActorDeferred<AJamBuildSpawner>(AJamBuildSpawner::StaticClass(), At, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (S)
		{
			S->bUseInventory = bInventory;
			S->Prompt = Text;
			S->bAnnounceOnScreen = true;
			S->Tags.Add(SayTag);
			if (RecipeJson) { S->RecipeJsonOverride = RecipeJson; }
			S->bAutoActivate = bAutoActivate;
			const APlayerController* PC = World->GetFirstPlayerController();
			S->Team = UJamStatics::GetActorTeam(PC ? PC->GetPawn() : nullptr);
			S->SetOwner(PC ? PC->GetPawn() : nullptr); // la cabeza de esta construccion no le apunta a quien la pidio
			S->FinishSpawning(At); // BeginPlay -> BuildNow: Jev (o respaldo) -> receta -> ensamblador -> activar
			GLastSay = S;
		}
		return S;
	}

	void JamSay(const TArray<FString>& Args, UWorld* World)
	{
		const FString Text = FString::Join(Args, TEXT(" ")).TrimStartAndEnd();
		if (!World || Text.IsEmpty())
		{
			Screen(TEXT("Uso: Jam.Say <que quieres construir>   (ej. Jam.Say un carro que queme todo)"));
			return;
		}
		SpawnSay(World, Text, SayTransform(World));
	}

	void ClearAll(UWorld* World)
	{
		for (TActorIterator<AJamBuildSpawner> It(World); It; ++It)
		{
			It->ClearBuild();
			if (It->ActorHasTag(SayTag)) { It->Destroy(); }
		}
		for (TActorIterator<AJamBuildPart> It(World); It; ++It)
		{
			It->Destroy();
		}
	}

	void JamSeat(const TArray<FString>&, UWorld* World);

	/** X / Jam.Clear: desarma SOLO la construccion del jugador (GLastSay) y le devuelve las piezas que siguen vivas. */
	void JamClear(const TArray<FString>&, UWorld* World)
	{
		if (!World) { return; }
		AJamBuildSpawner* Mine = GLastSay.Get();
		if (!Mine || Mine->CountAliveParts() == 0)
		{
			Screen(TEXT("No tienes construccion"), FColor::Orange);
			return;
		}
		// Si estaba montado en ella, baja antes de que se destruya el asiento (si no, se quedaria sin colision y sin movimiento).
		if (const APlayerController* PC = World->GetFirstPlayerController())
		{
			const APawn* Me = PC->GetPawn();
			const AJamBuildPart* Seat = Me ? Cast<AJamBuildPart>(Me->GetAttachParentActor()) : nullptr;
			if (Seat && Mine->GetParts().ContainsByPredicate([Seat](const TObjectPtr<AJamBuildPart>& P) { return P.Get() == Seat; })) { JamSeat(TArray<FString>(), World); }
		}
		int32 Back = 0;
		for (const auto& Item : Mine->GetRefundableItems()) { Back += Item.Value; }
		Mine->ClearBuild();
		if (!Mine->bUseInventory) { Mine->Destroy(); } // construccion de pruebas (sin inventario): no deja el spawner suelto
		Screen(Back > 0 ? FString::Printf(TEXT("Desarmaste tu construccion: +%d %s"), Back, Back == 1 ? TEXT("pieza") : TEXT("piezas")) : FString(TEXT("Desarmaste tu construccion")), FColor::Green);
	}

	/** Jam.ClearAll (solo pruebas): borra todas las construcciones y piezas del mapa, tambien las de los bots. */
	void JamClearAll(const TArray<FString>&, UWorld* World)
	{
		if (World)
		{
			ClearAll(World);
			Screen(TEXT("Todas las construcciones borradas"));
		}
	}

	void JamEnergy(const TArray<FString>& Args, UWorld* World)
	{
		if (!World) { return; }
		if (Args.Num() > 0) { AJamBuildPart::SetPlayerEnergy(World, FCString::Atof(*Args[0])); }
		Screen(FString::Printf(TEXT("Energia del jugador: %.0f / 150  (modo jam.Build.Energy=%d: 2 = barra del jugador, 1 = por construccion, 0 = infinita)"),
			AJamBuildPart::GetPlayerEnergy(World), AJamBuildPart::EnergyMode()));
	}

	// ---- Jam.Test: pruebas aisladas por mecanica + bateria de frases -> docs/building/REPORTE-POC.md -------------
	struct FJamCaseDef
	{
		const TCHAR* Key;     // aislada: clave de la mecanica (coincide con el extra o la plantilla); frase: nullptr
		const TCHAR* Text;    // frase para Jev, o descripcion de la prueba aislada
		const TCHAR* Recipe;  // aislada: receta JSON fija (sin modelo); frase: nullptr
		const TCHAR* Expect;  // frase de composicion: piezas exigidas, "Wheel=25,Cannon=4" (nullptr = no se exige)
	};

	const FJamCaseDef BuiltinCases[] = {
		// --- aisladas: una mecanica, receta fija, metrica propia ---
		{ TEXT("Roller"), TEXT("Rueda sola (raiz): debe rodar de pie"),
			TEXT("{\"name\":\"Rueda\",\"parts\":[{\"id\":\"body\",\"type\":\"BigWheel\"}],\"activate\":[\"body\"]}") },
		{ TEXT("head"), TEXT("Cabeza + rayo: debe girar hacia un objetivo que se mueve de lado"),
			TEXT("{\"name\":\"Centinela\",\"parts\":[{\"id\":\"body\",\"type\":\"Block\"},{\"id\":\"mount\",\"type\":\"Head\",\"attach_to\":\"body\",\"socket\":\"Top\"},{\"id\":\"gun\",\"type\":\"Beam\",\"attach_to\":\"mount\",\"socket\":\"Top_Center_Front\"}],\"activate\":[\"mount\",\"gun\"]}") },
		{ TEXT("hinge"), TEXT("Bisagra + hoja: al empujarla debe girar y seguir unida"),
			TEXT("{\"name\":\"Bisagra\",\"parts\":[{\"id\":\"body\",\"type\":\"Block\"},{\"id\":\"h\",\"type\":\"Hinge\",\"attach_to\":\"body\",\"socket\":\"Top\"},{\"id\":\"flap\",\"type\":\"Panel\",\"attach_to\":\"h\",\"socket\":\"Top\"}],\"activate\":[]}") },
		{ TEXT("stake"), TEXT("Estaca: un bloque con ventilador encendido no debe moverse"),
			TEXT("{\"name\":\"Anclado\",\"parts\":[{\"id\":\"body\",\"type\":\"Block\"},{\"id\":\"st\",\"type\":\"Stake\",\"attach_to\":\"body\",\"socket\":\"Left_Bottom\"},{\"id\":\"fan\",\"type\":\"Fan\",\"attach_to\":\"body\",\"socket\":\"Back\"}],\"activate\":[\"st\",\"fan\"]}") },
		{ TEXT("light"), TEXT("Foco: debe encender una luz"),
			TEXT("{\"name\":\"Faro\",\"parts\":[{\"id\":\"body\",\"type\":\"Block\"},{\"id\":\"l\",\"type\":\"Light\",\"attach_to\":\"body\",\"socket\":\"Front\"}],\"activate\":[\"l\"]}") },
		{ TEXT("stabilizer"), TEXT("Estabilizador: un dron golpeado de lado debe enderezarse"),
			TEXT("{\"name\":\"Dron\",\"parts\":[{\"id\":\"body\",\"type\":\"Plank\"},{\"id\":\"f0\",\"type\":\"Fan\",\"attach_to\":\"body\",\"socket\":\"Bottom_Front\"},{\"id\":\"f1\",\"type\":\"Fan\",\"attach_to\":\"body\",\"socket\":\"Bottom_Back\"},{\"id\":\"stab\",\"type\":\"Stabilizer\",\"attach_to\":\"body\",\"socket\":\"Top\"}],\"activate\":[\"f0\",\"f1\",\"stab\"]}") },
		{ TEXT("cannon"), TEXT("Cañon: la bola debe explotar al impactar y alcanzar al objetivo"),
			TEXT("{\"name\":\"Cañon\",\"parts\":[{\"id\":\"body\",\"type\":\"Block\"},{\"id\":\"gun\",\"type\":\"Cannon\",\"attach_to\":\"body\",\"socket\":\"Front\"}],\"activate\":[\"gun\"]}") },
		{ TEXT("battery"), TEXT("Bateria grande: el lanzallamas gasta la bateria (80) antes que la barra del jugador"),
			TEXT("{\"name\":\"Pila\",\"parts\":[{\"id\":\"body\",\"type\":\"Block\"},{\"id\":\"bat\",\"type\":\"BigBattery\",\"attach_to\":\"body\",\"socket\":\"Top\"},{\"id\":\"fl\",\"type\":\"Flamethrower\",\"attach_to\":\"body\",\"socket\":\"Front\"}],\"activate\":[\"fl\"]}") },
		// --- frases: todo el camino (Jev -> receta -> ensamblador) ---
		{ nullptr, TEXT("un carro con cohete") }, { nullptr, TEXT("un tanque") }, { nullptr, TEXT("un cohete") }, { nullptr, TEXT("un avion con alas y ventilador") },
		{ nullptr, TEXT("una torreta con laser") }, { nullptr, TEXT("un globo aerostatico con fuego debajo") }, { nullptr, TEXT("un carro con lanzallamas") },
		{ nullptr, TEXT("un hovercraft con vela y emisor de hielo") }, { nullptr, TEXT("un carro con una bomba") }, { nullptr, TEXT("un puente largo de tablas") },
		{ nullptr, TEXT("un trampolin con resorte que lance cosas") }, { nullptr, TEXT("un velero con ruedas") }, { nullptr, TEXT("una fortaleza con cañones") },
		{ nullptr, TEXT("una moto") }, { nullptr, TEXT("un trineo con ventilador") }, { nullptr, TEXT("un dron con un faro") }, { nullptr, TEXT("una catapulta") },
		{ nullptr, TEXT("un aspersor de fuego que gire") }, { nullptr, TEXT("una rueda sola que ruede quemando todo") }, { nullptr, TEXT("un planeador") },
		{ nullptr, TEXT("una torreta con cabeza que apunte sola") }, { nullptr, TEXT("un carro con manguera de agua") }, { nullptr, TEXT("un tanque electrico que de toques") },
		{ nullptr, TEXT("un carro con turbina y bateria") }, { nullptr, TEXT("un carro levitante con piedras antigravedad") }, { nullptr, TEXT("un dirigible con cañon") },
		{ nullptr, TEXT("una torreta anclada al suelo con una bisagra") }, { nullptr, TEXT("algo que ruede y queme todo") },
		{ nullptr, TEXT("hazme un carrito bien loco con lanzallamas, un globo, cohete, luces y una bateria extra") },
		// composicion por piezas: cantidades exactas
		{ nullptr, TEXT("arma un carro con 25 llantas"), nullptr, TEXT("Wheel=25") },
		{ nullptr, TEXT("una catapulta"), nullptr, TEXT("Spring=1") },
		{ nullptr, TEXT("un puente con 10 tablas"), nullptr, TEXT("Plank=10") },
		{ nullptr, TEXT("un cohete con 3 propulsores"), nullptr, TEXT("Rocket=3") },
		{ nullptr, TEXT("un tanque con 4 cañones"), nullptr, TEXT("Cannon=4") },
		{ nullptr, TEXT("una torre de 12 bloques"), nullptr, TEXT("Block=12") },
		{ nullptr, TEXT("un carro con seis ruedas y dos lanzallamas"), nullptr, TEXT("Wheel=6,Flamethrower=2") },
		// piezas poco probadas
		{ nullptr, TEXT("un carro con una viga larga al frente como lanza") }, { nullptr, TEXT("un tanque con plataforma giratoria") },
		{ nullptr, TEXT("un trineo con vela") }, { nullptr, TEXT("un carro con ventilador y una bateria") },
		{ nullptr, TEXT("un muro defensivo con escudo") }, { nullptr, TEXT("un carro con resorte que salte") },
	};

	/** Caso en ejecucion: los de la tabla de arriba o los de una bateria externa (JSON: frase, must, forbid, counts, capability). */
	struct FJamCase
	{
		FString Key, Text, Recipe, Expect;       // Expect = "Wheel=25,Cannon=4"
		TArray<FString> Must, Forbid, Caps;      // piezas que deben / no deben estar; canales que deben responder
		bool IsIso() const { return !Recipe.IsEmpty(); }
	};
	TArray<FJamCase> GCases;
	FString GReportPath, GBatteryName;
	#define NumCases GCases.Num()
	void JamSeat(const TArray<FString>&, UWorld* World);

	struct FJamTestRun
	{
		TWeakObjectPtr<UWorld> World;
		FTransform At;
		int32 Index = -1;
		float Timer = 0.f;
		float Duration = 10.f;
		TWeakObjectPtr<AJamBuildSpawner> Cur;
		TWeakObjectPtr<AStaticMeshActor> Dummy;
		FVector DummyStart = FVector::ZeroVector;
		// metricas del caso actual
		bool bStarted = false, bKicked = false;
		FVector Start = FVector::ZeroVector;
		int32 Parts = 0, MaxOn = 0, MaxBalls = 0, Hits = 0;
		double MaxSpeed = 0, MaxUp = 0, Disp = 0, MaxSpin = 0, DummyMoved = 0;
		bool bHadBomb = false, bExploded = false, bFrozen = false;
		// metricas de las pruebas aisladas
		double AimErrSum = 0, AimYawMin = 0, AimYawMax = 0, HingeMax = 0, HingeSep = 0, HingeDist0 = -1, MinUpZ = 1, LastUpZ = 1, Energy0 = -1, EnergyEnd = -1, PlayerEnergyMin = 100;
		int32 AimN = 0, Frames = 0;
		float Wall = 0.f;                  // segundos de reloj del caso (para los cuadros por segundo reales)
		bool bLightOn = false, bRootWheel = false;
		// "utilizable": fases con entrada simulada (mando -> quieta -> disparar -> soltar -> acelerar -> girar -> turbo)
		double BuiltAt = -1, IdleDisp = 0, DriveFwd = 0, SteerDeg = 0;
		int32 Phase = 0, IdleWeapons = 0, FireOn = 0, WeaponsAfter = 0, Fx0 = 0, FireFx = 0;
		bool bBoostOn = false, bPushPart = false, bLiftPart = false, bSeat = false, bMounted = false;
		FVector P0 = FVector::ZeroVector, Fwd0 = FVector::ForwardVector;
		FTransform Home;                   // donde estaba el jugador al empezar (vuelve ahi tras cada caso)
		int32 CohSum = 0, CohPass = 0, UsablePass = 0, DecisionPass = 0;
		TSet<FName> Types;                 // tipos de pieza presentes
		TMap<FName, int32> TypeCount;      // cuantas de cada tipo (maximo visto)
		// resultados
		int32 Pass = 0, IsoPass = 0, IsoCount = 0, PhraseCount = 0;
		TMap<FString, bool> Iso;
		FString Rows, IsoRows;
		FTSTicker::FDelegateHandle Handle;
	};
	FJamTestRun GTest;

	AJamBuildPart* PartById(const TArray<TObjectPtr<AJamBuildPart>>& Parts, FName Id)
	{
		for (AJamBuildPart* P : Parts)
		{
			if (IsValid(P) && P->RecipeId == Id) { return P; }
		}
		return nullptr;
	}

	/** "Wheel=25,Cannon=4": true si la construccion tiene esas cantidades. */
	bool CheckExpect(const FJamTestRun& T, const FString& Expect, FString& Out)
	{
		if (Expect.IsEmpty()) { return true; }
		TArray<FString> Items;
		Expect.ParseIntoArray(Items, TEXT(","));
		bool bAll = true;
		TArray<FString> Txt;
		for (const FString& I : Items)
		{
			FString Type, Num;
			I.Split(TEXT("="), &Type, &Num);
			const int32 Have = T.TypeCount.FindRef(FName(*Type));
			bAll &= Have == FCString::Atoi(*Num);
			Txt.Add(FString::Printf(TEXT("%s %d/%s"), *Type, Have, *Num));
		}
		Out = FString::Join(Txt, TEXT(", "));
		return bAll;
	}

	/** Canal de control que pide una "capability" de la bateria externa (texto libre en es/en). */
	FString ChannelOfCap(const FString& Cap)
	{
		const FString C = Cap.ToLower();
		auto Any = [&C](std::initializer_list<const TCHAR*> Ws) { for (const TCHAR* W : Ws) { if (C.Contains(W)) { return true; } } return false; };
		if (Any({TEXT("driv"), TEXT("muev"), TEXT("mov"), TEXT("conduc"), TEXT("manej"), TEXT("rueda"), TEXT("steer")})) { return TEXT("MOVER"); }
		if (Any({TEXT("fire"), TEXT("dispar"), TEXT("shoot"), TEXT("fuego"), TEXT("quem"), TEXT("laser"), TEXT("congel"), TEXT("electro"), TEXT("moj"), TEXT("weapon"), TEXT("attack")})) { return TEXT("FUEGO"); }
		if (Any({TEXT("fly"), TEXT("vuel"), TEXT("vol"), TEXT("levit"), TEXT("lift"), TEXT("altura"), TEXT("subir"), TEXT("hover")})) { return TEXT("ALTURA"); }
		if (Any({TEXT("boost"), TEXT("turbo"), TEXT("cohete"), TEXT("rocket"), TEXT("explot")})) { return TEXT("TURBO"); }
		if (Any({TEXT("power"), TEXT("encend"), TEXT("activ"), TEXT("ilumin"), TEXT("lanza"), TEXT("gira")})) { return TEXT("ENCENDER"); }
		return FString();
	}

	/**
	 * UTILIZABLE: cada canal de control que la construccion tiene (por las capacidades de sus piezas) o que la frase exige
	 * responde a la entrada simulada, y sin entrada las armas se quedan quietas.
	 */
	bool UsableVerdict(const FJamTestRun& T, const FJamCase& Case, FString& Out)
	{
		auto Has = [&T](std::initializer_list<const TCHAR*> Types) { for (const TCHAR* X : Types) { if (T.Types.Contains(FName(X))) { return true; } } return false; };
		const bool bWheels = Has({TEXT("Wheel"), TEXT("BigWheel")});
		const bool bWeapon = Has({TEXT("Cannon"), TEXT("Flamethrower"), TEXT("Frost"), TEXT("Beam"), TEXT("Shock"), TEXT("Hydrant")});
		const bool bBoost = Has({TEXT("Rocket"), TEXT("Bomb")});
		TSet<FString> Need;
		if (bWheels || T.bPushPart) { Need.Add(TEXT("MOVER")); Need.Add(TEXT("GIRAR")); }
		if (bWeapon) { Need.Add(TEXT("FUEGO")); }
		if (bBoost) { Need.Add(TEXT("TURBO")); }
		if (T.bLiftPart) { Need.Add(TEXT("ALTURA")); }
		for (const FString& Cap : Case.Caps) { const FString Ch = ChannelOfCap(Cap); if (!Ch.IsEmpty()) { Need.Add(Ch); } }
		bool bAll = true;
		TArray<FString> Txt;
		auto Chan = [&](const TCHAR* Name, bool bOk, const FString& Detail)
		{
			if (!Need.Contains(Name)) { return; }
			bAll &= bOk;
			Txt.Add(FString::Printf(TEXT("%s %s (%s)"), Name, bOk ? TEXT("ok") : TEXT("NO"), *Detail));
		};
		Chan(TEXT("MOVER"), T.DriveFwd > 100, FString::Printf(TEXT("W: %.0f cm al frente"), T.DriveFwd));
		Chan(TEXT("GIRAR"), T.SteerDeg > 15, FString::Printf(TEXT("D: %.0f grados"), T.SteerDeg));
		Chan(TEXT("FUEGO"), T.FireOn > 0 && T.WeaponsAfter == 0, FString::Printf(TEXT("clic: %d armas; %s; al soltar %d"), T.FireOn, T.FireFx > 0 ? TEXT("su efecto toco al objetivo") : TEXT("no toco al objetivo"), T.WeaponsAfter));
		Chan(TEXT("TURBO"), T.bBoostOn, T.bExploded ? TEXT("Espacio: la bomba se solto y exploto") : TEXT("Espacio: cohete/bomba"));
		Chan(TEXT("ALTURA"), T.MaxUp > 30, FString::Printf(TEXT("subio %.0f cm"), T.MaxUp));
		Chan(TEXT("ENCENDER"), T.MaxOn > 0, FString::Printf(TEXT("C: %d encendidas"), T.MaxOn));
		if (T.IdleWeapons > 0) { bAll = false; Txt.Add(FString::Printf(TEXT("sin entrada disparaban %d armas"), T.IdleWeapons)); }
		if (bWheels && !T.bLiftPart && !Has({TEXT("Sail")}) && T.IdleDisp > 50) { bAll = false; Txt.Add(FString::Printf(TEXT("sin entrada se fue %.0f cm"), T.IdleDisp)); }
		if (Need.Num() == 0)
		{
			const bool bStill = T.MaxOn > 0 || (T.Disp < 20 && FMath::Abs(T.LastUpZ) > 0.7);
			bAll &= bStill;
			Txt.Add(T.MaxOn > 0 ? FString::Printf(TEXT("C enciende %d dispositivos"), T.MaxOn) : FString::Printf(TEXT("estructura: se movio %.0f cm, %s"), T.Disp, bStill ? TEXT("en pie") : TEXT("se cayo")));
		}
		if (!T.bSeat && Need.Num() > 0) { Txt.Add(TEXT("sin asiento: mando desde la pieza")); }
		if (!T.bMounted) { Txt.Add(TEXT("OJO: la prueba no pudo tomar el mando (medicion no valida)")); }
		Out = FString::Join(Txt, TEXT("; "));
		return bAll;
	}

	/** Veredicto de una prueba aislada: metrica propia de esa mecanica. */
	bool IsoVerdict(const FJamTestRun& T, const FString& Key, FString& Out)
	{
		if (Key == TEXT("Roller"))
		{
			Out = FString::Printf(TEXT("rodo %.0f cm (max %.0f cm/s), inclinacion final del eje %.2f (0 = de pie)"), T.Disp, T.MaxSpeed, FMath::Abs(T.LastUpZ));
			return T.Disp > 300 && FMath::Abs(T.LastUpZ) < 0.5;
		}
		if (Key == TEXT("head"))
		{
			const double Err = T.AimN > 0 ? T.AimErrSum / T.AimN : 999.0;
			Out = FString::Printf(TEXT("error medio de punteria %.0f grados; la cabeza barrio %.0f grados siguiendo al objetivo"), Err, T.AimYawMax - T.AimYawMin);
			return Err < 25.0 && (T.AimYawMax - T.AimYawMin) > 30.0;
		}
		if (Key == TEXT("hinge"))
		{
			Out = FString::Printf(TEXT("la hoja giro hasta %.0f grados; separacion maxima de la union %.1f cm"), T.HingeMax, T.HingeSep);
			return T.HingeMax > 5.0 && T.HingeMax < 80.0 && T.HingeSep < 5.0;
		}
		if (Key == TEXT("stake"))
		{
			Out = FString::Printf(TEXT("con el ventilador encendido se movio %.1f cm"), T.Disp);
			return T.MaxOn > 0 && T.Disp < 15.0;
		}
		if (Key == TEXT("light"))
		{
			Out = T.bLightOn ? TEXT("foco encendido (componente de luz visible con intensidad > 0)") : TEXT("no se encontro luz encendida");
			return T.bLightOn;
		}
		if (Key == TEXT("stabilizer"))
		{
			Out = FString::Printf(TEXT("tras el golpe se inclino hasta up.z=%.2f y termino en up.z=%.2f (1 = derecho)"), T.MinUpZ, T.LastUpZ);
			return T.MinUpZ < 0.97 && T.LastUpZ > 0.95;
		}
		if (Key == TEXT("cannon"))
		{
			Out = FString::Printf(TEXT("%d bolas; explosiones que alcanzaron al objetivo: %d; objetivo empujado %.0f cm"), T.MaxBalls, T.Hits, T.DummyMoved);
			return T.Hits > 0;
		}
		if (Key == TEXT("battery"))
		{
			Out = FString::Printf(TEXT("bateria de la construccion %.1f -> %.1f; barra del jugador minima %.1f"), T.Energy0, T.EnergyEnd, T.PlayerEnergyMin);
			return T.Energy0 > 79.0 && T.EnergyEnd < T.Energy0 - 5.0 && T.PlayerEnergyMin > 99.0;
		}
		Out = TEXT("sin metrica");
		return false;
	}

	void TestFinishCase()
	{
		FJamTestRun& T = GTest;
		const FJamCase& Case = GCases[T.Index];
		const FString Status = T.Cur.IsValid() ? T.Cur->LastStatus : FString(TEXT("(sin construir)"));
		const bool bBuilt = Status.StartsWith(TEXT("OK"));
		const int32 Coh = T.Cur.IsValid() ? T.Cur->LastCoherence : -1;
		if (Case.IsIso())
		{
			FString What;
			const bool bPass = bBuilt && IsoVerdict(T, Case.Key, What);
			T.Iso.Add(Case.Key, bPass);
			++T.IsoCount;
			T.IsoPass += bPass ? 1 : 0;
			T.IsoRows += FString::Printf(TEXT("| `%s` | %s | %d | %s | %s |\n"), *Case.Key, *Case.Text, T.Parts, bBuilt ? *What : *Status, bPass ? TEXT("PASA") : TEXT("FALLA"));
			UE_LOG(LogTemp, Display, TEXT("JamTest %d/%d ISO %s | %s | %s"), T.Index + 1, NumCases, bPass ? TEXT("PASA") : TEXT("FALLA"), *Case.Key, bBuilt ? *What : *Status);
			return;
		}
		const TCHAR* Backend = Status.Contains(TEXT("fallback")) ? TEXT("respaldo (fallo Jev)") : Status.Contains(TEXT("(jev")) ? TEXT("Jev") : Status.Contains(TEXT("(fal")) ? TEXT("planificador LLM") : Status.Contains(TEXT("keywords")) ? TEXT("palabras clave") : TEXT("?");
		// 1) Decision: piezas que deben estar, las que no, y cantidades exactas.
		TArray<FString> Dec;
		bool bDecision = bBuilt;
		// "Wheel/BigWheel" = cualquiera de las dos.
		auto HasType = [&T](const FString& Type)
		{
			TArray<FString> Alts;
			Type.ParseIntoArray(Alts, TEXT("/"));
			for (const FString& A : Alts) { for (const FName& X : T.Types) { if (X.ToString().Equals(A.TrimStartAndEnd(), ESearchCase::IgnoreCase)) { return true; } } }
			return false;
		};
		for (const FString& M : Case.Must) { if (!HasType(M)) { bDecision = false; Dec.Add(TEXT("falta ") + M); } }
		for (const FString& F : Case.Forbid) { if (HasType(F)) { bDecision = false; Dec.Add(TEXT("sobra ") + F); } }
		FString CountWhy;
		if (!CheckExpect(T, Case.Expect, CountWhy)) { bDecision = false; }
		if (!CountWhy.IsEmpty()) { Dec.Add(CountWhy); }
		if (Status.Contains(TEXT("skipped=["))) { Dec.Add(TEXT("no todo cupo")); }
		// 2) Coherencia (sentido) y 3) utilizable (canales con entrada simulada).
		const bool bCoh = Coh >= 85;
		FString Use;
		const bool bUsable = bBuilt && UsableVerdict(T, Case, Use);
		TArray<FString> Seen;
		if (T.Hits > 0) { Seen.Add(FString::Printf(TEXT("efecto sobre el objetivo x%d"), T.Hits)); }
		if (T.bFrozen) { Seen.Add(TEXT("objetivo congelado")); }
		if (T.DummyMoved > 30) { Seen.Add(FString::Printf(TEXT("objetivo empujado %.0f cm"), T.DummyMoved)); }
		if (T.MaxBalls > 0) { Seen.Add(FString::Printf(TEXT("%d bolas"), T.MaxBalls)); }
		if (T.Wall > 1.f && (T.Parts >= 25 || T.Frames / T.Wall < 25.f)) { Seen.Add(FString::Printf(TEXT("%d piezas a %.0f fps"), T.Parts, T.Frames / T.Wall)); }
		if (FMath::Abs(T.LastUpZ) < 0.5 && !T.bRootWheel) { Seen.Add(TEXT("termino volcada")); }
		const bool bPassAll = bBuilt && bDecision && bCoh && bUsable;
		++T.PhraseCount;
		T.Pass += bPassAll ? 1 : 0;
		T.CohSum += FMath::Max(Coh, 0);
		T.CohPass += bCoh ? 1 : 0;
		T.UsablePass += bUsable ? 1 : 0;
		T.DecisionPass += bDecision ? 1 : 0;
		const FString CohNote = T.Cur.IsValid() ? T.Cur->LastCoherenceNote : FString();
		T.Rows += FString::Printf(TEXT("| %d | %s | %s | `%s` | %d | %s %s | %d%s | %s %s%s | %s |\n"), T.PhraseCount, *Case.Text, Backend,
			*Status.Replace(TEXT("|"), TEXT("/")), T.Parts,
			bDecision ? TEXT("ok") : TEXT("NO"), *FString::Join(Dec, TEXT("; ")),
			Coh, CohNote.IsEmpty() ? TEXT("") : *(TEXT(" (") + CohNote.Replace(TEXT("|"), TEXT("/")) + TEXT(")")),
			bUsable ? TEXT("SI:") : TEXT("NO:"), *Use, Seen.Num() ? *(TEXT(". Visto: ") + FString::Join(Seen, TEXT("; "))) : TEXT(""),
			bPassAll ? TEXT("PASA") : TEXT("FALLA"));
		UE_LOG(LogTemp, Display, TEXT("JamTest %d/%d %s | %s | coh=%d | decision=%d %s | utilizable=%d %s | %s"), T.Index + 1, NumCases, bPassAll ? TEXT("PASA") : TEXT("FALLA"), *Case.Text, Coh,
			bDecision, *FString::Join(Dec, TEXT("; ")), bUsable, *Use, *Status);
	}

	void TestMeasure(FJamTestRun& T, UWorld* World)
	{
		const FJamCase& Case = GCases[T.Index];
		const FString Key = Case.Key;
		AStaticMeshActor* D = T.Dummy.Get();
		int32 Balls = 0;
		for (TActorIterator<AStaticMeshActor> It(World); It; ++It) { Balls += It->ActorHasTag(TEXT("JamProjectile")) ? 1 : 0; }
		T.MaxBalls = FMath::Max(T.MaxBalls, Balls);
		// Cabeza: el objetivo va y viene de lado delante de la construccion.
		if (D && Key == TEXT("head"))
		{
			D->SetActorLocation(T.At.TransformPosition(FVector(450, 350 * FMath::Sin(T.Timer * 0.9f), 60)), false, nullptr, ETeleportType::TeleportPhysics);
			D->GetStaticMeshComponent()->SetPhysicsLinearVelocity(FVector::ZeroVector);
		}
		if (AJamBuildSpawner* S = T.Cur.Get())
		{
			const TArray<TObjectPtr<AJamBuildPart>>& Parts = S->GetParts();
			int32 On = 0, Hits = 0, WeaponsOn = 0, RocketsOn = 0;
			bool bBomb = false;
			TMap<FName, int32> Now;
			for (const AJamBuildPart* P : Parts)
			{
				if (!IsValid(P)) { continue; }
				T.Types.Add(P->PartType);
				++Now.FindOrAdd(P->PartType);
				On += P->bActive ? 1 : 0;
				Hits += P->EffectHits;
				bBomb |= P->Kind == EJamPartKind::Bomb;
				const EJamPartKind Kd = P->Kind;
				const double DirZ = FMath::Abs(P->GetActorQuat().RotateVector(P->LocalDirection).Z);
				WeaponsOn += (P->bActive && (Kd == EJamPartKind::Cannon || Kd == EJamPartKind::Flame || Kd == EJamPartKind::Frost || Kd == EJamPartKind::Beam || Kd == EJamPartKind::Shock || Kd == EJamPartKind::Hydrant)) ? 1 : 0;
				RocketsOn += (P->bActive && Kd == EJamPartKind::Rocket) ? 1 : 0;
				T.bSeat |= Kd == EJamPartKind::Seat;
				if (T.Phase < 2) // orientacion al armar (antes de que nada la vuelque)
				{
					T.bPushPart |= Kd == EJamPartKind::Fan && DirZ < 0.5;
					T.bLiftPart |= Kd == EJamPartKind::Balloon || Kd == EJamPartKind::Hover || ((Kd == EJamPartKind::Fan || Kd == EJamPartKind::Rocket) && DirZ > 0.5 && P != Parts[0]);
				}
				T.MaxSpin = FMath::Max(T.MaxSpin, P->Mesh->GetPhysicsAngularVelocityInRadians().Size());
				if (const USpotLightComponent* L = P->FindComponentByClass<USpotLightComponent>())
				{
					T.bLightOn |= P->bActive && L->IsVisible() && L->Intensity > 0.f;
				}
			}
			if (Parts.Num() > 0 && IsValid(Parts[0]))
			{
				AJamBuildPart* Root = Parts[0];
				T.bRootWheel = Root->Kind == EJamPartKind::Wheel;
				for (const auto& KV : Now) { T.TypeCount.FindOrAdd(KV.Key) = FMath::Max(T.TypeCount.FindRef(KV.Key), KV.Value); }
				T.Parts = FMath::Max(T.Parts, Parts.Num());
				T.MaxOn = FMath::Max(T.MaxOn, On);
				T.Hits = FMath::Max(T.Hits, Hits);
				T.bExploded |= T.bHadBomb && !bBomb;
				T.bHadBomb |= bBomb;
				const FVector L = Root->GetActorLocation();
				if (!T.bStarted)
				{
					T.bStarted = true;
					T.Start = L;
					T.Energy0 = Root->Build.IsValid() ? Root->Build->Energy : -1.0;
				}
				T.Disp = FMath::Max(T.Disp, FVector::Dist2D(L, T.Start));
				T.MaxUp = FMath::Max(T.MaxUp, L.Z - T.Start.Z);
				T.MaxSpeed = FMath::Max(T.MaxSpeed, Root->Mesh->GetPhysicsLinearVelocity().Size());
				T.LastUpZ = Root->GetActorUpVector().Z;
				T.EnergyEnd = Root->Build.IsValid() ? Root->Build->Energy : -1.0;
				T.PlayerEnergyMin = FMath::Min(T.PlayerEnergyMin, (double)AJamBuildPart::GetPlayerEnergy(World));

				if (Key == TEXT("head") && D)
				{
					if (const AJamBuildPart* Head = PartById(Parts, "mount"))
					{
						// Pegada arriba, la "Mira" de la cabeza es su eje de giro: el frente es el de su malla (igual que en AJamBuildPart::Tick).
						const FVector HeadDir = Head->GetActorQuat().RotateVector(Head->LocalDirection);
						const FVector Fwd = (FMath::Abs(HeadDir.Z) > 0.9 ? Head->GetActorForwardVector() : HeadDir).GetSafeNormal2D();
						const FVector To = (D->GetActorLocation() - Head->GetActorLocation()).GetSafeNormal2D();
						const double Yaw = FMath::RadiansToDegrees(FMath::Atan2(FVector::DotProduct(Fwd, T.At.GetUnitAxis(EAxis::Y)), FVector::DotProduct(Fwd, T.At.GetUnitAxis(EAxis::X))));
						if (T.Timer > 3.f)
						{
							T.AimErrSum += FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(Fwd, To), -1.0, 1.0)));
							++T.AimN;
							T.AimYawMin = T.AimN == 1 ? Yaw : FMath::Min(T.AimYawMin, Yaw);
							T.AimYawMax = T.AimN == 1 ? Yaw : FMath::Max(T.AimYawMax, Yaw);
						}
					}
				}
				if (!Case.IsIso())
				{
					// Fases de "utilizable" (escala de 12 s desde que aparecio la construccion). La entrada simulada entra por
					// el mismo camino que el teclado: Jam.Seat, y los globales de Jam.Drive / Jam.Fire que lee AJamTestCharacter::Tick.
					if (T.BuiltAt < 0) { T.BuiltAt = T.Timer; }
					const double K = T.Duration / 12.0, t = (T.Timer - T.BuiltAt) / K, NowS = FPlatformTime::Seconds();
					const FVector Fwd = Root->GetActorQuat().RotateVector(Root->LocalDirection).GetSafeNormal2D();
					const int32 Fx = Hits + T.MaxBalls + (T.DummyMoved > 10 ? 1 : 0) + (T.bFrozen ? 1 : 0);
					auto Enter = [&T](int32 Ph) { if (T.Phase < Ph) { T.Phase = Ph; return true; } return false; };
					if (t >= 1.0 && Enter(1)) // tomar el mando
					{
						const APlayerController* PC = World->GetFirstPlayerController();
						if (PC && PC->GetPawn() && !PC->GetPawn()->GetAttachParentActor()) { JamSeat(TArray<FString>(), World); }
						T.bMounted = PC && PC->GetPawn() && PC->GetPawn()->GetAttachParentActor() != nullptr;
					}
					if (t >= 2.0 && Enter(2)) { T.P0 = L; }                                          // quieta: sin entrada
					if (T.Phase == 2 && t >= 2.3) { T.IdleWeapons = FMath::Max(T.IdleWeapons, WeaponsOn); }
					if (t >= 3.5 && Enter(3)) { T.IdleDisp = FVector::Dist2D(L, T.P0); T.Fx0 = Fx; GFireUntil = NowS + 600.0; }   // clic (se suelta en la fase siguiente)
					if (T.Phase == 3) { T.FireOn = FMath::Max(T.FireOn, WeaponsOn); }
					if (t >= 6.0 && Enter(4)) { T.FireFx = Fx - T.Fx0; GFireUntil = 0.0; }           // soltar
					if (t >= 6.5 && Enter(5)) { T.WeaponsAfter = WeaponsOn; T.P0 = L; T.Fwd0 = Fwd; GDriveThrottle = 1.f; GDriveSteer = 0.f; GDriveUntil = NowS + 600.0; GLiftUntil = NowS + 600.0; } // W (+ Espacio mantenido: subir); se sueltan en la fase 7
					if (t >= 9.0 && Enter(6)) { T.DriveFwd = FVector::DotProduct(L - T.P0, T.Fwd0); T.Fwd0 = Fwd; GDriveSteer = 1.f; }          // W + D
					if (t >= 10.5 && Enter(7)) { T.SteerDeg = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(Fwd, T.Fwd0), -1.0, 1.0))); GDriveUntil = 0.0; GLiftUntil = 0.0; GBoostOnce = true; } // Espacio
					if (T.Phase == 7)
					{
						bool bBombArmed = false; // la bomba responde a Espacio armandose (mecha de 4 s): cuenta aunque explote despues
						for (const AJamBuildPart* P : Parts) { bBombArmed |= IsValid(P) && P->Kind == EJamPartKind::Bomb && P->bActive; }
						T.bBoostOn |= RocketsOn > 0 || bBombArmed || T.bExploded || (T.bHadBomb && !bBomb);
					}
				}
				if (Key == TEXT("hinge"))
				{
					if (AJamBuildPart* Hinge = PartById(Parts, "h"))
					{
						const double Dist = FVector::Dist(Hinge->GetActorLocation(), L);
						if (T.HingeDist0 < 0) { T.HingeDist0 = Dist; }
						T.HingeSep = FMath::Max(T.HingeSep, FMath::Abs(Dist - T.HingeDist0));
						// La bisagra gira sobre el eje Y de la construccion: se mide cuanto se aparta su frente del frente del cuerpo.
						T.HingeMax = FMath::Max(T.HingeMax, FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(Hinge->GetActorForwardVector(), Root->GetActorForwardVector()), -1.0, 1.0))));
						if (!T.bKicked && T.Timer > 3.f)
						{
							T.bKicked = true;
							Hinge->Mesh->AddAngularImpulseInRadians(T.At.GetUnitAxis(EAxis::Y) * 4.0, NAME_None, true);
						}
					}
				}
				if (Key == TEXT("stabilizer"))
				{
					if (!T.bKicked && T.Timer > 4.f)
					{
						T.bKicked = true;
						T.MinUpZ = 1.0;
						Root->Mesh->AddAngularImpulseInRadians(T.At.GetUnitAxis(EAxis::X) * 14.0, NAME_None, true);
					}
					if (T.bKicked) { T.MinUpZ = FMath::Min(T.MinUpZ, T.LastUpZ); }
				}
			}
		}
		if (D)
		{
			if (Key != TEXT("head")) { T.DummyMoved = FMath::Max(T.DummyMoved, FVector::Dist(D->GetActorLocation(), T.DummyStart)); }
			T.bFrozen |= D->GetStaticMeshComponent()->GetLinearDamping() > 1.f;
		}
	}

	void TestWriteReport(FJamTestRun& T)
	{
		const int32 Np = FMath::Max(T.PhraseCount, 1);
		FString Md = TEXT("---\ntitle: Reporte del POC de construccion por lenguaje natural\ntags:\n  - building\n  - pruebas\nstatus: generado\n---\n\n# Reporte del POC: frase -> modelo de decision -> construccion\n\n");
		Md += TEXT("> [!info]\n> Archivo GENERADO por el comando de consola `Jam.Test` (en PIE, `Source/GameJamm/Building/JamPlayConsole.cpp`). No editar a mano: volver a correr el comando.\n");
		Md += FString::Printf(TEXT("> Fecha: %s. Bateria: %s. Cada caso se construye delante del jugador y se mide durante %.0f s.\n\n"), *FDateTime::Now().ToString(TEXT("%Y-%m-%d %H:%M")), *GBatteryName, T.Duration);
		Md += FString::Printf(TEXT("**Resultado: %d de %d pruebas aisladas pasan. Frases: %d de %d pasan TODO (construida + decision + coherencia + utilizable).**\n\n"), T.IsoPass, T.IsoCount, T.Pass, T.PhraseCount);
		Md += TEXT("| Medida (frases) | Resultado |\n|---|---|\n");
		Md += FString::Printf(TEXT("| Decision correcta (piezas pedidas, prohibidas y cantidades) | %d de %d |\n"), T.DecisionPass, T.PhraseCount);
		Md += FString::Printf(TEXT("| Coherencia media (0-100) | %.1f |\n| Coherencia 85 o mas | %d de %d |\n"), (float)T.CohSum / Np, T.CohPass, T.PhraseCount);
		Md += FString::Printf(TEXT("| Utilizable (todos sus canales responden a la entrada simulada) | %d de %d |\n\n"), T.UsablePass, T.PhraseCount);
		if (T.IsoCount > 0)
		{
			Md += TEXT("## 1. Mecanicas verificadas aisladas\n\nUna mecanica por prueba, con receta fija (sin modelo de decision) y una metrica propia.\n\n");
			Md += TEXT("| Mecanica | Prueba | Piezas | Medido | Veredicto |\n|---|---|---|---|---|\n");
			Md += T.IsoRows;
		}
		Md += TEXT("\n## 2. Frases completas\n\nCamino completo: frase -> modelo de decision (piezas y cantidades) -> compositor con sentido -> validador de coherencia -> ensamblador, con un cubo-objetivo de 20 kg a 3.5 m al frente.\n\n");
		Md += TEXT("| # | Frase | Backend | Decision y receta (estado del spawner) | Piezas | Decision | Coherencia | Utilizable (canales) | Veredicto |\n|---|---|---|---|---|---|---|---|---|\n");
		Md += T.Rows;
		Md += TEXT("\n## Como leerlo\n\n- **Backend**: `Jev` = decidio el modelo de decision; `respaldo` = fallo la llamada y se uso el generador por palabras clave.\n");
		Md += TEXT("- **Decision**: `ok` si estan las piezas exigidas (`must`), no estan las prohibidas (`forbid`) y las cantidades pedidas coinciden (`tipo tengo/pedido`).\n");
		Md += TEXT("- **Coherencia**: puntaje 0-100 de `UJamAssembler::Coherence` (conexion, solapes, simetria, apoyo, empuje, tiro y aire libres, centro de masa, potencia). 85 o mas pasa; 70-84 se construye con aviso; menos de 70 se rearma. Entre parentesis, lo que se le dijo al jugador.\n");
		Md += TEXT("- **Utilizable**: el jugador toma el mando (asiento) y se simula la entrada por el mismo camino que el teclado. Fases: 2-3.5 s sin entrada (las armas deben estar quietas), 3.5-6 s clic (FUEGO), 6.5-9 s W (MOVER: mas de 100 cm al frente), 9-10.5 s W+D (GIRAR: mas de 15 grados), 10.5 s Espacio (TURBO: cohete o bomba). ALTURA = subio mas de 30 cm. Solo se exigen los canales que la construccion tiene por sus piezas (o los que la frase pide).\n");
		Md += TEXT("- **Veredicto**: PASA = construida + decision + coherencia 85 o mas + utilizable.\n");
		const bool bSaved = FFileHelper::SaveStringToFile(Md, *GReportPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		Screen(FString::Printf(TEXT("Jam.Test terminado: aisladas %d/%d, frases %d/%d (coherencia media %.1f, utilizables %d). Reporte %s: %s"), T.IsoPass, T.IsoCount, T.Pass, T.PhraseCount, (float)T.CohSum / Np, T.UsablePass, bSaved ? TEXT("guardado") : TEXT("NO guardado"), *GReportPath), FColor::Green);
		UE_LOG(LogTemp, Display, TEXT("JamTest DONE iso=%d/%d frases=%d/%d decision=%d coh_media=%.1f coh85=%d utilizable=%d saved=%d"), T.IsoPass, T.IsoCount, T.Pass, T.PhraseCount, T.DecisionPass, (float)T.CohSum / Np, T.CohPass, T.UsablePass, bSaved);
	}

	bool TestTick(float Dt)
	{
		FJamTestRun& T = GTest;
		UWorld* World = T.World.Get();
		if (!World)
		{
			return false;
		}
		// Tiempo de JUEGO, no de reloj: con el editor en segundo plano o con muchas piezas el juego va a 7-10 cuadros por segundo
		// y la fisica avanza mas despacio que el reloj; midiendo con reloj, un carro normal "avanzaba" 82 cm en vez de 270.
		T.Timer += World->GetDeltaSeconds();
		T.Wall += Dt;
		// Las frases se miden Duration segundos desde que APARECE la construccion (Jev tarda); 10 s de espera como mucho.
		const bool bWaitBuild = T.Index >= 0 && !GCases[T.Index].IsIso();
		if (T.Index >= 0 && T.Timer < (bWaitBuild ? (T.BuiltAt >= 0 ? T.BuiltAt + T.Duration : T.Duration + 10.0) : T.Duration))
		{
			++T.Frames;
			TestMeasure(T, World);
			return true;
		}

		// Cerrar el caso actual y pasar al siguiente.
		if (T.Index >= 0)
		{
			TestFinishCase();
		}
		// Soltar la entrada simulada, bajar del asiento y volver al punto de partida.
		GDriveUntil = GFireUntil = GLiftUntil = 0.0;
		GBoostOnce = false;
		if (const APlayerController* PC = World->GetFirstPlayerController())
		{
			if (ACharacter* Me = Cast<ACharacter>(PC->GetPawn()))
			{
				if (Me->GetAttachParentActor()) { JamSeat(TArray<FString>(), World); }
				Me->SetActorLocationAndRotation(T.Home.GetLocation(), T.Home.GetRotation(), false, nullptr, ETeleportType::TeleportPhysics);
				Me->GetCharacterMovement()->Velocity = FVector::ZeroVector;
			}
		}
		ClearAll(World);
		if (T.Dummy.IsValid()) { T.Dummy->Destroy(); }
		for (TActorIterator<AStaticMeshActor> It(World); It; ++It) { if (It->ActorHasTag(TEXT("JamProjectile"))) { It->Destroy(); } }
		++T.Index;
		if (T.Index >= NumCases)
		{
			TestWriteReport(T);
			T.Index = -1;
			return false;
		}
		// El jugador de la bateria no puede morir ni quedarse sin peon (si pasa, ningun caso posterior toma el mando).
		if (APlayerController* TestPC = World->GetFirstPlayerController())
		{
			if (!TestPC->GetPawn())
			{
				if (AGameModeBase* GM = World->GetAuthGameMode()) { GM->RestartPlayer(TestPC); }
				UE_LOG(LogTemp, Warning, TEXT("JamTest: el jugador no tenia peon; se reinicio"));
			}
			if (APawn* TestPawn = TestPC->GetPawn())
			{
				TestPawn->SetCanBeDamaged(false);
				if (!TestPawn->GetAttachParentActor()) { TestPawn->SetActorLocationAndRotation(T.Home.GetLocation(), T.Home.GetRotation(), false, nullptr, ETeleportType::TeleportPhysics); }
			}
		}
		// Siguiente caso: energia llena, objetivo nuevo, metricas a cero (se conservan los resultados acumulados).
		const FJamCase& Case = GCases[T.Index];
		const FString Key = Case.Key;
		AJamBuildPart::SetPlayerEnergy(World, 100.f);
		{
			FJamTestRun Fresh;
			Fresh.World = T.World; Fresh.At = T.At; Fresh.Index = T.Index; Fresh.Duration = T.Duration; Fresh.Handle = T.Handle;
			Fresh.Pass = T.Pass; Fresh.IsoPass = T.IsoPass; Fresh.IsoCount = T.IsoCount; Fresh.PhraseCount = T.PhraseCount;
			Fresh.Home = T.Home; Fresh.CohSum = T.CohSum; Fresh.CohPass = T.CohPass; Fresh.UsablePass = T.UsablePass; Fresh.DecisionPass = T.DecisionPass;
			Fresh.Iso = MoveTemp(T.Iso); Fresh.Rows = MoveTemp(T.Rows); Fresh.IsoRows = MoveTemp(T.IsoRows);
			T = MoveTemp(Fresh);
		}
		// Cubo-objetivo: en las frases, y en las aisladas que lo necesitan (cabeza y cañon).
		if (!Case.IsIso() || Key == TEXT("head") || Key == TEXT("cannon"))
		{
			T.DummyStart = T.At.TransformPosition(FVector(350, 0, 40));
			FActorSpawnParameters P;
			P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			if (AStaticMeshActor* D = World->SpawnActor<AStaticMeshActor>(T.DummyStart, FRotator::ZeroRotator, P))
			{
				UStaticMeshComponent* C = D->GetStaticMeshComponent();
				C->SetMobility(EComponentMobility::Movable);
				C->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
				C->SetWorldScale3D(FVector(0.6));
				C->SetCollisionProfileName(TEXT("PhysicsActor")); // como cualquier cuerpo fisico del juego: los emisores lo detectan
				C->SetSimulatePhysics(true);
				C->SetMassOverrideInKg(NAME_None, 20.f, true);
				T.Dummy = D;
			}
		}
		Screen(FString::Printf(TEXT("Jam.Test %d/%d: %s"), T.Index + 1, NumCases, *Case.Text), FColor::Cyan);
		T.Cur = SpawnSay(World, Case.Text, T.At, Case.IsIso() ? *Case.Recipe : nullptr, true, false);
		return true;
	}

	/**
	 * Bateria externa: JSON con una lista (o {"phrases":[...]}) de frases. Cada una: texto ("frase"/"phrase"/"text"),
	 * "must" (tipos que deben estar; como objeto = cantidades exactas), "forbid"/"must_not", "counts" {Tipo: n} y
	 * "capability" (canal o lista de canales que deben responder: conducir, disparar, volar, turbo, encender).
	 */
	bool LoadBattery(const FString& Path, TArray<FJamCase>& Out, FString& Err)
	{
		FString Json;
		TSharedPtr<FJsonValue> Root;
		if (!FFileHelper::LoadFileToString(Json, *Path) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Root) || !Root.IsValid())
		{
			Err = TEXT("no se pudo leer o no es JSON: ") + Path;
			return false;
		}
		const TArray<TSharedPtr<FJsonValue>>* List = nullptr;
		if (Root->Type == EJson::Array) { List = &Root->AsArray(); }
		else if (Root->Type == EJson::Object)
		{
			for (const TCHAR* K : { TEXT("phrases"), TEXT("frases"), TEXT("cases"), TEXT("tests") })
			{
				if (!List) { Root->AsObject()->TryGetArrayField(K, List); }
			}
		}
		if (!List) { Err = TEXT("el JSON no trae una lista de frases"); return false; }
		auto Strings = [](const TSharedPtr<FJsonObject>& O, std::initializer_list<const TCHAR*> Keys, TArray<FString>& To)
		{
			for (const TCHAR* K : Keys)
			{
				const TSharedPtr<FJsonValue> V = O->TryGetField(K);
				if (!V.IsValid()) { continue; }
				if (V->Type == EJson::String) { To.Add(V->AsString()); }
				else if (V->Type == EJson::Array) { for (const TSharedPtr<FJsonValue>& X : V->AsArray()) { if (X->Type == EJson::String) { To.Add(X->AsString()); } } }
			}
		};
		for (const TSharedPtr<FJsonValue>& V : *List)
		{
			FJamCase C;
			if (V->Type == EJson::String) { C.Text = V->AsString(); }
			else if (V->Type == EJson::Object)
			{
				const TSharedPtr<FJsonObject> O = V->AsObject();
				for (const TCHAR* K : { TEXT("frase"), TEXT("phrase"), TEXT("text"), TEXT("prompt") }) { if (C.Text.IsEmpty()) { O->TryGetStringField(K, C.Text); } }
				Strings(O, { TEXT("must") }, C.Must);
				Strings(O, { TEXT("forbid"), TEXT("must_not") }, C.Forbid);
				Strings(O, { TEXT("capability"), TEXT("capabilities"), TEXT("caps") }, C.Caps);
				TArray<FString> Counts;
				for (const TCHAR* K : { TEXT("counts"), TEXT("must") })
				{
					const TSharedPtr<FJsonObject>* Obj = nullptr;
					if (O->TryGetObjectField(K, Obj))
					{
						for (const auto& KV : (*Obj)->Values)
						{
							double Num = 0;
							if (KV.Value.IsValid() && KV.Value->TryGetNumber(Num)) { Counts.Add(FString::Printf(TEXT("%s=%d"), *FString(KV.Key), (int32)Num)); }
						}
					}
				}
				C.Expect = FString::Join(Counts, TEXT(","));
			}
			if (!C.Text.IsEmpty()) { Out.Add(C); }
		}
		return Out.Num() > 0;
	}

	void JamTest(const TArray<FString>& Args, UWorld* World)
	{
		// Jam.Test [segundos] [iso | frases | ruta.json [desde] [cuantas]]
		if (!World || !World->IsGameWorld())
		{
			Screen(TEXT("Jam.Test: correr dentro del juego (PIE)"), FColor::Red);
			return;
		}
		FTSTicker::GetCoreTicker().RemoveTicker(GTest.Handle);
		GTest = FJamTestRun();
		GTest.World = World;
		GTest.At = SayTransform(World);
		GTest.Duration = 12.f;
		int32 Arg = 0;
		if (Args.Num() > 0 && Args[0].IsNumeric()) { GTest.Duration = FMath::Clamp(FCString::Atof(*Args[0]), 3.f, 60.f); Arg = 1; }
		// La ruta puede traer espacios (la consola la parte en varios argumentos): se une hasta el que termina en .json.
		FString Mode = Args.Num() > Arg ? Args[Arg] : FString();
		while (!Mode.EndsWith(TEXT(".json")) && Mode.Contains(TEXT("/")) && Arg + 1 < Args.Num()) { Mode += TEXT(" ") + Args[++Arg]; }
		GCases.Reset();
		GBatteryName = TEXT("interna (aisladas + frases del POC)");
		GReportPath = FPaths::ProjectDir() / TEXT("docs/building/REPORTE-POC.md");
		if (Mode.EndsWith(TEXT(".json")))
		{
			TArray<FJamCase> All;
			FString Err;
			if (!LoadBattery(Mode, All, Err))
			{
				Screen(TEXT("Jam.Test: ") + Err, FColor::Red);
				return;
			}
			const int32 From = Args.Num() > Arg + 1 ? FMath::Clamp(FCString::Atoi(*Args[Arg + 1]), 0, All.Num()) : 0;
			const int32 Count = Args.Num() > Arg + 2 ? FCString::Atoi(*Args[Arg + 2]) : All.Num();
			for (int32 i = From; i < All.Num() && i < From + Count; ++i) { GCases.Add(All[i]); }
			GBatteryName = FString::Printf(TEXT("`%s` (frases %d a %d de %d)"), *FPaths::GetCleanFilename(Mode), From, From + GCases.Num() - 1, All.Num());
			GReportPath = FPaths::ProjectDir() / TEXT("docs/building/REPORTE-BATERIA.md");
		}
		else
		{
			for (const FJamCaseDef& D : BuiltinCases)
			{
				FJamCase C;
				C.Key = D.Key ? D.Key : TEXT(""); C.Text = D.Text; C.Recipe = D.Recipe ? D.Recipe : TEXT(""); C.Expect = D.Expect ? D.Expect : TEXT("");
				if ((Mode == TEXT("iso") && !C.IsIso()) || (Mode == TEXT("frases") && C.IsIso())) { continue; }
				GCases.Add(C);
			}
		}
		if (const APlayerController* PC = World->GetFirstPlayerController())
		{
			if (const APawn* Me = PC->GetPawn()) { GTest.Home = Me->GetActorTransform(); }
		}
		GTest.Timer = 1000.f; // arranca ya con el primer caso
		GTest.Handle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateStatic(&TestTick));
	}

	/** Jam.Coh: las ocho notas de coherencia de la ultima construccion. */
	void JamCoh(const TArray<FString>&, UWorld*)
	{
		AJamBuildSpawner* S = GLastSay.Get();
		FBuildRecipe R;
		FString Err;
		if (!S || !UBuildRecipeLibrary::ParseRecipeJson(S->LastRecipeJson, R, Err))
		{
			Screen(TEXT("Jam.Coh: no hay construccion"), FColor::Red);
			return;
		}
		const FJamCoherence C = UJamAssembler::Coherence(R);
		const FString Msg = FString::Printf(TEXT("Coherencia %d/100 | conexion %.2f solapes %.2f simetria %.2f apoyo %.2f empuje %.2f tiro %.2f masa %.2f potencia %.2f | %s"),
			C.Score, C.Sub[0], C.Sub[1], C.Sub[2], C.Sub[3], C.Sub[4], C.Sub[5], C.Sub[6], C.Sub[7], *C.ToText());
		Screen(Msg, C.Score >= 85 ? FColor::Green : FColor::Orange);
		UE_LOG(LogTemp, Display, TEXT("JamCoh %s"), *Msg);
	}

	void JamSeat(const TArray<FString>&, UWorld* World)
	{
		APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
		ACharacter* Me = PC ? Cast<ACharacter>(PC->GetPawn()) : nullptr;
		if (!Me)
		{
			Screen(TEXT("No puedes subirte ahora"), FColor::Orange);
			return;
		}
		if (Cast<AJamBuildPart>(Me->GetAttachParentActor()))
		{
			const AJamBuildPart* Seat = Cast<AJamBuildPart>(Me->GetAttachParentActor());
			const FVector Side = FVector(Seat->GetActorRightVector().X, Seat->GetActorRightVector().Y, 0).GetSafeNormal();
			const FVector Vel = Seat->Mesh->GetPhysicsLinearVelocity();
			Me->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
			Me->SetActorRotation(FRotator(0, Me->GetActorRotation().Yaw, 0));
			Me->AddActorWorldOffset(Side * 150.f + FVector(0, 0, 100));
			Me->SetActorEnableCollision(true);
			Me->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
			Me->GetCharacterMovement()->Velocity = Vel;
			Screen(TEXT("Te bajaste"));
			return;
		}
		// El asiento mas cercano de TU construccion (GLastSay); si no hay asiento, la pieza mas cercana de ella. Las de otros no cuentan.
		AJamBuildPart* Best = nullptr;
		double BestD = FMath::Square(1500.0);
		if (const AJamBuildSpawner* Mine = GLastSay.Get())
		{
			for (AJamBuildPart* P : Mine->GetParts())
			{
				if (!IsValid(P)) { continue; }
				const double D = FVector::DistSquared(P->GetActorLocation(), Me->GetActorLocation()) * (P->Kind == EJamPartKind::Seat ? 0.01 : 1.0);
				if (D < BestD)
				{
					BestD = D;
					Best = P;
				}
			}
		}
		if (!Best)
		{
			Screen(TEXT("No hay construccion tuya cerca. Pulsa B y pide una"), FColor::Orange);
			return;
		}
		Me->GetCharacterMovement()->SetMovementMode(MOVE_None);
		Me->SetActorEnableCollision(false);
		Me->SetActorLocation(Best->GetActorLocation() + FVector(0, 0, 110));
		Me->AttachToActor(Best, FAttachmentTransformRules::KeepWorldTransform);
		Screen(TEXT("Subiste. W/S acelera y frena, A/D gira, E para bajar"), FColor::Green);
	}

	void SayFromGame(UWorld* World, const FString& Phrase);

	/** Resultado de la voz: muestra lo que se entendio y construye (o avisa el motivo del fallo). */
	void OnVoiceResult(TWeakObjectPtr<UWorld> World, bool bOk, const FString& Text)
	{
		if (!World.IsValid())
		{
			return;
		}
		if (!bOk)
		{
			Screen(Text, FColor::Red);
			return;
		}
		Screen(FString::Printf(TEXT("Te escuche: \"%s\""), *Text));
		SayFromGame(World.Get(), Text);
	}

	void JamVoiceFile(const TArray<FString>& Args, UWorld* World)
	{
		TArray<uint8> Wav;
		const FString Path = FString::Join(Args, TEXT(" "));
		if (!World || !FFileHelper::LoadFileToArray(Wav, *Path))
		{
			Screen(TEXT("Uso: Jam.VoiceFile <ruta a un .wav>   (prueba de voz sin microfono)"), FColor::Red);
			return;
		}
		TWeakObjectPtr<UWorld> W(World);
		FJamVoice::TranscribeWav(Wav, [W](bool bOk, const FString& Text) { OnVoiceResult(W, bOk, Text); });
	}

	void JamType(const TArray<FString>& Args, UWorld* World)
	{
		// Lo mismo que escribir en la caja de texto (T) y pulsar Enter: la construccion sale apagada.
		if (World) { SayFromGame(World, FString::Join(Args, TEXT(" "))); }
	}

	UInventoryComponent* PlayerInventory(UWorld* World)
	{
		const APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
		return PC && PC->GetPawn() ? PC->GetPawn()->FindComponentByClass<UInventoryComponent>() : nullptr;
	}

	// Jam.Inv: lista las piezas del jugador. Jam.Give <Tipo> [n] (n negativo quita) | Jam.Give clear.
	void JamInv(const TArray<FString>&, UWorld* World)
	{
		UInventoryComponent* Inv = PlayerInventory(World);
		if (!Inv) { Screen(TEXT("Jam.Inv: el jugador no tiene inventario")); return; }
		TArray<FString> Lines;
		for (const auto& Item : Inv->Items) { if (Item.Value > 0) { Lines.Add(FString::Printf(TEXT("%s x%d"), *Item.Key.ToString(), Item.Value)); } }
		Lines.Sort();
		const FString Text = Lines.Num() ? FString::Join(Lines, TEXT(", ")) : TEXT("(vacio)");
		UE_LOG(LogTemp, Warning, TEXT("Jam.Inv: %s"), *Text);
		Screen(TEXT("Inventario: ") + Text, FColor::Cyan);
	}

	void JamGive(const TArray<FString>& Args, UWorld* World)
	{
		UInventoryComponent* Inv = PlayerInventory(World);
		if (!Inv || Args.Num() == 0) { Screen(TEXT("Uso: Jam.Give <Tipo> [n]  (ej. Jam.Give Wheel 2; n negativo quita; Jam.Give clear)")); return; }
		if (Args[0] == TEXT("clear")) { Inv->ClearInventory(); JamInv(Args, World); return; }
		if (Args[0] == TEXT("all"))
		{
			// Para probar: n de cada pieza conocida (ponytail: lista fija; si se agrega pieza nueva, sumarla aqui).
			static const TCHAR* All[] = { TEXT("Plank"), TEXT("Panel"), TEXT("Wall"), TEXT("Beam"), TEXT("Girder"), TEXT("Block"), TEXT("Cylinder"), TEXT("Ball"), TEXT("Ramp"), TEXT("Rod"), TEXT("Deck"), TEXT("UBlock"), TEXT("IceBlock"),
				TEXT("Hinge"), TEXT("Turntable"), TEXT("BallJoint"), TEXT("Stake"), TEXT("Rope"), TEXT("Wheel"), TEXT("BigWheel"), TEXT("WagonWheel"), TEXT("Cart"), TEXT("Sled"), TEXT("RailSkid"), TEXT("Roller"), TEXT("Spring"),
				TEXT("Fan"), TEXT("Propeller"), TEXT("Rocket"), TEXT("Jet"), TEXT("Turbine"), TEXT("Wing"), TEXT("Sail"), TEXT("Balloon"), TEXT("Hover"), TEXT("HoverStone"), TEXT("Stabilizer"), TEXT("Seat"), TEXT("Head"), TEXT("Bomb"),
				TEXT("Battery"), TEXT("BigBattery"), TEXT("Flamethrower"), TEXT("Frost"), TEXT("Shock"), TEXT("Cannon"), TEXT("Hydrant"), TEXT("Light"), TEXT("Mirror"), TEXT("Launcher"), TEXT("Spikes") };
			const int32 Each = Args.Num() > 1 ? FCString::Atoi(*Args[1]) : 10;
			for (const TCHAR* P : All) { Inv->AddItem(FName(*(FString(TEXT("Part.")) + P)), Each); }
			JamInv(Args, World);
			return;
		}
		const FName Id(*(Args[0].StartsWith(TEXT("Part.")) || Args[0].StartsWith(TEXT("Mat.")) ? Args[0] : TEXT("Part.") + Args[0]));
		const int32 N = Args.Num() > 1 ? FCString::Atoi(*Args[1]) : 1;
		if (N >= 0) { Inv->AddItem(Id, N); }
		else { Inv->RemoveItem(Id, FMath::Min(-N, Inv->GetCount(Id))); }
		JamInv(Args, World);
	}

	void JamPower(const TArray<FString>&, UWorld*)
	{
		// Lo mismo que la tecla C.
		if (AJamBuildSpawner* S = GLastSay.Get()) { S->SetBuildActive(!S->IsBuildActive()); }
	}

	// Jam.Drive <acelerador> <giro> [segundos]: simula W/S y A/D estando montado (para pruebas sin teclado).
	void JamDrive(const TArray<FString>& Args, UWorld*)
	{
		GDriveThrottle = Args.Num() > 0 ? FCString::Atof(*Args[0]) : 0.f;
		GDriveSteer = Args.Num() > 1 ? FCString::Atof(*Args[1]) : 0.f;
		GDriveUntil = FPlatformTime::Seconds() + (Args.Num() > 2 ? FCString::Atof(*Args[2]) : 3.f);
	}

	FAutoConsoleCommandWithWorldAndArgs CmdVoiceFile(TEXT("Jam.VoiceFile"), TEXT("Jam.VoiceFile <ruta.wav>: transcribe ese audio y construye lo que diga (prueba de voz sin microfono)."), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&JamVoiceFile));
	FAutoConsoleCommandWithWorldAndArgs CmdType(TEXT("Jam.Type"), TEXT("Jam.Type <frase>: como escribirla en la caja de texto (sale apagada; se enciende con C o Jam.Power)."), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&JamType));
	FAutoConsoleCommandWithWorldAndArgs CmdInv(TEXT("Jam.Inv"), TEXT("Muestra las piezas del inventario del jugador."), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&JamInv));
	FAutoConsoleCommandWithWorldAndArgs CmdGive(TEXT("Jam.Give"), TEXT("Jam.Give <Tipo> [n]: agrega (o quita con n negativo) piezas al inventario; Jam.Give clear lo vacia."), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&JamGive));
	FAutoConsoleCommandWithWorldAndArgs CmdPower(TEXT("Jam.Power"), TEXT("Prende o apaga la ultima construccion (tecla C)."), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&JamPower));
	void JamFire(const TArray<FString>& Args, UWorld*)
	{
		// Jam.Fire [segundos]: simula mantener el clic (armas) estando montado. Jam.Fire boost: simula Espacio (cohetes, bombas).
		if (Args.Num() > 0 && Args[0] == TEXT("boost")) { GBoostOnce = true; return; }
		GFireUntil = FPlatformTime::Seconds() + (Args.Num() > 0 ? FCString::Atof(*Args[0]) : 3.f);
	}
	FAutoConsoleCommandWithWorldAndArgs CmdFire(TEXT("Jam.Fire"), TEXT("Jam.Fire [segundos] | Jam.Fire boost: simula el clic (armas) o Espacio (cohetes) estando montado."), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&JamFire));
	FAutoConsoleCommandWithWorldAndArgs CmdDrive(TEXT("Jam.Drive"), TEXT("Jam.Drive <acelerador -1..1> <giro -1..1> [segundos]: simula W/S y A/D estando montado."), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&JamDrive));
	FAutoConsoleCommandWithWorldAndArgs CmdSay(TEXT("Jam.Say"), TEXT("Jam.Say <texto libre>: construye lo que pidas delante del jugador (modelo de decision Jev; respaldo por palabras clave)."), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&JamSay));
	FAutoConsoleCommandWithWorldAndArgs CmdClear(TEXT("Jam.Clear"), TEXT("Desarma tu construccion (tecla X) y te devuelve las piezas que siguen vivas."), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&JamClear));
	FAutoConsoleCommandWithWorldAndArgs CmdClearAll(TEXT("Jam.ClearAll"), TEXT("Borra todas las construcciones y piezas del mapa, tambien las de los bots (pruebas)."), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&JamClearAll));
	FAutoConsoleCommandWithWorldAndArgs CmdEnergy(TEXT("Jam.Energy"), TEXT("Jam.Energy [valor]: muestra (o fija) la barra de energia del jugador."), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&JamEnergy));
	FAutoConsoleCommandWithWorldAndArgs CmdTest(TEXT("Jam.Test"), TEXT("Jam.Test [segundos] [iso | frases | ruta.json [desde] [cuantas]]: bateria con coherencia y 'utilizable' (entrada simulada); escribe docs/building/REPORTE-POC.md (o REPORTE-BATERIA.md con JSON externo)."), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&JamTest));
	FAutoConsoleCommandWithWorldAndArgs CmdCoh(TEXT("Jam.Coh"), TEXT("Imprime las ocho notas de coherencia (sentido) de la ultima construccion."), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&JamCoh));
	FAutoConsoleCommandWithWorldAndArgs CmdSeat(TEXT("Jam.Seat"), TEXT("Sube al asiento de la construccion mas cercana, o baja."), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&JamSeat));
}

// ---------------------------------------------------------------------------------------------
// Personaje, caja de texto y HUD del banco de pruebas
// ---------------------------------------------------------------------------------------------

namespace
{
	/** Pedido hecho jugando (caja de texto o tecla 1..9): la construccion sale APAGADA y se enciende con C (GDD 6.3). */
	void SayFromGame(UWorld* World, const FString& Phrase)
	{
		SpawnSay(World, Phrase, SayTransform(World), nullptr, false);
	}

	const TCHAR* QuickPhrases[9] = {
		TEXT("un carro que queme todo"), TEXT("un tanque"), TEXT("una torreta con laser"), TEXT("un globo aerostatico con fuego debajo"),
		TEXT("un hovercraft con vela y emisor de hielo"), TEXT("un avion con alas"), TEXT("una catapulta"), TEXT("un dron con un faro"),
		TEXT("una rueda sola con lanzallamas"),
	};

	// Paleta del juego (docs/ART.md): fondo oscuro, acento amarillo "loot", verde pasto, rojo equipo.
	const FLinearColor ColPanel(0.05f, 0.07f, 0.10f, 0.78f);
	const FLinearColor ColAccent(0.95f, 0.76f, 0.19f, 1.f);
	const FLinearColor ColText(0.96f, 0.96f, 0.93f, 1.f);
	const FLinearColor ColDim(0.70f, 0.74f, 0.78f, 1.f);
	const FLinearColor ColGood(0.66f, 0.80f, 0.29f, 1.f);
	const FLinearColor ColBad(0.79f, 0.26f, 0.28f, 1.f);
	const FLinearColor ColIce(0.56f, 0.89f, 1.0f, 1.f);

	const TCHAR* KindName(EJamPartKind K)
	{
		switch (K)
		{
		case EJamPartKind::Wheel: return TEXT("Ruedas (motor)");
		case EJamPartKind::Fan: return TEXT("Helice / jet (empuje)");
		case EJamPartKind::Rocket: return TEXT("Cohete");
		case EJamPartKind::Balloon: return TEXT("Globo");
		case EJamPartKind::Cannon: return TEXT("Cañon");
		case EJamPartKind::Battery: return TEXT("Bateria");
		case EJamPartKind::Flame: return TEXT("Lanzallamas");
		case EJamPartKind::Wing: return TEXT("Alas");
		case EJamPartKind::Frost: return TEXT("Hielo");
		case EJamPartKind::Beam: return TEXT("Laser");
		case EJamPartKind::Bomb: return TEXT("Bomba");
		case EJamPartKind::Spring: return TEXT("Resorte");
		case EJamPartKind::Sail: return TEXT("Vela");
		case EJamPartKind::Hover: return TEXT("Piedra flotante");
		case EJamPartKind::Stabilizer: return TEXT("Estabilizador");
		case EJamPartKind::Shock: return TEXT("Rayo electrico");
		case EJamPartKind::Hydrant: return TEXT("Manguera");
		case EJamPartKind::Light: return TEXT("Foco");
		case EJamPartKind::Head: return TEXT("Cabeza que apunta");
		case EJamPartKind::Turntable: return TEXT("Plato giratorio");
		case EJamPartKind::Hinge: return TEXT("Bisagra");
		case EJamPartKind::Stake: return TEXT("Estaca (ancla)");
		case EJamPartKind::Spike: return TEXT("Puas (dañan al chocar)");
		default: return nullptr; // estructura, asiento, trineo: sin estado
		}
	}
}

static TAutoConsoleVariable<float> CVarJamMouseSens(TEXT("jam.Mouse.Sens"), 0.12f, TEXT("Sensibilidad del mouse: grados por unidad de movimiento (0.05 - 0.25)."));
static TAutoConsoleVariable<int32> CVarJamMouseInvertY(TEXT("jam.Mouse.InvertY"), 0, TEXT("1 = invertir el eje vertical del mouse y del stick."));
static TAutoConsoleVariable<float> CVarJamShake(TEXT("jam.Shake"), 1.f, TEXT("Intensidad de la sacudida de camara (0 = apagada, 1 = normal)."));
static TAutoConsoleVariable<int32> CVarJamHudDebug(TEXT("jam.Hud.Debug"), 0, TEXT("1 = muestra los paneles de depuracion del HUD: lista de elementos, decision del modelo, sentido, mecanicas y comandos de consola. 0 = HUD limpio (GDD 9)."));

AJamTestCharacter::AJamTestCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	// Camara en tercera persona sobre el hombro derecho (V alterna a primera persona).
	Arm = CreateDefaultSubobject<USpringArmComponent>(TEXT("Arm"));
	Arm->SetupAttachment(GetCapsuleComponent());
	Arm->TargetArmLength = 420.f;
	Arm->SocketOffset = FVector(0, 55, 0);
	Arm->TargetOffset = FVector(0, 0, 70);
	Arm->bUsePawnControlRotation = true;
	Arm->bInheritRoll = false;
	Arm->bEnableCameraLag = true;
	Arm->CameraLagSpeed = 12.f;
	Arm->CameraLagMaxDistance = 120.f;
	Arm->bDoCollisionTest = true;
	Arm->ProbeSize = 16.f;
	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(Arm);
	Camera->bUsePawnControlRotation = false;
	Camera->SetFieldOfView(90.f);

	// Cuerpo provisional: cilindro con "nariz" para ver hacia donde mira.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cyl(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
	Body->SetupAttachment(GetCapsuleComponent());
	Body->SetStaticMesh(Cyl.Object);
	Body->SetRelativeScale3D(FVector(0.6, 0.6, 1.7));
	Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> BodyMat(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	Body->SetMaterial(0, BodyMat.Object);
	Nose = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Nose"));
	Nose->SetupAttachment(GetCapsuleComponent());
	Nose->SetStaticMesh(Cube.Object);
	Nose->SetRelativeLocation(FVector(34, 0, 55));
	Nose->SetRelativeScale3D(FVector(0.25, 0.4, 0.12));
	Nose->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Nose->SetMaterial(0, BodyMat.Object);

	// Movimiento: responde rapido, frena donde sueltas, salto corto y con control en el aire.
	bUseControllerRotationYaw = false;
	UCharacterMovementComponent* Move = GetCharacterMovement();
	Move->bOrientRotationToMovement = true;
	Move->RotationRate = FRotator(0, 720, 0);
	Move->MaxWalkSpeed = 550.f;
	Move->MaxAcceleration = 3000.f;
	Move->BrakingDecelerationWalking = 2800.f;
	Move->BrakingFrictionFactor = 1.f;
	Move->AirControl = 0.4f;
	Move->BrakingDecelerationFalling = 200.f;
	Move->GravityScale = 1.8f;
	Move->JumpZVelocity = 720.f;
	Move->SetWalkableFloorAngle(50.f);
}

void AJamTestCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	if (APlayerController* PC = Cast<APlayerController>(NewController))
	{
		// Captura el mouse al entrar: no hace falta hacer clic en la ventana.
		PC->SetInputMode(FInputModeGameOnly());
		PC->bShowMouseCursor = false;
		FJamVoice::Get().WarmUp(); // deja listo el reconocedor de voz local
		// Al renacer (peon nuevo): la construccion del jugador pasa a ser de este peon, para volver a pedir y desarmar con reembolso.
		if (AJamBuildSpawner* Mine = GLastSay.Get())
		{
			if (Mine->GetOwner() != this) { Mine->RebindOwner(this); }
		}
	}
}

void AJamTestCharacter::Landed(const FHitResult& Hit)
{
	if (GetCharacterMovement()->Velocity.Z < -900.f)
	{
		Trauma = FMath::Min(1.f, Trauma + 0.15f);
	}
	Super::Landed(Hit);
}

void AJamTestCharacter::AddTrauma(UWorld* World, const FVector& Epicenter, float Amount, float InnerRadius, float OuterRadius)
{
	APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	if (AJamTestCharacter* Me = PC ? Cast<AJamTestCharacter>(PC->GetPawn()) : nullptr)
	{
		const float Dist = FVector::Dist(Me->GetActorLocation(), Epicenter);
		const float Falloff = FMath::GetMappedRangeValueClamped(FVector2D(InnerRadius, OuterRadius), FVector2D(1.f, 0.f), Dist);
		Me->Trauma = FMath::Min(1.f, Me->Trauma + Amount * Falloff);
	}
}

void AJamTestCharacter::OpenSayBox()
{
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC || GTyping)
	{
		return;
	}
	if (!SayWidget)
	{
		SayWidget = CreateWidget<UJamSayWidget>(PC, UJamSayWidget::StaticClass());
	}
	if (SayWidget && SayWidget->Box)
	{
		GTyping = true;
		SayWidget->Box->SetText(FText::GetEmpty());
		SayWidget->AddToViewport(10);
		PC->SetInputMode(FInputModeGameAndUI());
		SayWidget->Box->SetKeyboardFocus();
		SayWidget->OpenTime = GetWorld()->GetTimeSeconds();
	}
}

void AJamTestCharacter::DriveMount(AJamBuildPart* Seat, float Throttle, float Steer, bool bFire, bool bBoost, float DeltaSeconds)
{
	// Entrada digital suavizada para que el teclado no de tirones.
	SmoothThrottle = FMath::FInterpTo(SmoothThrottle, Throttle, DeltaSeconds, 4.f);
	SmoothSteer = FMath::FInterpTo(SmoothSteer, Steer, DeltaSeconds, 6.f);
	if (!Seat->Build.IsValid() || Seat->Build->Parts.Num() == 0 || !Seat->Build->Parts[0].IsValid())
	{
		return;
	}
	AJamBuildPart* Root = Seat->Build->Parts[0].Get();
	const FVector Right = Root->GetActorRightVector();
	bool bWheels = false;
	// A velocidad alta, limitar el giro para que la aceleracion lateral no vuelque el vehiculo.
	double HalfTrack = 0.0, Height = 1.0;
	const FVector Com = Root->GetBuildCenterOfMass();
	for (const TWeakObjectPtr<AJamBuildPart>& P : Seat->Build->Parts)
	{
		if (!P.IsValid() || P->Kind != EJamPartKind::Wheel) { continue; }
		HalfTrack = FMath::Max(HalfTrack, FMath::Abs(FVector::DotProduct(P->GetActorLocation() - Com, Right)));
		Height = FMath::Max(Height, Com.Z - P->GetActorLocation().Z + P->Mesh->Bounds.BoxExtent.Z);
	}
	const float MaxYawRate = HalfTrack > 1.0 ? JamDrivePhysics::YawRate(Root->Mesh->GetPhysicsLinearVelocity().Size2D(), HalfTrack, Height) : 1.5f;
	const float DriveSteer = SmoothSteer * MaxYawRate / 1.5f;
	// Controles por CAPACIDAD de las piezas que haya (no por plantilla):
	//   clic = armas mientras se mantiene; Espacio = encender cohetes / armar bombas; W = propulsores que empujan.
	for (const TWeakObjectPtr<AJamBuildPart>& P : Seat->Build->Parts)
	{
		if (!P.IsValid()) { continue; }
		const EJamPartKind Kd = P->Kind;
		const bool bWeapon = Kd == EJamPartKind::Cannon || Kd == EJamPartKind::Flame || Kd == EJamPartKind::Frost || Kd == EJamPartKind::Beam || Kd == EJamPartKind::Shock || Kd == EJamPartKind::Hydrant;
		if (bWeapon && P->bActive != bFire) { P->SetActive(bFire); }
		if (bBoost && (Kd == EJamPartKind::Rocket || Kd == EJamPartKind::Bomb) && !P->bActive) { P->SetActive(true); }
		if (Kd == EJamPartKind::Fan && FMath::Abs(P->GetActorQuat().RotateVector(P->LocalDirection).Z) < 0.5)
		{
			P->SetDrive(FMath::Max(SmoothThrottle, 0.f));
			const bool bPush = SmoothThrottle > 0.01f;
			if (P->bActive != bPush) { P->SetActive(bPush); }
		}
	}
	for (const TWeakObjectPtr<AJamBuildPart>& P : Seat->Build->Parts)
	{
		if (P.IsValid() && P->Kind == EJamPartKind::Wheel && P->DriveConstraint)
		{
			// Giro diferencial: las ruedas de un lado van mas rapido que las del otro.
			bWheels = true;
			const float Side = FVector::DotProduct(P->GetActorLocation() - Root->GetActorLocation(), Right) >= 0.0 ? 1.f : -1.f;
			P->SetDrive(FMath::Clamp(SmoothThrottle - 1.5f * DriveSteer * Side, -1.f, 1.f)); // a tope de giro, el lado interior va en reversa
		}
	}
	// Auto-enderezar: un vehiculo con ruedas que queda de cabeza o de lado, casi quieto, mas de 1 s, se voltea solo
	// (con piloto). Sin esto, un choque o un mal salto dejaban al jugador atorado sin poder hacer nada.
	// No aplica si la raiz es una rueda (monociclo, rueda sola: su eje va de lado por naturaleza) ni en el aire (aviones).
	if (bWheels && Root->Kind != EJamPartKind::Wheel)
	{
		static double GUpsideSince = -1.0;
		const double TNow = Root->GetWorld()->GetTimeSeconds();
		const FVector Up = Root->GetActorUpVector();
		bool bOnGround = false;
		if (Up.Z < 0.3f)
		{
			FCollisionQueryParams Q(SCENE_QUERY_STAT(JamUpright), false);
			for (const TWeakObjectPtr<AJamBuildPart>& P : Seat->Build->Parts) { if (P.IsValid()) { Q.AddIgnoredActor(P.Get()); } }
			Q.AddIgnoredActor(this);
			FHitResult Hit;
			const FVector At = Root->Mesh->GetCenterOfMass();
			bOnGround = Root->GetWorld()->LineTraceSingleByChannel(Hit, At, At - FVector(0, 0, 300.f), ECC_Visibility, Q);
		}
		// Asistencia de estabilidad (en el suelo): el par de reaccion de los motores de rueda levantaba la trompa (caballito)
		// y volteaba al carro hacia atras en cuanto se apretaba W (medido: fwdZ 0.2-0.4 y volcado en 1 s). Pasando de unos
		// grados (desde ~6), un par lo regresa y frena el cabeceo/alabeo; no toca la guiñada ni actua en el aire.
		if (Up.Z >= 0.3f && Up.Z < 0.995f)
		{
			FCollisionQueryParams Q2(SCENE_QUERY_STAT(JamUprightAssist), false);
			for (const TWeakObjectPtr<AJamBuildPart>& P : Seat->Build->Parts) { if (P.IsValid()) { Q2.AddIgnoredActor(P.Get()); } }
			Q2.AddIgnoredActor(this);
			FHitResult Hit2;
			const FVector At2 = Root->Mesh->GetCenterOfMass();
			if (Root->GetWorld()->LineTraceSingleByChannel(Hit2, At2, At2 - FVector(0, 0, 300.f), ECC_Visibility, Q2))
			{
				FVector W2 = Root->Mesh->GetPhysicsAngularVelocityInRadians();
				W2.Z = 0;
				Root->Mesh->AddTorqueInRadians(FVector::CrossProduct(Up, FVector::UpVector) * 120.f - W2 * 15.f, NAME_None, true);
			}
		}
		if (bOnGround && Root->Mesh->GetPhysicsLinearVelocity().Size() < 400.0)
		{
			if (GUpsideSince < 0.0 || TNow - GUpsideSince > 6.0) { GUpsideSince = TNow; }
			if (TNow - GUpsideSince > 1.0)
			{
				FVector W = Root->Mesh->GetPhysicsAngularVelocityInRadians();
				W.Z = 0;
				const FVector Axis = FVector::CrossProduct(Up, FVector::UpVector);
				const FVector AxisN = Axis.IsNearlyZero() ? Root->GetActorForwardVector() : Axis.GetSafeNormal();
				Root->Mesh->AddTorqueInRadians(AxisN * 14.f - W * 3.f, NAME_None, true);
				Root->Mesh->AddForce(FVector(0, 0, FMath::Max(Seat->Build->MassKg, 1.f) * 700.f)); // lo despega un poco para que gire
			}
		}
		else
		{
			GUpsideSince = -1.0;
		}
	}
	// Altura al mando: lo que flota (globos, helices hacia abajo) deja de subir sin control. Sin tecla mantiene la altura;
	// Espacio sube y Ctrl baja. Es una ayuda limitada: hacia arriba da como mucho 0.3 g, asi que no hace volar lo que no vuela.
	bool bLifts = false;
	for (const TWeakObjectPtr<AJamBuildPart>& LiftPart : Seat->Build->Parts)
	{
		if (!LiftPart.IsValid() || !LiftPart->bActive) { continue; }
		const EJamPartKind LiftKind = LiftPart->Kind;
		bLifts |= LiftKind == EJamPartKind::Balloon || ((LiftKind == EJamPartKind::Fan || LiftKind == EJamPartKind::Rocket) && LiftPart->GetActorQuat().RotateVector(LiftPart->LocalDirection).Z > 0.5);
	}
	if (bLifts)
	{
		// W/S inclina el objetivo del estabilizador: la fuerza horizontal sale de las helices inclinadas.
		// Al desmontar expira el objetivo y vuelve a nivelarse; no agrega propulsion a piezas sin empuje.
		if (!bWheels)
		{
			const FVector Forward = FVector::VectorPlaneProject(Root->GetActorForwardVector(), FVector::UpVector).GetSafeNormal();
			Seat->Build->PilotUp = (FVector::UpVector + Forward * SmoothThrottle * 0.3f).GetSafeNormal();
			Seat->Build->PilotTiltUntil = Root->GetWorld()->GetTimeSeconds() + 0.2f;
		}
		const double Vz = Root->Mesh->GetPhysicsLinearVelocity().Z;
		const double WantVz = 350.0 * GLiftInput; // cm/s
		Root->Mesh->AddForce(FVector(0, 0, FMath::Max(Seat->Build->MassKg, 1.f) * FMath::Clamp(12.0 * (WantVz - Vz), -2500.0, 300.0)));
	}
	if (!bWheels || FMath::Abs(SmoothSteer) > 0.05f)
	{
		// Giro garantizado al mando: se controla la VELOCIDAD de guiñada (objetivo 1.5 rad/s con A/D, 0 al soltar) con un par
		// limitado que incluye lo que hace falta para vencer el arrastre lateral de las llantas (crece con el peso y con lo
		// lejos que estan las ruedas) y el de las alas. Medido: con par proporcional solo a la masa, lo largo giraba 2-14 grados.
		double Inertia = 0.0, WheelArm = 0.0;
		int32 WheelN = 0;
		for (const TWeakObjectPtr<AJamBuildPart>& P : Seat->Build->Parts)
		{
			if (!P.IsValid()) { continue; }
			const double D2 = FVector::DistSquared2D(P->GetActorLocation(), Root->GetActorLocation());
			Inertia += P->Mesh->CalculateMass() * D2;
			if (P->Kind == EJamPartKind::Wheel) { WheelArm += FMath::Sqrt(D2); ++WheelN; }
		}
		const double BuildMass = FMath::Max((double)Seat->Build->MassKg, 1.0);
		Inertia = FMath::Max(Inertia, BuildMass * 6400.0);
		const double ScrubArm = FMath::Max(WheelN > 0 ? WheelArm / WheelN : 0.0, 100.0);
		const double MaxTorque = Inertia * 6.0 + BuildMass * 980.0 * 0.8 * ScrubArm;
		const double Wz = Root->Mesh->GetPhysicsAngularVelocityInRadians().Z;
		Root->Mesh->AddTorqueInRadians(FVector(0, 0, FMath::Clamp(Inertia * 20.0 * (MaxYawRate * SmoothSteer - Wz), -MaxTorque, MaxTorque)));
	}
}

void AJamTestCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	APlayerController* PC = Cast<APlayerController>(GetController());
	UWorld* World = GetWorld();
	if (!PC || !World)
	{
		return;
	}
	const float Now = World->GetTimeSeconds();
	UCharacterMovementComponent* Move = GetCharacterMovement();
	AJamBuildPart* Seat = Cast<AJamBuildPart>(GetAttachParentActor());

	// Al bajarse: las ruedas vuelven a su marcha normal.
	if (Mount.IsValid() && Mount.Get() != Seat && Mount->Build.IsValid())
	{
		for (const TWeakObjectPtr<AJamBuildPart>& P : Mount->Build->Parts) { if (P.IsValid()) { P->SetDrive(1.f); } }
	}
	Mount = Seat;

	// ---- Entrada (ponytail: teclas y mando leidos directo, sin assets de Enhanced Input; el personaje real los traera) ----
	float Fwd = 0.f, Strafe = 0.f, LookX = 0.f, LookY = 0.f;
	bool bJumpPressed = false, bJumpReleased = false;
	if (!GTyping)
	{
		float MX = 0.f, MY = 0.f;
		PC->GetInputMouseDelta(MX, MY);
		const float Sens = CVarJamMouseSens.GetValueOnGameThread();
		LookX = MX * Sens;
		LookY = MY * Sens;
		Fwd = (PC->IsInputKeyDown(EKeys::W) ? 1.f : 0.f) - (PC->IsInputKeyDown(EKeys::S) ? 1.f : 0.f);
		Strafe = (PC->IsInputKeyDown(EKeys::D) ? 1.f : 0.f) - (PC->IsInputKeyDown(EKeys::A) ? 1.f : 0.f);
		// Mando: zona muerta radial y reescalado; el stick derecho es velocidad (grados por segundo).
		float LX = 0.f, LY = 0.f, RX = 0.f, RY = 0.f;
		PC->GetInputAnalogStickState(EControllerAnalogStick::CAS_LeftStick, LX, LY);
		PC->GetInputAnalogStickState(EControllerAnalogStick::CAS_RightStick, RX, RY);
		const float LM = FMath::Sqrt(LX * LX + LY * LY);
		if (LM > 0.20f)
		{
			const float K = FMath::Min(1.f, (LM - 0.20f) / 0.80f) / LM;
			Strafe += LX * K;
			Fwd += LY * K;
		}
		const float RM = FMath::Sqrt(RX * RX + RY * RY);
		if (RM > 0.15f)
		{
			const float K = FMath::Square(FMath::Min(1.f, (RM - 0.15f) / 0.85f)) / RM;
			LookX += RX * K * 180.f * DeltaSeconds;
			LookY += RY * K * 110.f * DeltaSeconds;
		}
		Fwd = FMath::Clamp(Fwd, -1.f, 1.f);
		Strafe = FMath::Clamp(Strafe, -1.f, 1.f);
		if (FPlatformTime::Seconds() < GDriveUntil) { Fwd = GDriveThrottle; Strafe = GDriveSteer; } // Jam.Drive (pruebas)

		// Voz: mantener G para hablar; al soltar se transcribe y se construye.
		FJamVoice& Voice = FJamVoice::Get();
		if (PC->WasInputKeyJustPressed(EKeys::G) && !Voice.IsRecording())
		{
			FString Why;
			if (!Voice.Start(Why)) { Screen(Why, FColor::Red); Voice.bLastOk = false; Voice.LastText = Why; Voice.LastTime = FPlatformTime::Seconds(); }
		}
		if (Voice.IsRecording() && !PC->IsInputKeyDown(EKeys::G))
		{
			TWeakObjectPtr<UWorld> W(World);
			Voice.StopAndTranscribe([W](bool bOk, const FString& Text) { OnVoiceResult(W, bOk, Text); });
		}
		bJumpPressed = PC->WasInputKeyJustPressed(EKeys::SpaceBar) || PC->WasInputKeyJustPressed(EKeys::Gamepad_FaceButton_Bottom);
		bJumpReleased = PC->WasInputKeyJustReleased(EKeys::SpaceBar) || PC->WasInputKeyJustReleased(EKeys::Gamepad_FaceButton_Bottom);
	}
	if (CVarJamMouseInvertY.GetValueOnGameThread() != 0) { LookY = -LookY; }

	// ---- Camara: el control gira con mouse/stick; pitch limitado segun la vista ----
	FRotator R = PC->GetControlRotation();
	R.Yaw += LookX;
	R.Pitch = FMath::Clamp(FRotator::NormalizeAxis(R.Pitch) + LookY, bFirstPerson ? -85.f : -70.f, bFirstPerson ? 85.f : 50.f);
	R.Roll = 0.f;
	PC->SetControlRotation(R);

	// Brazo: a pie sobre el hombro; montado mas lejos, centrado y con mas retraso (absorbe los botes de la fisica).
	const float ArmTarget = bFirstPerson ? 0.f : Seat ? 650.f : 420.f;
	Arm->TargetArmLength = FMath::FInterpTo(Arm->TargetArmLength, ArmTarget, DeltaSeconds, 3.f);
	Arm->SocketOffset = FMath::VInterpTo(Arm->SocketOffset, (bFirstPerson || Seat) ? FVector::ZeroVector : FVector(0, 55, 0), DeltaSeconds, 3.f);
	Arm->TargetOffset = FMath::VInterpTo(Arm->TargetOffset, bFirstPerson ? FVector(0, 0, 64) : Seat ? FVector(0, 0, 120) : FVector(0, 0, 70), DeltaSeconds, 3.f);
	Arm->CameraLagSpeed = Seat ? 6.f : 12.f;
	Arm->CameraLagMaxDistance = Seat ? 250.f : 120.f;
	Arm->bEnableCameraLag = !bFirstPerson;
	Arm->bDoCollisionTest = !bFirstPerson;

	// ---- Movimiento ----
	if (Seat)
	{
		const bool bFire = !GTyping && (PC->IsInputKeyDown(EKeys::LeftMouseButton) || PC->GetInputAnalogKeyState(EKeys::Gamepad_RightTriggerAxis) > 0.5f || FPlatformTime::Seconds() < GFireUntil);
		GLiftInput = GTyping ? 0.f : ((PC->IsInputKeyDown(EKeys::SpaceBar) || FPlatformTime::Seconds() < GLiftUntil) ? 1.f : 0.f) - (PC->IsInputKeyDown(EKeys::LeftControl) ? 1.f : 0.f);
		DriveMount(Seat, Fwd, Strafe, bFire, bJumpPressed || GBoostOnce || (!GTyping && PC->WasInputKeyJustPressed(EKeys::LeftShift)), DeltaSeconds);
		GBoostOnce = false;
	}
	else
	{
		SmoothThrottle = SmoothSteer = 0.f;
		const FRotator Yaw(0, R.Yaw, 0);
		AddMovementInput(Yaw.Vector(), Fwd);
		AddMovementInput(FRotationMatrix(Yaw).GetUnitAxis(EAxis::Y), Strafe);

		// Salto con margen: coyote (0.12 s tras salir del borde), buffer (0.12 s antes de tocar suelo) y corte al soltar.
		if (Move->IsMovingOnGround()) { LastGroundTime = Now; }
		if (bJumpPressed) { JumpBufferedAt = Now; }
		if (JumpBufferedAt >= 0.f && LastGroundTime >= 0.f && Now - JumpBufferedAt < 0.12f && Now - LastGroundTime < 0.12f)
		{
			LaunchCharacter(FVector(0, 0, Move->JumpZVelocity), false, true);
			JumpBufferedAt = LastGroundTime = -1.f;
		}
		if (bJumpReleased && Move->IsFalling() && Move->Velocity.Z > 0.f) { Move->Velocity.Z *= 0.5f; }
	}

	// ---- Sensacion: sacudida por trauma y mas campo de vision con la velocidad ----
	const float Speed = Seat ? Seat->Mesh->GetPhysicsLinearVelocity().Size() : GetVelocity().Size();
	FovExtra = FMath::FInterpTo(FovExtra, FMath::GetMappedRangeValueClamped(FVector2D(800.f, 2500.f), FVector2D(0.f, 10.f), Speed), DeltaSeconds, 4.f);
	for (const TWeakObjectPtr<AJamBuildPart>& P : Seat && Seat->Build.IsValid() ? Seat->Build->Parts : TArray<TWeakObjectPtr<AJamBuildPart>>())
	{
		if (P.IsValid() && P->Kind == EJamPartKind::Rocket && P->bActive) { Trauma = FMath::Max(Trauma, 0.15f); }
	}
	Trauma = FMath::Max(0.f, Trauma - 1.5f * DeltaSeconds);
	const float Shake = Trauma * Trauma * FMath::Clamp(CVarJamShake.GetValueOnGameThread(), 0.f, 1.f);
	Camera->ClearAdditiveOffset();
	Camera->AddAdditiveOffset(FTransform(
		FRotator(1.2f * Shake * FMath::Sin(Now * 157.f), 1.2f * Shake * FMath::Sin(Now * 139.f + 1.3f), 0.8f * Shake * FMath::Sin(Now * 171.f + 2.1f)),
		FVector(0, 4.f * Shake * FMath::Sin(Now * 149.f + 0.7f), 4.f * Shake * FMath::Sin(Now * 163.f))), FovExtra);

	if (GTyping)
	{
		return;
	}
	// ---- Atajos: T/B/Enter = escribir; 1..9 = ejemplos; C = encender; X = limpiar; E = subir/bajar; V = vista; H = ayuda ----
	const FKey Digits[9] = { EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four, EKeys::Five, EKeys::Six, EKeys::Seven, EKeys::Eight, EKeys::Nine };
	for (int32 i = 0; i < 9; ++i)
	{
		if (PC->WasInputKeyJustPressed(Digits[i]))
		{
			SayFromGame(World, QuickPhrases[i]);
		}
	}
	if (PC->WasInputKeyJustPressed(EKeys::T) || PC->WasInputKeyJustPressed(EKeys::B) || PC->WasInputKeyJustPressed(EKeys::Enter)) { OpenSayBox(); }
	if (PC->WasInputKeyJustPressed(EKeys::C) || PC->WasInputKeyJustPressed(EKeys::Gamepad_RightShoulder))
	{
		// C = prender o apagar toda la construccion (GDD 6.3).
		if (AJamBuildSpawner* S = GLastSay.Get()) { S->SetBuildActive(!S->IsBuildActive()); }
	}
	if (PC->WasInputKeyJustPressed(EKeys::X)) { JamClear(TArray<FString>(), World); }
	if (PC->WasInputKeyJustPressed(EKeys::E) || PC->WasInputKeyJustPressed(EKeys::Gamepad_FaceButton_Left)) { JamSeat(TArray<FString>(), World); }
	if (PC->WasInputKeyJustPressed(EKeys::V))
	{
		bFirstPerson = !bFirstPerson;
		Body->SetOwnerNoSee(bFirstPerson);
		Nose->SetOwnerNoSee(bFirstPerson);
		Move->bOrientRotationToMovement = !bFirstPerson;
		bUseControllerRotationYaw = bFirstPerson;
	}
	if (PC->WasInputKeyJustPressed(EKeys::H))
	{
		if (AJamTestHUD* HUD = Cast<AJamTestHUD>(PC->GetHUD())) { HUD->bShowHelp = !HUD->bShowHelp; }
	}
}

AJamTestGameMode::AJamTestGameMode()
{
	DefaultPawnClass = AJamTestCharacter::StaticClass();
	HUDClass = AJamTestHUD::StaticClass();
}

bool UJamSayWidget::Initialize()
{
	const bool bOk = Super::Initialize();
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
		WidgetTree->RootWidget = Root;
		Box = WidgetTree->ConstructWidget<UMultiLineEditableTextBox>(UMultiLineEditableTextBox::StaticClass(), TEXT("SayBox"));
		Box->SetHintText(FText::FromString(TEXT("Que quieres construir? (Enter = construir, Esc = cerrar)   ej: un carro que queme todo")));
		Box->OnTextChanged.AddDynamic(this, &UJamSayWidget::OnChanged);
		if (UCanvasPanelSlot* CanvasSlot = Root->AddChildToCanvas(Box))
		{
			CanvasSlot->SetAnchors(FAnchors(0.5f, 0.82f));
			CanvasSlot->SetAlignment(FVector2D(0.5f, 0.5f));
			CanvasSlot->SetPosition(FVector2D(0, 0));
			CanvasSlot->SetSize(FVector2D(820, 46));
		}
	}
	return bOk;
}

void UJamSayWidget::OnChanged(const FText& Text)
{
	const FString S = Text.ToString();
	if (GTyping && (S.Contains(TEXT("\n")) || S.Contains(TEXT("\r"))))
	{
		Close(S.Replace(TEXT("\n"), TEXT(" ")).Replace(TEXT("\r"), TEXT(" ")).TrimStartAndEnd());
	}
}

void UJamSayWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	// Esc (o clic fuera) le quita el foco a la caja: se cierra sin construir.
	if (GTyping && Box && !Box->HasKeyboardFocus() && GetWorld() && GetWorld()->GetTimeSeconds() - OpenTime > 0.3f)
	{
		Close(FString());
	}
}

void UJamSayWidget::Close(const FString& Phrase)
{
	GTyping = false;
	RemoveFromParent();
	if (APlayerController* PC = GetOwningPlayer())
	{
		PC->SetInputMode(FInputModeGameOnly());
		if (!Phrase.IsEmpty())
		{
			SayFromGame(PC->GetWorld(), Phrase);
		}
	}
}

float AJamTestHUD::Panel(float X, float Y, float W, const FString& Title, const TArray<FString>& Lines, const FLinearColor& TitleColor)
{
	// K = escala segun la resolucion (ver DrawHUD): X, Y y W ya vienen escalados.
	const float Pad = 10.f * K, LineH = 17.f * K, TitleH = 24.f * K;
	const float H = Pad * 2 + TitleH + Lines.Num() * LineH;
	DrawRect(ColPanel, X, Y, W, H);
	DrawRect(TitleColor, X, Y, 4.f * K, H);
	DrawText(Title, TitleColor, X + Pad + 4 * K, Y + Pad - 2 * K, GEngine->GetMediumFont(), K);
	float Cy = Y + Pad + TitleH;
	for (const FString& L : Lines)
	{
		// "!" al inicio = linea resaltada; "#" = linea atenuada.
		const bool bHot = L.StartsWith(TEXT("!")), bDim = L.StartsWith(TEXT("#"));
		DrawText((bHot || bDim) ? L.Mid(1) : L, bHot ? ColAccent : bDim ? ColDim : ColText, X + Pad + 4 * K, Cy, GEngine->GetSmallFont(), K);
		Cy += LineH;
	}
	return Y + H + 8.f * K;
}

void AJamTestHUD::DrawHUD()
{
	Super::DrawHUD();
	if (!Canvas || !GEngine)
	{
		return;
	}
	UWorld* World = GetWorld();
	const float SX = Canvas->SizeX, SY = Canvas->SizeY;
	K = FMath::Clamp(SY / 800.f, 1.f, 3.f); // texto legible tambien en pantallas de alta resolucion
	const bool bDebug = CVarJamHudDebug.GetValueOnGameThread() != 0;

	DrawRect(FLinearColor(1, 1, 1, 0.6f), SX * 0.5f - 2, SY * 0.5f - 2, 4, 4); // punto central

	// 1) Barra de energia del jugador (abajo al centro, justo encima de la vida; GDD 9).
	const float E = AJamBuildPart::GetPlayerEnergy(World);
	const int32 Mode = AJamBuildPart::EnergyMode();
	const float BW = 360.f * K, BH = 14.f * K, BX = (SX - BW) * 0.5f, PanelTop = SY - 108.f * K, BY = PanelTop + 6.f * K;
	DrawRect(ColPanel, BX - 6 * K, PanelTop, BW + 12 * K, 44.f * K);
	// 15 segmentos de 10 (GDD 5): los 5 primeros son el minimo garantizado y van en otro tono.
	const float Blink = E < 1.f && FMath::Fmod(World->GetTimeSeconds(), 0.6f) < 0.3f ? 1.f : 0.f;
	for (int32 i = 0; i < 15; ++i)
	{
		const float Fill = FMath::Clamp((E - i * 10.f) / 10.f, 0.f, 1.f);
		const float X0 = BX + i * (BW / 15.f);
		DrawRect(E < 1.f ? FLinearColor(0.35f + 0.4f * Blink, 0.1f, 0.1f, 1.f) : FLinearColor(0.15f, 0.17f, 0.2f, 1.f), X0, BY, BW / 15.f - 3.f * K, BH);
		DrawRect(i < 5 ? ColGood : ColAccent, X0, BY, (BW / 15.f - 3.f * K) * Fill, BH);
	}
	DrawText(Mode == 2 ? FString::Printf(TEXT("ENERGIA  %.0f / 150%s"), E, E < 1.f ? TEXT("   AGOTADA: apaga con C") : TEXT(""))
		: Mode == 1 ? FString(TEXT("ENERGIA: cada construccion tiene su reserva")) : FString(TEXT("ENERGIA infinita")),
		E < 1.f ? ColBad : ColText, BX, BY + BH + 3 * K, GEngine->GetSmallFont(), K);

	// Los avisos de JamMsg se apilan encima de este bloque (y de la voz, si esta activa).
	MsgBaseY = PanelTop - 10.f * K;

	// Voz: microfono (encima de la energia). Solo mientras escucha o transcribe; lo que entendio sale como aviso.
	{
		const FJamVoice& Voice = FJamVoice::Get();
		FString Txt;
		FLinearColor Col = ColText;
		if (Voice.IsRecording()) { Txt = TEXT("(O)  ESCUCHANDO...  habla y suelta G"); Col = ColBad; }
		else if (Voice.Status == TEXT("transcribiendo")) { Txt = TEXT("(o)  transcribiendo tu voz..."); Col = ColAccent; }
		if (!Txt.IsEmpty())
		{
			float TW = 0.f, TH = 0.f;
			GetTextSize(Txt, TW, TH, GEngine->GetMediumFont(), K);
			const float X = (SX - TW) * 0.5f, Y = PanelTop - 14.f * K - TH;
			DrawRect(ColPanel, X - 12 * K, Y - 6 * K, TW + 24 * K, TH + 12 * K);
			if (Voice.IsRecording()) { DrawRect(ColBad, X - 12 * K, Y - 6 * K, 5 * K, TH + 12 * K); }
			DrawText(Txt, Col, X, Y, GEngine->GetMediumFont(), K);
			MsgBaseY = Y - 14.f * K;
		}
	}

	// 2) Construccion actual (arriba a la derecha). Sin construccion no se dibuja.
	const float RW = 400.f * K, RX = SX - RW - 14.f * K;
	float RY = 14.f * K;
	if (AJamBuildSpawner* S = GLastSay.Get())
	{
		TArray<FString> Lines;
		const FString& St = S->LastStatus;
		FString Pieces;
		int32 A = St.Find(TEXT("pieces=["));
		if (A != INDEX_NONE) { Pieces = St.Mid(A + 8); int32 End; if (Pieces.FindChar(TEXT(']'), End)) { Pieces = Pieces.Left(End); } }
		const TCHAR* Backend = St.Contains(TEXT("fallback")) ? TEXT("respaldo por palabras clave (fallo el modelo)") : St.Contains(TEXT("(jev")) ? TEXT("Jev (modelo de decision)")
			: St.Contains(TEXT("keywords")) ? TEXT("palabras clave (sin clave del modelo)") : St.Contains(TEXT("source=json")) ? TEXT("receta fija") : TEXT("...");
		Lines.Add(TEXT("!\"") + S->Prompt.Left(52) + TEXT("\""));
		if (!St.StartsWith(TEXT("OK")))
		{
			// Estado de espera o de fallo, en palabras del jugador (el detalle tecnico solo en depuracion).
			if (bDebug) { Lines.Add(St.Left(60)); }
			else if (St.StartsWith(TEXT("FAIL"))) { Lines.Add(TEXT("No se pudo armar. Prueba otra frase.")); }
			else if (St.StartsWith(TEXT("waiting"))) { Lines.Add(TEXT("Pensando...")); }
			else { Lines.Add(St.Left(60)); }
		}
		else
		{
			if (bDebug) { Lines.Add(FString::Printf(TEXT("Decidio: %s"), Backend)); }
			if (bDebug) { for (int32 c = 0; c < Pieces.Len(); c += 56) { Lines.Add((c == 0 ? TEXT("Piezas: ") : TEXT("        ")) + Pieces.Mid(c, 56)); } }
			// Mecanicas agrupadas por capacidad, con su estado.
			TMap<EJamPartKind, TPair<int32, FString>> Groups;
			TArray<EJamPartKind> Order;
			float Drain = 0.f;
			int32 Alive = 0;
			for (const AJamBuildPart* P : S->GetParts())
			{
				if (!IsValid(P)) { continue; }
				++Alive;
				Drain += P->GetEnergyDrain();
				if (KindName(P->Kind))
				{
					if (!Groups.Contains(P->Kind)) { Order.Add(P->Kind); }
					TPair<int32, FString>& G = Groups.FindOrAdd(P->Kind);
					++G.Key;
					const FString Txt = P->GetStatusText();
					if (G.Value.IsEmpty() || P->bActive) { G.Value = Txt; }
				}
			}
			Lines.Add(FString::Printf(TEXT("Piezas: %d    Gasto de energia: %.1f por segundo"), Alive, Drain));
			Lines.Add(S->IsBuildActive() ? TEXT("!ENCENDIDA  (C la apaga)") : TEXT("!APAGADA: pulsa C para encenderla"));
			if (bDebug)
			{
				int32 Skip = St.Find(TEXT("skipped=["));
				if (Skip != INDEX_NONE)
				{
					FString Sk = St.Mid(Skip + 9); int32 End; if (Sk.FindChar(TEXT(']'), End)) { Sk = Sk.Left(End); }
					Lines.Add(TEXT("#No cupo / no aplica: ") + Sk.Left(44));
				}
				if (S->LastCoherence >= 0)
				{
					Lines.Add(FString::Printf(TEXT("%sSentido: %d / 100"), S->LastCoherence >= 85 ? TEXT("") : TEXT("#"), S->LastCoherence));
					for (int32 c = 0; c < S->LastCoherenceNote.Len() && c < 168; c += 56) { Lines.Add(TEXT("#") + S->LastCoherenceNote.Mid(c, 56)); }
				}
			}
			// Controles disponibles segun las capacidades que tiene ESTA construccion.
			{
				bool bWheels = false, bWeapons = false, bRocket = false, bSeat = false, bPush = false;
				for (const AJamBuildPart* P : S->GetParts())
				{
					if (!IsValid(P)) { continue; }
					bWheels |= P->Kind == EJamPartKind::Wheel;
					bSeat |= P->Kind == EJamPartKind::Seat;
					bRocket |= P->Kind == EJamPartKind::Rocket || P->Kind == EJamPartKind::Bomb;
					bPush |= P->Kind == EJamPartKind::Fan;
					bWeapons |= P->Kind == EJamPartKind::Cannon || P->Kind == EJamPartKind::Flame || P->Kind == EJamPartKind::Frost || P->Kind == EJamPartKind::Beam || P->Kind == EJamPartKind::Shock || P->Kind == EJamPartKind::Hydrant;
				}
				TArray<FString> Ctl;
				Ctl.Add(TEXT("C encender"));
				if (bSeat) { Ctl.Add(TEXT("E subir")); }
				if (bSeat && (bWheels || bPush)) { Ctl.Add(TEXT("W/S A/D manejar")); }
				if (bSeat && bWeapons) { Ctl.Add(TEXT("Clic disparar")); }
				if (bSeat && bRocket) { Ctl.Add(TEXT("Espacio/Shift cohete o bomba")); }
				if (bSeat) { Ctl.Add(TEXT("Espacio/Ctrl subir/bajar si flota")); }
				// Lineas de hasta ~44 caracteres para que quepan en el panel.
				FString Row;
				for (const FString& C : Ctl)
				{
					if (!Row.IsEmpty() && Row.Len() + C.Len() + 3 > 44) { Lines.Add(TEXT("!") + Row); Row.Reset(); }
					Row += (Row.IsEmpty() ? FString() : FString(TEXT("   "))) + C;
				}
				if (!Row.IsEmpty()) { Lines.Add(TEXT("!") + Row); }
			}
			if (bDebug)
			{
				Lines.Add(TEXT("#Mecanicas:"));
				for (EJamPartKind Kd : Order)
				{
					const TPair<int32, FString>& G = Groups[Kd];
					const bool bOn = !G.Value.Contains(TEXT("apagad")) && !G.Value.Contains(TEXT("sin encender"));
					Lines.Add(FString::Printf(TEXT("%s  %s%s: %s"), bOn ? TEXT("!") : TEXT("#"), KindName(Kd), G.Key > 1 ? *FString::Printf(TEXT(" x%d"), G.Key) : TEXT(""), *G.Value).Replace(TEXT("!  "), TEXT("!> ")).Replace(TEXT("#  "), TEXT("#- ")));
				}
			}
		}
		RY = Panel(RX, RY, RW, TEXT("CONSTRUCCION ACTUAL"), Lines, ColAccent);
	}

	// 3) Depuracion (jam.Hud.Debug 1): elementos que el modelo puede usar (derecha, debajo): clave + palabra de ejemplo.
	if (bDebug)
	{
		TArray<FString> Lines;
		Lines.Add(TEXT("#Pide cualquiera en tu frase, p. ej. \"un carro con <palabra>\":"));
		const TArray<FJamExtraDef>& Extras = UJamBuildGenerator::ExtraTable();
		for (int32 i = 0; i < Extras.Num(); i += 2)
		{
			auto Cell = [&Extras](int32 k) { return k < Extras.Num() ? FString::Printf(TEXT("%-11s %s"), *Extras[k].Key.ToString(), Extras[k].Keywords.Num() > 0 ? *Extras[k].Keywords[0].TrimStartAndEnd() : TEXT("")) : FString(); };
			Lines.Add(FString::Printf(TEXT("%-30s %s"), *Cell(i), *Cell(i + 1)));
		}
		FString Archs;
		for (const FJamArchetypeDef& A : UJamBuildGenerator::ArchetypeTable()) { Archs += A.Name.ToString() + TEXT(" "); }
		Lines.Add(TEXT("#Plantillas: ") + Archs.Left(54));
		Lines.Add(TEXT("#            ") + Archs.Mid(54, 54));
		Panel(RX, RY, RW, TEXT("ELEMENTOS DISPONIBLES (clave -> palabra)"), Lines, ColIce);
	}

	// 4) Ayuda (H, abajo a la izquierda): solo los controles reales (GDD 11).
	if (!bShowHelp)
	{
		DrawText(TEXT("H = ayuda"), ColDim, 16 * K, SY - 28 * K, GEngine->GetSmallFont(), K);
		return;
	}
	{
		TArray<FString> Lines = {
			TEXT("WASD mover   Mouse mirar   Espacio saltar"),
			TEXT("!Clic   atacar o disparar"),
			TEXT("Q o rueda   cambiar arma (espada, arco, pedida)"),
			TEXT("!B (o T, Enter)   pedir por texto"),
			TEXT("!G (mantener)   pedir por voz"),
			TEXT("!C   encender o apagar tu construccion"),
			TEXT("E   subir o bajar del asiento"),
			TEXT("X   desarmar tu construccion"),
			TEXT("F   fusionar material a espada o arco"),
			TEXT("V   camara        H   cerrar ayuda"),
			TEXT("#Frases rapidas:"),
		};
		for (int32 i = 0; i < 9; ++i) { Lines.Add(FString::Printf(TEXT("!%d   %s"), i + 1, QuickPhrases[i])); }
		if (bDebug) { Lines.Add(TEXT("#Consola: Jam.Say <frase>  Jam.Clear  Jam.Seat  Jam.Energy 100  Jam.Test")); }
		Lines.Add(TEXT("#Los dispositivos gastan energia; apagada se recarga sola."));
		const float H = (20 + 24 + Lines.Num() * 17.f) * K;
		Panel(14 * K, SY - H - 14 * K, 430 * K, TEXT("COMO JUGAR"), Lines, ColGood);
	}
}
