#include "Building/JamAssembler.h"

#include "Building/JamBuildPart.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "PhysicsEngine/PhysicsConstraintComponent.h"

namespace
{
	/** Dirección de un token de socket en el marco del vehículo. "Center" = sin desplazamiento. */
	bool DirFromToken(const FString& Token, FVector& Out)
	{
		if (Token == TEXT("Top")) { Out = FVector(0, 0, 1); return true; }
		if (Token == TEXT("Bottom")) { Out = FVector(0, 0, -1); return true; }
		if (Token == TEXT("Front")) { Out = FVector(1, 0, 0); return true; }
		if (Token == TEXT("Back")) { Out = FVector(-1, 0, 0); return true; }
		if (Token == TEXT("Left")) { Out = FVector(0, -1, 0); return true; }
		if (Token == TEXT("Right") || Token == TEXT("Side")) { Out = FVector(0, 1, 0); return true; }
		if (Token == TEXT("Center")) { Out = FVector::ZeroVector; return true; }
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
		FVector Size = FVector(100.0);              // caja ya orientada
		FJamPartDef Def;
		FVector Facing = FVector::ForwardVector;    // hacia donde mira (empuje/disparo)
		FName Parent;
		AJamBuildPart* Actor = nullptr;
	};

	/**
	 * Socket = "Cara[_Borde[_Mira]]" (con "Attach_" opcional delante).
	 *  Cara  : cara del padre donde se pega.
	 *  Borde : corre la pieza hasta alinear ese borde con el del padre ("Center" = no correr).
	 *  Mira  : hacia donde apunta la pieza; por defecto, hacia afuera de la cara.
	 */
	bool ParseSocket(FName Socket, FVector& Dir, FVector& Shift, FVector& Facing)
	{
		FString S = Socket.IsNone() ? FString(TEXT("Top")) : Socket.ToString();
		S.RemoveFromStart(TEXT("Attach_")); // contrato de uniones: "Attach_Left" == "Left"
		TArray<FString> T;
		S.ParseIntoArray(T, TEXT("_"));
		Shift = FVector::ZeroVector;
		if (T.Num() == 0 || T.Num() > 3 || !DirFromToken(T[0], Dir) || Dir.IsZero())
		{
			return false;
		}
		Facing = Dir;
		if (T.Num() > 1 && !DirFromToken(T[1], Shift)) { return false; }
		if (T.Num() > 2 && (!DirFromToken(T[2], Facing) || Facing.IsZero())) { return false; }
		if (!FMath::IsNearlyZero(FVector::DotProduct(Shift, Dir))) { Shift = FVector::ZeroVector; } // borde paralelo a la cara: se ignora
		return true;
	}

	/** Caja de la pieza segun hacia donde mira: las piezas con eje largo (Axis 1) y las placas se orientan. */
	FVector OrientedSize(const FJamPartDef& D, const FVector& Facing)
	{
		// ponytail: permuta la caja (sirve para formas basicas); con meshes reales, leer los sockets Attach_* del mesh.
		const FVector S = D.Size;
		if (D.Axis != 1 && !D.bFaceAligned) { return S; }
		if (FMath::Abs(Facing.Z) > 0.5) { return FVector(S.Z, S.Y, S.X); }
		if (FMath::Abs(Facing.Y) > 0.5) { return FVector(S.Y, S.X, S.Z); }
		return S;
	}

