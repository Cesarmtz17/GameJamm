#include "Building/JamPlayConsole.h"

#include "Building/JamBuildPart.h"
#include "Building/JamBuildSpawner.h"
#include "Building/JamVoice.h"
#include "Blueprint/WidgetTree.h"
#include "Camera/CameraComponent.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/MultiLineEditableTextBox.h"
#include "Engine/Canvas.h"
#include "Building/JamAssembler.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SpotLightComponent.h"
#include "Containers/Ticker.h"
#include "Core/JamStatics.h"
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

	void Screen(const FString& Msg, FColor Color = FColor::Yellow)
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 8.f, Color, Msg);
		}
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

	AJamBuildSpawner* SpawnSay(UWorld* World, const FString& Text, const FTransform& At, const TCHAR* RecipeJson = nullptr, bool bAutoActivate = true)
	{
		AJamBuildSpawner* S = World->SpawnActorDeferred<AJamBuildSpawner>(AJamBuildSpawner::StaticClass(), At, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (S)
		{
			S->Prompt = Text;
			S->bAnnounceOnScreen = true;
			S->Tags.Add(SayTag);
			if (RecipeJson) { S->RecipeJsonOverride = RecipeJson; }
			S->bAutoActivate = bAutoActivate;
			const APlayerController* PC = World->GetFirstPlayerController();
			S->Team = UJamStatics::GetActorTeam(PC ? PC->GetPawn() : nullptr);
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
			Screen(TEXT("Uso: Jam.Say <que quieres construir>   (ej. Jam.Say un carro con cohete)"));
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

	void JamClear(const TArray<FString>&, UWorld* World)
	{
		if (World)
		{
			ClearAll(World);
			Screen(TEXT("Jam.Clear: construcciones borradas"));
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
	struct FJamCase
	{
		const TCHAR* Key;     // aislada: clave de la mecanica (coincide con el extra o la plantilla); frase: nullptr
		const TCHAR* Text;    // frase para Jev, o descripcion de la prueba aislada
		const TCHAR* Recipe;  // aislada: receta JSON fija (sin modelo); frase: nullptr
	};

	const FJamCase TestCases[] = {
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
		// piezas poco probadas
		{ nullptr, TEXT("un carro con una viga larga al frente como lanza") }, { nullptr, TEXT("un tanque con plataforma giratoria") },
		{ nullptr, TEXT("un trineo con vela") }, { nullptr, TEXT("un carro con ventilador y una bateria") },
		{ nullptr, TEXT("un muro defensivo con escudo") }, { nullptr, TEXT("un carro con resorte que salte") },
	};
	const int32 NumCases = UE_ARRAY_COUNT(TestCases);

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
		int32 AimN = 0;
		bool bLightOn = false;
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
		const FJamCase& Case = TestCases[T.Index];
		const FString Status = T.Cur.IsValid() ? T.Cur->LastStatus : FString(TEXT("(sin construir)"));
		const bool bBuilt = Status.StartsWith(TEXT("OK"));
		if (Case.Recipe)
		{
			FString What;
			const bool bPass = bBuilt && IsoVerdict(T, Case.Key, What);
			T.Iso.Add(Case.Key, bPass);
			++T.IsoCount;
			T.IsoPass += bPass ? 1 : 0;
			T.IsoRows += FString::Printf(TEXT("| `%s` | %s | %d | %s | %s |\n"), Case.Key, Case.Text, T.Parts, bBuilt ? *What : *Status, bPass ? TEXT("PASA") : TEXT("FALLA"));
			UE_LOG(LogTemp, Display, TEXT("JamTest %d/%d ISO %s | %s | %s"), T.Index + 1, NumCases, bPass ? TEXT("PASA") : TEXT("FALLA"), Case.Key, bBuilt ? *What : *Status);
			return;
		}
		const TCHAR* Backend = Status.Contains(TEXT("fallback")) ? TEXT("respaldo (fallo Jev)") : Status.Contains(TEXT("(jev")) ? TEXT("Jev") : Status.Contains(TEXT("keywords")) ? TEXT("palabras clave") : TEXT("?");
		TArray<FString> Seen;
		if (T.Disp > 60) { Seen.Add(FString::Printf(TEXT("se movio %.0f cm (max %.0f cm/s)"), T.Disp, T.MaxSpeed)); }
		if (T.MaxUp > 40) { Seen.Add(FString::Printf(TEXT("subio %.0f cm"), T.MaxUp)); }
		if (T.MaxSpin > 2) { Seen.Add(FString::Printf(TEXT("algo gira a %.1f rad/s"), T.MaxSpin)); }
		if (T.MaxBalls > 0) { Seen.Add(FString::Printf(TEXT("disparo/lanzo %d bolas"), T.MaxBalls)); }
		if (T.Hits > 0) { Seen.Add(FString::Printf(TEXT("su efecto toco al objetivo (%d)"), T.Hits)); }
		if (T.bFrozen) { Seen.Add(TEXT("objetivo congelado")); }
		if (T.DummyMoved > 30) { Seen.Add(FString::Printf(TEXT("objetivo empujado %.0f cm"), T.DummyMoved)); }
		if (T.bExploded) { Seen.Add(TEXT("la bomba exploto")); }
		const bool bPass = bBuilt && (Seen.Num() > 0 || T.MaxOn == 0);
		if (bBuilt && Seen.Num() == 0) { Seen.Add(T.MaxOn == 0 ? TEXT("estructura estatica, se mantuvo en pie") : TEXT("nada medible")); }
		// Mecanicas de esta construccion que tienen prueba aislada (por extra elegido o por pieza de la plantilla).
		TArray<FString> IsoCol;
		auto Has = [&Status](const TCHAR* Extra) { return Status.Contains(FString::Printf(TEXT("[%s"), Extra)) || Status.Contains(FString::Printf(TEXT(",%s"), Extra)); };
		auto Arch = [&Status](const TCHAR* Name) { return Status.Contains(FString::Printf(TEXT("archetype=%s "), Name)); };
		auto Add = [&](const TCHAR* Key, bool bUsed)
		{
			if (bUsed) { IsoCol.AddUnique(FString::Printf(TEXT("%s: %s"), Key, T.Iso.FindRef(Key) ? TEXT("PASA") : TEXT("FALLA"))); }
		};
		Add(TEXT("Roller"), Arch(TEXT("Roller")));
		Add(TEXT("head"), Has(TEXT("head")));
		Add(TEXT("hinge"), Has(TEXT("hinge")));
		Add(TEXT("stake"), Has(TEXT("stake")));
		Add(TEXT("light"), Has(TEXT("light")));
		Add(TEXT("stabilizer"), Has(TEXT("stabilizer")) || Arch(TEXT("Flyer")) || Arch(TEXT("Glider")) || Arch(TEXT("Bike")) || Arch(TEXT("Hover")) || Arch(TEXT("Rocket")));
		Add(TEXT("cannon"), Has(TEXT("cannon")) || Arch(TEXT("Tank")) || Arch(TEXT("Turret")));
		Add(TEXT("battery"), Has(TEXT("battery")) || Arch(TEXT("Turret")));
		++T.PhraseCount;
		T.Pass += bPass ? 1 : 0;
		T.Rows += FString::Printf(TEXT("| %d | %s | %s | `%s` | %d | %d | %s | %s | %s |\n"), T.PhraseCount, Case.Text, Backend,
			*Status.Replace(TEXT("|"), TEXT("/")), T.Parts, T.MaxOn, *FString::Join(Seen, TEXT("; ")), IsoCol.Num() > 0 ? *FString::Join(IsoCol, TEXT("; ")) : TEXT("-"), bPass ? TEXT("PASA") : TEXT("FALLA"));
		UE_LOG(LogTemp, Display, TEXT("JamTest %d/%d %s | %s | %s | %s"), T.Index + 1, NumCases, bPass ? TEXT("PASA") : TEXT("FALLA"), Case.Text, *Status, *FString::Join(Seen, TEXT("; ")));
	}

	void TestMeasure(FJamTestRun& T, UWorld* World)
	{
		const FJamCase& Case = TestCases[T.Index];
		const FString Key = Case.Key ? FString(Case.Key) : FString();
		AStaticMeshActor* D = T.Dummy.Get();
		// Cabeza: el objetivo va y viene de lado delante de la construccion.
		if (D && Key == TEXT("head"))
		{
			D->SetActorLocation(T.At.TransformPosition(FVector(450, 350 * FMath::Sin(T.Timer * 0.9f), 60)), false, nullptr, ETeleportType::TeleportPhysics);
			D->GetStaticMeshComponent()->SetPhysicsLinearVelocity(FVector::ZeroVector);
		}
		if (AJamBuildSpawner* S = T.Cur.Get())
		{
			const TArray<TObjectPtr<AJamBuildPart>>& Parts = S->GetParts();
			int32 On = 0, Hits = 0;
			bool bBomb = false;
			for (const AJamBuildPart* P : Parts)
			{
				if (!IsValid(P)) { continue; }
				On += P->bActive ? 1 : 0;
				Hits += P->EffectHits;
				bBomb |= P->Kind == EJamPartKind::Bomb;
				T.MaxSpin = FMath::Max(T.MaxSpin, P->Mesh->GetPhysicsAngularVelocityInRadians().Size());
				if (const USpotLightComponent* L = P->FindComponentByClass<USpotLightComponent>())
				{
					T.bLightOn |= P->bActive && L->IsVisible() && L->Intensity > 0.f;
				}
			}
			if (Parts.Num() > 0 && IsValid(Parts[0]))
			{
				AJamBuildPart* Root = Parts[0];
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
						const FVector Fwd = Head->GetActorQuat().RotateVector(Head->LocalDirection).GetSafeNormal2D();
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
						Root->Mesh->AddAngularImpulseInRadians(T.At.GetUnitAxis(EAxis::X) * 4.0, NAME_None, true);
					}
					if (T.bKicked) { T.MinUpZ = FMath::Min(T.MinUpZ, T.LastUpZ); }
				}
			}
		}
		int32 Balls = 0;
		for (TActorIterator<AStaticMeshActor> It(World); It; ++It) { Balls += It->ActorHasTag(TEXT("JamProjectile")) ? 1 : 0; }
		T.MaxBalls = FMath::Max(T.MaxBalls, Balls);
		if (D)
		{
			if (Key != TEXT("head")) { T.DummyMoved = FMath::Max(T.DummyMoved, FVector::Dist(D->GetActorLocation(), T.DummyStart)); }
			T.bFrozen |= D->GetStaticMeshComponent()->GetLinearDamping() > 1.f;
		}
	}

	void TestWriteReport(FJamTestRun& T)
	{
		FString Md = TEXT("---\ntitle: Reporte del POC de construccion por lenguaje natural\ntags:\n  - building\n  - pruebas\nstatus: generado\n---\n\n# Reporte del POC: frase -> modelo de decision -> construccion\n\n");
		Md += TEXT("> [!info]\n> Archivo GENERADO por el comando de consola `Jam.Test` (en PIE, `Source/GameJamm/Building/JamPlayConsole.cpp`). No editar a mano: volver a correr el comando.\n");
		Md += FString::Printf(TEXT("> Fecha: %s. Cada caso se construye delante del jugador y se mide durante %.0f s.\n\n"), *FDateTime::Now().ToString(TEXT("%Y-%m-%d %H:%M")), T.Duration);
		Md += FString::Printf(TEXT("**Resultado: %d de %d pruebas aisladas y %d de %d frases pasan.**\n\n"), T.IsoPass, T.IsoCount, T.Pass, T.PhraseCount);
		Md += TEXT("## 1. Mecanicas verificadas aisladas\n\nUna mecanica por prueba, con receta fija (sin modelo de decision) y una metrica propia.\n\n");
		Md += TEXT("| Mecanica | Prueba | Piezas | Medido | Veredicto |\n|---|---|---|---|---|\n");
		Md += T.IsoRows;
		Md += TEXT("\n## 2. Frases completas\n\nCamino completo: frase -> modelo de decision -> receta -> ensamblador, con un cubo-objetivo de 20 kg a 3.5 m al frente. ");
		Md += TEXT("\"PASA\" = se construyo y se midio al menos una mecanica (o es una estructura estatica sin dispositivos). No juzga si la plantilla elegida es la que una persona esperaria: eso se lee en la columna de decision.\n\n");
		Md += TEXT("| # | Frase | Backend | Decision y receta (estado del spawner) | Piezas | Encendidas | Mecanica observada | Mecanica verificada aislada | Veredicto |\n|---|---|---|---|---|---|---|---|---|\n");
		Md += T.Rows;
		Md += TEXT("\n## Como leerlo\n\n- **Backend**: `Jev` = decidio el modelo de decision; `respaldo` = fallo la llamada y se uso el generador por palabras clave.\n- **Decision**: plantilla (`archetype`), `skipped=[...]` = extras pedidos que no cupieron o no aplican, `extras=[...]` = extras que el modelo marco.\n- **Mecanica observada**: medido en el juego (posicion, velocidad, giro, proyectiles, efecto sobre el cubo-objetivo).\n- **Mecanica verificada aislada**: resultado de la seccion 1 para las mecanicas que lleva esa construccion (por extra elegido o por pieza de la plantilla).\n");
		const FString Path = FPaths::ProjectDir() / TEXT("docs/building/REPORTE-POC.md");
		const bool bSaved = FFileHelper::SaveStringToFile(Md, *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		Screen(FString::Printf(TEXT("Jam.Test terminado: aisladas %d/%d, frases %d/%d. Reporte %s: %s"), T.IsoPass, T.IsoCount, T.Pass, T.PhraseCount, bSaved ? TEXT("guardado") : TEXT("NO guardado"), *Path), FColor::Green);
		UE_LOG(LogTemp, Display, TEXT("JamTest DONE iso=%d/%d frases=%d/%d saved=%d"), T.IsoPass, T.IsoCount, T.Pass, T.PhraseCount, bSaved);
	}

	bool TestTick(float Dt)
	{
		FJamTestRun& T = GTest;
		UWorld* World = T.World.Get();
		if (!World)
		{
			return false;
		}
		T.Timer += Dt;
		if (T.Index >= 0 && T.Timer < T.Duration)
		{
			TestMeasure(T, World);
			return true;
		}

		// Cerrar el caso actual y pasar al siguiente.
		if (T.Index >= 0)
		{
			TestFinishCase();
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
		// Siguiente caso: energia llena, objetivo nuevo, metricas a cero (se conservan los resultados acumulados).
		const FJamCase& Case = TestCases[T.Index];
		const FString Key = Case.Key ? FString(Case.Key) : FString();
		AJamBuildPart::SetPlayerEnergy(World, 100.f);
		{
			FJamTestRun Fresh;
			Fresh.World = T.World; Fresh.At = T.At; Fresh.Index = T.Index; Fresh.Duration = T.Duration; Fresh.Handle = T.Handle;
			Fresh.Pass = T.Pass; Fresh.IsoPass = T.IsoPass; Fresh.IsoCount = T.IsoCount; Fresh.PhraseCount = T.PhraseCount;
			Fresh.Iso = MoveTemp(T.Iso); Fresh.Rows = MoveTemp(T.Rows); Fresh.IsoRows = MoveTemp(T.IsoRows);
			T = MoveTemp(Fresh);
		}
		// Cubo-objetivo: en las frases, y en las aisladas que lo necesitan (cabeza y cañon).
		if (!Case.Recipe || Key == TEXT("head") || Key == TEXT("cannon"))
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
		Screen(FString::Printf(TEXT("Jam.Test %d/%d: %s"), T.Index + 1, NumCases, Case.Text), FColor::Cyan);
		T.Cur = SpawnSay(World, Case.Text, T.At, Case.Recipe);
		return true;
	}

	void JamTest(const TArray<FString>& Args, UWorld* World)
	{
		if (!World || !World->IsGameWorld())
		{
			Screen(TEXT("Jam.Test: correr dentro del juego (PIE)"), FColor::Red);
			return;
		}
		FTSTicker::GetCoreTicker().RemoveTicker(GTest.Handle);
		GTest = FJamTestRun();
		GTest.World = World;
		GTest.At = SayTransform(World);
		if (Args.Num() > 0) { GTest.Duration = FMath::Clamp(FCString::Atof(*Args[0]), 3.f, 60.f); }
		GTest.Timer = GTest.Duration; // arranca ya con el primer caso
		GTest.Handle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateStatic(&TestTick));
	}

	void JamSeat(const TArray<FString>&, UWorld* World)
	{
		APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
		ACharacter* Me = PC ? Cast<ACharacter>(PC->GetPawn()) : nullptr;
		if (!Me)
		{
			Screen(TEXT("Jam.Seat: no hay personaje (usa PIE normal con AJamTestGameMode)"), FColor::Red);
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
			Screen(TEXT("Jam.Seat: te bajaste"));
			return;
		}
		// El asiento mas cercano; si no hay asiento, la pieza mas cercana.
		AJamBuildPart* Best = nullptr;
		double BestD = FMath::Square(1500.0);
		for (TActorIterator<AJamBuildPart> It(World); It; ++It)
		{
			const double D = FVector::DistSquared(It->GetActorLocation(), Me->GetActorLocation()) * (It->Kind == EJamPartKind::Seat ? 0.01 : 1.0);
			if (D < BestD)
			{
				BestD = D;
				Best = *It;
			}
		}
		if (!Best)
		{
			Screen(TEXT("No hay ninguna construccion a menos de 15 m para subirse"), FColor::Red);
			return;
		}
		Me->GetCharacterMovement()->SetMovementMode(MOVE_None);
		Me->SetActorEnableCollision(false);
		Me->SetActorLocation(Best->GetActorLocation() + FVector(0, 0, 110));
		Me->AttachToActor(Best, FAttachmentTransformRules::KeepWorldTransform);
		Screen(FString::Printf(TEXT("Montado en %s: W/S acelera y frena, A/D gira, E para bajar"), *Best->PartType.ToString()));
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
		Screen(FString::Printf(TEXT("Voz: entendi \"%s\""), *Text), FColor::Cyan);
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

	void JamPower(const TArray<FString>&, UWorld*)
	{
		// Lo mismo que la tecla C.
		if (AJamBuildSpawner* S = GLastSay.Get()) { S->SetBuildActive(!S->IsBuildActive()); }
	}

	// Jam.Drive <acelerador> <giro> [segundos]: simula W/S y A/D estando montado (para pruebas sin teclado).
	float GDriveThrottle = 0.f, GDriveSteer = 0.f;
	double GDriveUntil = 0.0;
	void JamDrive(const TArray<FString>& Args, UWorld*)
	{
		GDriveThrottle = Args.Num() > 0 ? FCString::Atof(*Args[0]) : 0.f;
		GDriveSteer = Args.Num() > 1 ? FCString::Atof(*Args[1]) : 0.f;
		GDriveUntil = FPlatformTime::Seconds() + (Args.Num() > 2 ? FCString::Atof(*Args[2]) : 3.f);
	}

	FAutoConsoleCommandWithWorldAndArgs CmdVoiceFile(TEXT("Jam.VoiceFile"), TEXT("Jam.VoiceFile <ruta.wav>: transcribe ese audio y construye lo que diga (prueba de voz sin microfono)."), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&JamVoiceFile));
	FAutoConsoleCommandWithWorldAndArgs CmdType(TEXT("Jam.Type"), TEXT("Jam.Type <frase>: como escribirla en la caja de texto (sale apagada; se enciende con C o Jam.Power)."), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&JamType));
	FAutoConsoleCommandWithWorldAndArgs CmdPower(TEXT("Jam.Power"), TEXT("Prende o apaga la ultima construccion (tecla C)."), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&JamPower));
	FAutoConsoleCommandWithWorldAndArgs CmdDrive(TEXT("Jam.Drive"), TEXT("Jam.Drive <acelerador -1..1> <giro -1..1> [segundos]: simula W/S y A/D estando montado."), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&JamDrive));
	FAutoConsoleCommandWithWorldAndArgs CmdSay(TEXT("Jam.Say"), TEXT("Jam.Say <texto libre>: construye lo que pidas delante del jugador (modelo de decision Jev; respaldo por palabras clave)."), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&JamSay));
	FAutoConsoleCommandWithWorldAndArgs CmdClear(TEXT("Jam.Clear"), TEXT("Borra todas las construcciones."), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&JamClear));
	FAutoConsoleCommandWithWorldAndArgs CmdEnergy(TEXT("Jam.Energy"), TEXT("Jam.Energy [valor]: muestra (o fija) la barra de energia del jugador."), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&JamEnergy));
	FAutoConsoleCommandWithWorldAndArgs CmdTest(TEXT("Jam.Test"), TEXT("Jam.Test [segundos por frase]: corre la bateria de frases y escribe docs/building/REPORTE-POC.md."), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&JamTest));
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
		TEXT("un carro con cohete"), TEXT("un tanque"), TEXT("una torreta con laser"), TEXT("un globo aerostatico con fuego debajo"),
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
		default: return nullptr; // estructura, asiento, trineo: sin estado
		}
	}
}

