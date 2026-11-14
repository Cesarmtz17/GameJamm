#include "Building/JamAssembler.h"

#include "Building/JamBuildPart.h"
#include "Building/JamLayout.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "PhysicsEngine/PhysicsConstraintComponent.h"

namespace
{
	void LockAll(UPhysicsConstraintComponent* C)
	{
		C->SetLinearXLimit(LCM_Locked, 0);
		C->SetLinearYLimit(LCM_Locked, 0);
		C->SetLinearZLimit(LCM_Locked, 0);
		C->SetAngularSwing1Limit(ACM_Locked, 0);
		C->SetAngularSwing2Limit(ACM_Locked, 0);
		C->SetAngularTwistLimit(ACM_Locked, 0);
	}
}

bool UJamAssembler::Assemble(UObject* WorldContextObject, const FBuildRecipe& Recipe, const FTransform& Origin,
	TArray<AJamBuildPart*>& OutParts, FString& OutError)
{
	OutParts.Reset();
	OutError.Reset();

	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	if (!World)
	{
		OutError = TEXT("No world");
		return false;
	}
	if (!UBuildRecipeLibrary::ValidateRecipe(Recipe, nullptr, OutError))
	{
		return false;
	}
	if (Recipe.Parts.Num() > MaxParts)
	{
		OutError = FString::Printf(TEXT("Too many parts (%d > %d)"), Recipe.Parts.Num(), MaxParts);
		return false;
	}

	// 1) Layout en el marco del vehículo (sin mundo; ver JamLayout, tiene tests).
	TMap<FName, FJamPlacedPart> Placed;
	if (!JamLayout::Compute(Recipe, Placed, OutError))
	{
		return false;
	}

	// 2) Subir el conjunto para que la pieza más baja quede sobre el origen.
	double MinZ = TNumericLimits<double>::Max();
	for (const auto& KV : Placed)
	{
		MinZ = FMath::Min(MinZ, KV.Value.Center.Z - KV.Value.Half.Z);
	}
	const FVector Lift(0, 0, -MinZ + 5.0);
	TMap<FName, AJamBuildPart*> Actors;

	// 3) Spawn de las piezas.
	for (const FBuildRecipePart& P : Recipe.Parts)
	{
		const FJamPlacedPart& It = Placed[P.Id];
		FQuat AxisRot = FQuat::Identity;
		if (It.Def.Axis == 1) { AxisRot = FQuat::FindBetweenNormals(FVector::UpVector, FVector::ForwardVector); }
		if (It.Def.Axis == 2) { AxisRot = FQuat::FindBetweenNormals(FVector::UpVector, FVector::RightVector); }

		const FTransform T(Origin.GetRotation() * AxisRot, Origin.TransformPosition(It.Center + Lift));
		FActorSpawnParameters SP;
		SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AJamBuildPart* Part = World->SpawnActor<AJamBuildPart>(AJamBuildPart::StaticClass(), T, SP);
		if (!Part)
		{
			OutError = TEXT("Spawn failed");
			DestroyBuild(OutParts); // no dejar piezas huérfanas en el mundo
			return false;
		}
		Part->RecipeId = P.Id;
		Part->InitFromDef(It.Def);

		// Dirección de empuje/disparo: ventiladores y cohetes empujan al lado contrario del socket; cañón y llama, hacia afuera.
		FVector BuildDir = It.OutwardDir;
		if (It.Def.Kind == EJamPartKind::Fan || It.Def.Kind == EJamPartKind::Rocket)
		{
			BuildDir = -BuildDir;
		}
		if (P.AttachTo.IsNone())
		{
			BuildDir = FVector::ForwardVector;
		}
		const FVector WorldDir = Origin.GetRotation().RotateVector(BuildDir);
		Part->LocalDirection = Part->GetActorQuat().UnrotateVector(WorldDir);

		Actors.Add(P.Id, Part);
		OutParts.Add(Part);
	}

	// 4) Constraints: soldadura (fixed) o motor de eje para ruedas.
	for (const FBuildRecipePart& P : Recipe.Parts)
	{
		if (P.AttachTo.IsNone())
		{
			continue;
		}
		const FJamPlacedPart& Child = Placed[P.Id];
		AJamBuildPart* ChildActor = Actors[P.Id];
		AJamBuildPart* ParentActor = Actors[P.AttachTo];

		UPhysicsConstraintComponent* C = NewObject<UPhysicsConstraintComponent>(ChildActor);
		C->RegisterComponent();
		C->AttachToComponent(ChildActor->Mesh, FAttachmentTransformRules::KeepRelativeTransform);
		C->SetWorldLocation(ChildActor->GetActorLocation());
		C->SetDisableCollision(true);

		if (Child.Def.Kind == EJamPartKind::Wheel)
		{
			// Eje de giro = Y del vehículo: la constraint gira sobre su eje X, así que se rota 90° en yaw.
			C->SetWorldRotation(Origin.GetRotation() * FQuat(FVector::UpVector, HALF_PI));
			C->SetLinearXLimit(LCM_Locked, 0);
			C->SetLinearYLimit(LCM_Locked, 0);
			C->SetLinearZLimit(LCM_Locked, 0);
			C->SetAngularSwing1Limit(ACM_Locked, 0);
			C->SetAngularSwing2Limit(ACM_Locked, 0);
			C->SetAngularTwistLimit(ACM_Free, 0);
			C->SetAngularDriveMode(EAngularDriveMode::TwistAndSwing);
			C->SetAngularVelocityDriveTwistAndSwing(true, false);
			C->SetAngularDriveParams(0.f, 5000.f, 0.f);
			C->SetAngularVelocityTarget(FVector::ZeroVector);
			ChildActor->DriveConstraint = C;
		}
		else
		{
			LockAll(C);
		}
		C->SetConstrainedComponents(ParentActor->Mesh, NAME_None, ChildActor->Mesh, NAME_None);
	}
	return true;
}

