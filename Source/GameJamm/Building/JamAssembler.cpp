#include "Building/JamAssembler.h"

#include "Building/JamBuildPart.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshSocket.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "PhysicsEngine/PhysicsConstraintComponent.h"

static TAutoConsoleVariable<int32> CVarJamMaxParts(TEXT("jam.Build.MaxParts"), UJamAssembler::MaxParts, TEXT("Limite de piezas por construccion (hasta 100)."));
static TAutoConsoleVariable<int32> CVarJamWeld(TEXT("jam.Build.Weld"), 1,
	TEXT("1 = las piezas sin movimiento propio (estructura, asiento, bateria) se sueldan en un solo cuerpo con su padre; 0 = cada pieza es un cuerpo unido por constraint."));

int32 UJamAssembler::GetMaxParts()
{
	return FMath::Clamp(CVarJamMaxParts.GetValueOnGameThread(), 1, 100);
}

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
		bool bOriented = false;                     // la malla va girada segun la cara (los sockets del mesh ya no coinciden con los ejes)
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

	/**
	 * Distancia del centro del padre a su cara Dir. Si la malla de arte del padre tiene el socket Attach_<Cara>
	 * (contrato de uniones), manda el socket; si no (o si la pieza va girada), la mitad de la caja.
	 */
	double FaceDistance(const FPlaced& P, const FVector& Dir)
	{
		const double Box = FVector::DotProduct(Abs(Dir), P.Size * 0.5);
		if (P.bOriented)
		{
			return Box;
		}
		const TCHAR* Name = Dir.Z > 0.5 ? TEXT("Attach_Top") : Dir.Z < -0.5 ? TEXT("Attach_Bottom") : Dir.X > 0.5 ? TEXT("Attach_Front")
			: Dir.X < -0.5 ? TEXT("Attach_Back") : Dir.Y > 0.5 ? TEXT("Attach_Right") : TEXT("Attach_Left");
		const UStaticMesh* M = AJamBuildPart::FindRealMesh(P.Def.Type);
		const UStaticMeshSocket* S = M ? M->FindSocket(Name) : nullptr;
		const double D = S ? FVector::DotProduct(S->RelativeLocation, Dir) : 0.0;
		return D > 1.0 ? D : Box;
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
					Item.bOriented = Item.Def.Axis == 1 || Item.Def.bFaceAligned;
					const FVector PH = Parent->Size * 0.5;
					const FVector CH = Item.Size * 0.5;
					Item.Center = Parent->Center + Dir * (FaceDistance(*Parent, Dir) + FVector::DotProduct(Abs(Dir), CH));
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
		// Tocarse no es encimarse... salvo dos ruedas: girando pegadas una a otra se frenan y la fisica revienta; van con 10 cm de aire.
		const bool bWheels = A.Def.Kind == EJamPartKind::Wheel && B.Def.Kind == EJamPartKind::Wheel;
		const FVector H = (A.Size + B.Size) * 0.5 + FVector(bWheels ? 10.0 : -1.0);
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
	if (Recipe.Parts.Num() > GetMaxParts())
	{
		OutError = FString::Printf(TEXT("Too many parts (%d > %d)"), Recipe.Parts.Num(), GetMaxParts());
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
	State->Energy = AJamBuildPart::EnergyMode() == 1 ? BaseEnergy : 0.f; // modo 2: solo baterias; el resto sale de la barra del jugador
	for (const FBuildRecipePart& P : Recipe.Parts)
	{
		FPlaced& It = Placed[P.Id];
		// El eje nativo del mesh (+Z) se alinea con la direccion a la que mira la pieza (el cohete, con la punta al frente).
		FQuat AxisRot = FQuat::Identity;
		if (It.Def.Axis == 1) { AxisRot = FQuat::FindBetweenNormals(FVector::UpVector, It.Def.Kind == EJamPartKind::Rocket ? -It.Facing : It.Facing); }
		if (It.Def.Axis == 2) { AxisRot = FQuat::FindBetweenNormals(FVector::UpVector, FVector::RightVector); }
		// Malla de arte: su +X es hacia donde mira (cohete y jet: su +X es la punta, la tobera va en -X).
		const bool bNoseFirst = It.Def.Kind == EJamPartKind::Rocket || It.Def.Type == "Jet";
		const bool bOriented = !P.AttachTo.IsNone() && (It.Def.Axis == 1 || It.Def.bFaceAligned);
		const FQuat VisualRot = Origin.GetRotation() * (bOriented ? FRotationMatrix::MakeFromX(bNoseFirst ? -It.Facing : It.Facing).ToQuat() : FQuat::Identity);

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
		Part->InitFromDef(It.Def, It.Size, VisualRot);
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

	// 4) Uniones segun la capacidad de la pieza hija: soldadura en un cuerpo, eje con motor, eje libre o bisagra.
	// (El padre va siempre antes que el hijo en este recorrido: se ordena por profundidad.)
	TMap<FName, float> Welded; // raiz de la isla soldada -> masa añadida
	auto RootOf = [&Placed](FName Id)
	{
		while (Placed[Id].Actor->bWelded) { Id = Placed[Id].Parent; }
		return Id;
	};
	TArray<const FBuildRecipePart*> ByDepth;
	for (const FBuildRecipePart& P : Recipe.Parts) { ByDepth.Add(&P); }
	auto Depth = [&Placed](FName Id) { int32 D = 0; while (!Placed[Id].Parent.IsNone() && D < 200) { Id = Placed[Id].Parent; ++D; } return D; };
	ByDepth.StableSort([&Depth](const FBuildRecipePart& A, const FBuildRecipePart& B) { return Depth(A.Id) < Depth(B.Id); });
	for (const FBuildRecipePart* PP : ByDepth)
	{
		const FBuildRecipePart& P = *PP;
		if (P.AttachTo.IsNone())
		{
			continue;
		}
		FPlaced& Child = Placed[P.Id];
		FPlaced& Parent = Placed[P.AttachTo];

		// Pegamento: una pieza sin movimiento ni fuerza propia (estructura, asiento, bateria, trineo) pegada a otra igual
		// se suelda en UN solo cuerpo rigido con ella. 25 ruedas sobre 5 tablas = 1 chasis + 25 ejes, no 30 cuerpos blandos.
		auto Inert = [](EJamPartKind Kd) { return Kd == EJamPartKind::Structural || Kd == EJamPartKind::Seat || Kd == EJamPartKind::Battery || Kd == EJamPartKind::Slide; };
		if (CVarJamWeld.GetValueOnGameThread() != 0 && Inert(Child.Def.Kind) && Inert(Parent.Def.Kind))
		{
			Child.Actor->Mesh->AttachToComponent(Parent.Actor->Mesh, FAttachmentTransformRules(EAttachmentRule::KeepWorld, EAttachmentRule::KeepWorld, EAttachmentRule::KeepWorld, true));
			Child.Actor->bWelded = true;
			Welded.FindOrAdd(RootOf(P.AttachTo)) += Child.Def.MassKg;
			continue;
		}

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
				// Motor con fuerza limitada (no infinita): una rueda sola no arrastra cualquier peso. ponytail: valor inicial, afinar jugando.
				C->SetAngularDriveParams(0.f, 5000.f, Kind == EJamPartKind::Wheel ? 2000000.f : 0.f);
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
	// La masa del cuerpo soldado es la suma de sus piezas.
	for (const auto& KV : Welded)
	{
		Placed[KV.Key].Actor->Mesh->SetMassOverrideInKg(NAME_None, Placed[KV.Key].Def.MassKg + KV.Value, true);
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
			{ Pt(TEXT("body"), TEXT("Plank")), Pt(TEXT("blk"), TEXT("Block"), TEXT("body"), TEXT("Top_Back")), Pt(TEXT("s0"), TEXT("Spring"), TEXT("body"), TEXT("Top_Front")),
			  Pt(TEXT("w0"), TEXT("Wheel"), TEXT("body"), TEXT("Left")), Pt(TEXT("w1"), TEXT("Wheel"), TEXT("body"), TEXT("Right")),
			  Pt(TEXT("arm"), TEXT("Girder"), TEXT("body"), TEXT("Back")), Pt(TEXT("ball"), TEXT("Ball"), TEXT("body"), TEXT("Front")) }),
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
			{ Pt(TEXT("body"), TEXT("Cylinder")), Pt(TEXT("r0"), TEXT("Rocket"), TEXT("body"), TEXT("Left_Center_Bottom")), Pt(TEXT("r1"), TEXT("Rocket"), TEXT("body"), TEXT("Right_Center_Bottom")),
			  Pt(TEXT("stab"), TEXT("Stabilizer"), TEXT("body"), TEXT("Top")) }),
		Arch(TEXT("Bridge"), TEXT("A long flat walkway made of planks with no moving parts: bridge, ramp, walkway, platform."),
			K({TEXT("puente"), TEXT("bridge"), TEXT("pasarela"), TEXT("walkway")}),
			{ Pt(TEXT("body"), TEXT("Plank")), Pt(TEXT("p1"), TEXT("Plank"), TEXT("body"), TEXT("Front")), Pt(TEXT("p2"), TEXT("Plank"), TEXT("body"), TEXT("Back")),
			  Pt(TEXT("p3"), TEXT("Plank"), TEXT("p1"), TEXT("Front")), Pt(TEXT("p4"), TEXT("Plank"), TEXT("p2"), TEXT("Back")) }),
	};
	return T;
}

const TArray<FJamExtraDef>& UJamBuildGenerator::ExtraTable()
{
	static const TArray<FName> Veh = N({TEXT("Car"), TEXT("Tank"), TEXT("Bike"), TEXT("Sled"), TEXT("Hover"), TEXT("Flyer"), TEXT("Glider"), TEXT("Airship")});
	static const TArray<FName> Ground = N({TEXT("Car"), TEXT("Tank"), TEXT("Bike"), TEXT("Sled")});
	static const TArray<FName> Static = N({TEXT("Turret"), TEXT("Fortress"), TEXT("Catapult"), TEXT("Spinner"), TEXT("Bridge")});
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
			Extra(TEXT("spin"), TEXT("The user explicitly asks for something that keeps spinning around: turntable, rotor, rotating platform, weapons that spin in circles."), TEXT("Nothing is asked to spin in circles. Wheels, fans, propellers, turbines, turrets, aiming and tracking do not count as spinning."),
				TEXT("Turntable"), K({TEXT("Top_Front|Top|Top_Back|Top_Left|Top_Right")}), Join({Veh, Static}),
				K({TEXT("giratori"), TEXT("que gire"), TEXT("gira "), TEXT("rotor"), TEXT("spinning"), TEXT("rotating"), TEXT("tornamesa")})),
			Extra(TEXT("fan"), TEXT("The user explicitly asks for a fan, ventilator or propeller."), TEXT("No fan or propeller is mentioned. Turbines, jets and rockets do not count as a fan."),
				TEXT("Fan"), K({JAM_REAR}), Veh, K({TEXT("ventilador"), TEXT("fan"), TEXT("helice"), TEXT("hélice"), TEXT("propel")})),
			Extra(TEXT("jet"), TEXT("The user explicitly asks for a jet engine, reactor or jet thruster."), TEXT("No jet engine is mentioned. Fans, turbines and rockets do not count as a jet."),
				TEXT("Jet"), K({JAM_REAR}), Veh, K({TEXT("jet"), TEXT("reactor"), TEXT("turbo ")})),
			Extra(TEXT("turbine"), TEXT("The user explicitly asks for a turbine."), TEXT("The word turbine is not mentioned. Fans, propellers and jets do not count as a turbine."),
				TEXT("Turbine"), K({TEXT("Top_Back_Back|Back|Top_Center_Back")}), Veh, K({TEXT("turbina"), TEXT("turbine")})),
			Extra(TEXT("rocket"), TEXT("The user asks for a rocket, nitro or a rocket booster."), TEXT("No rocket or nitro is mentioned. Jets and fans do not count as a rocket."),
				TEXT("Rocket"), K({JAM_REAR}), Veh, K({TEXT("cohete"), TEXT("rocket"), TEXT("nitro"), TEXT("booster")})),
			Extra(TEXT("flame"), TEXT("The user explicitly asks for a flamethrower, flames or to burn things."), TEXT("No flamethrower or burning is mentioned. Rockets, jets and engines do not count as fire."),
				TEXT("Flamethrower"), K({JAM_FRONT}), All, K({TEXT("fuego"), TEXT("llama"), TEXT("flame"), TEXT("fire"), TEXT("lanzallamas"), TEXT("quem")}), true),
			Extra(TEXT("cannon"), TEXT("The user explicitly asks for a cannon, gun or something that shoots projectiles."), TEXT("No cannon or gun is mentioned. Rockets, missiles, lasers, fire, ice, catapults and springs do not count as a cannon."),
				TEXT("Cannon"), K({JAM_FWD}), All, K({TEXT("cañon"), TEXT("cañón"), TEXT("canon"), TEXT("cannon"), TEXT("dispara"), TEXT("artiller"), TEXT("gun")}), true),
			Extra(TEXT("balloon"), TEXT("The user wants a balloon."), TEXT("No balloon is asked for."),
				TEXT("Balloon"), K({TEXT("seat:Top|Top|Top_Back|Top_Front")}), NoRoller, K({TEXT("globo"), TEXT("balloon")})),
			Extra(TEXT("wing"), TEXT("The user wants wings or a glider wing."), TEXT("No wings are asked for."),
				TEXT("Wing"), K({TEXT("Left"), TEXT("Right")}), N({TEXT("Car"), TEXT("Bike"), TEXT("Sled"), TEXT("Hover"), TEXT("Flyer"), TEXT("Airship")}),
				K({TEXT(" ala"), TEXT("alas"), TEXT("wing"), TEXT("planeador"), TEXT("glider")})),
			Extra(TEXT("sail"), TEXT("The user wants a sail that catches the wind."), TEXT("No sail is asked for."),
				TEXT("Sail"), K({TEXT("Top_Front|Top_Back|Top|seat:Top")}), Join({Ground, N({TEXT("Hover"), TEXT("Airship")})}), K({TEXT("vela"), TEXT("sail"), TEXT("viento")})),
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
			Extra(TEXT("hinge"), TEXT("The user explicitly asks for a hinge, door, hatch or flap."), TEXT("No hinge, door, hatch or flap is mentioned. Heads, turrets, aiming and anchors do not count."),
				TEXT("Hinge"), K({TEXT("Top_Back|Top_Front|Top")}), Join({Veh, Static}), K({TEXT("bisagra"), TEXT("puerta"), TEXT("compuerta"), TEXT("escotilla"), TEXT("hinge"), TEXT("door"), TEXT("flap"), TEXT("hatch")})),
			Extra(TEXT("ball"), TEXT("The user wants a heavy ball, sphere, boulder or wrecking ball."), TEXT("No ball or boulder is asked for. Balloons and bombs do not count."),
				TEXT("Ball"), K({TEXT("Front|Back|Top_Front|Top_Back")}), All, K({TEXT("bola"), TEXT("esfera"), TEXT("pelota"), TEXT("boulder"), TEXT("sphere"), TEXT("wrecking")})),
			Extra(TEXT("girder"), TEXT("The user explicitly asks for a long girder, wooden beam, pole or lance."), TEXT("No girder, pole or lance is mentioned. Guns, cannons and tanks do not count."),
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
			if (OutRecipe.Parts.Num() >= UJamAssembler::GetMaxParts() || (bIsMount && !MountSockets.IsEmpty()))
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
			if (!E.ChildType.IsNone() && OutRecipe.Parts.Num() < UJamAssembler::GetMaxParts())
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

// ---------------------------------------------------------------------------------------------
// COMPOSER por piezas: el modelo decide QUE piezas y CUANTAS; aqui se arma con ellas.
// Las plantillas (ArchetypeTable) solo quedan como "conceptos" (bolsas con nombre) para el respaldo sin red.
// ---------------------------------------------------------------------------------------------

const TArray<FJamPieceRule>& UJamBuildGenerator::PieceTable()
{
	static const TArray<FJamPieceRule> T = []()
	{
		TArray<FJamPieceRule> X;
		auto Rule = [&X](const TCHAR* Type, TArray<FString> Slots, const TCHAR* Ask, const TCHAR* NotAsk, TArray<FString> Words) -> FJamPieceRule&
		{
			FJamPieceRule R;
			R.Type = Type;
			R.Slots = MoveTemp(Slots);
			R.Ask = Ask;
			R.NotAsk = NotAsk;
			R.Words = MoveTemp(Words);
			return X.Add_GetRef(R);
		};
		// --- piezas que la tabla de extras no cubre: estructura, ruedas, asiento ---
		Rule(TEXT("Seat"), K({TEXT("Top|Top_Back|Top_Front")}), TEXT("The user asks for a seat, cockpit or driver place."), TEXT("No seat is mentioned."),
			K({TEXT("asiento"), TEXT("cabina"), TEXT("volante"), TEXT("seat")}));
		{
			FJamPieceRule& R = Rule(TEXT("Wheel"), K({TEXT("Left_Front|Left_Back|Left"), TEXT("Right_Front|Right_Back|Right")}),
				TEXT("The thing needs wheels to work (car, cart, truck, tank, bike, anything that drives on the ground) or the user mentions wheels or tires."),
				TEXT("It does not drive on wheels and no wheels are mentioned (flying, floating, static things)."),
				K({TEXT("rueda"), TEXT("llanta"), TEXT("neumatic"), TEXT("neumátic"), TEXT("wheel"), TEXT("tire")}));
			R.DefaultCount = 4;
			R.bCountable = true;
		}
		{
			FJamPieceRule& R = Rule(TEXT("BigWheel"), K({TEXT("Left_Front|Left_Back"), TEXT("Right_Front|Right_Back")}),
				TEXT("The user explicitly says the wheels must be big, huge, off-road or monster wheels."), TEXT("The user does not say the wheels are big. Normal wheels, motorcycles and tanks do not count."),
				K({TEXT("rueda grande"), TEXT("ruedas grandes"), TEXT("llanta grande"), TEXT("llantas grandes"), TEXT("ruedota"), TEXT("monster"), TEXT("todoterreno"), TEXT("big wheel")}));
			R.DefaultCount = 4;
		}
		// --- el resto sale de la tabla de extras (mismos textos, caras y palabras), en su orden ---
		for (const FJamExtraDef& E : UJamBuildGenerator::ExtraTable())
		{
			if (E.PartType.IsNone()) { continue; }
			FJamPieceRule R;
			R.Type = E.PartType;
			R.Slots = E.Sockets;
			R.Ask = E.TrueText;
			R.NotAsk = E.FalseText;
			R.Words = E.Keywords;
			R.bWeapon = E.bWeapon;
			R.ChildType = E.ChildType;
			R.ChildSocket = E.ChildSocket;
			R.MountSockets = E.MountSockets;
			X.Add(R);
		}
		Rule(TEXT("BigBattery"), K({JAM_ANY}), TEXT("The user explicitly asks for a BIG battery or a generator."), TEXT("No big battery or generator is mentioned. A normal battery does not count."),
			K({TEXT("bateria grande"), TEXT("batería grande"), TEXT("generador"), TEXT("big battery")}));
		// --- estructura: las repetidas se encadenan (fila de tablas = puente, pila de bloques = torre) ---
		auto Chain = [&](const TCHAR* Type, const TCHAR* Face, const TCHAR* Slots, const TCHAR* Ask, const TCHAR* NotAsk, TArray<FString> Words)
		{
			FJamPieceRule& R = Rule(Type, K({Slots}), Ask, NotAsk, MoveTemp(Words));
			R.Chain = Face;
			R.bCountable = true;
		};
		Chain(TEXT("Plank"), TEXT("Front"), TEXT("Front|Back|Top"),
			TEXT("The thing needs a flat board as its base or deck (vehicles, bridges, platforms, rafts) or the user mentions planks or boards."),
			TEXT("No flat board is needed (towers of blocks, walls, a lone wheel)."),
			K({TEXT("tabla"), TEXT("tablon"), TEXT("tablón"), TEXT("plancha"), TEXT("plank"), TEXT("board")}));
		Chain(TEXT("Block"), TEXT("Top"), TEXT("Top|Top_Back|Top_Front|Back"),
			TEXT("The thing is made of heavy blocks or needs one as a base or counterweight (towers, turrets, pillars, catapult weights) or the user mentions blocks, cubes or crates."),
			TEXT("No block, cube or crate is needed."),
			K({TEXT("bloque"), TEXT("cubo"), TEXT("caja"), TEXT("block"), TEXT("cube"), TEXT("crate")}));
		Chain(TEXT("Wall"), TEXT("Right"), TEXT("Front|Back|Left|Right"),
			TEXT("The thing is a wall, fort, bunker or barricade, or the user mentions walls."), TEXT("No wall is needed."),
			K({TEXT("muro"), TEXT("pared"), TEXT("muralla"), TEXT("barricada"), TEXT("wall")}));
		Chain(TEXT("Cylinder"), TEXT("Front"), TEXT("Top|Front|Back"),
			TEXT("The user mentions a cylinder, tube, barrel, log or the body of a rocket or missile."), TEXT("No cylinder, tube or barrel is needed."),
			K({TEXT("cilindro"), TEXT("tubo"), TEXT("barril"), TEXT("tronco"), TEXT("poste"), TEXT("cylinder"), TEXT("barrel")}));
		{
			FJamPieceRule& R = Rule(TEXT("Sled"), K({TEXT("Bottom")}), TEXT("The user asks for a sled, sleigh or skis."), TEXT("No sled is mentioned."),
				K({TEXT("trineo"), TEXT("esqui"), TEXT("esquí"), TEXT("sled"), TEXT("sleigh")}));
			R.DefaultCount = 1;
		}
		// --- ajustes por pieza: pares espejados, cantidades por defecto, modos ---
		for (FJamPieceRule& R : X)
		{
			const FString N = R.Type.ToString();
			if (N == TEXT("HoverStone")) { R.Slots = K({TEXT("Bottom_Front|Bottom"), TEXT("Bottom_Back|Bottom")}); R.DefaultCount = 2; }
			if (N == TEXT("Wing")) { R.Slots = K({TEXT("Left"), TEXT("Right")}); R.DefaultCount = 2; }
			if (N == TEXT("Fan") || N == TEXT("Jet") || N == TEXT("Turbine")) { R.LiftSlots = K({TEXT("Bottom_Front|Bottom_Left|Bottom"), TEXT("Bottom_Back|Bottom_Right|Bottom")}); R.bCountable = N == TEXT("Fan"); }
			if (N == TEXT("Rocket")) { R.LiftSlots = K({TEXT("Left_Center_Bottom|Bottom_Back"), TEXT("Right_Center_Bottom|Bottom_Front"), TEXT("Bottom")}); R.bCountable = true; R.Words.Add(TEXT("propulsor")); }
			if (N == TEXT("Spring")) { R.LiftSlots = K({TEXT("Top_Front|Top|Top_Back")}); }
			if (N == TEXT("Cannon") || N == TEXT("Balloon")) { R.bCountable = true; }
			if (N == TEXT("Girder")) { R.Chain = "Front"; R.Words.Add(TEXT("palo")); }
			if (N == TEXT("Ball")) { R.Words.Add(TEXT("roca")); R.Words.Add(TEXT("piedra")); }
		}
		// Criterios para el modelo: cada pieza dice para QUE COSAS hace falta (no solo cuando se la nombra).
		// Asi "un dron" -> helices + estabilizador, "una catapulta" -> resorte + palo + bola + bloque, sin plantilla.
		auto Ask = [&X](const TCHAR* Type, const TCHAR* True, const TCHAR* False)
		{
			for (FJamPieceRule& R : X)
			{
				if (R.Type == FName(Type)) { R.Ask = True; R.NotAsk = False; }
			}
		};
		Ask(TEXT("Fan"), TEXT("The user mentions a fan or propeller, or asks for a machine that flies or moves with propellers: drone, helicopter, quadcopter, airboat, fan-driven sled."),
			TEXT("No fan or propeller is mentioned and the thing does not fly or move with propellers. Turbines, jets and rockets do not count."));
		Ask(TEXT("Rocket"), TEXT("The user mentions a rocket, missile, booster, thruster or nitro, or asks for a rocket ship or missile."),
			TEXT("No rocket, missile, booster or nitro is mentioned."));
		Ask(TEXT("Balloon"), TEXT("The user mentions a balloon, or asks for an airship, blimp, zeppelin or hot air balloon."), TEXT("No balloon or airship is asked for."));
		Ask(TEXT("Wing"), TEXT("The user mentions wings, or asks for an airplane, plane or glider."), TEXT("No wings, airplane or glider is asked for."));
		Ask(TEXT("HoverStone"), TEXT("The user mentions levitation or anti-gravity, or asks for a hovercraft, hoverboard or floating platform."),
			TEXT("No levitation, hovercraft or hoverboard is asked for. Balloons, wings and propellers do not count."));
		Ask(TEXT("Spring"), TEXT("The user mentions a spring, jumping or bouncing, or asks for a catapult, launcher, trampoline or trebuchet."),
			TEXT("No spring, jumping, catapult or launcher is asked for."));
		Ask(TEXT("Cannon"), TEXT("The user mentions a cannon or gun, or asks for a tank, a turret or something that shoots projectiles."),
			TEXT("No cannon, gun, tank or turret is asked for. Rockets, lasers, fire, ice and catapults do not count."));
		Ask(TEXT("Stabilizer"), TEXT("The user mentions a stabilizer or gyroscope, or asks for something that must not tip over while flying or on two wheels: drone, helicopter, motorcycle, rocket."),
			TEXT("No stabilizer is mentioned and it is not a drone, helicopter, motorcycle or rocket."));
		Ask(TEXT("Girder"), TEXT("The user mentions a long beam, girder, pole or lance, or asks for a catapult (its throwing arm)."), TEXT("No girder, pole, lance or catapult is asked for."));
		Ask(TEXT("Ball"), TEXT("The user mentions a ball, boulder or wrecking ball, or asks for a catapult (its ammunition)."), TEXT("No ball, boulder or catapult is asked for. Balloons and bombs do not count."));
		Ask(TEXT("Sail"), TEXT("The user mentions a sail, or asks for a sailboat or something pushed by the wind."), TEXT("No sail or sailboat is asked for."));
		Ask(TEXT("Flamethrower"), TEXT("The user mentions a flamethrower, flames, fire or burning things."), TEXT("No flamethrower, fire or burning is mentioned. Rockets, jets and engines do not count as fire."));
		Ask(TEXT("Cylinder"), TEXT("The user explicitly mentions a cylinder, tube, barrel or log, or asks for a rocket ship or missile (its body)."),
			TEXT("No cylinder, tube, barrel, rocket ship or missile is asked for. Cannons, wheels and tanks do not count."));
		Ask(TEXT("Block"), TEXT("The user mentions blocks, cubes or crates, or asks for a tower, turret, pillar or catapult (its base or counterweight)."),
			TEXT("No block, cube, crate, tower, turret or catapult is asked for."));
		return X;
	}();
	return T;
}

namespace
{
	const FJamPieceRule* FindRule(FName Type)
	{
		return UJamBuildGenerator::PieceTable().FindByPredicate([Type](const FJamPieceRule& R) { return R.Type == Type; });
	}

	FJamPieceCount& BagItem(TArray<FJamPieceCount>& Bag, FName Type)
	{
		for (FJamPieceCount& I : Bag)
		{
			if (I.Type == Type) { return I; }
		}
		FJamPieceCount N;
		N.Type = Type;
		N.Count = 0;
		return Bag.Add_GetRef(N);
	}

	int32 NumberOf(const FString& Tok)
	{
		if (Tok.IsNumeric()) { return FCString::Atoi(*Tok); }
		static const TMap<FString, int32> Words = {
			{TEXT("dos"), 2}, {TEXT("tres"), 3}, {TEXT("cuatro"), 4}, {TEXT("cinco"), 5}, {TEXT("seis"), 6}, {TEXT("siete"), 7}, {TEXT("ocho"), 8}, {TEXT("nueve"), 9},
			{TEXT("diez"), 10}, {TEXT("once"), 11}, {TEXT("doce"), 12}, {TEXT("trece"), 13}, {TEXT("catorce"), 14}, {TEXT("quince"), 15}, {TEXT("dieciseis"), 16}, {TEXT("dieciséis"), 16},
			{TEXT("veinte"), 20}, {TEXT("veinticinco"), 25}, {TEXT("treinta"), 30}, {TEXT("cuarenta"), 40}, {TEXT("cincuenta"), 50},
			{TEXT("two"), 2}, {TEXT("three"), 3}, {TEXT("four"), 4}, {TEXT("five"), 5}, {TEXT("six"), 6}, {TEXT("eight"), 8}, {TEXT("ten"), 10}, {TEXT("twelve"), 12}, {TEXT("twenty"), 20},
			{TEXT("par"), 2}, {TEXT("docena"), 12} };
		const int32* N = Words.Find(Tok);
		return N ? *N : 0;
	}

	/** Pieza a la que se refiere una palabra (por raiz): "llantas" -> Wheel. La coincidencia mas larga gana. */
	FName TypeOfWord(const FString& Word, const FString& NextWord)
	{
		FName Best;
		int32 BestLen = 0;
		for (const FJamPieceRule& R : UJamBuildGenerator::PieceTable())
		{
			for (const FString& W : R.Words)
			{
				const FString Stem = W.TrimStartAndEnd();
				if (!Stem.Contains(TEXT(" ")) && Stem.Len() >= 3 && Stem.Len() > BestLen && Word.StartsWith(Stem))
				{
					Best = R.Type;
					BestLen = Stem.Len();
				}
			}
		}
		if (Best == "Wheel" && NextWord.StartsWith(TEXT("grande"))) { Best = "BigWheel"; }
		if (Best == "Battery" && NextWord.StartsWith(TEXT("grande"))) { Best = "BigBattery"; }
		return Best;
	}
}

void UJamBuildGenerator::ApplyNumerals(const FString& Prompt, TArray<FJamPieceCount>& Bag, bool bResetCounts)
{
	// "25 llantas", "cuatro cañones", "un par de alas": numero y, en las 3 palabras siguientes, una pieza.
	TArray<FString> Tok;
	Prompt.ToLower().Replace(TEXT(","), TEXT(" ")).Replace(TEXT("."), TEXT(" ")).ParseIntoArray(Tok, TEXT(" "));
	if (bResetCounts && Tok.ContainsByPredicate([](const FString& T) { return NumberOf(T) > 0; }))
	{
		for (FJamPieceCount& I : Bag)
		{
			const FJamPieceRule* Rule = FindRule(I.Type);
			I.Count = FMath::Max(Rule ? Rule->DefaultCount : 1, (I.Mode == "lift" && I.Type == "Fan") ? 2 : 1);
		}
	}
	for (int32 i = 0; i < Tok.Num(); ++i)
	{
		const int32 N = NumberOf(Tok[i]);
		if (N <= 0) { continue; }
		for (int32 j = i + 1; j < Tok.Num() && j <= i + 3; ++j)
		{
			const FName Type = TypeOfWord(Tok[j], j + 1 < Tok.Num() ? Tok[j + 1] : FString());
			if (!Type.IsNone())
			{
				BagItem(Bag, Type).Count = FMath::Clamp(N, 1, 100);
				if (Type == "BigWheel") { Bag.RemoveAll([](const FJamPieceCount& I) { return I.Type == "Wheel"; }); }
				break;
			}
		}
	}
}

void UJamBuildGenerator::BagFromKeywords(const FString& Prompt, TArray<FJamPieceCount>& Bag)
{
	Bag.Reset();
	const FString L = TEXT(" ") + Prompt.ToLower() + TEXT(" ");
	// 1) Concepto con nombre (carro, catapulta, dron...): su receta base, leida como bolsa de piezas con su modo.
	for (const FJamArchetypeDef& A : ArchetypeTable())
	{
		const bool bHit = A.Keywords.ContainsByPredicate([&L](const FString& W) { return L.Contains(W); });
		if (!bHit) { continue; }
		for (const FBuildRecipePart& P : A.Parts)
		{
			FJamPieceCount& I = BagItem(Bag, P.Type);
			++I.Count;
			const FString Sock = P.Socket.ToString();
			FJamPartDef D;
			UJamPartCatalog::Find(P.Type, D);
			if ((D.Kind == EJamPartKind::Fan || D.Kind == EJamPartKind::Rocket) && Sock.Contains(TEXT("Bottom"))) { I.Mode = "lift"; }
			if (D.Kind == EJamPartKind::Spring && Sock.StartsWith(TEXT("Top"))) { I.Mode = "launch"; }
		}
		break;
	}
	// 2) Piezas nombradas en la frase.
	for (const FJamPieceRule& R : PieceTable())
	{
		const bool bHit = R.Words.ContainsByPredicate([&L](const FString& W) { return L.Contains(W); });
		if (bHit && BagItem(Bag, R.Type).Count == 0) { BagItem(Bag, R.Type).Count = R.DefaultCount; }
	}
	Bag.RemoveAll([](const FJamPieceCount& I) { return I.Count <= 0; });
	if (Bag.ContainsByPredicate([](const FJamPieceCount& I) { return I.Type == "BigWheel"; }) && L.Contains(TEXT("grande")))
	{
		Bag.RemoveAll([](const FJamPieceCount& I) { return I.Type == "Wheel"; });
	}
	// 3) Numerales.
	ApplyNumerals(Prompt, Bag);
	if (Bag.Num() == 0)
	{
		BagItem(Bag, "Plank").Count = 1; // nada reconocible: un carro basico
		BagItem(Bag, "Wheel").Count = 4;
	}
}

bool UJamBuildGenerator::ComposeFromBag(const TArray<FJamPieceCount>& InBag, FBuildRecipe& R, FString& Note)
{
	R = FBuildRecipe();
	R.Name = TEXT("Construccion");
	TArray<FJamPieceCount> Bag = InBag;
	Bag.RemoveAll([](const FJamPieceCount& I) { FJamPartDef D; return I.Count <= 0 || !UJamPartCatalog::Find(I.Type, D); });
	const int32 Max = UJamAssembler::GetMaxParts();
	auto CountOf = [&Bag](FName T) { const FJamPieceCount* I = Bag.FindByPredicate([T](const FJamPieceCount& X) { return X.Type == T; }); return I ? I->Count : 0; };
	auto KindOf = [](FName T) { FJamPartDef D; UJamPartCatalog::Find(T, D); return D.Kind; };

	// 1) Raiz: una rueda sola rueda sola; si no, la primera pieza de estructura pedida; si no hay, una tabla.
	FName RootType = "Plank";
	const int32 Wheels = CountOf("Wheel") + CountOf("BigWheel");
	const bool bAnyChassis = Bag.ContainsByPredicate([&](const FJamPieceCount& I) { return KindOf(I.Type) == EJamPartKind::Structural || KindOf(I.Type) == EJamPartKind::Slide; });
	if (Wheels == 1 && !bAnyChassis)
	{
		RootType = CountOf("BigWheel") ? FName("BigWheel") : FName("Wheel");
	}
	else
	{
		for (const TCHAR* T : { TEXT("Sled"), TEXT("Plank"), TEXT("Cylinder"), TEXT("Block"), TEXT("Wall"), TEXT("Girder") })
		{
			if (CountOf(T) > 0) { RootType = T; break; }
		}
	}
	if (CountOf(RootType) > 0) { BagItem(Bag, RootType).Count -= 1; }
	R.Parts.Add(Pt(TEXT("body"), *RootType.ToString()));

	// Asiento gratis en todo lo que se mueve (GDD: el asiento es infinito), para poder montarse y dirigir.
	const bool bMoves = Bag.ContainsByPredicate([&](const FJamPieceCount& I)
	{
		const EJamPartKind Kd = KindOf(I.Type);
		return Kd == EJamPartKind::Wheel || Kd == EJamPartKind::Fan || Kd == EJamPartKind::Rocket || Kd == EJamPartKind::Balloon || Kd == EJamPartKind::Hover || Kd == EJamPartKind::Sail;
	});
	if (bMoves && KindOf(RootType) != EJamPartKind::Wheel && CountOf("Seat") == 0) { BagItem(Bag, "Seat").Count = 1; }

	// 2) Colocar. El chasis se alarga con tablas (adelante/atras) cuando ya no caben piezas.
	TArray<FString> Segs = { TEXT("body") };
	FString FrontEnd = TEXT("body"), BackEnd = TEXT("body");
	TMap<FName, FString> LastOf;      // ultima pieza colocada de cada tipo (para encadenar)
	LastOf.Add(RootType, TEXT("body"));
	TMap<FName, int32> Placed, Asked;
	FString MountSockets;
	int32 NextId = 0;
	auto Extend = [&]() -> bool
	{
		if (R.Parts.Num() >= Max - 1 || Segs.Num() >= 16 || KindOf(RootType) == EJamPartKind::Wheel) { return false; }
		const bool bFront = Segs.Num() % 2 == 1;
		const FString Id = FString::Printf(TEXT("seg%d"), Segs.Num());
		if (!TryAdd(R, FName(*Id), "Plank", (bFront ? FrontEnd : BackEnd) + (bFront ? TEXT(":Front") : TEXT(":Back")))) { return false; }
		(bFront ? FrontEnd : BackEnd) = Id;
		Segs.Add(Id);
		return true;
	};
	auto PlaceOne = [&](const FJamPieceRule& Rule, const FJamPieceCount& Item, int32 Index) -> bool
	{
		if (R.Parts.Num() >= Max) { return false; }
		const FString Id = !Rule.MountSockets.IsEmpty() ? FString(TEXT("mount"))
			: (Rule.Type == "Seat" && !LastOf.Contains("Seat")) ? FString(TEXT("seat")) // el primer asiento se llama "seat": hay caras "seat:Top"
			: FString::Printf(TEXT("%s%d"), *Rule.Type.ToString().ToLower(), NextId++);
		if (!Rule.MountSockets.IsEmpty() && !MountSockets.IsEmpty()) { return false; } // un solo mount
		// Repetidas de estructura: se encadenan a la anterior del mismo tipo (fila / pila).
		if (!Rule.Chain.IsNone())
		{
			if (const FString* Prev = LastOf.Find(Rule.Type))
			{
				if (TryAdd(R, FName(*Id), Rule.Type, *Prev + TEXT(":") + Rule.Chain.ToString())) { LastOf.Add(Rule.Type, Id); return true; }
			}
		}
		const TArray<FString>& Slots = (!Item.Mode.IsNone() && Rule.LiftSlots.Num() > 0) ? Rule.LiftSlots : Rule.Slots;
		if (Slots.Num() == 0) { return false; }
		const FString& Slot = Slots[Index % Slots.Num()];
		for (int32 Pass = 0; Pass < 2; ++Pass)
		{
			// Armas: primero sobre el mount (cabeza / plato), si hay.
			if (Pass == 0 && Rule.bWeapon && !MountSockets.IsEmpty() && TryAdd(R, FName(*Id), Rule.Type, WithPrefix(MountSockets, TEXT("mount:")))) { return true; }
			for (const FString& Seg : Segs)
			{
				// Los candidatos sin padre explicito ("seat:Top") van sobre este segmento del chasis.
				TArray<FString> Opts;
				Slot.ParseIntoArray(Opts, TEXT("|"));
				for (FString& O : Opts) { if (!O.Contains(TEXT(":"))) { O = Seg + TEXT(":") + O; } }
				if (TryAdd(R, FName(*Id), Rule.Type, FString::Join(Opts, TEXT("|"))))
				{
					LastOf.Add(Rule.Type, Id);
					if (!Rule.ChildType.IsNone() && R.Parts.Num() < Max) { TryAdd(R, FName(*(Id + TEXT("c"))), Rule.ChildType, Id + TEXT(":") + Rule.ChildSocket.ToString()); }
					if (!Rule.MountSockets.IsEmpty()) { MountSockets = Rule.MountSockets; }
					return true;
				}
			}
			if (!Extend()) { break; }
		}
		return false;
	};
	for (const FJamPieceRule& Rule : PieceTable())
	{
		const FJamPieceCount* Item = Bag.FindByPredicate([&Rule](const FJamPieceCount& X) { return X.Type == Rule.Type; });
		if (!Item || Item->Count <= 0) { continue; }
		Asked.Add(Rule.Type, Item->Count);
		for (int32 i = 0; i < Item->Count; ++i)
		{
			if (!PlaceOne(Rule, *Item, i)) { break; }
			++Placed.FindOrAdd(Rule.Type);
		}
	}

	// 3) Nota: lo que se armo y lo que no cupo.
	TArray<FString> Got, Miss;
	Got.Add(FString::Printf(TEXT("%s x%d"), *RootType.ToString(), 1 + Placed.FindRef(RootType) + (RootType == "Plank" ? Segs.Num() - 1 : 0)));
	for (const auto& KV : Asked)
	{
		const int32 P = Placed.FindRef(KV.Key);
		if (KV.Key != RootType && P > 0) { Got.Add(FString::Printf(TEXT("%s x%d"), *KV.Key.ToString(), P)); }
		if (P < KV.Value) { Miss.Add(FString::Printf(TEXT("%s x%d"), *KV.Key.ToString(), KV.Value - P)); }
	}
	if (RootType != "Plank" && Segs.Num() > 1) { Got.Add(FString::Printf(TEXT("Plank x%d"), Segs.Num() - 1)); }
	Note = FString::Printf(TEXT("pieces=[%s]"), *FString::Join(Got, TEXT(", ")));
	if (Miss.Num() > 0) { Note += FString::Printf(TEXT(" skipped=[%s]"), *FString::Join(Miss, TEXT(", "))); }

	for (const FBuildRecipePart& P : R.Parts)
	{
		const EJamPartKind Kd = KindOf(P.Type);
		if (Kd != EJamPartKind::Structural && Kd != EJamPartKind::Slide && Kd != EJamPartKind::Seat && Kd != EJamPartKind::Battery && Kd != EJamPartKind::Hinge)
		{
			R.Activate.Add(P.Id);
		}
	}
	FString Err;
	if (!UBuildRecipeLibrary::ValidateRecipe(R, nullptr, Err))
	{
		Note += TEXT(" invalid: ") + Err;
		return false;
	}
	return true;
}

bool UJamBuildGenerator::GenerateRecipe(const FString& Prompt, FBuildRecipe& OutRecipe, FString& OutNote)
{
	// Respaldo sin red: conceptos + palabras + numerales, compuesto por piezas.
	TArray<FJamPieceCount> Bag;
	BagFromKeywords(Prompt, Bag);
	return ComposeFromBag(Bag, OutRecipe, OutNote);
}