static TAutoConsoleVariable<float> CVarJamMouseSens(TEXT("jam.Mouse.Sens"), 0.12f, TEXT("Sensibilidad del mouse: grados por unidad de movimiento (0.05 - 0.25)."));
static TAutoConsoleVariable<int32> CVarJamMouseInvertY(TEXT("jam.Mouse.InvertY"), 0, TEXT("1 = invertir el eje vertical del mouse y del stick."));
static TAutoConsoleVariable<float> CVarJamShake(TEXT("jam.Shake"), 1.f, TEXT("Intensidad de la sacudida de camara (0 = apagada, 1 = normal)."));

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

void AJamTestCharacter::DriveMount(AJamBuildPart* Seat, float Throttle, float Steer, float DeltaSeconds)
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
	for (const TWeakObjectPtr<AJamBuildPart>& P : Seat->Build->Parts)
	{
		if (P.IsValid() && P->Kind == EJamPartKind::Wheel && P->DriveConstraint)
		{
			// Giro diferencial: las ruedas de un lado van mas rapido que las del otro.
			bWheels = true;
			const float Side = FVector::DotProduct(P->GetActorLocation() - Root->GetActorLocation(), Right) >= 0.0 ? 1.f : -1.f;
			P->SetDrive(FMath::Clamp(SmoothThrottle - 1.5f * SmoothSteer * Side, -1.f, 1.f)); // a tope de giro, el lado interior va en reversa
		}
	}
	if (!bWheels || FMath::Abs(SmoothSteer) > 0.05f)
	{
		// Guiñada asistida (sin ruedas, o para ayudar al giro): aceleracion angular, independiente de la masa.
		// Torque proporcional a la masa de toda la construccion (la raiz sola es muy ligera): ~4 rad/s2, con freno al soltar.
		const double Wz = Root->Mesh->GetPhysicsAngularVelocityInRadians().Z;
		const double Mass = FMath::Max(Seat->Build->MassKg, 1.f);
		Root->Mesh->AddTorqueInRadians(FVector(0, 0, Mass * 6400.0 * (4.0 * SmoothSteer - 1.5 * Wz * (bWheels ? 0.3 : 1.0))));
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
		DriveMount(Seat, Fwd, Strafe, DeltaSeconds);
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
		Box->SetHintText(FText::FromString(TEXT("Que quieres construir? (Enter = construir, Esc = cerrar)   ej: un carro con cohete y lanzallamas")));
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

	DrawRect(FLinearColor(1, 1, 1, 0.6f), SX * 0.5f - 2, SY * 0.5f - 2, 4, 4); // punto central

	// 1) Barra de energia del jugador (arriba al centro).
	const float E = AJamBuildPart::GetPlayerEnergy(World);
	const int32 Mode = AJamBuildPart::EnergyMode();
	const float BW = 390.f * K, BH = 20.f * K, BX = (SX - BW) * 0.5f, BY = 14.f * K;
	DrawRect(ColPanel, BX - 6 * K, BY - 6 * K, BW + 12 * K, BH + 34 * K);
	// 15 segmentos de 10 (GDD 5): los 5 primeros son el minimo garantizado y van en otro tono.
	const float Blink = E < 1.f && FMath::Fmod(World->GetTimeSeconds(), 0.6f) < 0.3f ? 1.f : 0.f;
	for (int32 i = 0; i < 15; ++i)
	{
		const float Fill = FMath::Clamp((E - i * 10.f) / 10.f, 0.f, 1.f);
		const float X0 = BX + i * (BW / 15.f);
		DrawRect(E < 1.f ? FLinearColor(0.35f + 0.4f * Blink, 0.1f, 0.1f, 1.f) : FLinearColor(0.15f, 0.17f, 0.2f, 1.f), X0, BY, BW / 15.f - 3.f * K, BH);
		DrawRect(i < 5 ? ColGood : ColAccent, X0, BY, (BW / 15.f - 3.f * K) * Fill, BH);
	}
	DrawText(Mode == 2 ? FString::Printf(TEXT("ENERGIA  %.0f / 150%s"), E, E < 1.f ? TEXT("   AGOTADA: apaga (C) y se recarga sola hasta 50") : TEXT(""))
		: Mode == 1 ? FString(TEXT("ENERGIA: reserva propia de cada construccion (jam.Build.Energy 1)")) : FString(TEXT("ENERGIA infinita (jam.Build.Energy 0)")),
		E < 1.f ? ColBad : ColText, BX, BY + BH + 3 * K, GEngine->GetSmallFont(), K);

	// Voz: microfono (debajo de la barra de energia).
	{
		const FJamVoice& Voice = FJamVoice::Get();
		const double Ago = FPlatformTime::Seconds() - Voice.LastTime;
		FString Txt;
		FLinearColor Col = ColText;
		if (Voice.IsRecording()) { Txt = TEXT("(O)  ESCUCHANDO...  habla y suelta G"); Col = ColBad; }
		else if (Voice.Status == TEXT("transcribiendo")) { Txt = TEXT("(o)  transcribiendo tu voz..."); Col = ColAccent; }
		else if (Ago < 7.0) { Txt = Voice.bLastOk ? FString::Printf(TEXT("(o)  Entendi: \"%s\"   voz %.1f s (%s)"), *Voice.LastText.Left(60), Voice.LastSeconds, *Voice.LastBackend) : Voice.LastText.Left(120); Col = Voice.bLastOk ? ColIce : ColBad; }
		if (!Txt.IsEmpty())
		{
			float TW = 0.f, TH = 0.f;
			GetTextSize(Txt, TW, TH, GEngine->GetMediumFont(), K);
			const float X = (SX - TW) * 0.5f, Y = 78.f * K;
			DrawRect(ColPanel, X - 12 * K, Y - 6 * K, TW + 24 * K, TH + 12 * K);
			if (Voice.IsRecording()) { DrawRect(ColBad, X - 12 * K, Y - 6 * K, 5 * K, TH + 12 * K); }
			DrawText(Txt, Col, X, Y, GEngine->GetMediumFont(), K);
		}
	}

	// 2) Construccion actual y sus mecanicas (arriba a la derecha).
	const float RW = 400.f * K, RX = SX - RW - 14.f * K;
	float RY = 14.f * K;
	{
		TArray<FString> Lines;
		if (AJamBuildSpawner* S = GLastSay.Get())
		{
			const FString& St = S->LastStatus;
			FString Arch;
			int32 A = St.Find(TEXT("archetype="));
			if (A != INDEX_NONE) { Arch = St.Mid(A + 10); int32 Sp; if (Arch.FindChar(TEXT(' '), Sp)) { Arch = Arch.Left(Sp); } Arch.RemoveFromEnd(TEXT(")")); }
			const TCHAR* Backend = St.Contains(TEXT("fallback")) ? TEXT("respaldo por palabras clave (fallo el modelo)") : St.Contains(TEXT("(jev")) ? TEXT("Jev (modelo de decision)")
				: St.Contains(TEXT("keywords")) ? TEXT("palabras clave (sin clave del modelo)") : St.Contains(TEXT("source=json")) ? TEXT("receta fija") : TEXT("...");
			Lines.Add(TEXT("!\"") + S->Prompt.Left(52) + TEXT("\""));
			if (!St.StartsWith(TEXT("OK")))
			{
				Lines.Add(St.Left(60));
			}
			else
			{
				Lines.Add(FString::Printf(TEXT("Plantilla: %s    Decidio: %s"), *Arch, Backend));
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
				int32 Skip = St.Find(TEXT("skipped=["));
				if (Skip != INDEX_NONE)
				{
					FString Sk = St.Mid(Skip + 9); int32 End; if (Sk.FindChar(TEXT(']'), End)) { Sk = Sk.Left(End); }
					Lines.Add(TEXT("#No cupo / no aplica: ") + Sk.Left(44));
				}
				Lines.Add(TEXT("#Mecanicas:"));
				for (EJamPartKind Kd : Order)
				{
					const TPair<int32, FString>& G = Groups[Kd];
					const bool bOn = !G.Value.Contains(TEXT("apagad")) && !G.Value.Contains(TEXT("sin encender"));
					Lines.Add(FString::Printf(TEXT("%s  %s%s: %s"), bOn ? TEXT("!") : TEXT("#"), KindName(Kd), G.Key > 1 ? *FString::Printf(TEXT(" x%d"), G.Key) : TEXT(""), *G.Value).Replace(TEXT("!  "), TEXT("!> ")).Replace(TEXT("#  "), TEXT("#- ")));
				}
			}
		}
		else
		{
			Lines.Add(TEXT("#Todavia no has construido nada."));
			Lines.Add(TEXT("Pulsa T y escribe, o 1 para un carro con cohete; luego C."));
		}
		RY = Panel(RX, RY, RW, TEXT("CONSTRUCCION ACTUAL"), Lines, ColAccent);
	}

	if (!bShowHelp)
	{
		DrawText(TEXT("H = ayuda"), ColDim, 16 * K, SY - 28 * K, GEngine->GetSmallFont(), K);
		return;
	}

	// 3) Elementos que el modelo puede usar (derecha, debajo): clave + palabra de ejemplo.
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

	// 4) Ayuda (abajo a la izquierda).
	{
		TArray<FString> Lines = {
			TEXT("WASD mover   Mouse mirar   Espacio saltar"),
			TEXT("!T, B o Enter   escribir que construir (Enter construye, Esc cierra)"),
			TEXT("!G (mantener)   HABLAR: di que construir y suelta"),
			TEXT("!C   ENCENDER / APAGAR la construccion (sale apagada)"),
			TEXT("X   borrar todo    E   subir / bajar (montado: W/S A/D dirigen)"),
			TEXT("V   primera / tercera persona   H   ocultar ayuda   ~   consola"),
			TEXT("#Ejemplos rapidos:"),
		};
		for (int32 i = 0; i < 9; ++i) { Lines.Add(FString::Printf(TEXT("!%d   %s"), i + 1, QuickPhrases[i])); }
		Lines.Add(TEXT("#Consola: Jam.Say <frase>  Jam.Clear  Jam.Seat  Jam.Energy 100  Jam.Test"));
		Lines.Add(TEXT("#Los dispositivos gastan energia; apagada se recarga sola hasta 50."));
		const float H = (20 + 24 + Lines.Num() * 17.f) * K;
		Panel(14 * K, SY - H - 14 * K, 470 * K, TEXT("COMO JUGAR"), Lines, ColGood);
	}
}
