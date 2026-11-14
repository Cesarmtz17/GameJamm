#include "Building/JamAssembler.h"

#include "Building/JamBuildPart.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "PhysicsEngine/PhysicsConstraintComponent.h"

namespace
{
	/** Dirección de un token de socket en el marco del vehículo. */
	bool DirFromToken(const FString& Token, FVector& Out)
	{
		if (Token == TEXT("Top")) { Out = FVector(0, 0, 1); return true; }
		if (Token == TEXT("Bottom")) { Out = FVector(0, 0, -1); return true; }
		if (Token == TEXT("Front")) { Out = FVector(1, 0, 0); return true; }
		if (Token == TEXT("Back")) { Out = FVector(-1, 0, 0); return true; }
		if (Token == TEXT("Left")) { Out = FVector(0, -1, 0); return true; }
		if (Token == TEXT("Right") || Token == TEXT("Side")) { Out = FVector(0, 1, 0); return true; }
		return false;
	}

	FVector Abs(const FVector& V) { return FVector(FMath::Abs(V.X), FMath::Abs(V.Y), FMath::Abs(V.Z)); }

	void LockAll(UPhysicsConstraintComponent* C)
	{
		C->SetLinearXLimit(LCM_Locked, 0);
		C->SetLinearYLimit(LCM_Locked, 0);
		C->SetLinearZLimit(LCM_Locked, 0);
		C->SetAngularSwing1Limit(ACM_Locked, 0);
		C->SetAngularSwing2Limit(ACM_Locked, 0);
		C->SetAngularTwistLimit(ACM_Locked, 0);
	}

	struct FPlaced
	{
		FVector Center = FVector::ZeroVector;
		FJamPartDef Def;
		FVector OutwardDir = FVector::UpVector;
		FName Parent;
		AJamBuildPart* Actor = nullptr;
	};
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

	// 1) Layout en el marco del vehículo (el padre debe existir antes; se repite hasta resolver).
	TMap<FName, FPlaced> Placed;
	TArray<const FBuildRecipePart*> Pending;
	for (const FBuildRecipePart& P : Recipe.Parts)
	{
		Pending.Add(&P);
	}

	int32 Guard = Pending.Num() + 1;
	while (Pending.Num() > 0 && Guard-- > 0)
	{
		for (int32 i = Pending.Num() - 1; i >= 0; --i)
		{
			const FBuildRecipePart& P = *Pending[i];
			FPlaced Item;
			if (!UJamPartCatalog::Find(P.Type, Item.Def))
			{
				OutError = FString::Printf(TEXT("Unknown part type '%s'"), *P.Type.ToString());
				return false;
			}
			Item.Parent = P.AttachTo;

			if (P.AttachTo.IsNone())
			{
				Item.Center = FVector::ZeroVector;
			}
			else
			{
				const FPlaced* Parent = Placed.Find(P.AttachTo);
				if (!Parent)
				{
					continue; // el padre aún no está colocado
				}
				TArray<FString> Tokens;
				(P.Socket.IsNone() ? FString(TEXT("Top")) : P.Socket.ToString()).ParseIntoArray(Tokens, TEXT("_"));
				FVector Dir = FVector::UpVector;
				if (Tokens.Num() == 0 || !DirFromToken(Tokens[0], Dir))
				{
					OutError = FString::Printf(TEXT("Unknown socket '%s' on part '%s'"), *P.Socket.ToString(), *P.Id.ToString());
					return false;
				}
				const FVector PH = Parent->Def.Size * 0.5;
				const FVector CH = Item.Def.Size * 0.5;
				Item.OutwardDir = Dir;
				Item.Center = Parent->Center + Dir * (FVector::DotProduct(Abs(Dir), PH) + FVector::DotProduct(Abs(Dir), CH));
				// token secundario: desplaza hacia un borde del padre (p. ej. Left_Front).
				FVector Shift;
				if (Tokens.Num() > 1 && DirFromToken(Tokens[1], Shift))
				{
					const double Edge = FVector::DotProduct(Abs(Shift), PH) - FVector::DotProduct(Abs(Shift), CH);
					Item.Center += Shift * FMath::Max(0.0, Edge);
				}
			}
			Placed.Add(P.Id, Item);
			Pending.RemoveAt(i);
		}
	}
	if (Pending.Num() > 0)
	{
		OutError = TEXT("Unresolvable parent references");
		return false;
	}

	// 2) Subir el conjunto para que la pieza más baja quede sobre el origen.
	double MinZ = TNumericLimits<double>::Max();
	for (const auto& KV : Placed)
	{
		MinZ = FMath::Min(MinZ, KV.Value.Center.Z - KV.Value.Def.Size.Z * 0.5);
	}
	const FVector Lift(0, 0, -MinZ + 5.0);

	// 3) Spawn de las piezas.
	for (const FBuildRecipePart& P : Recipe.Parts)
	{
		FPlaced& It = Placed[P.Id];
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

		It.Actor = Part;
		OutParts.Add(Part);
	}

	// 4) Constraints: soldadura (fixed) o motor de eje para ruedas.
	for (const FBuildRecipePart& P : Recipe.Parts)
	{
		if (P.AttachTo.IsNone())
		{
			continue;
		}
		FPlaced& Child = Placed[P.Id];
		FPlaced& Parent = Placed[P.AttachTo];

		UPhysicsConstraintComponent* C = NewObject<UPhysicsConstraintComponent>(Child.Actor);
		C->RegisterComponent();
		C->AttachToComponent(Child.Actor->Mesh, FAttachmentTransformRules::KeepRelativeTransform);
		C->SetWorldLocation(Child.Actor->GetActorLocation());
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
			Child.Actor->DriveConstraint = C;
		}
		else
		{
			LockAll(C);
		}
		C->SetConstrainedComponents(Parent.Actor->Mesh, NAME_None, Child.Actor->Mesh, NAME_None);
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