void UJamAssembler::SetBuildActive(const TArray<AJamBuildPart*>& Parts, const TArray<FName>& Ids, bool bActive)
{
	for (AJamBuildPart* P : Parts)
	{
		if (P && Ids.Contains(P->RecipeId))
		{
			P->SetActive(bActive);
		}
	}
}

void UJamAssembler::DestroyBuild(TArray<AJamBuildPart*>& Parts)
{
	for (AJamBuildPart* P : Parts)
	{
		if (IsValid(P))
		{
			P->Destroy();
		}
	}
	Parts.Reset();
}

// ---------------------------------------------------------------------------------------------
// Generador prompt -> receta (offline)
// ---------------------------------------------------------------------------------------------

namespace
{
	bool Has(const FString& L, std::initializer_list<const TCHAR*> Words)
	{
		for (const TCHAR* W : Words)
		{
			if (L.Contains(W))
			{
				return true;
			}
		}
		return false;
	}

	void Add(FBuildRecipe& R, const TCHAR* Id, const TCHAR* Type, const TCHAR* Parent = nullptr, const TCHAR* Socket = nullptr)
	{
		FBuildRecipePart P;
		P.Id = Id;
		P.Type = Type;
		P.AttachTo = Parent ? FName(Parent) : NAME_None;
		P.Socket = Socket ? FName(Socket) : NAME_None;
		R.Parts.Add(P);
	}
}

namespace
{
	TArray<FName> N(std::initializer_list<const TCHAR*> L)
	{
		TArray<FName> O;
		for (const TCHAR* S : L) { O.Add(FName(S)); }
		return O;
	}
	TArray<FString> K(std::initializer_list<const TCHAR*> L)
	{
		TArray<FString> O;
		for (const TCHAR* S : L) { O.Add(S); }
		return O;
	}
}

