#include "Building/JamLayout.h"

DEFINE_LOG_CATEGORY_STATIC(LogJamLayout, Log, All);

namespace
{
	/** Token de cara -> dirección. Acepta sinónimos que suelen escribir los LLM. FString == ignora mayúsculas. */
	bool TokenDir(const FString& T, FVector& Out)
	{
		if (T == TEXT("Top") || T == TEXT("Up") || T == TEXT("Above") || T == TEXT("Upper")) { Out = FVector(0, 0, 1); return true; }
		if (T == TEXT("Bottom") || T == TEXT("Down") || T == TEXT("Below") || T == TEXT("Under") || T == TEXT("Underneath") || T == TEXT("Lower")) { Out = FVector(0, 0, -1); return true; }
		if (T == TEXT("Front") || T == TEXT("Forward") || T == TEXT("Fwd") || T == TEXT("Nose")) { Out = FVector(1, 0, 0); return true; }
		if (T == TEXT("Back") || T == TEXT("Rear") || T == TEXT("Behind") || T == TEXT("Tail")) { Out = FVector(-1, 0, 0); return true; }
		if (T == TEXT("Left")) { Out = FVector(0, -1, 0); return true; }
		if (T == TEXT("Right") || T == TEXT("Side")) { Out = FVector(0, 1, 0); return true; } // "Side" = alias heredado de Right
		return false;
	}

	const TCHAR* DirName(const FVector& D)
	{
		if (D.Z > 0.5) { return TEXT("Top"); }
		if (D.Z < -0.5) { return TEXT("Bottom"); }
		if (D.X > 0.5) { return TEXT("Front"); }
		if (D.X < -0.5) { return TEXT("Back"); }
		if (D.Y < -0.5) { return TEXT("Left"); }
		return TEXT("Right");
	}

	FVector AbsV(const FVector& V) { return FVector(FMath::Abs(V.X), FMath::Abs(V.Y), FMath::Abs(V.Z)); }
}

bool JamLayout::ParseSocket(const FString& Socket, FVector& OutDir, FVector& OutShift, bool& bOutHasShift)
{
	bOutHasShift = false;
	FString S = Socket.TrimStartAndEnd();
	S.RemoveFromStart(TEXT("Attach_"));
	S.ReplaceInline(TEXT("-"), TEXT("_"));
	S.ReplaceInline(TEXT(" "), TEXT("_"));

	TArray<FString> Tokens;
	S.ParseIntoArray(Tokens, TEXT("_"), true);

	bool bHasDir = false;
	for (const FString& T : Tokens)
	{
		FVector D;
		if (!TokenDir(T, D))
		{
			continue; // "Attach", "side" repetido, palabras sueltas...
		}
		if (!bHasDir)
		{
			OutDir = D;
			bHasDir = true;
		}
		else if (FMath::Abs(FVector::DotProduct(D, OutDir)) < 0.5)
		{
			OutShift = D; // solo cuenta si es de otro eje ("Left_Front" sí, "Left_Side" no)
			bOutHasShift = true;
			break;
		}
	}
	return bHasDir;
}

FString JamLayout::SocketToString(const FVector& Dir, const FVector& Shift, bool bHasShift)
{
	FString Out = DirName(Dir);
	if (bHasShift)
	{
		Out += TEXT("_");
		Out += DirName(Shift);
	}
	return Out;
}

bool JamLayout::Overlaps(const FJamPlacedPart& A, const FJamPlacedPart& B, double Tolerance)
{
	return FMath::Abs(A.Center.X - B.Center.X) < A.Half.X + B.Half.X - Tolerance
		&& FMath::Abs(A.Center.Y - B.Center.Y) < A.Half.Y + B.Half.Y - Tolerance
		&& FMath::Abs(A.Center.Z - B.Center.Z) < A.Half.Z + B.Half.Z - Tolerance;
}

bool JamLayout::Compute(const FBuildRecipe& Recipe, TMap<FName, FJamPlacedPart>& Placed, FString& OutError)
{
	Placed.Reset();

	// En el orden de la receta: lo que se pide primero ocupa la cara primero.
	TArray<const FBuildRecipePart*> Pending;
	for (const FBuildRecipePart& P : Recipe.Parts)
	{
		Pending.Add(&P);
	}

	bool bProgress = true;
	while (Pending.Num() > 0 && bProgress)
	{
		bProgress = false;
		for (int32 i = 0; i < Pending.Num();)
		{
			const FBuildRecipePart& P = *Pending[i];
			FJamPlacedPart Item;
			if (!UJamPartCatalog::Find(P.Type, Item.Def))
			{
				OutError = FString::Printf(TEXT("Unknown part type '%s'"), *P.Type.ToString());
				return false;
			}
			Item.Half = Item.Def.Size * 0.5;
			Item.Parent = P.AttachTo;

			if (!P.AttachTo.IsNone())
			{
				const FJamPlacedPart* Parent = Placed.Find(P.AttachTo);
				if (!Parent)
				{
					++i; // el padre aún no está colocado
					continue;
				}

				FVector Dir = FVector::UpVector;
				FVector Shift = FVector::ZeroVector;
				bool bShift = false;
				if (!P.Socket.IsNone() && !ParseSocket(P.Socket.ToString(), Dir, Shift, bShift))
				{
					UE_LOG(LogJamLayout, Warning, TEXT("Unknown socket '%s' on part '%s'; using Top"), *P.Socket.ToString(), *P.Id.ToString());
					Dir = FVector::UpVector;
					bShift = false;
				}

				const FVector AbsDir = AbsV(Dir);
				Item.OutwardDir = Dir;
				Item.Center = Parent->Center + Dir * (FVector::DotProduct(AbsDir, Parent->Half) + FVector::DotProduct(AbsDir, Item.Half));
				if (bShift)
				{
					const FVector AbsShift = AbsV(Shift);
					const double Edge = FVector::DotProduct(AbsShift, Parent->Half) - FVector::DotProduct(AbsShift, Item.Half);
					Item.Center += Shift * FMath::Max(0.0, Edge);
				}

				// Choques con piezas ya colocadas (el padre no cuenta: padre/hijo no colisionan).
				// Se empuja hacia afuera por la misma cara: dos piezas en "Back" quedan una detrás de otra.
				const int32 Axis = AbsDir.X > 0.5 ? 0 : (AbsDir.Y > 0.5 ? 1 : 2);
				const double Sign = Dir[Axis] > 0.0 ? 1.0 : -1.0;
				for (int32 Iter = 0; Iter < 64; ++Iter)
				{
					bool bMoved = false;
					for (const TPair<FName, FJamPlacedPart>& KV : Placed)
					{
						if (KV.Key == P.AttachTo || !Overlaps(Item, KV.Value))
						{
							continue;
						}
						const FJamPlacedPart& Q = KV.Value;
						const double Target = Q.Center[Axis] + Sign * (Q.Half[Axis] + Item.Half[Axis] + Gap);
						if (Sign * (Target - Item.Center[Axis]) > 0.0)
						{
							Item.Center[Axis] = Target;
							bMoved = true;
						}
					}
					if (!bMoved)
					{
						break;
					}
				}
			}

			Placed.Add(P.Id, Item);
			Pending.RemoveAt(i);
			bProgress = true;
		}
	}

	if (Pending.Num() > 0)
	{
		OutError = TEXT("Unresolvable parent references");
		return false;
	}
	return true;
}
