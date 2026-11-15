#include "Building/JamAssembler.h"

#include "Algo/Reverse.h"
#include "Building/JamBuildPart.h"
#include "Building/JamDrivePhysics.h"
#include "Building/JamConnectionRules.h"
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
		FName Id;
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
				Item.Id = P.Id;
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
					// Separar el cilindro del chasis: el borde no debe rozar al girar.
					if (Item.Def.Kind == EJamPartKind::Wheel) { Item.Center += Dir * 2.0; }
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
		// Una rueda tampoco puede rozar una placa (escudo, muro, puas) que no sea su padre (medido: un escudo pegado detras de las ruedas clavaba el
		// tanque aunque fuera del mismo chasis): 5 cm de aire en su plano de giro.
		const bool bWheelRub = !bWheels && ((A.Def.Kind == EJamPartKind::Wheel && B.Def.bFaceAligned) || (B.Def.Kind == EJamPartKind::Wheel && A.Def.bFaceAligned)) && A.Parent != B.Id && B.Parent != A.Id && !A.Parent.IsNone() && !B.Parent.IsNone();
		const FVector H = (A.Size + B.Size) * 0.5 + (bWheelRub ? FVector(5.0, -1.0, 5.0) : FVector(bWheels ? 10.0 : -1.0));
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

// ---------------------------------------------------------------------------------------------
// SENTIDO: hechos de una receta (layout + quien gira/apunta), rayos tapados, piezas bajo el suelo y
// puntaje de coherencia. Todo sin mundo: geometria y estatica sobre las cajas del catalogo.
// ---------------------------------------------------------------------------------------------

static TAutoConsoleVariable<int32> CVarJamCoherence(TEXT("jam.Build.Coherence"), 2, TEXT("0 = apagado, 1 = puntua y avisa, 2 = puntua y repara (recompone la bolsa si < jam.Build.MinCoherence)."));
static TAutoConsoleVariable<int32> CVarJamMinCoherence(TEXT("jam.Build.MinCoherence"), 70, TEXT("Coherencia minima: por debajo, la receta se recompone antes de construir."));

int32 UJamAssembler::CoherenceMode() { return CVarJamCoherence.GetValueOnGameThread(); }
int32 UJamAssembler::MinCoherence() { return CVarJamMinCoherence.GetValueOnGameThread(); }

namespace
{
	const FJamPieceRule* RuleOf(FName Type)
	{
		return UJamBuildGenerator::PieceTable().FindByPredicate([Type](const FJamPieceRule& R) { return R.Type == Type; });
	}

	struct FFacts
	{
		TMap<FName, FPlaced> Pl;
		TArray<FName> Ids;          // en el orden de la receta
		FName Root;
		TSet<FName> Spun, Aimed;    // con un Turntable / Head de ancestro (o todas si la raiz es rueda)
	};

	bool MakeFacts(const FBuildRecipe& R, FFacts& F)
	{
		FString Err;
		if (R.Parts.Num() == 0 || !LayoutAll(R, F.Pl, Err)) { return false; }
		for (const FBuildRecipePart& P : R.Parts)
		{
			F.Ids.Add(P.Id);
			if (P.AttachTo.IsNone() && F.Root.IsNone()) { F.Root = P.Id; }
		}
		if (F.Root.IsNone()) { return false; }
		const bool bRootWheel = F.Pl[F.Root].Def.Kind == EJamPartKind::Wheel;
		for (const FName& Id : F.Ids)
		{
			bool bSpun = bRootWheel, bAimed = false;
			int32 Guard = 0;
			for (FName A = F.Pl[Id].Parent; !A.IsNone() && Guard++ < 200; A = F.Pl[A].Parent)
			{
				bSpun |= F.Pl[A].Def.Kind == EJamPartKind::Turntable;
				bAimed |= F.Pl[A].Def.Kind == EJamPartKind::Head;
			}
			if (bSpun) { F.Spun.Add(Id); }
			if (bAimed) { F.Aimed.Add(Id); }
		}
		return true;
	}

	double LoOf(const FPlaced& A, int32 i) { return A.Center[i] - A.Size[i] * 0.5; }
	double HiOf(const FPlaced& A, int32 i) { return A.Center[i] + A.Size[i] * 0.5; }

	/** Segmento O -> O + D*Len contra la caja (encogida 1 cm). Metodo de las losas. */
	bool RayHitsBox(const FVector& O, const FVector& D, double Len, const FPlaced& B)
	{
		// ponytail: rayo fino por el centro de la pieza, no un barrido de caja; barrido si llegan piezas a 45 grados.
		double T0 = 0.0, T1 = Len;
		for (int32 i = 0; i < 3; ++i)
		{
			const double Lo = LoOf(B, i) + 1.0, Hi = HiOf(B, i) - 1.0;
			if (FMath::Abs(D[i]) < 1e-6)
			{
				if (O[i] < Lo || O[i] > Hi) { return false; }
			}
			else
			{
				double A = (Lo - O[i]) / D[i], C = (Hi - O[i]) / D[i];
				if (A > C) { Swap(A, C); }
				T0 = FMath::Max(T0, A);
				T1 = FMath::Min(T1, C);
				if (T0 > T1) { return false; }
			}
		}
		return true;
	}

	/** Piezas cuya linea de tiro / chorro / columna libre choca con una pieza propia. Lo girado por un Turntable no se revisa (barre 360). */
	void BlockedRays(const FFacts& F, TArray<FName>& Out)
	{
		for (const FName& Id : F.Ids)
		{
			const FPlaced& A = F.Pl[Id];
			const FJamPieceRule* Rule = RuleOf(A.Def.Type);
			if (!Rule) { continue; }
			TArray<TPair<FVector, double>, TInlineAllocator<2>> Rays;
			if (Rule->ClearCm > 0.f && !F.Spun.Contains(Id) && !A.Parent.IsNone()) { Rays.Emplace(A.Facing, (double)Rule->ClearCm); }
			if (Rule->UpCm > 0.f) { Rays.Emplace(FVector::UpVector, (double)Rule->UpCm); }
			for (const TPair<FVector, double>& Ray : Rays)
			{
				const FVector O = A.Center + Ray.Key * (A.Size * 0.5 + FVector(1.0));
				bool bHit = false;
				for (const FName& J : F.Ids)
				{
					if (J != Id && RayHitsBox(O, Ray.Key, Ray.Value, F.Pl[J])) { bHit = true; break; }
				}
				if (bHit) { Out.Add(Id); break; }
			}
		}
	}

	/**
	 * Piezas mas bajas de lo que deben: con ruedas, por debajo de las ruedas + 2 cm; sin ruedas, por debajo de la estructura.
	 * Exentas por dato (rule.Ground): touch (ruedas), below (piedra flotante, resorte, estaca, trineo) y lift si mira al suelo.
	 */
	void GroundBad(const FFacts& F, TArray<FName>& Out)
	{
		double WheelMin = TNumericLimits<double>::Max(), StructMin = TNumericLimits<double>::Max();
		for (const FName& Id : F.Ids)
		{
			const FPlaced& A = F.Pl[Id];
			const EJamPartKind Kd = A.Def.Kind;
			if (Kd == EJamPartKind::Wheel && Id != F.Root) { WheelMin = FMath::Min(WheelMin, LoOf(A, 2)); }
			if (Kd == EJamPartKind::Structural || Kd == EJamPartKind::Slide || Kd == EJamPartKind::Wheel) { StructMin = FMath::Min(StructMin, LoOf(A, 2)); }
		}
		if (WheelMin == TNumericLimits<double>::Max() && StructMin == TNumericLimits<double>::Max()) { return; }
		const double Ref = WheelMin != TNumericLimits<double>::Max() ? WheelMin + 2.0 : StructMin - 1.0;
		for (const FName& Id : F.Ids)
		{
			const FPlaced& A = F.Pl[Id];
			const FJamPieceRule* Rule = RuleOf(A.Def.Type);
			const FString G = Rule ? Rule->Ground : FString();
			const bool bExempt = A.Def.Kind == EJamPartKind::Wheel || G == TEXT("touch") || G == TEXT("below") || (G == TEXT("lift") && A.Facing.Z < -0.5);
			if (!bExempt && LoOf(A, 2) < Ref) { Out.Add(Id); }
		}
	}

	FString JoinIds(const TArray<FName>& Ids, int32 Max = 3)
	{
		TArray<FString> S;
		for (int32 i = 0; i < Ids.Num() && i < Max; ++i) { S.Add(Ids[i].ToString()); }
		return FString::Join(S, TEXT(","));
	}
}

FBox UJamAssembler::LayoutBounds(const FBuildRecipe& Recipe)
{
	FBox Box(ForceInit);
	TMap<FName, FPlaced> Placed;
	FString Err;
	if (LayoutAll(Recipe, Placed, Err))
	{
		for (const auto& KV : Placed) { Box += FBox(KV.Value.Center - KV.Value.Size * 0.5, KV.Value.Center + KV.Value.Size * 0.5); }
	}
	return Box;
}

FString FJamCoherence::ToText() const
{
	FString S = FString::Printf(TEXT("coh=%d"), Score);
	for (const FString& G : Grave) { S += TEXT(" GRAVE ") + G + TEXT(";"); }
	for (const FString& G : Minor) { S += TEXT(" ") + G + TEXT(";"); }
	return S;
}