const TArray<FJamArchetypeDef>& UJamBuildGenerator::ArchetypeTable()
{
	static const TArray<FJamArchetypeDef> T = {
		{ "Car", TEXT("A ground vehicle: car, cart, truck, buggy, anything that drives on wheels.") },
		{ "Tank", TEXT("An armored ground vehicle with a big gun: tank, assault vehicle, war machine on big wheels.") },
		{ "Flyer", TEXT("Something that flies or hovers with wings or propellers: plane, drone, helicopter, glider.") },
		{ "Hover", TEXT("A floating platform or hovercraft that levitates on anti-gravity stones instead of wheels.") },
		{ "Turret", TEXT("A fixed defensive or shooting emplacement: turret, cannon tower.") },
		{ "Fortress", TEXT("A static defensive structure made of walls: fort, castle, bunker, barricade.") },
		{ "Rocket", TEXT("A rocket or missile that launches upward.") },
	};
	return T;
}

const TArray<FJamExtraDef>& UJamBuildGenerator::ExtraTable()
{
	// Orden = prioridad cuando se llega al limite de piezas (MaxParts).
	static const TArray<FJamExtraDef> T = {
		{ "fan", TEXT("The user wants a fan, propeller or extra air thrust."), TEXT("No fan or propeller is asked for."), "Fan", N({TEXT("Back")}), {}, N({TEXT("Car"), TEXT("Tank"), TEXT("Hover"), TEXT("Flyer")}), K({TEXT("ventilador"), TEXT("fan"), TEXT("helice"), TEXT("hélice"), TEXT("propel")}) },
		{ "jet", TEXT("The user wants a jet engine, jet thruster or strong continuous engine thrust."), TEXT("No jet engine is asked for."), "Jet", N({TEXT("Back_Top")}), {}, N({TEXT("Car"), TEXT("Tank"), TEXT("Hover"), TEXT("Flyer")}), K({TEXT("jet"), TEXT("reactor"), TEXT("turbo"), TEXT("propulsor")}) },
		{ "turbine", TEXT("The user wants a big turbine."), TEXT("No turbine is asked for."), "Turbine", N({TEXT("Back_Bottom")}), {}, N({TEXT("Car"), TEXT("Hover"), TEXT("Flyer")}), K({TEXT("turbina"), TEXT("turbine")}) },
		{ "rocket", TEXT("The user wants a rocket, nitro or a rocket booster."), TEXT("No rocket or nitro is asked for."), "Rocket", N({TEXT("Back")}), {}, N({TEXT("Car"), TEXT("Tank"), TEXT("Flyer"), TEXT("Hover")}), K({TEXT("cohete"), TEXT("rocket"), TEXT("nitro")}) },
		{ "flame", TEXT("The user wants fire, flames or a flamethrower."), TEXT("No fire is asked for."), "Flamethrower", N({TEXT("Front")}), {}, N({TEXT("Car"), TEXT("Tank"), TEXT("Flyer"), TEXT("Hover"), TEXT("Turret"), TEXT("Fortress")}), K({TEXT("fuego"), TEXT("llama"), TEXT("flame"), TEXT("fire"), TEXT("lanzallamas")}) },
		{ "cannon", TEXT("The user wants a cannon, gun or something that shoots projectiles."), TEXT("No cannon or shooting is asked for."), "Cannon", N({TEXT("Top")}), N({TEXT("Bottom")}), N({TEXT("Car"), TEXT("Tank"), TEXT("Flyer"), TEXT("Hover"), TEXT("Fortress"), TEXT("Rocket")}), K({TEXT("cañon"), TEXT("cañón"), TEXT("canon"), TEXT("cannon"), TEXT("dispara"), TEXT("arma")}) },
		{ "balloon", TEXT("The user wants a balloon or floating lift."), TEXT("No balloon is asked for."), "Balloon", N({TEXT("Top")}), {}, N({TEXT("Car"), TEXT("Tank"), TEXT("Flyer"), TEXT("Hover"), TEXT("Turret"), TEXT("Fortress")}), K({TEXT("globo"), TEXT("balloon")}) },
		{ "wing", TEXT("The user wants wings or a glider wing."), TEXT("No wings are asked for."), "Wing", N({TEXT("Left"), TEXT("Right")}), {}, N({TEXT("Car"), TEXT("Flyer"), TEXT("Hover")}), K({TEXT("ala"), TEXT("alas"), TEXT("wing"), TEXT("planeador"), TEXT("glider")}) },
		{ "hover", TEXT("The user wants anti-gravity stones, levitation or floating without wings."), TEXT("No levitation is asked for."), "HoverStone", N({TEXT("Bottom_Front"), TEXT("Bottom_Back")}), {}, N({TEXT("Car"), TEXT("Tank"), TEXT("Rocket")}), K({TEXT("levit"), TEXT("antigrav"), TEXT("flota"), TEXT("hover")}) },
		{ "wall", TEXT("The user wants a wall, armor plate, shield or battering ram on the front."), TEXT("No wall or armor is asked for."), "Wall", N({TEXT("Front")}), {}, N({TEXT("Car"), TEXT("Tank"), TEXT("Turret"), TEXT("Hover")}), K({TEXT("pared"), TEXT("muro"), TEXT("wall"), TEXT("escudo"), TEXT("shield"), TEXT("blind"), TEXT("armor"), TEXT("ariete")}) },
		{ "frost", TEXT("The user wants ice, freezing or a frost emitter that slows things."), TEXT("No ice or freezing is asked for."), "Frost", N({TEXT("Top_Front")}), {}, N({TEXT("Car"), TEXT("Tank"), TEXT("Flyer"), TEXT("Hover"), TEXT("Turret"), TEXT("Fortress")}), K({TEXT("hielo"), TEXT("congel"), TEXT("frost"), TEXT("freeze"), TEXT("ice")}) },
		{ "beam", TEXT("The user wants a laser, ray or energy beam emitter."), TEXT("No laser or beam is asked for."), "Beam", N({TEXT("Top_Back")}), {}, N({TEXT("Car"), TEXT("Tank"), TEXT("Flyer"), TEXT("Hover"), TEXT("Turret"), TEXT("Fortress")}), K({TEXT("laser"), TEXT("láser"), TEXT("rayo"), TEXT("beam"), TEXT("electri")}) },
		{ "bomb", TEXT("The user wants a bomb or explosive that blows up."), TEXT("No bomb or explosive is asked for."), "Bomb", N({TEXT("Right_Top")}), {}, N({TEXT("Car"), TEXT("Tank"), TEXT("Flyer"), TEXT("Hover"), TEXT("Turret")}), K({TEXT("bomba"), TEXT("bomb"), TEXT("explos"), TEXT("dinamita")}) },
		{ "spring", TEXT("The user wants a spring, jumping or bouncing."), TEXT("No spring or jumping is asked for."), "Spring", N({TEXT("Bottom")}), {}, N({TEXT("Car"), TEXT("Tank"), TEXT("Hover")}), K({TEXT("resorte"), TEXT("spring"), TEXT("salta"), TEXT("brinc"), TEXT("jump")}) },
		{ "sail", TEXT("The user wants a sail that catches the wind."), TEXT("No sail is asked for."), "Sail", N({TEXT("Left_Top")}), {}, N({TEXT("Car"), TEXT("Tank"), TEXT("Hover")}), K({TEXT("vela"), TEXT("sail"), TEXT("viento")}) },
		{ "bigwheels", TEXT("The user wants big, off-road or monster wheels."), TEXT("No big wheels are asked for."), NAME_None, {}, {}, N({TEXT("Car")}), K({TEXT("ruedas grandes"), TEXT("monster"), TEXT("todoterreno"), TEXT("off-road"), TEXT("big wheel")}) },
	};
	return T;
}