	/** Punto único a reemplazar por Decisions: elegir de un conjunto finito. */
	FName ChooseArchetype(const FString& L)
	{
		if (Has(L, {TEXT("volar"), TEXT("vuela"), TEXT("avion"), TEXT("avión"), TEXT("fly"), TEXT("plane"), TEXT("dron"), TEXT("drone"), TEXT("helic")}))
		{
			return "Flyer";
		}
		if (Has(L, {TEXT("torreta"), TEXT("turret")}))
		{
			return "Turret";
		}
		if (Has(L, {TEXT("cohete"), TEXT("rocket"), TEXT("misil"), TEXT("missile")}))
		{
			return "Rocket";
		}
		return "Car";
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

void UJamBuildGenerator::ChoiceFromKeywords(const FString& Prompt, FJamBuildChoice& OutChoice)
{
	const FString L = Prompt.ToLower();
	OutChoice = FJamBuildChoice();
	OutChoice.Archetype = ChooseArchetype(L);
	OutChoice.bFan = Has(L, {TEXT("ventilador"), TEXT("fan"), TEXT("turbo"), TEXT("helice"), TEXT("hélice")});
	OutChoice.bRocket = Has(L, {TEXT("cohete"), TEXT("rocket"), TEXT("nitro")});
	OutChoice.bFlame = Has(L, {TEXT("fuego"), TEXT("llama"), TEXT("flame"), TEXT("fire")});
	OutChoice.bCannon = Has(L, {TEXT("cañon"), TEXT("cañón"), TEXT("canon"), TEXT("cannon")});
	OutChoice.bBalloon = Has(L, {TEXT("globo"), TEXT("balloon")});
}

bool UJamBuildGenerator::ComposeRecipe(const FJamBuildChoice& C, FBuildRecipe& OutRecipe, FString& OutNote)
{
	OutRecipe = FBuildRecipe();
	const FName Archetype = C.Archetype;
	OutNote = FString::Printf(TEXT("archetype=%s"), *Archetype.ToString());

	if (Archetype == "Car")
	{
		OutRecipe.Name = TEXT("Carro");
		Add(OutRecipe, TEXT("body"), TEXT("Plank"));
		Add(OutRecipe, TEXT("w0"), TEXT("Wheel"), TEXT("body"), TEXT("Left_Front"));
		Add(OutRecipe, TEXT("w1"), TEXT("Wheel"), TEXT("body"), TEXT("Right_Front"));
		Add(OutRecipe, TEXT("w2"), TEXT("Wheel"), TEXT("body"), TEXT("Left_Back"));
		Add(OutRecipe, TEXT("w3"), TEXT("Wheel"), TEXT("body"), TEXT("Right_Back"));
		Add(OutRecipe, TEXT("seat"), TEXT("Seat"), TEXT("body"), TEXT("Top"));
	}
	else if (Archetype == "Flyer")
	{
		OutRecipe.Name = TEXT("Volador");
		Add(OutRecipe, TEXT("body"), TEXT("Plank"));
		Add(OutRecipe, TEXT("f0"), TEXT("Fan"), TEXT("body"), TEXT("Bottom_Front"));
		Add(OutRecipe, TEXT("f1"), TEXT("Fan"), TEXT("body"), TEXT("Bottom_Back"));
		Add(OutRecipe, TEXT("seat"), TEXT("Seat"), TEXT("body"), TEXT("Top"));
	}
	else if (Archetype == "Turret")
	{
		OutRecipe.Name = TEXT("Torreta");
		Add(OutRecipe, TEXT("body"), TEXT("Block"));
		Add(OutRecipe, TEXT("gun"), TEXT("Cannon"), TEXT("body"), TEXT("Front"));
		Add(OutRecipe, TEXT("bat"), TEXT("Battery"), TEXT("body"), TEXT("Back"));
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

	// Extras (solo donde tienen sentido para el arquetipo).
	const bool bGround = Archetype == "Car";
	if (bGround && C.bFan)
	{
		Add(OutRecipe, TEXT("fanx"), TEXT("Fan"), TEXT("body"), TEXT("Back"));
	}
	if ((bGround || Archetype == "Flyer") && C.bRocket)
	{
		Add(OutRecipe, TEXT("rx"), TEXT("Rocket"), TEXT("body"), TEXT("Back"));
	}
	if (Archetype != "Rocket" && C.bFlame)
	{
		Add(OutRecipe, TEXT("flame"), TEXT("Flamethrower"), TEXT("body"), TEXT("Front"));
	}
	if (Archetype != "Turret" && C.bCannon)
	{
		Add(OutRecipe, TEXT("gunx"), TEXT("Cannon"), TEXT("body"), Archetype == "Flyer" ? TEXT("Bottom") : TEXT("Top"));
	}
	if (Archetype != "Rocket" && C.bBalloon)
	{
		Add(OutRecipe, TEXT("ball"), TEXT("Balloon"), TEXT("body"), TEXT("Top"));
	}

	// Se enciende todo lo que no es estructural/asiento/batería.
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