	/** Layout en el marco del vehículo (el padre debe existir antes; se repite hasta resolver). */
	bool LayoutAll(const FBuildRecipe& Recipe, TMap<FName, FPlaced>& Placed, FString& OutError)
	{
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
				Item.Size = Item.Def.Size;
				if (!P.AttachTo.IsNone())
				{
					const FPlaced* Parent = Placed.Find(P.AttachTo);
					if (!Parent)
					{
						continue; // el padre aún no está colocado
					}
					FVector Dir, Shift;
					if (!ParseSocket(P.Socket, Dir, Shift, Item.Facing))
					{
						OutError = FString::Printf(TEXT("Unknown socket '%s' on part '%s'"), *P.Socket.ToString(), *P.Id.ToString());
						return false;
					}
					Item.Size = OrientedSize(Item.Def, Item.Facing);
					const FVector PH = Parent->Size * 0.5;
					const FVector CH = Item.Size * 0.5;
					Item.Center = Parent->Center + Dir * (FVector::DotProduct(Abs(Dir), PH) + FVector::DotProduct(Abs(Dir), CH));
					Item.Center += Shift * (FVector::DotProduct(Abs(Shift), PH) - FVector::DotProduct(Abs(Shift), CH));
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
		return true;
	}

	bool BoxesOverlap(const FPlaced& A, const FPlaced& B)
	{
		const FVector D = Abs(A.Center - B.Center);
		const FVector H = (A.Size + B.Size) * 0.5 - FVector(1.0); // tocarse no es encimarse
		return D.X < H.X && D.Y < H.Y && D.Z < H.Z;
	}
}

bool UJamAssembler::HasOverlap(const FBuildRecipe& Recipe, FName PartId, FString& OutInfo)
{
	TMap<FName, FPlaced> Placed;
	if (!LayoutAll(Recipe, Placed, OutInfo))
	{
		return true;
	}
	for (int32 i = 0; i < Recipe.Parts.Num(); ++i)
	{
		for (int32 j = i + 1; j < Recipe.Parts.Num(); ++j)
		{
			const FName A = Recipe.Parts[i].Id, B = Recipe.Parts[j].Id;
			if ((PartId.IsNone() || A == PartId || B == PartId) && BoxesOverlap(Placed[A], Placed[B]))
			{
				OutInfo = FString::Printf(TEXT("%s overlaps %s"), *A.ToString(), *B.ToString());
				return true;
			}
		}
	}
	return false;
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

	// 1) Layout.
	TMap<FName, FPlaced> Placed;
	if (!LayoutAll(Recipe, Placed, OutError))
	{
		return false;
	}

	// 2) Subir el conjunto para que la pieza más baja quede sobre el origen.
	double MinZ = TNumericLimits<double>::Max();
	for (const auto& KV : Placed)
	{
		MinZ = FMath::Min(MinZ, KV.Value.Center.Z - KV.Value.Size.Z * 0.5);
	}
	const FVector Lift(0, 0, -MinZ + 5.0);

	// 3) Spawn de las piezas + estado compartido de la construccion (energia, masa, miembros; la raiz va primero).
	const TSharedPtr<FJamBuildState> State = MakeShared<FJamBuildState>();
	State->Energy = BaseEnergy;
	for (const FBuildRecipePart& P : Recipe.Parts)
	{
		FPlaced& It = Placed[P.Id];
		// El eje nativo del mesh (+Z) se alinea con la direccion a la que mira la pieza (el cohete, con la punta al frente).
		FQuat AxisRot = FQuat::Identity;
		if (It.Def.Axis == 1) { AxisRot = FQuat::FindBetweenNormals(FVector::UpVector, It.Def.Kind == EJamPartKind::Rocket ? -It.Facing : It.Facing); }
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
		Part->InitFromDef(It.Def, It.Size);
		Part->Build = State;
		State->MassKg += It.Def.MassKg;
		if (It.Def.Kind == EJamPartKind::Battery) { State->Energy += It.Def.Power; }
		if (P.AttachTo.IsNone()) { State->Parts.Insert(Part, 0); } else { State->Parts.Add(Part); }

		// Dirección de empuje/disparo: ventiladores y cohetes empujan al lado contrario de donde miran; el resto, hacia donde mira.
		FVector BuildDir = It.Facing;
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

	// 4) Constraints segun la capacidad de la pieza hija: soldadura, eje con motor, eje libre o bisagra.
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

		const EJamPartKind Kind = Child.Def.Kind;
		const bool bAxleY = Kind == EJamPartKind::Wheel || Kind == EJamPartKind::Hinge;       // gira sobre Y del vehiculo
		const bool bAxleZ = Kind == EJamPartKind::Turntable || Kind == EJamPartKind::Head;    // gira sobre Z del vehiculo
		if (bAxleY || bAxleZ)
		{
			// La constraint gira (twist) sobre su eje X: se orienta ese eje al eje de giro.
			C->SetWorldRotation(bAxleY ? Origin.GetRotation() * FQuat(FVector::UpVector, HALF_PI)
				: FRotationMatrix::MakeFromX(Origin.GetRotation().RotateVector(FVector::UpVector)).ToQuat());
			C->SetLinearXLimit(LCM_Locked, 0);
			C->SetLinearYLimit(LCM_Locked, 0);
			C->SetLinearZLimit(LCM_Locked, 0);
			C->SetAngularSwing1Limit(ACM_Locked, 0);
			C->SetAngularSwing2Limit(ACM_Locked, 0);
			if (Kind == EJamPartKind::Hinge)
			{
				C->SetAngularTwistLimit(ACM_Limited, Child.Def.Power); // Power = grados de libertad
			}
			else
			{
				C->SetAngularTwistLimit(ACM_Free, 0);
			}
			if (Kind == EJamPartKind::Wheel || Kind == EJamPartKind::Turntable)
			{
				C->SetAngularDriveMode(EAngularDriveMode::TwistAndSwing);
				C->SetAngularVelocityDriveTwistAndSwing(true, false);
				C->SetAngularDriveParams(0.f, 5000.f, 0.f);
				C->SetAngularVelocityTarget(FVector::ZeroVector);
				Child.Actor->DriveConstraint = C;
			}
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
// Tablas (arquetipos = plantillas de piezas basicas; extras = lo que el modelo de decision elige)
// y generador decisiones -> receta.
// ---------------------------------------------------------------------------------------------

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
	FBuildRecipePart Pt(const TCHAR* Id, const TCHAR* Type, const TCHAR* Parent = nullptr, const TCHAR* Socket = nullptr)
	{
		FBuildRecipePart P;
		P.Id = Id;
		P.Type = Type;
		P.AttachTo = Parent ? FName(Parent) : NAME_None;
		P.Socket = Socket ? FName(Socket) : NAME_None;
		return P;
	}
	FJamArchetypeDef Arch(const TCHAR* Name, const TCHAR* Desc, TArray<FString> Keys, TArray<FBuildRecipePart> Parts)
	{
		FJamArchetypeDef A;
		A.Name = Name;
		A.Description = Desc;
		A.Keywords = MoveTemp(Keys);
		A.Parts = MoveTemp(Parts);
		return A;
	}
	FJamExtraDef Extra(const TCHAR* Key, const TCHAR* True, const TCHAR* False, const TCHAR* Type, TArray<FString> Sockets, TArray<FName> Allowed, TArray<FString> Keys, bool bWeapon = false)
	{
		FJamExtraDef E;
		E.Key = Key;
		E.TrueText = True;
		E.FalseText = False;
		E.PartType = Type ? FName(Type) : NAME_None;
		E.Sockets = MoveTemp(Sockets);
		E.Allowed = MoveTemp(Allowed);
		E.Keywords = MoveTemp(Keys);
		E.bWeapon = bWeapon;
		return E;
	}
	TArray<FName> Join(std::initializer_list<TArray<FName>> Ls)
	{
		TArray<FName> O;
		for (const TArray<FName>& L : Ls) { O.Append(L); }
		return O;
	}

	// Caras candidatas (primera libre gana). Atras mirando atras = empuja hacia delante.
	#define JAM_REAR TEXT("Back|Top_Back_Back|Left_Back_Back|Right_Back_Back|Top_Center_Back")
	// Arriba o al frente, siempre mirando al frente.
	#define JAM_FWD TEXT("Top_Front_Front|Top_Center_Front|Top_Back_Front|Top_Left_Front|Top_Right_Front|Left_Front_Front|Right_Front_Front|Bottom_Front_Front|Bottom_Back_Front")
	#define JAM_FRONT TEXT("Front|") JAM_FWD
	#define JAM_ANY TEXT("Top_Back|Top_Front|Top|Top_Left|Top_Right|Back|Front")
}

const TArray<FJamArchetypeDef>& UJamBuildGenerator::ArchetypeTable()
{
	// Orden fijo: es el orden de las opciones que ve Jev y la prioridad del respaldo por palabras clave.
	static const TArray<FJamArchetypeDef> T = {
		Arch(TEXT("Tank"), TEXT("An armored ground vehicle with a big gun: tank, assault vehicle, war machine."),
			K({TEXT("tanque"), TEXT("tank")}),
			{ Pt(TEXT("body"), TEXT("Plank")), Pt(TEXT("w0"), TEXT("BigWheel"), TEXT("body"), TEXT("Left_Front")), Pt(TEXT("w1"), TEXT("BigWheel"), TEXT("body"), TEXT("Right_Front")),
			  Pt(TEXT("w2"), TEXT("BigWheel"), TEXT("body"), TEXT("Left_Back")), Pt(TEXT("w3"), TEXT("BigWheel"), TEXT("body"), TEXT("Right_Back")),
			  Pt(TEXT("seat"), TEXT("Seat"), TEXT("body"), TEXT("Top_Back")), Pt(TEXT("gun"), TEXT("Cannon"), TEXT("body"), TEXT("Top_Front_Front")) }),
		Arch(TEXT("Fortress"), TEXT("A static defensive structure made of walls: fort, castle, bunker, barricade, wall."),
			K({TEXT("fortaleza"), TEXT("castillo"), TEXT("fortress"), TEXT("castle"), TEXT("bunker"), TEXT("búnker"), TEXT("barricada"), TEXT("muralla")}),
			{ Pt(TEXT("body"), TEXT("Wall")), Pt(TEXT("wl"), TEXT("Wall"), TEXT("body"), TEXT("Left_Front")), Pt(TEXT("wr"), TEXT("Wall"), TEXT("body"), TEXT("Right_Front")),
			  Pt(TEXT("tower"), TEXT("Block"), TEXT("body"), TEXT("Top")) }),
		Arch(TEXT("Catapult"), TEXT("A machine that throws things through the air: catapult, launcher, trebuchet."),
			K({TEXT("catapulta"), TEXT("catapult"), TEXT("trebuchet"), TEXT("lanzador")}),
			{ Pt(TEXT("body"), TEXT("Plank")), Pt(TEXT("blk"), TEXT("Block"), TEXT("body"), TEXT("Top_Back")), Pt(TEXT("s0"), TEXT("Spring"), TEXT("body"), TEXT("Top_Front")) }),
		Arch(TEXT("Spinner"), TEXT("A stationary machine that spins in place spraying its weapons all around: sprinkler, spinning top, rotating trap."),
			K({TEXT("aspersor"), TEXT("trompo"), TEXT("spinner"), TEXT("sprinkler")}),
			{ Pt(TEXT("body"), TEXT("Block")), Pt(TEXT("mount"), TEXT("Turntable"), TEXT("body"), TEXT("Top")),
			  Pt(TEXT("f0"), TEXT("Flamethrower"), TEXT("mount"), TEXT("Front")), Pt(TEXT("f1"), TEXT("Flamethrower"), TEXT("mount"), TEXT("Back")) }),
		Arch(TEXT("Roller"), TEXT("A single wheel that rolls by itself with weapons on its sides: lone wheel, unicycle, rolling wheel of fire."),
			K({TEXT("rueda infernal"), TEXT("rueda sola"), TEXT("rueda que"), TEXT("llanta con"), TEXT("roller"), TEXT("monociclo"), TEXT("unicycle")}),
			{ Pt(TEXT("body"), TEXT("BigWheel")), Pt(TEXT("f0"), TEXT("Flamethrower"), TEXT("body"), TEXT("Right")), Pt(TEXT("f1"), TEXT("Flamethrower"), TEXT("body"), TEXT("Left")) }),
		Arch(TEXT("Airship"), TEXT("A slow aircraft lifted by balloons: hot air balloon, airship, blimp, zeppelin."),
			K({TEXT("dirigible"), TEXT("zeppelin"), TEXT("globo aerost"), TEXT("airship"), TEXT("blimp")}),
			{ Pt(TEXT("body"), TEXT("Plank")), Pt(TEXT("b0"), TEXT("Balloon"), TEXT("body"), TEXT("Top_Front")), Pt(TEXT("b1"), TEXT("Balloon"), TEXT("body"), TEXT("Top_Back")),
			  Pt(TEXT("fire"), TEXT("Flamethrower"), TEXT("body"), TEXT("Top")), Pt(TEXT("fan"), TEXT("Fan"), TEXT("body"), TEXT("Back")), Pt(TEXT("seat"), TEXT("Seat"), TEXT("body"), TEXT("Bottom")) }),
		Arch(TEXT("Glider"), TEXT("An aircraft with no wheels that glides on its wings after a rocket launch: glider, airplane."),
			K({TEXT("planeador"), TEXT("glider"), TEXT("ala delta"), TEXT("avion"), TEXT("avión"), TEXT("airplane"), TEXT("plane")}),
			{ Pt(TEXT("body"), TEXT("Plank")), Pt(TEXT("wl"), TEXT("Wing"), TEXT("body"), TEXT("Left")), Pt(TEXT("wr"), TEXT("Wing"), TEXT("body"), TEXT("Right")),
			  Pt(TEXT("r0"), TEXT("Rocket"), TEXT("body"), TEXT("Back")), Pt(TEXT("seat"), TEXT("Seat"), TEXT("body"), TEXT("Top")), Pt(TEXT("stab"), TEXT("Stabilizer"), TEXT("body"), TEXT("Top_Back")) }),
		Arch(TEXT("Bike"), TEXT("A two-wheeled vehicle: motorcycle, bike, scooter."),
			K({TEXT("moto"), TEXT("bike"), TEXT("scooter")}),
			{ Pt(TEXT("body"), TEXT("Plank")), Pt(TEXT("w0"), TEXT("BigWheel"), TEXT("body"), TEXT("Front")), Pt(TEXT("w1"), TEXT("BigWheel"), TEXT("body"), TEXT("Back")),
			  Pt(TEXT("seat"), TEXT("Seat"), TEXT("body"), TEXT("Top")), Pt(TEXT("stab"), TEXT("Stabilizer"), TEXT("body"), TEXT("Top_Back")) }),
		Arch(TEXT("Sled"), TEXT("A vehicle without wheels that slides on the ground pushed by a fan: sled, sleigh, toboggan."),
			K({TEXT("trineo"), TEXT("sled"), TEXT("sleigh")}),
			{ Pt(TEXT("body"), TEXT("Sled")), Pt(TEXT("seat"), TEXT("Seat"), TEXT("body"), TEXT("Top")), Pt(TEXT("fan"), TEXT("Fan"), TEXT("body"), TEXT("Back")) }),
		Arch(TEXT("Hover"), TEXT("A platform that levitates just above the ground on anti-gravity stones: hovercraft, hoverboard, floating platform."),
			K({TEXT("hovercraft"), TEXT("aerodeslizador"), TEXT("plataforma flot"), TEXT("levitante"), TEXT("hover")}),
			{ Pt(TEXT("body"), TEXT("Plank")), Pt(TEXT("hv0"), TEXT("HoverStone"), TEXT("body"), TEXT("Bottom_Front")), Pt(TEXT("hv1"), TEXT("HoverStone"), TEXT("body"), TEXT("Bottom_Back")),
			  Pt(TEXT("seat"), TEXT("Seat"), TEXT("body"), TEXT("Top")), Pt(TEXT("stab"), TEXT("Stabilizer"), TEXT("body"), TEXT("Bottom")) }),
		Arch(TEXT("Flyer"), TEXT("A machine that flies with propellers pointing down: drone, helicopter, quadcopter."),
			K({TEXT("volar"), TEXT("vuela"), TEXT("fly"), TEXT("dron"), TEXT("drone"), TEXT("helicop"), TEXT("helicóp")}),
			{ Pt(TEXT("body"), TEXT("Plank")), Pt(TEXT("f0"), TEXT("Fan"), TEXT("body"), TEXT("Bottom_Front")), Pt(TEXT("f1"), TEXT("Fan"), TEXT("body"), TEXT("Bottom_Back")),
			  Pt(TEXT("seat"), TEXT("Seat"), TEXT("body"), TEXT("Top")), Pt(TEXT("stab"), TEXT("Stabilizer"), TEXT("body"), TEXT("Top_Back")) }),
		Arch(TEXT("Turret"), TEXT("A single small gun fixed on a block: turret, gun emplacement."),
			K({TEXT("torreta"), TEXT("turret")}),
			{ Pt(TEXT("body"), TEXT("Block")), Pt(TEXT("gun"), TEXT("Cannon"), TEXT("body"), TEXT("Front")), Pt(TEXT("bat"), TEXT("Battery"), TEXT("body"), TEXT("Back")) }),
		Arch(TEXT("Car"), TEXT("A ground vehicle on four wheels (car, cart, truck, buggy), even when it also carries wings, jets, sails or weapons."),
			K({TEXT("coche"), TEXT("carro"), TEXT("auto"), TEXT("car"), TEXT("vehic"), TEXT("buggy"), TEXT("camion")}),
			{ Pt(TEXT("body"), TEXT("Plank")), Pt(TEXT("w0"), TEXT("Wheel"), TEXT("body"), TEXT("Left_Front")), Pt(TEXT("w1"), TEXT("Wheel"), TEXT("body"), TEXT("Right_Front")),
			  Pt(TEXT("w2"), TEXT("Wheel"), TEXT("body"), TEXT("Left_Back")), Pt(TEXT("w3"), TEXT("Wheel"), TEXT("body"), TEXT("Right_Back")), Pt(TEXT("seat"), TEXT("Seat"), TEXT("body"), TEXT("Top")) }),
		Arch(TEXT("Rocket"), TEXT("A rocket or missile that launches upward."),
			K({TEXT("cohete"), TEXT("rocket"), TEXT("misil"), TEXT("missile")}),
			{ Pt(TEXT("body"), TEXT("Cylinder")), Pt(TEXT("r0"), TEXT("Rocket"), TEXT("body"), TEXT("Bottom")) }),
	};
	return T;
}

const TArray<FJamExtraDef>& UJamBuildGenerator::ExtraTable()
{
	static const TArray<FName> Veh = N({TEXT("Car"), TEXT("Tank"), TEXT("Bike"), TEXT("Sled"), TEXT("Hover"), TEXT("Flyer"), TEXT("Glider"), TEXT("Airship")});
	static const TArray<FName> Ground = N({TEXT("Car"), TEXT("Tank"), TEXT("Bike"), TEXT("Sled")});
	static const TArray<FName> Static = N({TEXT("Turret"), TEXT("Fortress"), TEXT("Catapult"), TEXT("Spinner")});
	static const TArray<FName> NoRoller = Join({Veh, Static, N({TEXT("Rocket")})});
	static const TArray<FName> All;

	// Orden = prioridad cuando se llega al limite de piezas (MaxParts). Los "mount" van primero: las armas se montan en ellos.
	// Textos: cada true/false habla de UNA sola cosa y excluye a sus vecinos (fan vs turbine vs jet, beam vs shock...).
	static const TArray<FJamExtraDef> T = []()
	{
		TArray<FJamExtraDef> X = {
			Extra(TEXT("head"), TEXT("The user wants it to aim at or track enemies by itself: auto-aim, self-aiming head, sensor that follows targets."), TEXT("Nothing is asked to aim or track targets by itself."),
				TEXT("Head"), K({TEXT("Top_Front|Top|Top_Back|Top_Left|Top_Right")}), Join({Veh, Static}),
				K({TEXT("cabeza"), TEXT("apunta sol"), TEXT("apunte sol"), TEXT("automátic"), TEXT("automatic"), TEXT("centinela"), TEXT("sentry"), TEXT("auto-aim")})),
			Extra(TEXT("spin"), TEXT("The user explicitly asks for something that keeps spinning around: turntable, rotor, rotating platform, weapons that spin in circles."), TEXT("Nothing is asked to spin in circles. Wheels, turrets, aiming and tracking do not count as spinning."),
				TEXT("Turntable"), K({TEXT("Top_Front|Top|Top_Back|Top_Left|Top_Right")}), Join({Veh, Static}),
				K({TEXT("giratori"), TEXT("que gire"), TEXT("gira "), TEXT("rotor"), TEXT("spinning"), TEXT("rotating"), TEXT("tornamesa")})),
			Extra(TEXT("fan"), TEXT("The user explicitly asks for a fan, ventilator or propeller."), TEXT("No fan or propeller is mentioned. Turbines, jets and rockets do not count as a fan."),
				TEXT("Fan"), K({JAM_REAR}), Veh, K({TEXT("ventilador"), TEXT("fan"), TEXT("helice"), TEXT("hélice"), TEXT("propel")})),
			Extra(TEXT("jet"), TEXT("The user explicitly asks for a jet engine, reactor or jet thruster."), TEXT("No jet engine is mentioned. Fans, turbines and rockets do not count as a jet."),
				TEXT("Jet"), K({JAM_REAR}), Veh, K({TEXT("jet"), TEXT("reactor"), TEXT("turbo "), TEXT("propulsor")})),
			Extra(TEXT("turbine"), TEXT("The user explicitly asks for a turbine."), TEXT("The word turbine is not mentioned. Fans, propellers and jets do not count as a turbine."),
				TEXT("Turbine"), K({TEXT("Top_Back_Back|Back|Top_Center_Back")}), Veh, K({TEXT("turbina"), TEXT("turbine")})),
			Extra(TEXT("rocket"), TEXT("The user asks for a rocket, nitro or a rocket booster."), TEXT("No rocket or nitro is mentioned. Jets and fans do not count as a rocket."),
				TEXT("Rocket"), K({JAM_REAR}), Veh, K({TEXT("cohete"), TEXT("rocket"), TEXT("nitro"), TEXT("booster")})),
			Extra(TEXT("flame"), TEXT("The user wants fire, flames or a flamethrower."), TEXT("No fire is asked for."),
				TEXT("Flamethrower"), K({JAM_FRONT}), All, K({TEXT("fuego"), TEXT("llama"), TEXT("flame"), TEXT("fire"), TEXT("lanzallamas"), TEXT("quem")}), true),
			Extra(TEXT("cannon"), TEXT("The user wants a cannon, gun or something that shoots projectiles."), TEXT("No cannon or projectile shooting is asked for. Lasers, fire and ice do not count."),
				TEXT("Cannon"), K({JAM_FWD}), All, K({TEXT("cañon"), TEXT("cañón"), TEXT("canon"), TEXT("cannon"), TEXT("dispara"), TEXT("artiller"), TEXT("gun")}), true),
			Extra(TEXT("balloon"), TEXT("The user wants a balloon."), TEXT("No balloon is asked for."),
				TEXT("Balloon"), K({TEXT("seat:Top|Top|Top_Back|Top_Front")}), NoRoller, K({TEXT("globo"), TEXT("balloon")})),
			Extra(TEXT("wing"), TEXT("The user wants wings or a glider wing."), TEXT("No wings are asked for."),
				TEXT("Wing"), K({TEXT("Left"), TEXT("Right")}), N({TEXT("Car"), TEXT("Bike"), TEXT("Sled"), TEXT("Hover"), TEXT("Flyer"), TEXT("Airship")}),
				K({TEXT(" ala"), TEXT("alas"), TEXT("wing"), TEXT("planeador"), TEXT("glider")})),
			Extra(TEXT("sail"), TEXT("The user wants a sail that catches the wind."), TEXT("No sail is asked for."),
				TEXT("Sail"), K({TEXT("Top_Front|Top_Back|Top")}), Join({Ground, N({TEXT("Hover"), TEXT("Airship")})}), K({TEXT("vela"), TEXT("sail"), TEXT("viento")})),
			Extra(TEXT("hover"), TEXT("The user wants anti-gravity stones or levitation."), TEXT("No levitation or anti-gravity is asked for. Balloons, wings and propellers do not count."),
				TEXT("HoverStone"), K({TEXT("Bottom_Front"), TEXT("Bottom_Back")}), NoRoller, K({TEXT("levit"), TEXT("antigrav"), TEXT("flota"), TEXT("hover")})),
			Extra(TEXT("wall"), TEXT("The user wants a shield, armor plate, wall or battering ram on the front."), TEXT("No shield, armor or wall is asked for."),
				TEXT("Panel"), K({TEXT("Front_Bottom|Front")}), Join({Veh, N({TEXT("Turret"), TEXT("Catapult"), TEXT("Spinner")})}),
				K({TEXT("pared"), TEXT("muro"), TEXT("wall"), TEXT("escudo"), TEXT("shield"), TEXT("blind"), TEXT("armor"), TEXT("ariete")})),
			Extra(TEXT("frost"), TEXT("The user wants ice, freezing or a frost emitter that slows things."), TEXT("No ice or freezing is asked for."),
				TEXT("Frost"), K({JAM_FWD}), All, K({TEXT("hielo"), TEXT("congel"), TEXT("frost"), TEXT("freez"), TEXT("escarcha")}), true),
			Extra(TEXT("beam"), TEXT("The user wants a laser or a long-range energy beam."), TEXT("No laser or beam is asked for. Electricity and lightning do not count as a laser."),
				TEXT("Beam"), K({JAM_FWD}), All, K({TEXT("laser"), TEXT("láser"), TEXT("beam"), TEXT("haz de")}), true),
			Extra(TEXT("shock"), TEXT("The user wants electricity, lightning, an electric shock or a tesla coil."), TEXT("No electricity or lightning is asked for. Lasers do not count as electricity."),
				TEXT("Shock"), K({JAM_FWD}), All, K({TEXT("electri"), TEXT("eléctri"), TEXT("rayo"), TEXT("relampago"), TEXT("relámpago"), TEXT("descarga"), TEXT("tesla"), TEXT("shock"), TEXT("lightning"), TEXT("taser")}), true),
			Extra(TEXT("bomb"), TEXT("The user wants a bomb or explosive that blows up."), TEXT("No bomb or explosive is asked for."),
				TEXT("Bomb"), K({TEXT("Right|Left|Top_Back|Top_Front|Back")}), All, K({TEXT("bomba"), TEXT("bomb"), TEXT("explos"), TEXT("dinamita"), TEXT("tnt")})),
			Extra(TEXT("spring"), TEXT("The user wants a spring, jumping or bouncing."), TEXT("No spring or jumping is asked for."),
				TEXT("Spring"), K({TEXT("Bottom|Bottom_Front|Bottom_Back")}), Ground, K({TEXT("resorte"), TEXT("muelle"), TEXT("spring"), TEXT("salta"), TEXT("brinc"), TEXT("jump"), TEXT("bounce")})),
			Extra(TEXT("stabilizer"), TEXT("The user explicitly asks for a stabilizer, gyroscope or for it to keep its balance."), TEXT("No stabilizer, gyroscope or balancing is mentioned."),
				TEXT("Stabilizer"), K({JAM_ANY}), Veh, K({TEXT("estabiliz"), TEXT("giroscop"), TEXT("equilibr"), TEXT("stabiliz"), TEXT("gyro")})),
			Extra(TEXT("stake"), TEXT("The user wants it anchored, staked or fixed to the ground."), TEXT("No anchoring to the ground is asked for."),
				TEXT("Stake"), K({TEXT("Back_Bottom|Left_Bottom|Right_Bottom|Front_Bottom")}), Static, K({TEXT("estaca"), TEXT("ancla"), TEXT("clavad"), TEXT("fijo al suelo"), TEXT("stake"), TEXT("anchor")})),
			Extra(TEXT("battery"), TEXT("The user wants a battery, extra power or longer running time."), TEXT("No battery or extra power is asked for."),
				TEXT("Battery"), K({JAM_ANY}), All, K({TEXT("bateria"), TEXT("batería"), TEXT("pila"), TEXT("battery")})),
			Extra(TEXT("light"), TEXT("The user wants a light, headlight, lamp or spotlight."), TEXT("No light or lamp is asked for. Lasers and fire do not count as a light."),
				TEXT("Light"), K({JAM_FRONT}), All, K({TEXT("luz"), TEXT("luces"), TEXT("faro"), TEXT("lampara"), TEXT("lámpara"), TEXT("linterna"), TEXT("headlight"), TEXT("lamp"), TEXT("spotlight")}), true),
			Extra(TEXT("hydrant"), TEXT("The user wants water, a hose, a hydrant or a water cannon."), TEXT("No water or hose is asked for."),
				TEXT("Hydrant"), K({JAM_FRONT}), All, K({TEXT("agua"), TEXT("manguera"), TEXT("hidrante"), TEXT("chorro"), TEXT("water"), TEXT("hose"), TEXT("hydrant")}), true),
			Extra(TEXT("hinge"), TEXT("The user wants a hinge, door, hatch, flap or swinging part."), TEXT("No hinge or swinging part is asked for."),
				TEXT("Hinge"), K({TEXT("Top_Back|Top_Front|Top")}), Join({Veh, Static}), K({TEXT("bisagra"), TEXT("puerta"), TEXT("compuerta"), TEXT("escotilla"), TEXT("hinge"), TEXT("door"), TEXT("flap"), TEXT("hatch")})),
			Extra(TEXT("ball"), TEXT("The user wants a heavy ball, sphere, boulder or wrecking ball."), TEXT("No ball or boulder is asked for. Balloons and bombs do not count."),
				TEXT("Ball"), K({TEXT("Front|Back|Top_Front|Top_Back")}), All, K({TEXT("bola"), TEXT("esfera"), TEXT("pelota"), TEXT("boulder"), TEXT("sphere"), TEXT("wrecking")})),
			Extra(TEXT("girder"), TEXT("The user wants a long beam, girder, pole, lance or ramming arm sticking out."), TEXT("No long girder, pole or lance is asked for."),
				TEXT("Girder"), K({TEXT("Front|Back|Top")}), NoRoller, K({TEXT("viga"), TEXT("larguero"), TEXT("lanza "), TEXT("girder"), TEXT("lance")})),
			Extra(TEXT("bigwheels"), TEXT("The user wants big, off-road or monster wheels."), TEXT("No big wheels are asked for."),
				nullptr, {}, N({TEXT("Car")}), K({TEXT("ruedas grandes"), TEXT("llantas grandes"), TEXT("monster"), TEXT("todoterreno"), TEXT("off-road"), TEXT("big wheel")})),
		};
		for (FJamExtraDef& E : X)
		{
			if (E.Key == "head") { E.MountSockets = TEXT("Top_Center_Front"); }
			if (E.Key == "spin") { E.MountSockets = TEXT("Top_Center_Front|Front|Back|Left|Right"); }
			if (E.Key == "cannon") { E.FlyerSockets = K({TEXT("Bottom_Center_Front|") JAM_FWD}); }
			if (E.Key == "hinge") { E.ChildType = "Panel"; E.ChildSocket = "Top"; }
		}
		return X;
	}();
	return T;
}

void UJamBuildGenerator::ChoiceFromKeywords(const FString& Prompt, FJamBuildChoice& OutChoice)
{
	const FString L = TEXT(" ") + Prompt.ToLower() + TEXT(" ");
	OutChoice = FJamBuildChoice(); // Car por defecto
	bool bFound = false;
	for (const FJamArchetypeDef& A : ArchetypeTable())
	{
		for (const FString& W : A.Keywords)
		{
			if (!bFound && L.Contains(W))
			{
				OutChoice.Archetype = A.Name;
				bFound = true;
			}
		}
	}
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

namespace
{
	/** Prueba cada cara candidata ("padre:Cara|...") y deja la pieza en la primera donde no se encima con nada. */
	bool TryAdd(FBuildRecipe& R, FName Id, FName Type, const FString& Candidates)
	{
		TArray<FString> Opts;
		Candidates.ParseIntoArray(Opts, TEXT("|"));
		for (const FString& Opt : Opts)
		{
			FString Parent = TEXT("body"), Socket = Opt;
			Opt.Split(TEXT(":"), &Parent, &Socket);
			if (!R.Parts.ContainsByPredicate([&](const FBuildRecipePart& P) { return P.Id == FName(*Parent); }))
			{
				continue;
			}
			FBuildRecipePart P;
			P.Id = Id;
			P.Type = Type;
			P.AttachTo = FName(*Parent);
			P.Socket = FName(*Socket);
			R.Parts.Add(P);
			FString Info;
			if (!UJamAssembler::HasOverlap(R, Id, Info))
			{
				return true;
			}
			R.Parts.Pop();
		}
		return false;
	}

	FString WithPrefix(const FString& Candidates, const TCHAR* Prefix)
	{
		TArray<FString> Opts;
		Candidates.ParseIntoArray(Opts, TEXT("|"));
		for (FString& O : Opts) { O = Prefix + O; }
		return FString::Join(Opts, TEXT("|"));
	}
}

bool UJamBuildGenerator::ComposeRecipe(const FJamBuildChoice& C, FBuildRecipe& OutRecipe, FString& OutNote)
{
	OutRecipe = FBuildRecipe();
	const FName Archetype = C.Archetype;
	OutNote = FString::Printf(TEXT("archetype=%s"), *Archetype.ToString());

	const FJamArchetypeDef* Def = ArchetypeTable().FindByPredicate([&](const FJamArchetypeDef& A) { return A.Name == Archetype; });
	if (!Def)
	{
		OutNote += TEXT(" invalid: unknown archetype");
		return false;
	}
	OutRecipe.Name = Archetype.ToString();
	OutRecipe.Parts = Def->Parts;

	// Lo que gira o apunta ("mount"): las armas se montan ahi antes que en el cuerpo.
	FString MountSockets;
	auto MountOf = [](FName Type) -> FString
	{
		for (const FJamExtraDef& E : ExtraTable())
		{
			if (E.PartType == Type && !E.MountSockets.IsEmpty()) { return E.MountSockets; }
		}
		return FString();
	};
	for (FBuildRecipePart& P : OutRecipe.Parts)
	{
		if (P.Id == "mount") { MountSockets = MountOf(P.Type); }
	}

	// Modificadores (no agregan pieza) antes que los extras, porque cambian el espacio libre: ruedas grandes.
	if (C.Extras.Contains(FName("bigwheels")) && Archetype == "Car")
	{
		for (FBuildRecipePart& P : OutRecipe.Parts)
		{
			if (P.Type == "Wheel") { P.Type = "BigWheel"; }
		}
	}

	// Extras: cada pieza va en su primera cara libre; lo que no cabe o no aplica se reporta en skipped=[...].
	TArray<FString> Skipped;
	for (const FJamExtraDef& E : ExtraTable())
	{
		if (!C.Extras.Contains(E.Key))
		{
			continue;
		}
		if (E.Allowed.Num() > 0 && !E.Allowed.Contains(Archetype))
		{
			Skipped.Add(E.Key.ToString());
			continue;
		}
		if (E.PartType.IsNone())
		{
			continue; // modificador, ya aplicado
		}
		const bool bIsMount = !E.MountSockets.IsEmpty();
		const TArray<FString>& Socks = (Archetype == "Flyer" && E.FlyerSockets.Num() > 0) ? E.FlyerSockets : E.Sockets;
		int32 Added = 0;
		for (int32 i = 0; i < Socks.Num(); ++i)
		{
			if (OutRecipe.Parts.Num() >= UJamAssembler::MaxParts || (bIsMount && !MountSockets.IsEmpty()))
			{
				break;
			}
			FString Candidates = Socks[i];
			if (E.bWeapon && !MountSockets.IsEmpty())
			{
				Candidates = WithPrefix(MountSockets, TEXT("mount:")) + TEXT("|") + Candidates;
			}
			const FName Id = bIsMount ? FName("mount") : FName(*FString::Printf(TEXT("%s%d"), *E.Key.ToString(), i));
			if (!TryAdd(OutRecipe, Id, E.PartType, Candidates))
			{
				continue;
			}
			++Added;
			if (!E.ChildType.IsNone() && OutRecipe.Parts.Num() < UJamAssembler::MaxParts)
			{
				TryAdd(OutRecipe, FName(*(Id.ToString() + TEXT("c"))), E.ChildType, Id.ToString() + TEXT(":") + E.ChildSocket.ToString());
			}
			if (bIsMount)
			{
				MountSockets = E.MountSockets;
				// El arma base del arquetipo ("gun") pasa al mount si cabe: cabeza + cañon = torreta autonoma.
				if (FBuildRecipePart* Gun = OutRecipe.Parts.FindByPredicate([](const FBuildRecipePart& P) { return P.Id == "gun"; }))
				{
					const FBuildRecipePart Old = *Gun;
					FString First, Rest, Info;
					if (!MountSockets.Split(TEXT("|"), &First, &Rest)) { First = MountSockets; }
					Gun->AttachTo = "mount";
					Gun->Socket = FName(*First);
					if (UJamAssembler::HasOverlap(OutRecipe, "gun", Info)) { *Gun = Old; }
				}
			}
		}
		if (Added < Socks.Num())
		{
			Skipped.Add(E.Key.ToString());
		}
	}
	if (Skipped.Num() > 0)
	{
		OutNote += FString::Printf(TEXT(" skipped=[%s]"), *FString::Join(Skipped, TEXT(",")));
	}
	TArray<FString> Chosen;
	for (const FName& X : C.Extras) { Chosen.Add(X.ToString()); }
	OutNote += FString::Printf(TEXT(" extras=[%s]"), *FString::Join(Chosen, TEXT(",")));

	// Se enciende todo lo que no es estructural/asiento/bateria.
	for (const FBuildRecipePart& P : OutRecipe.Parts)
	{
		FJamPartDef D;
		if (UJamPartCatalog::Find(P.Type, D) && D.Kind != EJamPartKind::Structural && D.Kind != EJamPartKind::Slide && D.Kind != EJamPartKind::Seat && D.Kind != EJamPartKind::Battery && D.Kind != EJamPartKind::Hinge)
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