void UJamBuildGenerator::ChoiceFromKeywords(const FString& Prompt, FJamBuildChoice& OutChoice)
{
	const FString L = Prompt.ToLower();
	OutChoice = FJamBuildChoice();
	if (Has(L, {TEXT("tanque"), TEXT("tank")})) { OutChoice.Archetype = "Tank"; }
	else if (Has(L, {TEXT("fortaleza"), TEXT("castillo"), TEXT("fortress"), TEXT("castle"), TEXT("bunker"), TEXT("barricada")})) { OutChoice.Archetype = "Fortress"; }
	else if (Has(L, {TEXT("hovercraft"), TEXT("plataforma flot"), TEXT("levitante"), TEXT("hover")})) { OutChoice.Archetype = "Hover"; }
	else if (Has(L, {TEXT("volar"), TEXT("vuela"), TEXT("avion"), TEXT("avión"), TEXT("fly"), TEXT("plane"), TEXT("dron"), TEXT("drone"), TEXT("helic")})) { OutChoice.Archetype = "Flyer"; }
	else if (Has(L, {TEXT("torreta"), TEXT("turret")})) { OutChoice.Archetype = "Turret"; }
	else if (Has(L, {TEXT("coche"), TEXT("carro"), TEXT("auto"), TEXT("car"), TEXT("vehic"), TEXT("buggy"), TEXT("camion")})) { OutChoice.Archetype = "Car"; }
	else if (Has(L, {TEXT("cohete"), TEXT("rocket"), TEXT("misil"), TEXT("missile")})) { OutChoice.Archetype = "Rocket"; }
	else { OutChoice.Archetype = "Car"; }

	for (const FJamExtraDef& E : ExtraTable())
	{
		for (const FString& W : E.Keywords)
		{
			if (L.Contains(W))
			{
				OutChoice.Extras.AddUnique(E.Key);
				break;
			}
		}
	}
}