FJamCoherence UJamAssembler::Coherence(const FBuildRecipe& Recipe)
{
	// Pesos y umbrales = BuildKnowledge.json, bloque "coherence".
	static const float W8[8] = { 10, 15, 15, 15, 10, 15, 15, 5 };
	FJamCoherence C;
	FFacts F;
	if (!MakeFacts(Recipe, F))
	{
		C.Grave.Add(TEXT("receta invalida"));
		return C;
	}
	const int32 N = F.Ids.Num();
	double M = 0.0;
	FVector Com = FVector::ZeroVector, Lo(TNumericLimits<double>::Max()), Hi(-TNumericLimits<double>::Max());
	for (const auto& KV : F.Pl)
	{
		M += KV.Value.Def.MassKg;
		Com += KV.Value.Center * KV.Value.Def.MassKg;
		for (int32 i = 0; i < 3; ++i) { Lo[i] = FMath::Min(Lo[i], LoOf(KV.Value, i)); Hi[i] = FMath::Max(Hi[i], HiOf(KV.Value, i)); }
	}
	M = FMath::Max(M, 0.001);
	Com /= M;
	const double L = Hi.X - Lo.X, Wd = Hi.Y - Lo.Y, Z0 = Lo.Z;
	const FPlaced& RootP = F.Pl[F.Root];
	const bool bRootWheel = RootP.Def.Kind == EJamPartKind::Wheel;

	// C1 conexion: toda pieza toca a su padre (area >= 25 cm2). ponytail: 10 cm de holgura por los sockets Attach_* de las mallas de arte.
	int32 Loose = 0;
	for (const FName& Id : F.Ids)
	{
		const FPlaced& A = F.Pl[Id];
		if (A.Parent.IsNone()) { continue; }
		const FPlaced& B = F.Pl[A.Parent];
		double Ov[3];
		for (int32 j = 0; j < 3; ++j) { Ov[j] = FMath::Min(HiOf(A, j), HiOf(B, j)) - FMath::Max(LoOf(A, j), LoOf(B, j)); }
		if (Ov[0] > Ov[1]) { Swap(Ov[0], Ov[1]); }
		if (Ov[1] > Ov[2]) { Swap(Ov[1], Ov[2]); }
		if (Ov[0] > Ov[1]) { Swap(Ov[0], Ov[1]); }
		if (Ov[0] < -10.0 || FMath::Max(Ov[1], 0.0) * FMath::Max(Ov[2], 0.0) < 25.0) { ++Loose; C.Offenders.AddUnique(Id); }
	}
	C.Sub[0] = 1.f - (float)Loose / FMath::Max(1, N - 1);
	if (Loose) { C.Minor.Add(FString::Printf(TEXT("%d sin contacto"), Loose)); }

	// C2 solapes
	int32 Pairs = 0;
	for (int32 x = 0; x < N; ++x)
	{
		for (int32 y = x + 1; y < N; ++y)
		{
			if (BoxesOverlap(F.Pl[F.Ids[x]], F.Pl[F.Ids[y]])) { ++Pairs; C.Offenders.AddUnique(F.Ids[y]); }
		}
	}
	C.Sub[1] = 1.f - FMath::Min(1.f, Pairs / FMath::Max(1.f, N / 4.f));
	if (Pairs) { C.Minor.Add(FString::Printf(TEXT("%d solapes"), Pairs)); }

	// C3 simetria izquierda-derecha
	int32 Sym = 0;
	for (const FName& Id : F.Ids)
	{
		const FPlaced& A = F.Pl[Id];
		if (FMath::Abs(A.Center.Y) <= 1.0 || F.Spun.Contains(Id)) { ++Sym; continue; }
		for (const auto& KV : F.Pl)
		{
			const FPlaced& B = KV.Value;
			if (B.Def.Type == A.Def.Type && FMath::Abs(B.Center.X - A.Center.X) < 2 && FMath::Abs(B.Center.Y + A.Center.Y) < 2 && FMath::Abs(B.Center.Z - A.Center.Z) < 2
				&& FMath::Abs(B.Facing.Y + A.Facing.Y) < 0.1 && FMath::Abs(B.Facing.X - A.Facing.X) < 0.1)
			{
				++Sym;
				break;
			}
		}
	}
	C.Sub[2] = (float)(0.5 * Sym / N + 0.5 * FMath::Max(0.0, 1.0 - FMath::Abs(Com.Y) / FMath::Max(1.0, 0.15 * Wd)));
	if (Sym < N) { C.Minor.Add(FString::Printf(TEXT("%d sin espejo"), N - Sym)); }
	if (FMath::Abs(Com.Y) > 0.15 * Wd) { C.Grave.Add(FString::Printf(TEXT("ladeado: centro de masa %.0f cm a un lado"), Com.Y)); }

	// C4 apoyo: las ruedas tocan el suelo y nada arrastra; sin ruedas, lo mas bajo es algo que puede apoyarse
	TArray<const FPlaced*> Wheels;
	double WheelMin = TNumericLimits<double>::Max();
	for (const FName& Id : F.Ids)
	{
		const FPlaced& A = F.Pl[Id];
		if (A.Def.Kind == EJamPartKind::Wheel && Id != F.Root) { Wheels.Add(&A); WheelMin = FMath::Min(WheelMin, LoOf(A, 2)); }
	}
	int32 Touch = 0;
	for (const FPlaced* Wh : Wheels) { Touch += LoOf(*Wh, 2) <= WheelMin + 3.0 ? 1 : 0; }
	const float TouchFrac = Wheels.Num() ? (float)Touch / Wheels.Num() : 1.f;
	TArray<FName> Drag;
	GroundBad(F, Drag);
	C.Sub[3] = TouchFrac * (Drag.Num() ? 0.5f : 1.f);
	if (TouchFrac < 1.f) { C.Grave.Add(TEXT("ruedas en el aire")); }
	if (Drag.Num()) { C.Grave.Add((Wheels.Num() ? TEXT("arrastra: ") : TEXT("se apoya en: ")) + JoinIds(Drag)); C.Offenders.Append(Drag); }

	// C5 empuje: lo que empuja, empuja hacia +X sin par de guiñada; lo que sustenta, sobre el centro de masa
	struct FForce { const FPlaced* P; FVector Fv; };
	TArray<FForce> Prop, Lift;
	bool bDown = false, bAnyForce = false;
	double RocketPower = 0.0;
	for (const auto& KV : F.Pl)
	{
		if (KV.Value.Def.Kind == EJamPartKind::Rocket && FMath::Abs(KV.Value.Facing.Z) < 0.5 && !F.Spun.Contains(KV.Key)) { RocketPower += KV.Value.Def.Power; }
	}
	const double BoostScale = Wheels.Num() >= 2 ? JamDrivePhysics::GroundBoostScale(M, RocketPower) : 1.0;
	for (const FName& Id : F.Ids)
	{
		const FPlaced& A = F.Pl[Id];
		const EJamPartKind Kd = A.Def.Kind;
		FVector Fv;
		if ((Kd == EJamPartKind::Fan || Kd == EJamPartKind::Rocket) && !F.Spun.Contains(Id) && !A.Parent.IsNone()) { Fv = -A.Facing * A.Def.Power; }
		else if (Kd == EJamPartKind::Balloon || Kd == EJamPartKind::Hover) { Fv = FVector(0, 0, A.Def.Power); }
		else { continue; }
		if (Kd == EJamPartKind::Rocket && FMath::Abs(Fv.Z) < 1.0) { Fv *= BoostScale; }
		bAnyForce = true;
		if (FMath::Abs(Fv.Z) < 1.0) { Prop.Add({ &A, Fv }); }
		else if (Fv.Z > 1.0) { Lift.Add({ &A, Fv }); }
		else { bDown = true; C.Offenders.AddUnique(Id); }
	}
	double E = 1.0;
	if (Prop.Num())
	{
		double Sx = 0, Tot = 0, Yaw = 0;
		for (const FForce& P : Prop)
		{
			Sx += P.Fv.X;
			Tot += FMath::Sqrt(P.Fv.X * P.Fv.X + P.Fv.Y * P.Fv.Y);
			// El cohete aplica su fuerza en el centro de masa durante Tick, sin par.
			if (P.P->Def.Kind != EJamPartKind::Rocket)
			{
				Yaw += (P.P->Center.X - Com.X) * P.Fv.Y - (P.P->Center.Y - Com.Y) * P.Fv.X;
			}
		}
		const double YawRel = FMath::Abs(Yaw) / FMath::Max(1.0, Tot * L / 2);
		E = FMath::Min3(E, FMath::Max(0.0, Sx / Tot), FMath::Max(0.0, 1.0 - YawRel / 0.3));
		if (Sx / Tot < 0.9) { C.Grave.Add(TEXT("el empuje no va al frente")); }
		if (YawRel > 0.15) { C.Grave.Add(TEXT("empuje descentrado")); }
	}
	double Up = 0.0;
	for (const FForce& P : Lift) { Up += P.Fv.Z; }
	if (Lift.Num())
	{
		double Cx = 0, Cy = 0;
		for (const FForce& P : Lift)
		{
			const FVector At = P.P->Def.Kind == EJamPartKind::Rocket ? Com : P.P->Center;
			Cx += At.X * P.Fv.Z; Cy += At.Y * P.Fv.Z;
		}
		const double Dist = FMath::Sqrt(FMath::Square(Cx / Up - Com.X) + FMath::Square(Cy / Up - Com.Y));
		const double Off = Dist / FMath::Max(1.0, L / 2) * FMath::Min(1.0, Up / (M * 980.0)); // pesa segun cuanto del peso carga
		E = FMath::Min(E, FMath::Max(0.0, 1.0 - Off / 0.3));
		if (Off > 0.15) { C.Grave.Add(FString::Printf(TEXT("sustentacion descentrada (%.0f cm del centro de masa)"), Dist)); }
	}
	if (bDown) { E = 0.0; C.Grave.Add(TEXT("empuja contra el suelo")); }
	C.Sub[4] = (float)E;

	// C6 linea de tiro, chorro y columna libre + armas hacia el frente
	const bool bMobile = Wheels.Num() > 0 || bAnyForce || F.Pl.Array().ContainsByPredicate([](const TPair<FName, FPlaced>& KV) { return KV.Value.Def.Kind == EJamPartKind::Sail || KV.Value.Def.Kind == EJamPartKind::Slide; });
	int32 Need = 0;
	TArray<FName> Blocked, Back;
	BlockedRays(F, Blocked);
	for (const FName& Id : F.Ids)
	{
		const FPlaced& A = F.Pl[Id];
		const FJamPieceRule* Rule = RuleOf(A.Def.Type);
		if (!Rule) { continue; }
		const bool bSpun = F.Spun.Contains(Id);
		if ((Rule->ClearCm > 0.f && !bSpun && !A.Parent.IsNone()) || Rule->UpCm > 0.f) { ++Need; }
		if (Rule->bWeapon && !bSpun && !F.Aimed.Contains(Id) && !A.Parent.IsNone() && (A.Facing.Z < -0.5 || (bMobile && A.Facing.X < -0.5))) { Back.Add(Id); }
	}
	TSet<FName> BadRay(Blocked);
	BadRay.Append(Back);
	C.Sub[5] = 1.f - (float)BadRay.Num() / FMath::Max(1, Need);
	if (Blocked.Num()) { C.Grave.Add(TEXT("tapado: ") + JoinIds(Blocked)); C.Offenders.Append(Blocked); }
	if (Back.Num()) { C.Grave.Add(TEXT("arma hacia atras/abajo: ") + JoinIds(Back)); C.Offenders.Append(Back); }

	// C7 centro de masa dentro del apoyo. ponytail: apoyo = rectangulo (todo va alineado a ejes); casco convexo si llegan piezas a 45 grados.
	double R4[4] = { TNumericLimits<double>::Max(), -TNumericLimits<double>::Max(), TNumericLimits<double>::Max(), -TNumericLimits<double>::Max() };
	if (Wheels.Num() >= 2)
	{
		for (const FPlaced* Wh : Wheels)
		{
			R4[0] = FMath::Min(R4[0], Wh->Center.X); R4[1] = FMath::Max(R4[1], Wh->Center.X);
			R4[2] = FMath::Min(R4[2], Wh->Center.Y); R4[3] = FMath::Max(R4[3], Wh->Center.Y);
		}
	}
	else
	{
		for (const auto& KV : F.Pl)
		{
			if (LoOf(KV.Value, 2) > Z0 + 3.0) { continue; }
			R4[0] = FMath::Min(R4[0], LoOf(KV.Value, 0)); R4[1] = FMath::Max(R4[1], HiOf(KV.Value, 0));
			R4[2] = FMath::Min(R4[2], LoOf(KV.Value, 1)); R4[3] = FMath::Max(R4[3], HiOf(KV.Value, 1));
		}
	}
	const bool bStab = F.Pl.Array().ContainsByPredicate([](const TPair<FName, FPlaced>& KV) { return KV.Value.Def.Kind == EJamPartKind::Stabilizer; });
	double Mm = 1.0;
	for (int32 Axis = 0; Axis < 2; ++Axis)
	{
		const TCHAR* Name = Axis == 0 ? TEXT("largo") : TEXT("ancho");
		const double A0 = R4[Axis * 2], A1 = R4[Axis * 2 + 1], Half = (A1 - A0) / 2;
		double Mg;
		if (Half < 10.0) // apoyo en linea (moto, rueda sola): lo sostiene el estabilizador, el giro o la sustentacion
		{
			Mg = (bStab || bRootWheel || Up > 0) ? 1.0 : 0.5;
			if (Mg < 1.0) { C.Minor.Add(FString::Printf(TEXT("se balancea: apoyo en linea (%s) sin estabilizador"), Name)); }
		}
		else
		{
			Mg = FMath::Clamp((Half - FMath::Abs(Com[Axis] - (A0 + A1) / 2)) / Half / 0.2, 0.0, 1.0);
			if (Mg <= 0.0) { C.Grave.Add(FString::Printf(TEXT("centro de masa fuera del apoyo (%s)"), Name)); }
			else if (Mg < 1.0) { C.Minor.Add(FString::Printf(TEXT("centro de masa al borde (%s)"), Name)); }
		}
		Mm = FMath::Min(Mm, Mg);
	}
	const double Track = FMath::Max(R4[3] - R4[2], 1.0), H = Com.Z - Z0;
	if (H > (bMobile ? 0.75 : 1.5) * Track && !bStab && Up == 0.0 && !bRootWheel)
	{
		Mm *= bMobile ? 0.5 : 0.7;
		C.Minor.Add(FString::Printf(TEXT("alto, puede caerse: centro de masa a %.0f cm sobre %.0f de ancho"), H, Track));
	}
	C.Sub[6] = (float)Mm;

	// C8 potencia: sustentacion contra peso y carga por rueda
	double Pw = 1.0;
	if (Up > 0 && Wheels.Num() == 0 && Up < 1.2 * M * 980.0)
	{
		Pw = FMath::Max(0.0, (Up / (M * 980.0) - 0.6) / 0.6);
		C.bNoLift = true;
		(Up < M * 980.0 ? C.Grave : C.Minor).Add(FString::Printf(TEXT("no despega: pesa %.0f kg y levanta %.0f"), M, Up / 980.0));
	}
	if (Wheels.Num())
	{
		double Cap = 0;
		for (const FPlaced* Wh : Wheels) { Cap += Wh->Def.Type == "BigWheel" ? 150.0 : 60.0; } // BuildKnowledge.json: coherence.wheel_load_kg
		if (M > Cap) { Pw = FMath::Min(Pw, 0.5); C.Minor.Add(FString::Printf(TEXT("sobrecarga: %.0f kg sobre %d ruedas"), M, Wheels.Num())); }
	}
	C.Sub[7] = (float)Pw;

	double Total = 0;
	for (int32 i = 0; i < 8; ++i) { Total += W8[i] * C.Sub[i]; }
	if (C.Sub[0] < 1.f) { Total = 0; }                               // pieza suelta: fatal
	else if (Pairs) { Total = FMath::Min(Total, 40.0); }             // solape: fatal
	else if (C.Grave.Num()) { Total = FMath::Min(Total, 69.0); }     // grave: no pasa sin reparar
	C.Score = FMath::RoundToInt(Total);
	return C;
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
	// Dos pasadas: primero TODAS las soldaduras y despues las uniones. Si una pieza se suelda al chasis despues de crear la union
	// de una rueda, la rueda choca con esa pieza (un escudo detras de las ruedas clavaba el tanque).
	for (int32 JoinPass = 0; JoinPass < 2; ++JoinPass)
	for (const FBuildRecipePart* PP : ByDepth)
	{
		const FBuildRecipePart& P = *PP;
		if (P.AttachTo.IsNone() || (JoinPass == 1 && Placed[P.Id].Actor->bWelded))
		{
			continue;
		}
		FPlaced& Child = Placed[P.Id];
		FPlaced& Parent = Placed[P.AttachTo];
		Child.Actor->ParentPart = Parent.Actor;

		// Pegamento: una pieza sin movimiento ni fuerza propia (estructura, asiento, bateria, trineo) pegada a otra igual
		// se suelda en UN solo cuerpo rigido con ella. 25 ruedas sobre 5 tablas = 1 chasis + 25 ejes, no 30 cuerpos blandos.
		if (CVarJamWeld.GetValueOnGameThread() != 0 && JamConnections::Inert(Child.Def.Kind) && JamConnections::Inert(Parent.Def.Kind))
		{
			if (JoinPass == 0)
			{
				Child.Actor->Mesh->AttachToComponent(Parent.Actor->Mesh, FAttachmentTransformRules(EAttachmentRule::KeepWorld, EAttachmentRule::KeepWorld, EAttachmentRule::KeepWorld, true));
				Child.Actor->bWelded = true;
				Welded.FindOrAdd(RootOf(P.AttachTo)) += Child.Def.MassKg;
			}
			continue;
		}
		if (JoinPass == 0)
		{
			continue;
		}

		UPhysicsConstraintComponent* C = NewObject<UPhysicsConstraintComponent>(Child.Actor);
		C->RegisterComponent();
		C->AttachToComponent(Child.Actor->Mesh, FAttachmentTransformRules::KeepRelativeTransform);
		C->SetWorldLocation(Child.Actor->GetActorLocation());
		C->SetDisableCollision(true);

		const EJamPartKind Kind = Child.Def.Kind;
		const bool bAxleY = JamConnections::AxleY(Kind);       // gira sobre Y del vehiculo
		const bool bAxleZ = JamConnections::AxleZ(Kind);    // gira sobre Z del vehiculo
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
			// Rueda con Power 0 (rueda de carreta): eje libre, sin motor. Un motor con velocidad objetivo 0 la frenaria.
			if (JamConnections::Motor(Child.Def))
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
		// Si el padre esta soldado a otra pieza, el cuerpo fisico real es la raiz de esa isla: la union se hace contra ella.
		// (Contra el componente soldado, el marco de la union salia desplazado lo que mide el tramo y la fisica reventaba:
		// todo lo que colgaba de un tramo añadido del chasis - ruedas, cañones - tiraba hacia otro sitio.)
		C->SetConstrainedComponents(Placed[RootOf(P.AttachTo)].Actor->Mesh, NAME_None, Child.Actor->Mesh, NAME_None);
	}
	// Limitar el par total por traccion y margen longitudinal del centro de masa.
	// El limite anterior (2e6 por rueda) levantaba el chasis antes de hacerlo avanzar.
	{
		int32 Motors = 0;
		double Mass = 0.0, ComX = 0.0;
		double MinX = TNumericLimits<double>::Max(), MaxX = -TNumericLimits<double>::Max();
		for (const auto& KV : Placed)
		{
			const FPlaced& A = KV.Value;
			Mass += A.Def.MassKg;
			ComX += A.Center.X * A.Def.MassKg;
			if (A.Def.Kind == EJamPartKind::Wheel && !A.Parent.IsNone())
			{
				MinX = FMath::Min(MinX, A.Center.X); MaxX = FMath::Max(MaxX, A.Center.X);
				Motors += A.Actor->DriveConstraint != nullptr ? 1 : 0;
			}
		}
		ComX /= FMath::Max(Mass, 0.001);
		// Apoyo en un solo eje (dos ruedas laterales): traccion limitada, equilibrio a cargo del estabilizador/piloto.
		const double Margin = MaxX - MinX > 1.0 ? FMath::Max(0.0, FMath::Min(ComX - MinX, MaxX - ComX)) : 10.0;
		for (const auto& KV : Placed)
		{
			const FPlaced& A = KV.Value;
			if (A.Def.Kind != EJamPartKind::Wheel || !A.Actor->DriveConstraint) { continue; }
			const float Torque = JamDrivePhysics::WheelTorque(Mass, A.Def.Size.Z * 0.5, Motors, Margin);
			A.Actor->DriveConstraint->SetAngularDriveParams(0.f, 5000.f, FMath::Max(Torque, 1.f)); // Chaos: 0 significa fuerza ilimitada
		}
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
	// Con asiento, cohetes y bombas son del piloto (Espacio/Shift): prender con C no los dispara. Sin asiento no hay quien
	// los dispare, asi que C si los enciende (un cohete solo, un ramo de bombas).
	bool bSeat = false;
	for (const AJamBuildPart* P : Parts) { bSeat |= P && P->Kind == EJamPartKind::Seat; }
	for (AJamBuildPart* P : Parts)
	{
		if (P && Ids.Contains(P->RecipeId))
		{
			if (bActive && bSeat && (P->Kind == EJamPartKind::Rocket || P->Kind == EJamPartKind::Bomb))
			{
				continue;
			}
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
	static const TArray<FJamArchetypeDef> T = []()
	{
	TArray<FJamArchetypeDef> X = {
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
		// --- conceptos nuevos (bolsas con nombre para el respaldo sin red; van despues: la prioridad es el orden) ---
		Arch(TEXT("Boat"), TEXT("A boat or raft: a hull of planks with a sail and a propeller."),
			K({TEXT("barco"), TEXT("barquito"), TEXT("lancha"), TEXT("bote"), TEXT("velero"), TEXT("balsa"), TEXT("canoa"), TEXT("kayak"), TEXT("yate"), TEXT("trajinera"), TEXT("panga"), TEXT("catamaran"), TEXT("boat"), TEXT("ship"), TEXT("moto de agua")}),
			{ Pt(TEXT("body"), TEXT("Plank")), Pt(TEXT("p1"), TEXT("Plank"), TEXT("body"), TEXT("Front")), Pt(TEXT("p2"), TEXT("Plank"), TEXT("body"), TEXT("Back")),
			  Pt(TEXT("sail"), TEXT("Sail"), TEXT("body"), TEXT("Top_Front")), Pt(TEXT("fan"), TEXT("Fan"), TEXT("p2"), TEXT("Back")), Pt(TEXT("seat"), TEXT("Seat"), TEXT("body"), TEXT("Top_Back")) }),
		Arch(TEXT("Submarine"), TEXT("A tube-shaped hull pushed by a propeller."), K({TEXT("submarino"), TEXT("submarine")}),
			{ Pt(TEXT("body"), TEXT("Cylinder")), Pt(TEXT("fan"), TEXT("Fan"), TEXT("body"), TEXT("Back")), Pt(TEXT("seat"), TEXT("Seat"), TEXT("body"), TEXT("Top")) }),
		Arch(TEXT("Tower"), TEXT("Blocks piled up: tower, ladder, shelf."), K({TEXT("torre"), TEXT("tower"), TEXT("atalaya"), TEXT("escalera"), TEXT("librero"), TEXT("ropero"), TEXT("techo"), TEXT("alto")}),
			{ Pt(TEXT("body"), TEXT("Block")), Pt(TEXT("b1"), TEXT("Block"), TEXT("body"), TEXT("Top")), Pt(TEXT("b2"), TEXT("Block"), TEXT("b1"), TEXT("Top")), Pt(TEXT("b3"), TEXT("Block"), TEXT("b2"), TEXT("Top")) }),
		Arch(TEXT("Chair"), TEXT("Something to sit on: chair, throne, bench."), K({TEXT("silla"), TEXT("sillon"), TEXT("trono"), TEXT("banca"), TEXT("sofa"), TEXT("caballo"), TEXT("toro"), TEXT("columpio")}),
			{ Pt(TEXT("body"), TEXT("Block")), Pt(TEXT("seat"), TEXT("Seat"), TEXT("body"), TEXT("Top")) }),
		Arch(TEXT("Table"), TEXT("A board on two legs: table, bed, desk."), K({TEXT("mesa"), TEXT("cama"), TEXT("escritorio")}),
			{ Pt(TEXT("body"), TEXT("Plank")), Pt(TEXT("l0"), TEXT("Block"), TEXT("body"), TEXT("Bottom_Front")), Pt(TEXT("l1"), TEXT("Block"), TEXT("body"), TEXT("Bottom_Back")) }),
		Arch(TEXT("Robot"), TEXT("A body of blocks with a head that looks at targets."), K({TEXT("robot"), TEXT("mecha")}),
			{ Pt(TEXT("body"), TEXT("Block")), Pt(TEXT("torso"), TEXT("Block"), TEXT("body"), TEXT("Top")), Pt(TEXT("mount"), TEXT("Head"), TEXT("torso"), TEXT("Top")) }),
	};
	// Sinonimos medidos en la bateria de 300 frases.
	auto Syn = [&X](const TCHAR* Name, TArray<FString> Add)
	{
		for (FJamArchetypeDef& A : X) { if (A.Name == FName(Name)) { A.Keywords.Append(Add); } }
	};
		Syn(TEXT("Car"), K({TEXT("troca"), TEXT("camioneta"), TEXT("vocho"), TEXT("vochito"), TEXT("carrito"), TEXT("carreta"), TEXT("carretilla"), TEXT("taxi"), TEXT("combi"), TEXT("trailer"), TEXT("metrobus"), TEXT("autobus"), TEXT("kart"), TEXT("patineta"), TEXT("ambulancia"), TEXT("tractor"), TEXT("excavadora"), TEXT("montacargas"), TEXT("podadora"), TEXT("cuatrimoto"), TEXT("truck"), TEXT("triciclo"), TEXT("automovil"), TEXT("aspiradora")}));
		Syn(TEXT("Bike"), K({TEXT("motocicleta"), TEXT("bici"), TEXT("bicicleta")}));
		Syn(TEXT("Flyer"), K({TEXT("cuadricoptero"), TEXT("abeja"), TEXT("luciernaga"), TEXT("helicopter"), TEXT("vuele"), TEXT("volador"), TEXT("voladora"), TEXT("elevador")}));
		Syn(TEXT("Glider"), K({TEXT("avioneta"), TEXT("avioncito"), TEXT("pajaro"), TEXT("murcielago"), TEXT("bombardero"), TEXT("parapente")}));
		Syn(TEXT("Airship"), K({TEXT("zepelin")}));
		Syn(TEXT("Hover"), K({TEXT("alfombra"), TEXT("ovni"), TEXT("nave"), TEXT("flote")}));
		Syn(TEXT("Fortress"), K({TEXT("fuerte"), TEXT("refugio"), TEXT("trinchera"), TEXT("caseta"), TEXT("cerca"), TEXT("jaula"), TEXT("domo")}));
		Syn(TEXT("Catapult"), K({TEXT("trabuquete"), TEXT("resortera")}));
		Syn(TEXT("Bridge"), K({TEXT("cruzar"), TEXT("barranco")}));
	for (FJamArchetypeDef& A : X) { if (A.Name == "Spinner") { A.Keywords.Remove(TEXT("trompo")); } }
	return X;
	}();
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
		// --- piezas de solo datos (reusan una capacidad existente) ---
		Rule(TEXT("Deck"), K({TEXT("Top")}), TEXT("The user explicitly asks for a wide square platform, deck or stage."),
			TEXT("The words platform, deck or stage are not mentioned. Vehicles, aircraft, boats and bridges use a normal plank and do not count."), K({TEXT("plataforma"), TEXT("tarima"), TEXT("cubierta"), TEXT("deck")}));
		Rule(TEXT("Rod"), K({TEXT("Front|Back|Top")}), TEXT("The user asks for a thin rod, pole, mast or antenna."), TEXT("No thin rod, mast or antenna is mentioned. A thick girder or a cannon does not count."),
			K({TEXT("varilla"), TEXT("barra "), TEXT("asta"), TEXT("antena"), TEXT("mastil"), TEXT("mástil"), TEXT("rod")})).Chain = "Front";
		Rule(TEXT("UBlock"), K({JAM_ANY}), TEXT("The user explicitly asks for a heavy counterweight or ballast."), TEXT("No counterweight or ballast is mentioned. A normal block is enough."),
			K({TEXT("contrapeso"), TEXT("lastre"), TEXT("horquilla"), TEXT("bloque en u")}));
		Rule(TEXT("Cart"), K({TEXT("Bottom")}), TEXT("The user explicitly asks for a wagon, trailer or minecart that is pushed or towed and has no motor."), TEXT("No wagon, trailer or minecart is mentioned. Cars, trucks, carts that drive and anything with a motor do not count."),
			K({TEXT("carreta"), TEXT("vagon"), TEXT("vagón"), TEXT("vagoneta"), TEXT("remolque"), TEXT("wagon"), TEXT("trailer")}));
		Rule(TEXT("RailSkid"), K({TEXT("Bottom_Left"), TEXT("Bottom_Right")}), TEXT("The user wants it to slide very fast on skids or rails with almost no friction."), TEXT("No skids or rails are mentioned."),
			K({TEXT("patin "), TEXT("patín "), TEXT("riel"), TEXT("barandal"), TEXT("deslizador"), TEXT("skid")})).DefaultCount = 2;
		Rule(TEXT("IceBlock"), K({TEXT("Top|Bottom")}), TEXT("The user mentions a block of ice or a slippery block."), TEXT("No block of ice is mentioned. Freezing enemies does not count: that is the frost emitter."),
			K({TEXT("bloque de hielo"), TEXT("tempano"), TEXT("témpano"), TEXT("cubo de hielo"), TEXT("ice block")})).Chain = "Top";
		Rule(TEXT("Propeller"), K({JAM_REAR}), TEXT("The user asks for a helicopter rotor or big propeller blades on top."), TEXT("No helicopter rotor or big blades are mentioned. A normal fan or a turbine is enough."),
			K({TEXT("aspas"), TEXT("helice grande"), TEXT("hélice grande"), TEXT("helice de helicoptero"), TEXT("hélice de helicóptero"), TEXT("rotor de helic")})).LiftSlots = K({TEXT("Top_Center_Bottom")});
		Rule(TEXT("Launcher"), K({TEXT("Top|Top_Front")}), TEXT("The user wants a big launch pad that throws something or someone far."), TEXT("No launch pad is mentioned. A small jump or a catapult spring is enough: that is the spring."),
			K({TEXT("plataforma de lanzamiento"), TEXT("lanzadera"), TEXT("launch pad")})).LiftSlots = K({TEXT("Top_Front|Top|Top_Back")});
		Rule(TEXT("WagonWheel"), K({TEXT("Left_Front|Left_Back"), TEXT("Right_Front|Right_Back")}), TEXT("The user explicitly says wooden wheels, wagon wheels or free wheels without a motor."), TEXT("The user does not explicitly say wooden, wagon or free wheels. Cars, trucks, tanks, bikes and anything that drives use normal wheels and do not count."),
			K({TEXT("rueda de carreta"), TEXT("ruedas de carreta"), TEXT("rueda de madera"), TEXT("ruedas de madera"), TEXT("rueda libre"), TEXT("ruedas libres")})).DefaultCount = 4;
		Rule(TEXT("Spikes"), K({TEXT("Front_Bottom|Front")}), TEXT("The thing must hurt by touching or ramming: spikes, blades, saw, drill."), TEXT("No spikes, blades or ramming damage is mentioned. It attacks from a distance or not at all."),
			K({TEXT("puas"), TEXT("púas"), TEXT("pinchos"), TEXT("picos"), TEXT("espinas"), TEXT("cuchillas"), TEXT("sierra"), TEXT("spikes"), TEXT("blades")}));
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
		// --- COLOCACION CON SENTIDO: mismo bloque "place" de cada elemento en BuildKnowledge.json (la prueba de tablas los compara). ---
		// Orden = orden de colocacion: apoyo -> sustentacion -> empuje -> asiento -> monturas -> armas -> alas/vela -> escudo -> estructura -> equilibrio -> anclas.
		// Solo se escriben las caras del lado izquierdo/delantero: el espejo (Sym) las completa.
		{
			int32 OrderN = 0;
			auto Find = [&X](const TCHAR* Type) -> FJamPieceRule& { return *X.FindByPredicate([Type](const FJamPieceRule& R) { return R.Type == FName(Type); }); };
			auto Place = [&](std::initializer_list<const TCHAR*> Types, TArray<FString> Faces, const TCHAR* Sym, const TCHAR* Ord, float Clear = 0.f, const TCHAR* Ground = TEXT(""))
			{
				for (const TCHAR* TypeName : Types)
				{
					FJamPieceRule& R = Find(TypeName);
					R.PlaceOrder = ++OrderN; R.Faces = Faces; R.Sym = Sym; R.Order = Ord; R.ClearCm = Clear; R.Ground = Ground;
				}
			};
			auto Mode = [&](std::initializer_list<const TCHAR*> Types, const TCHAR* Name, TArray<FString> Faces, const TCHAR* Sym, const TCHAR* Ord)
			{
				for (const TCHAR* TypeName : Types) { Find(TypeName).Modes.Add(Name, FJamPlaceMode{ Faces, Sym, Ord }); }
			};
			const TArray<FString> Thrust = K({TEXT("Back_Bottom_Back"), TEXT("Top_Back_Back"), TEXT("Left_Back_Back"), TEXT("Top_Center_Back")});
			const TArray<FString> Weapon = K({TEXT("Top_Front_Front"), TEXT("Front_Bottom_Front"), TEXT("Top_Center_Front"), TEXT("Left_Front_Front"), TEXT("Left_Center_Front"), TEXT("Top_Back_Front")});
			const TArray<FString> Balance = K({TEXT("Top"), TEXT("Top_Back"), TEXT("Top_Front"), TEXT("Back_Bottom"), TEXT("Front_Bottom"), TEXT("Left_Center"), TEXT("Bottom")});
			Place({TEXT("Wheel"), TEXT("BigWheel"), TEXT("WagonWheel")}, K({TEXT("Left_Front"), TEXT("Left"), TEXT("Left_Back")}), TEXT("xy"), TEXT("outer"), 0.f, TEXT("touch"));
			Place({TEXT("HoverStone")}, K({TEXT("Bottom_Front"), TEXT("Bottom")}), TEXT("xy"), TEXT("outer"), 0.f, TEXT("below"));
			Place({TEXT("RailSkid")}, K({TEXT("Bottom_Left")}), TEXT("y"), TEXT("center"), 0.f, TEXT("below"));
			Place({TEXT("Balloon")}, K({TEXT("Top"), TEXT("Top_Front")}), TEXT("xy"), TEXT("center"));
			Place({TEXT("Fan"), TEXT("Jet"), TEXT("Turbine")}, Thrust, TEXT("y"), TEXT("back"), 80.f, TEXT("lift"));
			Place({TEXT("Rocket")}, Thrust, TEXT("y"), TEXT("back"), 120.f, TEXT("lift"));
			Place({TEXT("Propeller")}, Thrust, TEXT("y"), TEXT("back"), 0.f, TEXT("lift")); // rotor: las aspas sobresalen del cuerpo, no se le exige chorro libre
			Place({TEXT("Spring"), TEXT("Launcher")}, K({TEXT("Bottom"), TEXT("Bottom_Front")}), TEXT("xy"), TEXT("outer"), 0.f, TEXT("below"));
			Place({TEXT("Sled"), TEXT("Cart")}, K({TEXT("Bottom")}), TEXT("xy"), TEXT("center"), 0.f, TEXT("below"));
			Place({TEXT("Seat")}, K({TEXT("Top"), TEXT("Top_Back"), TEXT("Top_Front")}), TEXT("y"), TEXT("center"));
			Place({TEXT("Head")}, K({TEXT("Top_Front"), TEXT("Top"), TEXT("Top_Back")}), TEXT("y"), TEXT("front"));
			Place({TEXT("Turntable")}, K({TEXT("Top_Front"), TEXT("Top"), TEXT("Top_Back")}), TEXT("y"), TEXT("center"));
			Place({TEXT("Cannon")}, Weapon, TEXT("y"), TEXT("front"), 300.f);
			Place({TEXT("Flamethrower"), TEXT("Frost")}, Weapon, TEXT("y"), TEXT("front"), 250.f);
			Place({TEXT("Beam")}, Weapon, TEXT("y"), TEXT("front"), 400.f);
			Place({TEXT("Shock")}, Weapon, TEXT("y"), TEXT("front"), 200.f);
			Place({TEXT("Hydrant")}, Weapon, TEXT("y"), TEXT("front"), 250.f);
			Place({TEXT("Light")}, Weapon, TEXT("y"), TEXT("front"), 150.f);
			Place({TEXT("Wing")}, K({TEXT("Left"), TEXT("Left_Front"), TEXT("Left_Back"), TEXT("Top")}), TEXT("xy"), TEXT("center"));
			Place({TEXT("Sail")}, K({TEXT("Top_Front"), TEXT("Top"), TEXT("Top_Back")}), TEXT("y"), TEXT("center"));
			Place({TEXT("Panel")}, K({TEXT("Front_Bottom"), TEXT("Left_Bottom"), TEXT("Back_Bottom"), TEXT("Top_Back_Back")}), TEXT("y"), TEXT("front")); // despues de las armas: no les tapa el tiro
			Place({TEXT("Spikes")}, K({TEXT("Front_Bottom"), TEXT("Left_Bottom"), TEXT("Top_Front_Front")}), TEXT("y"), TEXT("front")); // despues de las armas, como el escudo
			Place({TEXT("Ball")}, K({TEXT("Front_Bottom"), TEXT("Top_Front"), TEXT("Back_Bottom")}), TEXT("y"), TEXT("front"));
			Place({TEXT("Girder"), TEXT("Rod")}, K({TEXT("Front_Bottom"), TEXT("Top")}), TEXT("y"), TEXT("front"));
			Place({TEXT("Hinge")}, K({TEXT("Top_Back"), TEXT("Top_Front"), TEXT("Top")}), TEXT("y"), TEXT("back"));
			Place({TEXT("Plank"), TEXT("Deck")}, K({TEXT("Top")}), TEXT("y"), TEXT("center"));
			Place({TEXT("Block"), TEXT("IceBlock")}, Balance, TEXT("y"), TEXT("balance"));
			Place({TEXT("Wall")}, K({TEXT("Left_Center_Front"), TEXT("Front"), TEXT("Back")}), TEXT("y"), TEXT("front"));
			Place({TEXT("Cylinder")}, K({TEXT("Top"), TEXT("Front_Bottom")}), TEXT("y"), TEXT("center"));
			Place({TEXT("Battery"), TEXT("BigBattery"), TEXT("Stabilizer"), TEXT("Bomb"), TEXT("UBlock")}, Balance, TEXT("y"), TEXT("balance")); // pesos moviles: equilibran el centro de masa
			Place({TEXT("Stake")}, K({TEXT("Left_Bottom"), TEXT("Back_Bottom"), TEXT("Front_Bottom")}), TEXT("xy"), TEXT("outer"), 0.f, TEXT("below"));
			Mode({TEXT("Propeller")}, TEXT("lift"), K({TEXT("Top_Center_Bottom"), TEXT("Top_Front_Bottom")}), TEXT("xy"), TEXT("center")); // encima, soplando hacia abajo
			Mode({TEXT("Launcher")}, TEXT("launch"), K({TEXT("Top_Front"), TEXT("Top"), TEXT("Top_Back")}), TEXT("y"), TEXT("front"));
			Mode({TEXT("Wheel"), TEXT("BigWheel"), TEXT("WagonWheel")}, TEXT("inline"), K({TEXT("Front")}), TEXT("xy"), TEXT("outer")); // moto
			Mode({TEXT("Fan"), TEXT("Jet"), TEXT("Turbine")}, TEXT("lift"), K({TEXT("Left_Front_Bottom"), TEXT("Bottom_Front"), TEXT("Bottom")}), TEXT("xy"), TEXT("outer")); // 4 = cuadricoptero
			Mode({TEXT("Rocket")}, TEXT("lift"), K({TEXT("Left_Center_Bottom"), TEXT("Bottom")}), TEXT("xy"), TEXT("center"));
			Mode({TEXT("Spring")}, TEXT("launch"), K({TEXT("Top_Front"), TEXT("Top"), TEXT("Top_Back")}), TEXT("y"), TEXT("front"));
			Find(TEXT("Wheel")).Odd = Find(TEXT("BigWheel")).Odd = Find(TEXT("WagonWheel")).Odd = K({TEXT("Back"), TEXT("Front")}); // la llanta 25 va en la linea central
			Find(TEXT("Seat")).UpCm = 100.f;     // nada encima del piloto
			Find(TEXT("Balloon")).UpCm = 200.f;
			Find(TEXT("Bomb")).Avoid = N({TEXT("Flame"), TEXT("Rocket"), TEXT("Fan")});
			Find(TEXT("Wall")).Chain = NAME_None; // los muros van en U (pares espejados), no en fila
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
		// Textos v2 (medidos contra 300 frases reales): cada uno dice para que CLASE de cosas hace falta la pieza y que cosas no cuentan,
		// con ejemplos de animales, muebles y barcos, no solo vehiculos.
		Ask(TEXT("Seat"), TEXT("The thing is something a person sits on or rides (chair, armchair, throne, bench, saddle, horse, mechanical bull, swing) or the user mentions a seat or cockpit."),
			TEXT("Nobody sits on it and no seat is mentioned."));
		Ask(TEXT("Wheel"), TEXT("The thing rolls on the ground on wheels (car, truck, bus, taxi, tractor, cart, wheelbarrow, skateboard, bike, tank, toy car, any land vehicle or land animal that must walk or run) or the user mentions wheels or tires."),
			TEXT("It does not roll on the ground and no wheels are mentioned: boats, ships, aircraft, drones, hoverboards, flying or floating things, furniture, towers, walls, traps, weapons standing in place. Also false if the user says 'without wheels'."));
		Ask(TEXT("Head"), TEXT("The thing must aim, watch or track by itself: auto-aim, sentry, sentinel, guard robot, any robot or creature with a head that looks at targets."),
			TEXT("Nothing is asked to aim or track targets by itself and it is not a robot or sentry."));
		Ask(TEXT("Turntable"), TEXT("The thing keeps spinning around in place: turntable, rotor, carousel, spinning top, pinwheel, blender, sprinkler, rotating trap, ferris wheel, weapons that spin in circles."),
			TEXT("Nothing spins in circles in place. Wheels, fans, propellers, turbines, turrets, aiming and tracking do not count."));
		Ask(TEXT("Fan"), TEXT("The thing flies or moves by propellers: drone, helicopter, quadcopter, flying insect, motorboat, speedboat, jet ski, airboat, fan-driven sled, windmill, or the user mentions a fan or propeller."),
			TEXT("No fan or propeller is mentioned and the thing does not fly or move with propellers. Turbines, jets, rockets, sails, wings and balloons do not count."));
		Ask(TEXT("Balloon"), TEXT("The thing floats up in the air by being lighter than air: balloon, hot air balloon, airship, blimp, zeppelin, a house lifted by balloons."),
			TEXT("No balloon or airship is asked for. Balls, drones, planes and hovercraft do not count."));
		Ask(TEXT("Wing"), TEXT("The thing flies with wings: airplane, glider, paraglider, bomber, fighter jet, bird, bat, bee, dragon, kite, paper plane, or the user mentions wings."),
			TEXT("It has no wings: helicopters, drones, balloons, rockets, hovercraft, ground and water things. Also false if the user says 'without wings'."));
		Ask(TEXT("Sail"), TEXT("The thing is pushed by the wind: sailboat, sailing ship, pirate ship, boat or raft without a motor, catamaran, kite, land yacht, or the user mentions a sail."),
			TEXT("No sail is needed: motor vehicles, aircraft, static things. Also false if the user says 'without a sail'."));
		Ask(TEXT("HoverStone"), TEXT("The thing floats a little above the ground without wings or propellers: hovercraft, hoverboard, flying carpet, flying skateboard, UFO, flying saucer, levitating platform, anti-gravity, something that 'floats' in the air."),
			TEXT("No levitation is asked for. Boats on water, balloons, wings, drones and helicopters do not count."));
		Ask(TEXT("Panel"), TEXT("The thing needs a flat protective plate: shield, armor plating, armored vehicle, barrier, fence, door leaf, cage side, battering ram front."),
			TEXT("No shield, armor, plate or barrier is asked for."));
		Ask(TEXT("Shock"), TEXT("The user wants a weapon or trap that electrocutes: electric shock, lightning, taser, tesla coil, electric trap, electric fence, electric animal."),
			TEXT("No electric attack is asked for. Lamps, lights, batteries, generators, power plants, robots and lasers do not count."));
		Ask(TEXT("Bomb"), TEXT("The thing must explode: bomb, mine, minefield, dynamite, TNT, grenade, explosive, a bomber that drops bombs."),
			TEXT("Nothing is asked to explode. Firefighters, cannons, rockets and balls do not count."));
		Ask(TEXT("Spring"), TEXT("The thing jumps, bounces or throws by spring force: spring, trampoline, pogo, catapult, trebuchet, slingshot, launcher, frog, kangaroo, grasshopper, seesaw, something that sends things flying upward."),
			TEXT("No spring, jumping, bouncing, catapult or launcher is asked for."));
		Ask(TEXT("Stake"), TEXT("The user explicitly says it must be anchored, nailed, staked or fixed to the ground, or asks for spikes."),
			TEXT("The user does not mention anchoring, nailing, stakes or spikes. Towers, walls, forts, traps and other static things do NOT count by themselves."));
		Ask(TEXT("Light"), TEXT("The thing gives light: lamp, headlight, spotlight, lighthouse, flashlight, firefly, siren lights, something to see at night."),
			TEXT("No light is asked for. Lasers, fire and electricity do not count as a light."));
		Ask(TEXT("Hydrant"), TEXT("The thing sprays water or puts out fire: hose, hydrant, water cannon, fire truck, firefighter, sprinkler of water, an animal that sprays water."),
			TEXT("No water spraying or fire extinguishing is asked for. Boats and things that float on water do not count."));
		Ask(TEXT("Hinge"), TEXT("The thing swings on a hinge: door, gate, hatch, flap, trapdoor, seesaw, swing, drawbridge."),
			TEXT("Nothing swings on a hinge. Heads, turrets, aiming and anchors do not count."));
		Ask(TEXT("Ball"), TEXT("The thing is or uses a heavy ball: ball, football, boulder, wrecking ball, rolling stone, catapult or slingshot ammunition."),
			TEXT("No ball or boulder is asked for. Balloons, bombs and wheels do not count."));
		Ask(TEXT("Girder"), TEXT("The thing needs a long beam or pole: crane arm, mast, lance, battering ram, spikes, ladder rail, swing frame, catapult arm, antenna."),
			TEXT("No long beam or pole is needed. Guns, cannons and tanks do not count."));
		Ask(TEXT("Cannon"), TEXT("The thing shoots projectiles: cannon, gun, machine gun, shotgun, pistol, bazooka, mortar, turret, tank, warship, pirate ship cannons, sniper, anything that 'shoots' or must do damage from far."),
			TEXT("Nothing shoots projectiles. Rockets, lasers, fire, ice, water and catapults do not count. Also false if the user says 'without cannon' or 'no weapons'."));
		Ask(TEXT("Flamethrower"), TEXT("The user wants to throw fire or burn enemies: flamethrower, fire trap, fire breath, dragon, something that spits fire."),
			TEXT("Nothing is asked to throw fire. Putting OUT a fire, firefighters, fire trucks, rockets, jets and engines do not count. Also false if the user says 'without fire'."));
		Ask(TEXT("Plank"), TEXT("The thing needs a flat board as its floor, deck, top or base: every vehicle, boat, ship, raft, canoe, surfboard, bridge, platform, table, bed, bench, shelf, ladder, slide, seesaw, aircraft body."),
			TEXT("No flat board is needed: a tower of blocks, a wall, a lone wheel, a lone weapon, a ball."));
		Ask(TEXT("Block"), TEXT("The thing is built from solid blocks or needs a heavy body or base: tower, watchtower, pillar, turret base, castle, robot body, animal body, wardrobe, stairs, catapult counterweight, or the user mentions blocks, cubes or crates."),
			TEXT("No solid block is needed: vehicles, boats and aircraft do not count."));
		Ask(TEXT("Wall"), TEXT("The thing encloses or protects with walls: wall, fort, fortress, castle, bunker, barricade, trench, shelter, hut, guard post, cage, fence, dome, house."),
			TEXT("No wall is needed: vehicles, aircraft, boats and single weapons do not count."));
		Ask(TEXT("Cylinder"), TEXT("The thing has a round tube body: rocket ship, missile, submarine, torpedo, bazooka, cylinder, tube, barrel, log, pipe."),
			TEXT("No tube-shaped body is needed. Cannons, wheels and tanks do not count."));
		// Sinonimos medidos en la bateria de 300 frases; se quitan las palabras que disparaban la pieza equivocada.
		auto Words = [&X](const TCHAR* Type, TArray<FString> Add, TArray<FString> Del)
		{
			for (FJamPieceRule& R : X)
			{
				if (R.Type != FName(Type)) { continue; }
				R.Words.RemoveAll([&Del](const FString& W) { return Del.Contains(W); });
				R.Words.Append(Add);
			}
		};
		Words(TEXT("Cannon"), K({TEXT("ametralladora"), TEXT("metralleta"), TEXT("escopeta"), TEXT("mortero"), TEXT("bazuca"), TEXT("pistola"), TEXT("de guerra"), TEXT("torretas"), TEXT("turret"), TEXT("dano"), TEXT("balas")}), {});
		Words(TEXT("Hydrant"), K({TEXT("bombero"), TEXT("incendio"), TEXT("de agua"), TEXT("eche agua"), TEXT("echa agua"), TEXT("apague"), TEXT("apagar el fuego"), TEXT("apagar incendios")}), {});
		Words(TEXT("Spring"), K({TEXT("resortera"), TEXT("trampolin"), TEXT("rana"), TEXT("canguro"), TEXT("sube y baja")}), {});
		Words(TEXT("Light"), K({TEXT("luciernaga"), TEXT("de noche")}), {});
		Words(TEXT("Turntable"), K({TEXT("carrusel"), TEXT("trompo"), TEXT("rehilete"), TEXT("licuadora")}), {});
		Words(TEXT("Girder"), K({TEXT("grua"), TEXT("columpio"), TEXT("picos")}), {});
		Words(TEXT("Hinge"), K({TEXT("porton")}), {});
		Words(TEXT("Sail"), K({TEXT("papalote")}), {});
		Words(TEXT("Bomb"), K({TEXT("mina"), TEXT("minado"), TEXT("granada")}), {});
		Words(TEXT("BigBattery"), K({TEXT("planta de luz")}), {});
		Words(TEXT("Wall"), K({TEXT("defienda"), TEXT("proteger"), TEXT("protegerme")}), {});
		Words(TEXT("Wheel"), K({TEXT("yanta")}), {});
		Words(TEXT("Fan"), K({TEXT("molino")}), {});
		Words(TEXT("Sled"), K({TEXT("nieve")}), {});
		Words(TEXT("Frost"), K({TEXT("freeze")}), {});
		Words(TEXT("Ball"), K({TEXT("balon")}), {});
		// "quem" (de la tabla de extras) mide 4 letras y HitWords lo toma como palabra exacta: "queme", "quema" o "quemar" no lo
		// activaban y "un carro que queme todo" (tecla 1) salia sin lanzallamas. Con 5+ letras compara por prefijo.
		Words(TEXT("Flamethrower"), K({TEXT("escupa fuego"), TEXT("eche fuego"), TEXT("echa fuego"), TEXT("de fuego"), TEXT("con fuego"),
			TEXT("queme"), TEXT("quema"), TEXT("quemar"), TEXT("quemen"), TEXT("incendia"), TEXT("llamas")}), {});
		Words(TEXT("Hydrant"), {}, K({TEXT("agua")}));
		Words(TEXT("Flamethrower"), {}, K({TEXT("fuego"), TEXT("fire")}));
		Words(TEXT("Panel"), {}, K({TEXT("pared"), TEXT("muro"), TEXT("wall")})); // muro / pared es Wall, no Panel
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

	/**
	 * Normaliza por SONIDO para comparar lo que se oyo con las palabras clave: minusculas, sin acentos, ñ -> ni, b = v, ll = y,
	 * c/k/qu, s/z/c suave, h muda. "yantas" = "llantas", "abion" = "avion", "elicoptero" = "helicoptero".
	 */
	FString Phon(const FString& In)
	{
		FString S = In.ToLower();
		static const TCHAR* Acc[][2] = { {TEXT("á"), TEXT("a")}, {TEXT("é"), TEXT("e")}, {TEXT("í"), TEXT("i")}, {TEXT("ó"), TEXT("o")}, {TEXT("ú"), TEXT("u")}, {TEXT("ü"), TEXT("u")}, {TEXT("ñ"), TEXT("ni")} };
		for (const auto& A : Acc) { S.ReplaceInline(A[0], A[1]); }
		FString O;
		auto Soft = [&S](int32 i) { return i < S.Len() && (S[i] == TEXT('e') || S[i] == TEXT('i')); };
		for (int32 i = 0; i < S.Len(); ++i)
		{
			const TCHAR C = S[i];
			const TCHAR Nx = i + 1 < S.Len() ? S[i + 1] : TEXT('\0');
			if (C == TEXT('q') && Nx == TEXT('u') && Soft(i + 2)) { O += TEXT('s'); ++i; }          // que -> se (igual que "ce")
			else if (C == TEXT('g') && Nx == TEXT('u') && Soft(i + 2)) { O += TEXT('g'); ++i; }
			else if (C == TEXT('k') || C == TEXT('c')) { O += (Nx == TEXT('h') && C == TEXT('c')) ? TEXT('c') : Soft(i + 1) ? TEXT('s') : TEXT('c'); }
			else if (C == TEXT('h')) { if (i > 0 && S[i - 1] == TEXT('c')) { O += C; } }
			else if (C == TEXT('l') && Nx == TEXT('l')) { O += TEXT('y'); ++i; }
			else if (C == TEXT('v')) { O += TEXT('b'); }
			else if (C == TEXT('z')) { O += TEXT('s'); }
			else { O += C; }
		}
		return O;
	}

	/** Texto del jugador normalizado y partido en palabras (mas las palabras pegadas de dos en dos: "cata pulta"). */
	struct FJamText
	{
		TArray<FString> Toks, Joined;
		FString Line; // " tok tok tok "
		explicit FJamText(const FString& Text)
		{
			FString Cur;
			for (const TCHAR C : Phon(Text) + TEXT(" "))
			{
				if (FChar::IsAlnum(C)) { Cur += C; }
				else if (!Cur.IsEmpty()) { Toks.Add(Cur); Cur.Reset(); }
			}
			Joined = Toks;
			for (int32 i = 0; i + 1 < Toks.Num(); ++i) { Joined.Add(Toks[i] + Toks[i + 1]); }
			Line = TEXT(" ") + FString::Join(Toks, TEXT(" ")) + TEXT(" ");
		}
	};

	/**
	 * ¿Alguna palabra clave aparece? Por PALABRA, no por subcadena ("troca" no es "roca", "barranco" no es "barra"):
	 * frases de varias palabras al inicio de palabra; claves cortas (4 letras o menos, o terminadas en espacio) = palabra exacta
	 * (o su plural); el resto = inicio de palabra.
	 */
	bool HitWords(const FJamText& T, const TArray<FString>& Words)
	{
		for (const FString& W : Words)
		{
			const FString Raw = W.TrimStartAndEnd();
			const FString S = Phon(Raw);
			if (S.IsEmpty()) { continue; }
			if (S.Contains(TEXT(" ")))
			{
				if (T.Line.Contains(TEXT(" ") + S)) { return true; }
			}
			else if (W.EndsWith(TEXT(" ")) || Raw.Len() <= 4)
			{
				if (T.Toks.Contains(S) || T.Toks.Contains(S + TEXT("s")) || T.Toks.Contains(S + TEXT("es"))) { return true; }
			}
			else if ((S.Len() >= 6 ? T.Joined : T.Toks).ContainsByPredicate([&S](const FString& Tok) { return Tok.StartsWith(S); }))
			{
				return true;
			}
		}
		return false;
	}

	int32 NumberOf(const FString& Tok)
	{
		if (Tok.IsNumeric()) { return FCString::Atoi(*Tok); }
		static const TMap<FString, int32> Words = []()
		{
			const TMap<FString, int32> Raw = {
			{TEXT("dos"), 2}, {TEXT("tres"), 3}, {TEXT("cuatro"), 4}, {TEXT("cinco"), 5}, {TEXT("seis"), 6}, {TEXT("siete"), 7}, {TEXT("ocho"), 8}, {TEXT("nueve"), 9},
			{TEXT("diez"), 10}, {TEXT("once"), 11}, {TEXT("doce"), 12}, {TEXT("trece"), 13}, {TEXT("catorce"), 14}, {TEXT("quince"), 15}, {TEXT("dieciseis"), 16}, {TEXT("dieciséis"), 16},
			{TEXT("veinte"), 20}, {TEXT("veinticinco"), 25}, {TEXT("treinta"), 30}, {TEXT("cuarenta"), 40}, {TEXT("cincuenta"), 50},
			{TEXT("two"), 2}, {TEXT("three"), 3}, {TEXT("four"), 4}, {TEXT("five"), 5}, {TEXT("six"), 6}, {TEXT("eight"), 8}, {TEXT("ten"), 10}, {TEXT("twelve"), 12}, {TEXT("twenty"), 20},
			{TEXT("par"), 2}, {TEXT("docena"), 12} };
			TMap<FString, int32> Out;
			for (const auto& KV : Raw) { Out.Add(Phon(KV.Key), KV.Value); }
			return Out;
		}();
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
				const FString Stem = Phon(W.TrimStartAndEnd());
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
	const TArray<FString> Tok = FJamText(Prompt).Toks; // normalizado por sonido: "beinticinco yantas" = 25 llantas
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

FString UJamBuildGenerator::StripNegations(const FString& Prompt, TArray<FName>& OutNegated)
{
	// "sin ruedas", "que no tenga cañon", "no quiero armas", "no un carro": esas piezas se quitan y el tramo negado no cuenta como pedido.
	static const TSet<FString> Verbs = { TEXT("quiero"), TEXT("tenga"), TEXT("tiene"), TEXT("use"), TEXT("usa"), TEXT("uses"), TEXT("eche"), TEXT("echa"), TEXT("lleve"), TEXT("lleva"), TEXT("traiga"), TEXT("trae"), TEXT("pongas"), TEXT("ponga") };
	static const TSet<FString> Stop = { TEXT(","), TEXT("pero"), TEXT("y"), TEXT("para"), TEXT("pa"), TEXT("que"), TEXT("con"), TEXT("quiero"), TEXT("nomas") };
	static const TSet<FString> Articles = { TEXT("un"), TEXT("una"), TEXT("unos"), TEXT("unas") };
	TArray<FString> Toks, Keep;
	Prompt.ToLower().Replace(TEXT(","), TEXT(" , ")).ParseIntoArray(Toks, TEXT(" "));
	for (int32 i = 0; i < Toks.Num();)
	{
		int32 Span = INDEX_NONE;
		if (Toks[i] == TEXT("sin")) { Span = i + 1; }
		else if (Toks[i] == TEXT("no") && i + 1 < Toks.Num() && Verbs.Contains(Toks[i + 1])) { Span = i + 2; }
		else if (Toks[i] == TEXT("no") && i + 1 < Toks.Num() && Articles.Contains(Toks[i + 1])) { Span = i + 1; }
		if (Span == INDEX_NONE) { Keep.Add(Toks[i]); ++i; continue; }
		int32 j = Span;
		while (j < Toks.Num() && j < Span + 3 && !Stop.Contains(Toks[j])) { ++j; }
		for (int32 k = Span; k < j; ++k)
		{
			const FString& W = Toks[k];
			if (W.StartsWith(TEXT("arma"))) { for (const FJamPieceRule& R : PieceTable()) { if (R.bWeapon) { OutNegated.AddUnique(R.Type); } } }
			if (W.StartsWith(TEXT("motor"))) { for (const TCHAR* T : { TEXT("Fan"), TEXT("Jet"), TEXT("Turbine"), TEXT("Rocket") }) { OutNegated.AddUnique(T); } }
			if (W == TEXT("fuego")) { OutNegated.AddUnique("Flamethrower"); }
			const FJamText One(W);
			for (const FJamPieceRule& R : PieceTable()) { if (HitWords(One, R.Words)) { OutNegated.AddUnique(R.Type); } }
		}
		if (OutNegated.Contains("Wheel")) { OutNegated.AddUnique("BigWheel"); }
		i = j;
	}
	return FString::Join(Keep, TEXT(" ")).Replace(TEXT(" ,"), TEXT(","));
}

void UJamBuildGenerator::RefineBag(const FString& Prompt, const TArray<FName>& Negated, TArray<FJamPieceCount>& Bag)
{
	auto KindOf = [](FName T) { FJamPartDef D; UJamPartCatalog::Find(T, D); return D.Kind; };
	const FJamText Text(Prompt);
	const bool bDigits = Text.Toks.ContainsByPredicate([](const FString& T) { return NumberOf(T) > 0; }); // el jugador dijo algun numero
	// Cantidad implicita en el nombre: triciclo = 3 ruedas, cuadricoptero = 4 helices, moto / bici = 2.
	struct FImplied { const TCHAR* Stem; EJamPartKind Kind; int32 Count; };
	static const FImplied Implied[] = { { TEXT("triciclo"), EJamPartKind::Wheel, 3 }, { TEXT("cuatrimoto"), EJamPartKind::Wheel, 4 }, { TEXT("cuadric"), EJamPartKind::Fan, 4 },
		{ TEXT("bici"), EJamPartKind::Wheel, 2 }, { TEXT("moto"), EJamPartKind::Wheel, 2 }, { TEXT("motocicleta"), EJamPartKind::Wheel, 2 }, { TEXT("scooter"), EJamPartKind::Wheel, 2 } };
	for (const FString& Tok : Text.Toks)
	{
		for (const FImplied& I : Implied)
		{
			const FString Stem = Phon(I.Stem);
			if (bDigits || !(Tok == Stem || (FCString::Strlen(I.Stem) > 5 && Tok.StartsWith(Stem)))) { continue; }
			bool bFirstWheel = true;
			for (int32 k = 0; k < Bag.Num(); ++k)
			{
				if (KindOf(Bag[k].Type) != I.Kind) { continue; }
				Bag[k].Count = I.Count;
				if (I.Kind == EJamPartKind::Wheel && !bFirstWheel) { Bag.RemoveAt(k--); } // un solo tipo de rueda
				bFirstWheel = false;
			}
		}
	}
	Bag.RemoveAll([&Negated](const FJamPieceCount& I) { return Negated.Contains(I.Type); });
	auto Has = [&Bag](std::initializer_list<const TCHAR*> Types) { for (const TCHAR* T : Types) { if (Bag.ContainsByPredicate([T](const FJamPieceCount& I) { return I.Type == FName(T) && I.Count > 0; })) { return true; } } return false; };
	// Alas sin empuje no vuelan: +1 cohete para despegar (misma idea que la sustentacion automatica).
	if (Has({TEXT("Wing")}) && !Has({TEXT("Fan"), TEXT("Jet"), TEXT("Turbine"), TEXT("Rocket"), TEXT("Balloon"), TEXT("Propeller")}) && !Negated.Contains("Rocket"))
	{
		FJamPieceCount R;
		R.Type = "Rocket";
		Bag.Add(R);
	}
	// Una torre o un muro no es UN bloque: cosa estatica cuya unica estructura es un Block o un Wall sin numero -> 3.
	const bool bMoves = Bag.ContainsByPredicate([&KindOf](const FJamPieceCount& I)
	{
		const EJamPartKind Kd = KindOf(I.Type);
		return Kd == EJamPartKind::Wheel || Kd == EJamPartKind::Fan || Kd == EJamPartKind::Rocket || Kd == EJamPartKind::Balloon || Kd == EJamPartKind::Hover || Kd == EJamPartKind::Sail;
	});
	if (!bDigits && !bMoves)
	{
		FJamPieceCount* Only = nullptr;
		int32 Structs = 0;
		for (FJamPieceCount& I : Bag)
		{
			if (KindOf(I.Type) == EJamPartKind::Structural || KindOf(I.Type) == EJamPartKind::Slide) { ++Structs; Only = &I; }
		}
		if (Structs == 1 && (Only->Type == "Block" || Only->Type == "Wall") && Only->Count == 1) { Only->Count = 3; }
	}
}

void UJamBuildGenerator::BagFromKeywords(const FString& Prompt, TArray<FJamPieceCount>& Bag, bool bDefaultCar)
{
	Bag.Reset();
	const FJamText Text(Prompt);
	// 1) Concepto con nombre (carro, catapulta, dron...): su receta base, leida como bolsa de piezas con su modo.
	for (const FJamArchetypeDef& A : ArchetypeTable())
	{
		if (!HitWords(Text, A.Keywords)) { continue; }
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
		if (HitWords(Text, R.Words) && BagItem(Bag, R.Type).Count == 0) { BagItem(Bag, R.Type).Count = R.DefaultCount; }
	}
	Bag.RemoveAll([](const FJamPieceCount& I) { return I.Count <= 0; });
	if (Bag.ContainsByPredicate([](const FJamPieceCount& I) { return I.Type == "BigWheel"; }) && Text.Toks.ContainsByPredicate([](const FString& T) { return T.StartsWith(TEXT("grande")); }))
	{
		Bag.RemoveAll([](const FJamPieceCount& I) { return I.Type == "Wheel"; });
	}
	// 3) Numerales.
	ApplyNumerals(Prompt, Bag);
	if (Bag.Num() == 0 && bDefaultCar)
	{
		BagItem(Bag, "Plank").Count = 1; // nada reconocible y nadie mas decidio: un carro basico
		BagItem(Bag, "Wheel").Count = 4;
	}
}

namespace
{
	EJamPartKind KindOfType(FName T)
	{
		FJamPartDef D;
		UJamPartCatalog::Find(T, D);
		return D.Kind;
	}

	/** Espejo de una cara: intercambia dos tokens ("Left_Front_Bottom" con Left<->Right = "Right_Front_Bottom"). */
	FString SwapTokens(const FString& Socket, const TCHAR* A, const TCHAR* B)
	{
		TArray<FString> T;
		Socket.ParseIntoArray(T, TEXT("_"));
		for (FString& X : T) { X = X == A ? FString(B) : X == B ? FString(A) : X; }
		return FString::Join(T, TEXT("_"));
	}

	/**
	 * Una cara candidata vale si la pieza nueva no se encima Y la receta entera sigue con todas las lineas de tiro,
	 * chorros y columnas libres, nada por debajo de las ruedas y nada a menos de 30 cm de lo que debe evitar.
	 */
	bool FitsParts(const FBuildRecipe& R, const TArray<FName>& NewIds)
	{
		FFacts F;
		if (!MakeFacts(R, F)) { return false; }
		for (const FName& A : NewIds)
		{
			for (const FName& B : F.Ids)
			{
				if (A != B && BoxesOverlap(F.Pl[A], F.Pl[B])) { return false; }
			}
		}
		TArray<FName> Bad;
		BlockedRays(F, Bad);
		GroundBad(F, Bad);
		if (Bad.Num() > 0) { return false; }
		auto Avoids = [&F](FName X, FName Y)
		{
			const FPlaced& PX = F.Pl[X];
			const FPlaced& PY = F.Pl[Y];
			const FJamPieceRule* Rule = RuleOf(PX.Def.Type);
			if (X == Y || !Rule || !Rule->Avoid.Contains(FName(*StaticEnum<EJamPartKind>()->GetNameStringByValue((int64)PY.Def.Kind)))) { return false; }
			const FVector D = Abs(PX.Center - PY.Center), H = (PX.Size + PY.Size) * 0.5 + FVector(30.0);
			return D.X < H.X && D.Y < H.Y && D.Z < H.Z;
		};
		for (const FName& A : NewIds)
		{
			for (const FName& B : F.Ids)
			{
				if (Avoids(A, B) || Avoids(B, A)) { return false; }
			}
		}
		return true;
	}

	bool WithinBudget(const FBuildRecipe& Recipe, const TMap<FName, int32>* Budget)
	{
		if (!Budget) { return true; }
		for (const auto& Item : UBuildRecipeLibrary::GetRequiredItems(Recipe))
		{
			if (Budget->FindRef(Item.Key) < Item.Value) { return false; }
		}
		return true;
	}

	struct FComposeOut
	{
		FBuildRecipe R;
		TArray<TPair<FName, int32>> Skipped;   // lo que no cupo: tipo, cuantas
		int32 Miss = 0;
		FJamCoherence Coh;
	};

	/** Compone la bolsa (ya sin la raiz) sobre un chasis de NSeg tramos decidido ANTES de colocar. */
	void ComposeN(TArray<FJamPieceCount> Bag, FName RootType, int32 NSeg, FComposeOut& Out, const TMap<FName, int32>* Budget)
	{
		typedef TPair<FName, FString> FSpot; // (padre, cara)
		const int32 Max = UJamAssembler::GetMaxParts();
		FBuildRecipe& R = Out.R;
		R.Parts.Add(Pt(TEXT("body"), *RootType.ToString()));
		const bool bRootWheel = KindOfType(RootType) == EJamPartKind::Wheel;
		auto Has = [&R](FName Id) { return R.Parts.ContainsByPredicate([Id](const FBuildRecipePart& P) { return P.Id == Id; }); };

		// Chasis: las tablas pedidas cuentan como tramos; se alterna frente / atras para que la raiz quede en el centro.
		if (RootType == "Plank")
		{
			if (const FJamPieceCount* Pl = Bag.FindByPredicate([](const FJamPieceCount& I) { return I.Type == "Plank"; })) { NSeg = FMath::Max(NSeg, 1 + Pl->Count); }
			Bag.RemoveAll([](const FJamPieceCount& I) { return I.Type == "Plank"; });
		}
		TArray<FName> Segs = { FName("body") };
		FName Ends[2] = { FName("body"), FName("body") }; // frente, atras
		for (int32 k = 1; k < NSeg && !bRootWheel && R.Parts.Num() < Max; ++k)
		{
			const bool bFront = k % 2 == 1;
			const FName Id(*FString::Printf(TEXT("seg%d"), k));
			R.Parts.Add(Pt(*Id.ToString(), TEXT("Plank"), *Ends[bFront ? 0 : 1].ToString(), bFront ? TEXT("Front") : TEXT("Back")));
			if (!WithinBudget(R, Budget) || !FitsParts(R, { Id })) { R.Parts.Pop(); break; }
			Ends[bFront ? 0 : 1] = Id;
			Segs.Add(Id);
		}
		TMap<FName, double> SegX;
		{
			FFacts F;
			MakeFacts(R, F);
			for (const FName& S : Segs) { SegX.Add(S, F.Pl[S].Center.X); }
		}
		Segs.Sort([&SegX](const FName& A, const FName& B) { return SegX[A] < SegX[B]; }); // de atras hacia delante
		const double Xc = (SegX[Segs[0]] + SegX[Segs.Last()]) / 2;
		TMap<FName, FName> Mirror;
		for (int32 i = 0; i < Segs.Num(); ++i) { Mirror.Add(Segs[i], Segs[Segs.Num() - 1 - i]); }

		auto SegOrder = [&](const FString& Order)
		{
			TArray<FName> O = Segs;
			if (Order == TEXT("front")) { Algo::Reverse(O); }
			else if (Order == TEXT("outer")) // extremos primero; a igual distancia, el de delante
			{
				O.StableSort([&](const FName& A, const FName& B) { const double DA = FMath::Abs(SegX[A] - Xc), DB = FMath::Abs(SegX[B] - Xc); return DA != DB ? DA > DB : SegX[A] > SegX[B]; });
			}
			else if (Order != TEXT("back")) // center / balance
			{
				O.StableSort([&](const FName& A, const FName& B) { const double DA = FMath::Abs(SegX[A] - Xc), DB = FMath::Abs(SegX[B] - Xc); return DA != DB ? DA < DB : SegX[A] > SegX[B]; });
			}
			return O;
		};
		// Grupo espejado: la cara + su espejo izquierda-derecha (y) + su espejo frente-atras (x). "Top" es su propio espejo.
		auto Group = [&Mirror](FName Seg, const FString& Sock, const FString& Sym)
		{
			TArray<FSpot> G = { FSpot(Seg, Sock) };
			if (Sym.Contains(TEXT("y"))) { for (int32 i = 0, n = G.Num(); i < n; ++i) { G.AddUnique(FSpot(G[i].Key, SwapTokens(G[i].Value, TEXT("Left"), TEXT("Right")))); } }
			if (Sym.Contains(TEXT("x"))) { for (int32 i = 0, n = G.Num(); i < n; ++i) { const FName* Ms = Mirror.Find(G[i].Key); G.AddUnique(FSpot(Ms ? *Ms : G[i].Key, SwapTokens(G[i].Value, TEXT("Front"), TEXT("Back")))); } }
			return G;
		};
		int32 NextId = 0;
		// Coloca el grupo entero o nada.
		auto Put = [&](const FJamPieceRule& Rule, const TArray<FSpot>& G) -> TArray<FName>
		{
			TArray<FName> Ids;
			for (const FSpot& S : G)
			{
				const FName Id = !Rule.MountSockets.IsEmpty() ? FName("mount") : (Rule.Type == "Seat" && !Has("seat")) ? FName("seat")
					: FName(*FString::Printf(TEXT("%s%d"), *Rule.Type.ToString().ToLower(), NextId++));
				R.Parts.Add(Pt(*Id.ToString(), *Rule.Type.ToString(), *S.Key.ToString(), *S.Value));
				Ids.Add(Id);
			}
			if (R.Parts.Num() <= Max && WithinBudget(R, Budget) && FitsParts(R, Ids)) { return Ids; }
			R.Parts.SetNum(R.Parts.Num() - G.Num());
			return TArray<FName>();
		};
		// Distancia del centro de masa al centro del apoyo (ruedas) o del chasis: lo que minimizan las piezas "balance".
		auto BalanceCost = [&]()
		{
			FFacts F;
			MakeFacts(R, F);
			double M = 0;
			FVector Com = FVector::ZeroVector, Wc = FVector::ZeroVector;
			int32 Nw = 0;
			for (const auto& KV : F.Pl)
			{
				M += KV.Value.Def.MassKg;
				Com += KV.Value.Center * KV.Value.Def.MassKg;
				if (KV.Value.Def.Kind == EJamPartKind::Wheel && KV.Key != F.Root) { Wc += KV.Value.Center; ++Nw; }
			}
			Com /= FMath::Max(M, 0.001);
			const FVector Ctr = Nw ? Wc / Nw : FVector(Xc, 0, 0);
			return FMath::Sqrt(FMath::Square(Com.X - Ctr.X) + FMath::Square(Com.Y - Ctr.Y));
		};

		TArray<const FJamPieceRule*> Rules;
		for (const FJamPieceRule& Rl : UJamBuildGenerator::PieceTable()) { Rules.Add(&Rl); }
		Rules.StableSort([](const FJamPieceRule& A, const FJamPieceRule& B) { return A.PlaceOrder < B.PlaceOrder; });
		FString MountSockets;
		FName LaunchSpring;   // resorte en modo lanzar con la cara de arriba libre: la bola o la bomba van encima (las lanza)
		static const TArray<FString> RootWheelFaces = { TEXT("Left") };
		for (const FJamPieceRule* RulePtr : Rules)
		{
			const FJamPieceRule& Rule = *RulePtr;
			const FJamPieceCount* Item = Bag.FindByPredicate([&Rule](const FJamPieceCount& X) { return X.Type == Rule.Type; });
			if (!Item || Item->Count <= 0) { continue; }
			const FJamPlaceMode* Mode = Rule.Modes.Find(Item->Mode);
			const TArray<FString>* Faces = Mode ? &Mode->Faces : &Rule.Faces;
			FString Sym = Mode ? Mode->Sym : Rule.Sym, Order = Mode ? Mode->Order : Rule.Order;
			if (bRootWheel) { Faces = &RootWheelFaces; Sym = TEXT("y"); Order = TEXT("center"); } // rueda raiz: todo va en el eje, en pares
			int32 Left = Item->Count;
			FName LastId = Rule.Type == RootType ? FName("body") : NAME_None;
			while (Left > 0)
			{
				TArray<FName> Done;
				if (!Rule.MountSockets.IsEmpty() && !MountSockets.IsEmpty()) { break; } // un solo mount
				if (!LaunchSpring.IsNone() && (Rule.Type == "Ball" || Rule.Type == "Bomb"))
				{
					Done = Put(Rule, { FSpot(LaunchSpring, TEXT("Top")) });
					if (Done.Num()) { LaunchSpring = NAME_None; }
				}
				if (Done.Num() == 0 && !Rule.Chain.IsNone() && !LastId.IsNone()) // estructura repetida: fila / pila
				{
					Done = Put(Rule, { FSpot(LastId, Rule.Chain.ToString()) });
				}
				if (Done.Num() == 0 && Rule.bWeapon && !MountSockets.IsEmpty()) // armas: primero en la montura (cabeza / plato)
				{
					TArray<FString> Ms;
					MountSockets.ParseIntoArray(Ms, TEXT("|"));
					for (const FString& S : Ms)
					{
						Done = Put(Rule, { FSpot(FName("mount"), S) });
						if (Done.Num()) { break; }
					}
				}
				if (Done.Num() == 0 && Order == TEXT("balance")) // equilibrio: la cara valida que deja el centro de masa mas cerca del apoyo
				{
					double BestCost = TNumericLimits<double>::Max();
					TArray<FSpot> BestG;
					for (const FName& Seg : SegOrder(Order))
					{
						for (const FString& Face : *Faces)
						{
							const TArray<FSpot> G = Group(Seg, Face, Sym);
							if (G.Num() > Left || Put(Rule, G).Num() == 0) { continue; }
							const double Cost = BalanceCost();
							R.Parts.SetNum(R.Parts.Num() - G.Num());
							if (Cost < BestCost - 0.5) { BestCost = Cost; BestG = G; }
						}
					}
					if (BestG.Num()) { Done = Put(Rule, BestG); }
				}
				else if (Done.Num() == 0)
				{
					// A: espejo completo y paridad (cantidad par -> solo grupos pares); B: espejo completo; C: sin espejo frente-atras + caras impares.
					for (int32 Pass = 0; Pass < 3 && Done.Num() == 0; ++Pass)
					{
						TArray<FString> PassFaces = *Faces;
						if (Pass == 2) { PassFaces.Append(Rule.Odd); }
						const FString PassSym = Pass == 2 ? Sym.Replace(TEXT("x"), TEXT("")) : Sym;
						for (const FName& Seg : SegOrder(Order))
						{
							for (const FString& Face : PassFaces)
							{
								const TArray<FSpot> G = Group(Seg, Face, PassSym);
								if (G.Num() <= Left && !(Pass == 0 && Left % 2 == 0 && G.Num() % 2 == 1)) { Done = Put(Rule, G); }
								if (Done.Num()) { break; }
							}
							if (Done.Num()) { break; }
						}
					}
				}
				if (Done.Num() == 0) { break; }
				Left -= Done.Num();
				LastId = Done.Last();
				if (!Rule.MountSockets.IsEmpty()) { MountSockets = Rule.MountSockets; }
				if (Item->Mode == "launch" && KindOfType(Rule.Type) == EJamPartKind::Spring) { LaunchSpring = Done[0]; }
				if (!Rule.ChildType.IsNone() && R.Parts.Num() < Max)
				{
					const FName ChildId(*(Done[0].ToString() + TEXT("c")));
					R.Parts.Add(Pt(*ChildId.ToString(), *Rule.ChildType.ToString(), *Done[0].ToString(), *Rule.ChildSocket.ToString()));
					if (!WithinBudget(R, Budget) || !FitsParts(R, { ChildId })) { R.Parts.Pop(); }
				}
			}
			if (Left > 0) { Out.Skipped.Emplace(Rule.Type, Left); Out.Miss += Left; }
		}
		Out.Coh = UJamAssembler::Coherence(R);
	}

	/** Crece el chasis (1, 3, 5... tramos) mientras ayude. Gana: sin fallo grave, menos piezas sin colocar, mas puntaje, mas corto. */
	void ComposeGrow(const TArray<FJamPieceCount>& Bag, FName RootType, FComposeOut& Best, const TMap<FName, int32>* Budget)
	{
		const int32 MinScore = UJamAssembler::MinCoherence();
		auto Better = [MinScore](const FComposeOut& A, const FComposeOut& B)
		{
			const bool BadA = A.Coh.Score < MinScore, BadB = B.Coh.Score < MinScore;
			if (BadA != BadB) { return !BadA; }
			if (A.Miss != B.Miss) { return A.Miss < B.Miss; }
			return A.Coh.Score > B.Coh.Score; // a igualdad gana el primero = el mas corto
		};
		int32 Worse = 0;
		for (int32 NSeg = 1; NSeg < 17; NSeg += 2)
		{
			FComposeOut Cur;
			ComposeN(Bag, RootType, NSeg, Cur, Budget);
			if (NSeg == 1 || Better(Cur, Best)) { Best = MoveTemp(Cur); Worse = 0; }
			else { ++Worse; }
			if ((Best.Miss == 0 && Best.Coh.Score >= 85) || Worse >= 2) { break; }
		}
	}
}

void UJamBuildGenerator::BagOfRecipe(const FBuildRecipe& Recipe, TArray<FJamPieceCount>& OutBag)
{
	OutBag.Reset();
	FFacts F;
	const bool bLayout = MakeFacts(Recipe, F);
	for (const FBuildRecipePart& P : Recipe.Parts)
	{
		FJamPartDef D;
		if (!UJamPartCatalog::Find(P.Type, D)) { continue; }
		FJamPieceCount& I = BagItem(OutBag, P.Type);
		++I.Count;
		const FString Sock = P.Socket.ToString();
		if (P.AttachTo.IsNone()) { continue; }
		if ((D.Kind == EJamPartKind::Fan || D.Kind == EJamPartKind::Rocket) && bLayout && F.Pl[P.Id].Facing.Z < -0.5) { I.Mode = "lift"; }
		if (D.Kind == EJamPartKind::Spring && Sock.StartsWith(TEXT("Top"))) { I.Mode = "launch"; }
		if (D.Kind == EJamPartKind::Wheel && (Sock == TEXT("Front") || Sock == TEXT("Back"))) { I.Mode = "inline"; }
	}
}

FJamCoherence UJamBuildGenerator::Repair(FBuildRecipe& Recipe, FString& OutWhat)
{
	OutWhat.Reset();
	const FJamCoherence C = UJamAssembler::Coherence(Recipe);
	if (C.Score >= UJamAssembler::MinCoherence() || UJamAssembler::CoherenceMode() < 2) { return C; }
	// Reparacion universal: recomponer la misma bolsa con las reglas (cubre reorientar, mover y quitar). Se queda con la mejor.
	TArray<FJamPieceCount> Bag;
	BagOfRecipe(Recipe, Bag);
	FBuildRecipe R2;
	FString Note;
	if (!ComposeFromBag(Bag, R2, Note)) { return C; }
	const FJamCoherence C2 = UJamAssembler::Coherence(R2);
	if (C2.Score <= C.Score) { return C; }
	R2.Name = Recipe.Name;
	Recipe = R2;
	OutWhat = FString::Printf(TEXT("la rearme (%s)"), C.Grave.Num() ? *C.Grave[0] : C.Minor.Num() ? *C.Minor[0] : TEXT("no tenia sentido"));
	return C2;
}

bool UJamBuildGenerator::ComposeFromBag(const TArray<FJamPieceCount>& InBag, FBuildRecipe& R, FString& Note, const TMap<FName, int32>* Budget)
{
	R = FBuildRecipe();
	TArray<FJamPieceCount> Bag = InBag;
	Bag.RemoveAll([](const FJamPieceCount& I) { FJamPartDef D; return I.Count <= 0 || !UJamPartCatalog::Find(I.Type, D); });
	auto CountOf = [&Bag](FName T) { const FJamPieceCount* I = Bag.FindByPredicate([T](const FJamPieceCount& X) { return X.Type == T; }); return I ? I->Count : 0; };

	// 1) Raiz: una rueda sola rueda sola; si no, la primera pieza de estructura pedida; si no hay, una tabla.
	FName RootType = "Plank";
	const int32 Wheels = CountOf("Wheel") + CountOf("BigWheel");
	const bool bAnyChassis = Bag.ContainsByPredicate([](const FJamPieceCount& I) { return KindOfType(I.Type) == EJamPartKind::Structural || KindOfType(I.Type) == EJamPartKind::Slide; });
	if (Wheels == 1 && !bAnyChassis)
	{
		RootType = CountOf("BigWheel") ? FName("BigWheel") : FName("Wheel");
	}
	else
	{
		for (const TCHAR* T : { TEXT("Sled"), TEXT("Cart"), TEXT("Plank"), TEXT("Deck"), TEXT("Cylinder"), TEXT("Block"), TEXT("IceBlock"), TEXT("UBlock"), TEXT("Wall"), TEXT("Girder") })
		{
			if (CountOf(T) > 0) { RootType = T; break; }
		}
		// El modelo (pregunta "root") o el planificador pueden señalar la pieza raiz: manda si es una pieza de estructura de la bolsa.
		for (FJamPieceCount& I : Bag)
		{
			if (I.Mode == "root")
			{
				I.Mode = NAME_None;
				if (KindOfType(I.Type) == EJamPartKind::Structural || KindOfType(I.Type) == EJamPartKind::Slide) { RootType = I.Type; }
			}
		}
	}
	if (Budget && RootType != "Seat" && Budget->FindRef(FName(*(TEXT("Part.") + RootType.ToString()))) <= 0) { Note = TEXT("Falta pieza base: ") + RootType.ToString(); return false; }
	if (CountOf(RootType) > 0) { BagItem(Bag, RootType).Count -= 1; }
	const int32 AskedPlanks = CountOf("Plank");
	// Piezas que solo tienen sentido en un modo: el rotor sustenta, la plataforma de lanzamiento lanza.
	for (FJamPieceCount& I : Bag)
	{
		if (I.Type == "Propeller" && I.Mode.IsNone()) { I.Mode = "lift"; }
		if (I.Type == "Launcher" && I.Mode.IsNone()) { I.Mode = "launch"; }
	}

	// Asiento gratis en todo lo que se mueve (GDD: el asiento es infinito), para poder montarse y dirigir.
	const bool bMoves = Bag.ContainsByPredicate([](const FJamPieceCount& I)
	{
		const EJamPartKind Kd = KindOfType(I.Type);
		return I.Count > 0 && (Kd == EJamPartKind::Wheel || Kd == EJamPartKind::Fan || Kd == EJamPartKind::Rocket || Kd == EJamPartKind::Balloon || Kd == EJamPartKind::Hover || Kd == EJamPartKind::Sail);
	});
	if (bMoves && KindOfType(RootType) != EJamPartKind::Wheel && CountOf("Seat") == 0) { BagItem(Bag, "Seat").Count = 1; }
	// Dos ruedas + estabilizador = moto: las ruedas van en linea (delante y atras), no a los lados.
	if (Wheels == 2 && CountOf("Stabilizer") > 0)
	{
		for (FJamPieceCount& I : Bag) { if (KindOfType(I.Type) == EJamPartKind::Wheel && I.Mode.IsNone()) { I.Mode = "inline"; } }
	}

	// 2) Componer con sentido. Reparacion global: volador que no despega -> el codigo agrega sustentacion (hasta 4) y avisa.
	TArray<FString> Warn;
	int32 LiftAdded = 0;
	FName LiftType;
	FComposeOut Best;
	for (int32 Round = 0; Round < 5; ++Round)
	{
		Best = FComposeOut();
		ComposeGrow(Bag, RootType, Best, Budget);
		if (!Best.Coh.bNoLift || Round == 4) { break; }
		FJamPieceCount* LiftItem = Bag.FindByPredicate([](const FJamPieceCount& I)
		{
			const EJamPartKind Kd = KindOfType(I.Type);
			return I.Count > 0 && (Kd == EJamPartKind::Balloon || Kd == EJamPartKind::Hover || ((Kd == EJamPartKind::Fan || Kd == EJamPartKind::Rocket) && I.Mode == "lift"));
		});
		if (!LiftItem || (Budget && LiftItem->Count >= Budget->FindRef(FName(*(TEXT("Part.") + LiftItem->Type.ToString()))))) { break; }
		++LiftItem->Count;
		++LiftAdded;
		LiftType = LiftItem->Type;
	}
	if (LiftAdded > 0) { Warn.Add(FString::Printf(TEXT("agregue %d %s para que despegue"), LiftAdded, *LiftType.ToString())); }
	R = Best.R;
	R.Name = TEXT("Construccion");

	// 3) Nota: lo que se armo, lo que no cupo, avisos y coherencia.
	TArray<FName> Types;
	TMap<FName, int32> Got;
	bool bWeapon = false, bShieldAway = false;
	int32 Segs = 0;
	for (const FBuildRecipePart& P : R.Parts)
	{
		Types.AddUnique(P.Type);
		++Got.FindOrAdd(P.Type);
		Segs += P.Id.ToString().StartsWith(TEXT("seg")) ? 1 : 0;
		const FJamPieceRule* Rule = RuleOf(P.Type);
		bWeapon |= Rule && Rule->bWeapon;
		bShieldAway |= P.Type == "Panel" && !P.Socket.ToString().StartsWith(TEXT("Front")) && !P.Id.ToString().EndsWith(TEXT("c"));
	}
	TArray<FString> GotS, MissS;
	for (const FName& T : Types) { GotS.Add(FString::Printf(TEXT("%s x%d"), *T.ToString(), Got[T])); }
	for (const TPair<FName, int32>& S : Best.Skipped) { MissS.Add(FString::Printf(TEXT("%s x%d"), *S.Key.ToString(), S.Value)); }
	if (RootType == "Plank" ? Segs > AskedPlanks : Segs > 0) { Warn.Add(FString::Printf(TEXT("chasis mas largo: +%d tablas para que todo quepa con sentido"), RootType == "Plank" ? Segs - AskedPlanks : Segs)); }
	if (bWeapon && bShieldAway) { Warn.Add(TEXT("el escudo va atras o a los lados para no tapar el arma")); }
	Note = FString::Printf(TEXT("pieces=[%s]"), *FString::Join(GotS, TEXT(", ")));
	if (MissS.Num() > 0) { Note += FString::Printf(TEXT(" skipped=[%s]"), *FString::Join(MissS, TEXT(", "))); }
	if (Warn.Num() > 0) { Note += FString::Printf(TEXT(" aviso=[%s]"), *FString::Join(Warn, TEXT("; "))); }
	Note += TEXT(" ") + Best.Coh.ToText();

	// Con asiento, cohetes y bombas NO se encienden con C: los dispara el jugador con Espacio (un cohete gastado al encender no sirve de turbo).
	const bool bSeat = Got.Contains("Seat");
	for (const FBuildRecipePart& P : R.Parts)
	{
		const EJamPartKind Kd = KindOfType(P.Type);
		if (Kd != EJamPartKind::Structural && Kd != EJamPartKind::Slide && Kd != EJamPartKind::Seat && Kd != EJamPartKind::Battery && Kd != EJamPartKind::Hinge
			&& !(bSeat && (Kd == EJamPartKind::Rocket || Kd == EJamPartKind::Bomb)))
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
	// Respaldo sin red: negaciones + conceptos + palabras + numerales, compuesto por piezas.
	TArray<FName> Negated;
	const FString Clean = StripNegations(Prompt, Negated);
	TArray<FJamPieceCount> Bag;
	BagFromKeywords(Clean, Bag);
	RefineBag(Clean, Negated, Bag);
	return ComposeFromBag(Bag, OutRecipe, OutNote);
}