bool UJamBuildGenerator::ComposeRecipe(const FJamBuildChoice& C, FBuildRecipe& OutRecipe, FString& OutNote)
{
	OutRecipe = FBuildRecipe();
	const FName Archetype = C.Archetype;
	OutNote = FString::Printf(TEXT("archetype=%s"), *Archetype.ToString());
	const bool bBigWheels = C.Extras.Contains(FName("bigwheels")) && Archetype == "Car";
	const TCHAR* WheelT = bBigWheels ? TEXT("BigWheel") : TEXT("Wheel");

	if (Archetype == "Car")
	{
		OutRecipe.Name = TEXT("Carro");
		Add(OutRecipe, TEXT("body"), TEXT("Plank"));
		Add(OutRecipe, TEXT("w0"), WheelT, TEXT("body"), TEXT("Left_Front"));
		Add(OutRecipe, TEXT("w1"), WheelT, TEXT("body"), TEXT("Right_Front"));
		Add(OutRecipe, TEXT("w2"), WheelT, TEXT("body"), TEXT("Left_Back"));
		Add(OutRecipe, TEXT("w3"), WheelT, TEXT("body"), TEXT("Right_Back"));
		Add(OutRecipe, TEXT("seat"), TEXT("Seat"), TEXT("body"), TEXT("Top"));
	}
	else if (Archetype == "Tank")
	{
		OutRecipe.Name = TEXT("Tanque");
		Add(OutRecipe, TEXT("body"), TEXT("Plank"));
		Add(OutRecipe, TEXT("w0"), TEXT("BigWheel"), TEXT("body"), TEXT("Left_Front"));
		Add(OutRecipe, TEXT("w1"), TEXT("BigWheel"), TEXT("body"), TEXT("Right_Front"));
		Add(OutRecipe, TEXT("w2"), TEXT("BigWheel"), TEXT("body"), TEXT("Left_Back"));
		Add(OutRecipe, TEXT("w3"), TEXT("BigWheel"), TEXT("body"), TEXT("Right_Back"));
		Add(OutRecipe, TEXT("seat"), TEXT("Seat"), TEXT("body"), TEXT("Top_Back"));
		Add(OutRecipe, TEXT("gun"), TEXT("Cannon"), TEXT("body"), TEXT("Top_Front"));
	}
	else if (Archetype == "Flyer")
	{
		OutRecipe.Name = TEXT("Volador");
		Add(OutRecipe, TEXT("body"), TEXT("Plank"));
		Add(OutRecipe, TEXT("f0"), TEXT("Fan"), TEXT("body"), TEXT("Bottom_Front"));
		Add(OutRecipe, TEXT("f1"), TEXT("Fan"), TEXT("body"), TEXT("Bottom_Back"));
		Add(OutRecipe, TEXT("seat"), TEXT("Seat"), TEXT("body"), TEXT("Top"));
	}
	else if (Archetype == "Hover")
	{
		OutRecipe.Name = TEXT("Hovercraft");
		Add(OutRecipe, TEXT("body"), TEXT("Plank"));
		Add(OutRecipe, TEXT("hv0"), TEXT("HoverStone"), TEXT("body"), TEXT("Bottom_Front"));
		Add(OutRecipe, TEXT("hv1"), TEXT("HoverStone"), TEXT("body"), TEXT("Bottom_Back"));
		Add(OutRecipe, TEXT("seat"), TEXT("Seat"), TEXT("body"), TEXT("Top"));
	}
	else if (Archetype == "Turret")
	{
		OutRecipe.Name = TEXT("Torreta");
		Add(OutRecipe, TEXT("body"), TEXT("Block"));
		Add(OutRecipe, TEXT("gun"), TEXT("Cannon"), TEXT("body"), TEXT("Front"));
		Add(OutRecipe, TEXT("bat"), TEXT("Battery"), TEXT("body"), TEXT("Back"));
	}
	else if (Archetype == "Fortress")
	{
		OutRecipe.Name = TEXT("Fortaleza");
		Add(OutRecipe, TEXT("body"), TEXT("Wall"));
		Add(OutRecipe, TEXT("wl"), TEXT("Wall"), TEXT("body"), TEXT("Left"));
		Add(OutRecipe, TEXT("wr"), TEXT("Wall"), TEXT("body"), TEXT("Right"));
		Add(OutRecipe, TEXT("tower"), TEXT("Block"), TEXT("body"), TEXT("Top_Back"));
	}
	else if (Archetype == "Rocket")
	{
		OutRecipe.Name = TEXT("Cohete");
		Add(OutRecipe, TEXT("body"), TEXT("Cylinder"));
		Add(OutRecipe, TEXT("r0"), TEXT("Rocket"), TEXT("body"), TEXT("Bottom"));
	}
	else
	{
		OutNote += TEXT(" invalid: unknown archetype");
		return false;
	}

	// Extras: una pieza por socket, solo donde el arquetipo lo permite y hasta MaxParts.
	int32 Skipped = 0;
	for (const FJamExtraDef& E : UJamBuildGenerator::ExtraTable())
	{
		if (!C.Extras.Contains(E.Key) || E.PartType.IsNone())
		{
			continue;
		}
		if (E.Allowed.Num() > 0 && !E.Allowed.Contains(Archetype))
		{
			++Skipped;
			continue;
		}
		const TArray<FName>& Socks = (Archetype == "Flyer" && E.FlyerSockets.Num() > 0) ? E.FlyerSockets : E.Sockets;
		for (int32 i = 0; i < Socks.Num(); ++i)
		{
			if (OutRecipe.Parts.Num() >= UJamAssembler::MaxParts)
			{
				++Skipped;
				break;
			}
			FBuildRecipePart P;
			P.Id = FName(*FString::Printf(TEXT("%s%d"), *E.Key.ToString(), i));
			P.Type = E.PartType;
			P.AttachTo = "body";
			P.Socket = Socks[i];
			OutRecipe.Parts.Add(P);
		}
	}
	if (Skipped > 0)
	{
		OutNote += FString::Printf(TEXT(" skipped=%d"), Skipped);
	}
	OutNote += FString::Printf(TEXT(" extras=%d"), C.Extras.Num());

	// Se enciende todo lo que no es estructural/asiento/bateria.
	for (const FBuildRecipePart& P : OutRecipe.Parts)
	{
		FJamPartDef D;
		if (UJamPartCatalog::Find(P.Type, D) && D.Kind != EJamPartKind::Structural && D.Kind != EJamPartKind::Seat && D.Kind != EJamPartKind::Battery)
		{
			OutRecipe.Activate.Add(P.Id);
		}
	}

	FString Err;
	if (!UBuildRecipeLibrary::ValidateRecipe(OutRecipe, nullptr, Err))
	{
		OutNote += TEXT(" invalid: ") + Err;
		return false;
	}
	return true;
}

bool UJamBuildGenerator::GenerateRecipe(const FString& Prompt, FBuildRecipe& OutRecipe, FString& OutNote)
{
	FJamBuildChoice C;
	ChoiceFromKeywords(Prompt, C);
	return ComposeRecipe(C, OutRecipe, OutNote);
}
