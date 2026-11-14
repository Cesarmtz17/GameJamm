// Solver por capacidades. Port fiel de Content/Game/Building/Data/solver_proto.py (solve / try_template):
// si cambias la lógica aquí, cámbiala allá y corre  python3 -I Content/Game/Building/Data/solver_proto.py
#include "Building/JamSolver.h"

#include "Building/JamBuildPart.h"
#include "Core/InventoryComponent.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

DEFINE_LOG_CATEGORY_STATIC(LogJamSolver, Log, All);

// Namespace con nombre (no anónimo): el módulo compila en unity build y los helpers de otros .cpp no deben chocar.
namespace JamSolverImpl
{
	const int32 InfiniteCount = 1000000;

	/** Un elemento de BuildKnowledge.json (solo lo que usa el solver). */
	struct FKnElement
	{
		FName Type;
		bool bInfinite = false;
		float MassKg = 0.f;
		float EnergyCostPerSecond = 0.f;
		float EnergyCapacity = 0.f;
		TArray<FString> Provides;
		TArray<FString> Faces;
		TArray<FName> Substitutes;
		FString LiftFace; // "Top" / "Bottom" / vacío
	};

	struct FKnSlot
	{
		FString Role;
		int32 Count = 1;
		bool bHasMin = false;
		int32 Min = 0;
		bool bOptional = false;
		bool bPairs = false;
		bool bRoot = false;
		bool bPreferWanted = false;
		bool bHasFace = false;
		FString Face;
		TArray<FString> Faces;
		TArray<FName> PartPref;
		FString On;
	};

	struct FKnTemplate
	{
		FName Id;
		FString NameEs;
		FName CodeArchetype;
		FString Orientation;
		bool bAnchored = false;
		TArray<FKnSlot> Slots;
		TArray<FString> Requires;
		TArray<FName> Fallbacks;
	};

	struct FKnowledge
	{
		bool bTried = false;
		bool bLoaded = false;
		int32 MaxParts = UJamAssembler::MaxParts;
		float EnergyStart = 60.f;
		float WarnDurationSeconds = 15.f;
		TArray<FKnElement> Elements;
		TArray<FKnTemplate> Templates;
	};

	FKnowledge& Knowledge()
	{
		static FKnowledge Instance;
		return Instance;
	}

	// ------------------------------------------------------------------------------------------
	// Lectura del JSON
	// ------------------------------------------------------------------------------------------

	FString JsonString(const TSharedPtr<FJsonObject>& O, const TCHAR* Key)
	{
		FString S;
		O->TryGetStringField(Key, S);
		return S;
	}

	bool JsonBool(const TSharedPtr<FJsonObject>& O, const TCHAR* Key)
	{
		bool bValue = false;
		O->TryGetBoolField(Key, bValue);
		return bValue;
	}

	double JsonNumber(const TSharedPtr<FJsonObject>& O, const TCHAR* Key, double Default)
	{
		double Value = Default;
		if (!O->TryGetNumberField(Key, Value))
		{
			return Default;
		}
		return Value;
	}

	TArray<FString> JsonStrings(const TSharedPtr<FJsonObject>& O, const TCHAR* Key)
	{
		TArray<FString> Out;
		const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
		if (O->TryGetArrayField(Key, Arr) && Arr)
		{
			for (const TSharedPtr<FJsonValue>& V : *Arr)
			{
				FString S;
				if (V.IsValid() && V->TryGetString(S) && !S.IsEmpty())
				{
					Out.Add(S);
				}
			}
		}
		return Out;
	}

	TArray<FName> JsonNames(const TSharedPtr<FJsonObject>& O, const TCHAR* Key)
	{
		TArray<FName> Out;
		for (const FString& S : JsonStrings(O, Key))
		{
			Out.Add(FName(*S));
		}
		return Out;
	}

	/** nullptr si el valor no es un objeto JSON. */
	const TSharedPtr<FJsonObject>* JsonAsObject(const TSharedPtr<FJsonValue>& V)
	{
		const TSharedPtr<FJsonObject>* Obj = nullptr;
		if (V.IsValid() && V->TryGetObject(Obj) && Obj && Obj->IsValid())
		{
			return Obj;
		}
		return nullptr;
	}

	bool ParseKnowledge(const FString& Json, FKnowledge& Out, FString& OutError)
	{
		TSharedPtr<FJsonObject> Root;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
		if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
		{
			OutError = TEXT("knowledge is not valid JSON");
			return false;
		}

		const TArray<TSharedPtr<FJsonValue>>* Elements = nullptr;
		const TArray<TSharedPtr<FJsonValue>>* Templates = nullptr;
		if (!Root->TryGetArrayField(TEXT("elements"), Elements) || !Elements || Elements->Num() == 0
			|| !Root->TryGetArrayField(TEXT("templates"), Templates) || !Templates || Templates->Num() == 0)
		{
			OutError = TEXT("knowledge without elements/templates");
			return false;
		}

		for (const TSharedPtr<FJsonValue>& V : *Elements)
		{
			const TSharedPtr<FJsonObject>* Obj = JsonAsObject(V);
			if (!Obj)
			{
				OutError = TEXT("element is not an object");
				return false;
			}
			const TSharedPtr<FJsonObject>& O = *Obj;
			FKnElement E;
			const FString Type = JsonString(O, TEXT("type"));
			if (Type.IsEmpty())
			{
				OutError = TEXT("element without type");
				return false;
			}
			E.Type = FName(*Type);
			E.bInfinite = JsonBool(O, TEXT("infinite"));
			E.MassKg = static_cast<float>(JsonNumber(O, TEXT("mass_kg"), 0.0));
			E.EnergyCostPerSecond = static_cast<float>(JsonNumber(O, TEXT("energy_cost_per_s"), 0.0));
			E.EnergyCapacity = static_cast<float>(JsonNumber(O, TEXT("energy_capacity"), 0.0));
			E.Provides = JsonStrings(O, TEXT("provides"));
			E.Faces = JsonStrings(O, TEXT("faces"));
			E.Substitutes = JsonNames(O, TEXT("substitutes"));
			E.LiftFace = JsonString(O, TEXT("lift_face"));
			Out.Elements.Add(E);
		}

		for (const TSharedPtr<FJsonValue>& V : *Templates)
		{
			const TSharedPtr<FJsonObject>* Obj = JsonAsObject(V);
			if (!Obj)
			{
				OutError = TEXT("template is not an object");
				return false;
			}
			const TSharedPtr<FJsonObject>& O = *Obj;
			FKnTemplate T;
			const FString Id = JsonString(O, TEXT("id"));
			if (Id.IsEmpty())
			{
				OutError = TEXT("template without id");
				return false;
			}
			T.Id = FName(*Id);
			T.NameEs = JsonString(O, TEXT("es"));
			const FString CodeArchetype = JsonString(O, TEXT("code_archetype")); // null en el JSON -> vacío
			T.CodeArchetype = CodeArchetype.IsEmpty() ? FName() : FName(*CodeArchetype);
			T.Orientation = JsonString(O, TEXT("orientation"));
			T.bAnchored = JsonBool(O, TEXT("anchored"));
			T.Requires = JsonStrings(O, TEXT("requires"));
			T.Fallbacks = JsonNames(O, TEXT("fallbacks"));

			const TArray<TSharedPtr<FJsonValue>>* Slots = nullptr;
			if (O->TryGetArrayField(TEXT("slots"), Slots) && Slots)
			{
				for (const TSharedPtr<FJsonValue>& SV : *Slots)
				{
					const TSharedPtr<FJsonObject>* SlotObj = JsonAsObject(SV);
					if (!SlotObj)
					{
						OutError = FString::Printf(TEXT("template %s: slot is not an object"), *Id);
						return false;
					}
					const TSharedPtr<FJsonObject>& S = *SlotObj;
					FKnSlot Slot;
					Slot.Role = JsonString(S, TEXT("role"));
					if (Slot.Role.IsEmpty())
					{
						OutError = FString::Printf(TEXT("template %s: slot without role"), *Id);
						return false;
					}
					Slot.Count = FMath::Max(1, FMath::RoundToInt32(JsonNumber(S, TEXT("count"), 1.0)));
					double MinValue = 0.0;
					Slot.bHasMin = S->TryGetNumberField(TEXT("min"), MinValue);
					Slot.Min = FMath::RoundToInt32(MinValue);
					Slot.bOptional = JsonBool(S, TEXT("optional"));
					Slot.bPairs = JsonBool(S, TEXT("pairs"));
					Slot.bRoot = JsonBool(S, TEXT("root"));
					Slot.bPreferWanted = JsonBool(S, TEXT("prefer_wanted"));
					Slot.Face = JsonString(S, TEXT("face"));
					Slot.bHasFace = !Slot.Face.IsEmpty();
					Slot.Faces = JsonStrings(S, TEXT("faces"));
					Slot.PartPref = JsonNames(S, TEXT("part_pref"));
					Slot.On = JsonString(S, TEXT("on"));
					T.Slots.Add(Slot);
				}
			}
			Out.Templates.Add(T);
		}

		Out.MaxParts = FMath::Min(FMath::RoundToInt32(JsonNumber(Root, TEXT("max_parts"), UJamAssembler::MaxParts)), static_cast<int32>(UJamAssembler::MaxParts));
		if (Out.MaxParts < 1)
		{
			Out.MaxParts = UJamAssembler::MaxParts;
		}
		const TSharedPtr<FJsonObject>* Energy = nullptr;
		if (Root->TryGetObjectField(TEXT("energy"), Energy) && Energy && Energy->IsValid())
		{
			Out.EnergyStart = static_cast<float>(JsonNumber(*Energy, TEXT("start"), 60.0));
			Out.WarnDurationSeconds = static_cast<float>(JsonNumber(*Energy, TEXT("warn_duration_s"), 15.0));
		}
		return true;
	}

	// ------------------------------------------------------------------------------------------
	// Consultas
	// ------------------------------------------------------------------------------------------

	const FKnElement* FindElem(const FKnowledge& K, FName Type)
	{
		for (const FKnElement& E : K.Elements)
		{
			if (E.Type == Type)
			{
				return &E;
			}
		}
		return nullptr;
	}

	/** Por id de plantilla y, si no, por code_archetype (Fortress -> Wall). */
	const FKnTemplate* FindTemplate(const FKnowledge& K, FName Name)
	{
		if (Name.IsNone())
		{
			return nullptr;
		}
		for (const FKnTemplate& T : K.Templates)
		{
			if (T.Id == Name)
			{
				return &T;
			}
		}
		for (const FKnTemplate& T : K.Templates)
		{
			if (T.CodeArchetype == Name)
			{
				return &T;
			}
		}
		return nullptr;
	}

	bool ProvidesRole(const FKnElement* E, const FString& Role)
	{
		if (!E)
		{
			return false;
		}
		if (E->Provides.Contains(Role))
		{
			return true;
		}
		if (Role == TEXT("weapon"))
		{
			for (const FString& P : E->Provides)
			{
				if (P.StartsWith(TEXT("weapon_")))
				{
					return true;
				}
			}
		}
		return false;
	}

	bool IsWeapon(const FKnElement* E)
	{
		return E && (ProvidesRole(E, TEXT("weapon")) || E->Provides.Contains(TEXT("bomb")));
	}

	/** El solver solo usa piezas que el JSON conoce Y que existen en el catálogo de código (lo que el ensamblador sabe spawnear). */
	bool IsUsable(const FKnowledge& K, FName Type)
	{
		if (!FindElem(K, Type))
		{
			return false;
		}
		FJamPartDef Def;
		return UJamPartCatalog::Find(Type, Def) && Def.Type == Type;
	}

	FString Primary(const FString& Face)
	{
		FString Left, Right;
		return Face.Split(TEXT("_"), &Left, &Right) ? Left : Face;
	}

	FString Secondary(const FString& Face)
	{
		FString Left, Right;
		if (!Face.Split(TEXT("_"), &Left, &Right))
		{
			return FString();
		}
		FString Second, Rest;
		return Right.Split(TEXT("_"), &Second, &Rest) ? Second : Right;
	}

	FString WithPrimary(const FString& Face, const FString& NewPrimary)
	{
		const FString Sec = Secondary(Face);
		return (!Sec.IsEmpty() && Sec != NewPrimary) ? NewPrimary + TEXT("_") + Sec : NewPrimary;
	}

	void AddMissing(TArray<FJamSolverMissing>& Missing, FName Type, int32 Count)
	{
		if (Count <= 0 || Type.IsNone())
		{
			return;
		}
		for (FJamSolverMissing& M : Missing)
		{
			if (M.Type == Type)
			{
				M.Count += Count;
				return;
			}
		}
		FJamSolverMissing M;
		M.Type = Type;
		M.Count = Count;
		Missing.Add(M);
	}

	struct FInv
	{
		TMap<FName, int32> Items;
		bool bUnlimited = false;
	};

	int32 Avail(const FKnowledge& K, const FInv& Inv, FName Type)
	{
		const FKnElement* E = FindElem(K, Type);
		if (Inv.bUnlimited || (E && E->bInfinite))
		{
			return InfiniteCount;
		}
		return Inv.Items.FindRef(Type);
	}

	void Take(const FKnowledge& K, FInv& Inv, FName Type, int32 Count)
	{
		const FKnElement* E = FindElem(K, Type);
		if (Inv.bUnlimited || (E && E->bInfinite))
		{
			return;
		}
		Inv.Items.FindOrAdd(Type) -= Count;
	}

	/** Orden de preferencia: deseados (si aplica), part_pref, piezas con la capacidad (orden del JSON) y sus sustitutos (1 nivel). */
	TArray<FName> SlotCandidates(const FKnowledge& K, const FKnSlot& Slot, const TArray<FName>& Wanted, bool bOnlyUsable)
	{
		TArray<FName> Raw;
		for (const FName& T : Wanted)
		{
			Raw.AddUnique(T);
		}
		for (const FName& T : Slot.PartPref)
		{
			Raw.AddUnique(T);
		}
		for (const FKnElement& E : K.Elements)
		{
			if (ProvidesRole(&E, Slot.Role))
			{
				Raw.AddUnique(E.Type);
			}
		}
		const TArray<FName> Base = Raw;
		for (const FName& T : Base)
		{
			if (const FKnElement* E = FindElem(K, T))
			{
				for (const FName& S : E->Substitutes)
				{
					Raw.AddUnique(S);
				}
			}
		}
		if (!bOnlyUsable)
		{
			return Raw;
		}
		TArray<FName> Out;
		for (const FName& T : Raw)
		{
			if (IsUsable(K, T))
			{
				Out.Add(T);
			}
		}
		return Out;
	}

	/** Caras de las N piezas colocadas. Si se redujo y las que quedan comparten borde ("Left_Front","Right_Front"), se centran. */
	TArray<FString> SlotFaces(const FKnSlot& Slot, int32 N)
	{
		TArray<FString> Kept;
		if (Slot.Faces.Num() == 0)
		{
			const FString Face = Slot.bHasFace ? Slot.Face : FString(TEXT("Top"));
			for (int32 i = 0; i < N; ++i)
			{
				Kept.Add(Face);
			}
			return Kept;
		}
		for (int32 i = 0; i < N; ++i)
		{
			Kept.Add(Slot.Faces[FMath::Min(i, Slot.Faces.Num() - 1)]);
		}
		if (N > 0 && N < Slot.Faces.Num())
		{
			const FString Sec = Secondary(Kept[0]);
			bool bKeptShare = !Sec.IsEmpty();
			for (const FString& F : Kept)
			{
				bKeptShare = bKeptShare && Secondary(F) == Sec;
			}
			bool bAllShare = true;
			for (const FString& F : Slot.Faces)
			{
				bAllShare = bAllShare && Secondary(F) == Sec;
			}
			if (bKeptShare && !bAllShare)
			{
				for (FString& F : Kept)
				{
					F = Primary(F);
				}
			}
		}
		return Kept;
	}

	bool IsLiftRole(const FString& Role)
	{
		return Role == TEXT("lift") || Role == TEXT("hover_static");
	}

	/** Una pieza de sustento va en SU cara de sustento (globo arriba, ventilador abajo) aunque el slot diga otra. */
	FString AdjustFace(const FKnowledge& K, FName Type, const FString& Role, const FString& Face)
	{
		const FKnElement* E = FindElem(K, Type);
		if (IsLiftRole(Role) && E && !E->LiftFace.IsEmpty() && Primary(Face) != E->LiftFace)
		{
			return WithPrimary(Face, E->LiftFace);
		}
		return Face;
	}

	/** Cara de un sustituto de un extra: respeta la cara pedida salvo que no pueda ir ahí. */
	FString SubstituteFace(const FKnowledge& K, FName WantedType, FName UsedType, const FString& Face)
	{
		const FKnElement* W = FindElem(K, WantedType);
		const FKnElement* U = FindElem(K, UsedType);
		if (!U)
		{
			return Face;
		}
		if (W && !W->LiftFace.IsEmpty() && !U->LiftFace.IsEmpty() && Primary(Face) == W->LiftFace)
		{
			return WithPrimary(Face, U->LiftFace);
		}
		if (U->Faces.Num() == 0 || U->Faces.Contains(TEXT("*")) || U->Faces.Contains(Primary(Face)))
		{
			return Face;
		}
		return U->Faces[0];
	}

	/** Primera cara de la lista `faces` de la pieza que esté libre en la raíz. */
	FString FreeFace(const FKnowledge& K, const TArray<FBuildRecipePart>& Parts, FName RootId, FName Type)
	{
		static const TCHAR* const AllFaces[] = { TEXT("Top"), TEXT("Front"), TEXT("Back"), TEXT("Left"), TEXT("Right"), TEXT("Bottom") };
		TArray<FString> Faces;
		const FKnElement* E = FindElem(K, Type);
		if (E)
		{
			Faces = E->Faces;
		}
		if (Faces.Num() == 0 || Faces.Contains(TEXT("*")))
		{
			Faces.Reset();
			for (const TCHAR* F : AllFaces)
			{
				Faces.Add(F);
			}
		}
		TArray<FString> Used;
		for (const FBuildRecipePart& P : Parts)
		{
			if (P.AttachTo == RootId)
			{
				Used.Add(Primary(P.Socket.ToString()));
			}
		}
		for (const FString& F : Faces)
		{
			if (!Used.Contains(F))
			{
				return F;
			}
		}
		return Faces[0];
	}

	int32 CountType(const TArray<FBuildRecipePart>& Parts, FName Type)
	{
		int32 Count = 0;
		for (const FBuildRecipePart& P : Parts)
		{
			Count += P.Type == Type ? 1 : 0;
		}
		return Count;
	}

	bool HasId(const TArray<FBuildRecipePart>& Parts, FName Id)
	{
		for (const FBuildRecipePart& P : Parts)
		{
			if (P.Id == Id)
			{
				return true;
			}
		}
		return false;
	}

	FName UniqueId(const TArray<FBuildRecipePart>& Parts, const FString& Base)
	{
		if (!HasId(Parts, FName(*Base)))
		{
			return FName(*Base);
		}
		int32 i = 2;
		while (HasId(Parts, FName(*FString::Printf(TEXT("%s_%d"), *Base, i))))
		{
			++i;
		}
		return FName(*FString::Printf(TEXT("%s_%d"), *Base, i));
	}

	FName FirstAvailable(const FKnowledge& K, const FInv& Inv, const TArray<FName>& Candidates, int32 Unit)
	{
		for (const FName& C : Candidates)
		{
			if (Avail(K, Inv, C) >= Unit)
			{
				return C;
			}
		}
		return FName();
	}

	TArray<FName> UsableSubstitutes(const FKnowledge& K, FName Type)
	{
		TArray<FName> Out;
		if (const FKnElement* E = FindElem(K, Type))
		{
			for (const FName& S : E->Substitutes)
			{
				if (IsUsable(K, S))
				{
					Out.Add(S);
				}
			}
		}
		return Out;
	}

	FBuildRecipePart MakePart(FName Id, FName Type, FName Parent, const FString& Socket)
	{
		FBuildRecipePart P;
		P.Id = Id;
		P.Type = Type;
		P.AttachTo = Parent;
		P.Socket = Socket.IsEmpty() ? FName() : FName(*Socket);
		return P;
	}

	// ------------------------------------------------------------------------------------------
	// Un intento con una plantilla
	// ------------------------------------------------------------------------------------------

	struct FAttempt
	{
		bool bOk = false;
		TArray<FBuildRecipePart> Parts;
		TArray<FJamSolverSubstitution> Substitutions;
		TArray<FJamSolverReduction> Reductions;
		TArray<FJamSolverMissing> Missing;
		TArray<FName> Unsupported;
		TArray<FName> Trimmed;
		TArray<FName> Added;
	};

	void AddSubstitution(FAttempt& Att, const FString& Role, FName Wanted, FName Used, int32 Count)
	{
		FJamSolverSubstitution S;
		S.Role = Role;
		S.Wanted = Wanted;
		S.Used = Used;
		S.Count = Count;
		Att.Substitutions.Add(S);
	}

	/** bOk = false si un slot obligatorio no llega a su mínimo o no se cumple `requires`. */
	FAttempt TryTemplate(const FKnowledge& K, const FKnTemplate& Tpl, const FJamSolverRequest& Req, int32 MaxParts)
	{
		static const FName BigWheelsKey(TEXT("bigwheels"));
		static const FName BigWheelType(TEXT("BigWheel"));
		static const FName FlyerName(TEXT("Flyer"));

		FAttempt Att;
		TArray<FBuildRecipePart>& Parts = Att.Parts;
		FInv Inv;
		Inv.Items = Req.Inventory;
		Inv.bUnlimited = Req.bUnlimitedInventory;
		const TArray<FJamExtraDef>& ExtraTable = UJamBuildGenerator::ExtraTable();

		// Lo que pidió el jugador: extras (orden de ExtraTable) y luego WantTypes (orden de llegada).
		TArray<FName> WantOrder;
		TMap<FName, int32> WantCount;
		for (const FJamExtraDef& Ex : ExtraTable)
		{
			if (!Req.Choice.Extras.Contains(Ex.Key))
			{
				continue;
			}
			const FName T = !Ex.PartType.IsNone() ? Ex.PartType : (Ex.Key == BigWheelsKey ? BigWheelType : FName());
			if (!T.IsNone())
			{
				WantOrder.AddUnique(T);
			}
		}
		for (const FName& T : Req.Choice.WantTypes)
		{
			if (T.IsNone())
			{
				continue;
			}
			WantOrder.AddUnique(T);
			WantCount.FindOrAdd(T) += 1;
		}
		for (const FName& T : WantOrder)
		{
			if (!IsUsable(K, T))
			{
				Att.Unsupported.AddUnique(T);
			}
		}
		TMap<FName, int32> Fulfilled; // tipo deseado -> piezas colocadas en su nombre por los slots

		TArray<FKnSlot> DefaultSlots;
		if (Tpl.Slots.Num() == 0)
		{
			FKnSlot Chassis;
			Chassis.Role = TEXT("chassis");
			DefaultSlots.Add(Chassis);
		}
		const TArray<FKnSlot>& Slots = Tpl.Slots.Num() > 0 ? Tpl.Slots : DefaultSlots;

		int32 RootIdx = INDEX_NONE;
		for (int32 i = 0; i < Slots.Num() && RootIdx == INDEX_NONE; ++i)
		{
			if (Slots[i].bRoot)
			{
				RootIdx = i;
			}
		}
		for (int32 i = 0; i < Slots.Num() && RootIdx == INDEX_NONE; ++i)
		{
			if (Slots[i].Role == TEXT("chassis"))
			{
				RootIdx = i;
			}
		}
		if (RootIdx == INDEX_NONE)
		{
			RootIdx = 0;
		}
		TArray<int32> Order;
		Order.Add(RootIdx);
		for (int32 i = 0; i < Slots.Num(); ++i)
		{
			if (i != RootIdx)
			{
				Order.Add(i);
			}
		}

		FName RootId;
		TMap<FString, FName> RoleFirst;
		TMap<FString, int32> RoleCounter;
		bool bFailed = false;

		for (const int32 SlotIdx : Order)
		{
			const FKnSlot& Slot = Slots[SlotIdx];
			const FString& Role = Slot.Role;
			const bool bIsRoot = SlotIdx == RootIdx;
			const bool bOptional = Slot.bOptional && !bIsRoot;
			const int32 Count = bIsRoot ? 1 : Slot.Count;

			TArray<FName> WantedForRole;
			for (const FName& T : WantOrder)
			{
				if (ProvidesRole(FindElem(K, T), Role))
				{
					WantedForRole.Add(T);
				}
			}
			FName Trigger;
			bool bOnlyProviders = false;
			if (bOptional)
			{
				if (Role == TEXT("control"))
				{
					// El Seat es gratis: siempre.
				}
				else if (WantedForRole.Num() > 0)
				{
					Trigger = WantedForRole[0];
				}
				else if (Role == TEXT("anchor") && Tpl.bAnchored)
				{
					bOnlyProviders = true; // anclar solo con una pieza que ancle de verdad; si no hay, va suelta sin avisar
				}
				else
				{
					continue;
				}
			}
			int32 Minimum = bOptional ? 0 : (Slot.bHasMin ? Slot.Min : Count);
			if (bIsRoot)
			{
				Minimum = 1;
			}
			const bool bUseWanted = bOptional || Slot.bPreferWanted;
			TArray<FName> Candidates = SlotCandidates(K, Slot, bUseWanted ? WantedForRole : TArray<FName>(), true);
			if (bOnlyProviders)
			{
				Candidates.RemoveAll([&K, &Role](const FName& T) { return !ProvidesRole(FindElem(K, T), Role); });
			}
			const FName FirstChoice = Candidates.Num() > 0 ? Candidates[0] : FName();
			const int32 Unit = Slot.bPairs ? 2 : 1;

			TArray<FName> Placed;
			while (Placed.Num() + Unit <= Count && Parts.Num() + Placed.Num() + Unit <= MaxParts)
			{
				const FName T = FirstAvailable(K, Inv, Candidates, Unit);
				if (T.IsNone())
				{
					break;
				}
				Take(K, Inv, T, Unit);
				for (int32 u = 0; u < Unit; ++u)
				{
					Placed.Add(T);
				}
			}
			const int32 N = Placed.Num();

			if (N < Minimum)
			{
				bFailed = true;
				if (!FirstChoice.IsNone())
				{
					// Lo que sobra del tipo preferido (p. ej. 1 rueda suelta de un par) ya no hay que pedirlo.
					AddMissing(Att.Missing, FirstChoice, FMath::Max(1, Minimum - N - Avail(K, Inv, FirstChoice)));
				}
				else
				{
					const TArray<FName> Raw = SlotCandidates(K, Slot, TArray<FName>(), false);
					if (Raw.Num() > 0)
					{
						Att.Unsupported.AddUnique(Raw[0]);
					}
				}
				if (bIsRoot)
				{
					break;
				}
				continue;
			}
			if (N == 0)
			{
				if (!Trigger.IsNone())
				{
					if (IsUsable(K, Trigger))
					{
						AddMissing(Att.Missing, Trigger, 1);
					}
					Fulfilled.FindOrAdd(Trigger) += 1; // ya se reportó: que no lo repita la fase de extras
				}
				continue;
			}
			if (N < Count && Minimum > 0)
			{
				FJamSolverReduction R;
				R.Role = Role;
				R.Wanted = Count;
				R.Got = N;
				Att.Reductions.Add(R);
				AddMissing(Att.Missing, FirstChoice, FMath::Max(1, Count - N - Avail(K, Inv, FirstChoice)));
			}

			const TArray<FString> Faces = SlotFaces(Slot, N);
			FName Parent = RootId;
			if (!Slot.On.IsEmpty())
			{
				if (const FName* OnPart = RoleFirst.Find(Slot.On))
				{
					Parent = *OnPart;
				}
			}
			for (int32 k = 0; k < N; ++k)
			{
				const FName T = Placed[k];
				FName PartId;
				if (bIsRoot)
				{
					PartId = FName(TEXT("body"));
				}
				else if (Role == TEXT("control"))
				{
					PartId = UniqueId(Parts, TEXT("seat"));
				}
				else
				{
					int32& Counter = RoleCounter.FindOrAdd(Role);
					PartId = UniqueId(Parts, FString::Printf(TEXT("%s%d"), *Role, Counter));
					++Counter;
				}
				Parts.Add(MakePart(PartId, T, bIsRoot ? FName() : Parent, bIsRoot ? FString() : AdjustFace(K, T, Role, Faces[k])));
				if (bIsRoot)
				{
					RootId = PartId;
				}
				if (!RoleFirst.Contains(Role))
				{
					RoleFirst.Add(Role, PartId);
				}
			}

			TArray<FName> Seen;
			for (const FName& T : Placed)
			{
				if (Seen.Contains(T))
				{
					continue;
				}
				Seen.Add(T);
				int32 C = 0;
				for (const FName& X : Placed)
				{
					C += X == T ? 1 : 0;
				}
				if (T != FirstChoice)
				{
					AddSubstitution(Att, Role, FirstChoice, T, C);
					if (!ProvidesRole(FindElem(K, T), Role))
					{
						AddMissing(Att.Missing, FirstChoice, C); // sustituto pasivo: lo ideal sigue faltando
					}
				}
				if (Trigger.IsNone() && Slot.bPreferWanted && WantOrder.Contains(T))
				{
					Fulfilled.FindOrAdd(T) += C;
				}
			}
			if (!Trigger.IsNone())
			{
				Fulfilled.FindOrAdd(Trigger) += N;
			}
		}

		if (bFailed || RootId.IsNone())
		{
			return Att;
		}

		// Capacidades obligatorias del arquetipo (`requires`): nunca devolver algo que no se mueva / no ataque.
		if (Tpl.Requires.Contains(TEXT("move")))
		{
			static const TCHAR* const MoveCaps[] = { TEXT("ground_drive"), TEXT("spin"), TEXT("thrust"), TEXT("burst_thrust"), TEXT("lift") };
			bool bCanMove = false;
			for (const FBuildRecipePart& P : Parts)
			{
				if (const FKnElement* E = FindElem(K, P.Type))
				{
					for (const TCHAR* Cap : MoveCaps)
					{
						bCanMove = bCanMove || E->Provides.Contains(Cap);
					}
				}
			}
			if (!bCanMove)
			{
				TArray<FName> Thrusters;
				for (const FKnElement& E : K.Elements)
				{
					if (E.Provides.Contains(TEXT("thrust")) && IsUsable(K, E.Type))
					{
						Thrusters.AddUnique(E.Type);
					}
				}
				for (const FKnElement& E : K.Elements)
				{
					if (E.Provides.Contains(TEXT("burst_thrust")) && IsUsable(K, E.Type))
					{
						Thrusters.AddUnique(E.Type);
					}
				}
				const FName T = Parts.Num() < MaxParts ? FirstAvailable(K, Inv, Thrusters, 1) : FName();
				if (T.IsNone())
				{
					AddMissing(Att.Missing, Thrusters.Num() > 0 ? Thrusters[0] : FName(), 1);
					return Att;
				}
				Take(K, Inv, T, 1);
				Parts.Add(MakePart(UniqueId(Parts, TEXT("auto0")), T, RootId, TEXT("Back")));
				Att.Added.Add(T);
			}
		}
		if (Tpl.Requires.Contains(TEXT("attack")))
		{
			bool bCanAttack = false;
			for (const FBuildRecipePart& P : Parts)
			{
				bCanAttack = bCanAttack || IsWeapon(FindElem(K, P.Type));
			}
			if (!bCanAttack)
			{
				FName FirstWeapon;
				for (const FKnElement& E : K.Elements)
				{
					if (FirstWeapon.IsNone() && IsWeapon(&E) && IsUsable(K, E.Type))
					{
						FirstWeapon = E.Type;
					}
				}
				AddMissing(Att.Missing, FirstWeapon, 1);
				return Att;
			}
		}

		// Extras elegidos (ExtraTable): una pieza por socket, con sustitutos; lo que no cabe se recorta.
		const FName CodeArch = Tpl.CodeArchetype;
		const int32 SlotParts = Parts.Num(); // las piezas de la plantilla; los extras no se deduplican entre sí
		for (const FJamExtraDef& Ex : ExtraTable)
		{
			if (!Req.Choice.Extras.Contains(Ex.Key) || Ex.PartType.IsNone())
			{
				continue;
			}
			const FName T = Ex.PartType;
			if (!CodeArch.IsNone() && Ex.Allowed.Num() > 0 && !Ex.Allowed.Contains(CodeArch))
			{
				continue; // el arquetipo no admite este extra (igual que ComposeRecipe por tabla)
			}
			if (Fulfilled.FindRef(T) > 0 || !IsUsable(K, T))
			{
				continue;
			}
			const TArray<FName>& Sockets = (CodeArch == FlyerName && Ex.FlyerSockets.Num() > 0) ? Ex.FlyerSockets : Ex.Sockets;
			for (int32 i = 0; i < Sockets.Num(); ++i)
			{
				const FString Socket = Sockets[i].ToString();
				bool bAlreadyThere = false;
				for (int32 p = 0; p < SlotParts; ++p)
				{
					bAlreadyThere = bAlreadyThere || (Parts[p].Type == T && Primary(Parts[p].Socket.ToString()) == Primary(Socket));
				}
				if (bAlreadyThere)
				{
					continue; // la plantilla ya trae esa pieza en esa cara
				}
				if (Parts.Num() >= MaxParts)
				{
					Att.Trimmed.Add(T);
					continue;
				}
				FName Used = T;
				if (Avail(K, Inv, T) < 1)
				{
					Used = FirstAvailable(K, Inv, UsableSubstitutes(K, T), 1);
				}
				if (Used.IsNone())
				{
					AddMissing(Att.Missing, T, 1);
					continue;
				}
				Take(K, Inv, Used, 1);
				const FString Face = Used == T ? Socket : SubstituteFace(K, T, Used, Socket);
				Parts.Add(MakePart(UniqueId(Parts, FString::Printf(TEXT("%s%d"), *Ex.Key.ToString(), i)), Used, RootId, Face));
				if (Used != T)
				{
					AddSubstitution(Att, Ex.Key.ToString(), T, Used, 1);
				}
			}
		}

		// Piezas deseadas sueltas (want_<Tipo> de Jev): en la primera cara libre de su lista `faces`.
		TArray<FName> SeenWants;
		int32 WantIdx = 0;
		for (const FName& T : Req.Choice.WantTypes)
		{
			if (T.IsNone() || SeenWants.Contains(T))
			{
				continue;
			}
			SeenWants.Add(T);
			if (!IsUsable(K, T))
			{
				continue;
			}
			const int32 Need = WantCount.FindRef(T) - FMath::Max(CountType(Parts, T), Fulfilled.FindRef(T));
			for (int32 n = 0; n < Need; ++n)
			{
				if (Parts.Num() >= MaxParts)
				{
					Att.Trimmed.Add(T);
					continue;
				}
				FName Used = T;
				if (Avail(K, Inv, T) < 1)
				{
					Used = FirstAvailable(K, Inv, UsableSubstitutes(K, T), 1);
				}
				if (Used.IsNone())
				{
					AddMissing(Att.Missing, T, 1);
					continue;
				}
				Take(K, Inv, Used, 1);
				const FString Face = FreeFace(K, Parts, RootId, Used);
				Parts.Add(MakePart(UniqueId(Parts, FString::Printf(TEXT("want%d"), WantIdx)), Used, RootId, Face));
				++WantIdx;
				if (Used != T)
				{
					AddSubstitution(Att, TEXT("want"), T, Used, 1);
				}
			}
		}

		Att.bOk = true;
		return Att;
	}

	// ------------------------------------------------------------------------------------------
	// Resultado
	// ------------------------------------------------------------------------------------------

	/** "carro / coche / camioneta" -> "Carro". */
	FString DisplayName(const FKnTemplate& Tpl)
	{
		FString Name = Tpl.NameEs;
		int32 Cut = INDEX_NONE;
		if (Name.FindChar(TEXT('/'), Cut))
		{
			Name.LeftInline(Cut);
		}
		if (Name.FindChar(TEXT(':'), Cut))
		{
			Name.LeftInline(Cut);
		}
		Name.TrimStartAndEndInline();
		if (Name.IsEmpty())
		{
			return Tpl.Id.ToString();
		}
		return Name.Left(1).ToUpper() + Name.Mid(1);
	}

	/** Mensaje del asistente (doc 13 §3.3, pasos 6 y 7). */
	FString BuildMessage(const FJamSolverResult& Res, const FKnTemplate& RequestedTpl)
	{
		TArray<FString> Lines;
		if (Res.bBuildable)
		{
			Lines.Add(FString::Printf(TEXT("Armé: %s (%d piezas)."), *Res.Recipe.Name, Res.Recipe.Parts.Num()));
			if (Res.bDegraded)
			{
				Lines.Add(FString::Printf(TEXT("No alcanzó para %s."), *DisplayName(RequestedTpl)));
			}
		}
		else
		{
			Lines.Add(FString::Printf(TEXT("No puedo armar %s con lo que tienes."), *DisplayName(RequestedTpl)));
		}
		for (const FJamSolverSubstitution& S : Res.Substitutions)
		{
			Lines.Add(FString::Printf(TEXT("Usé %s en vez de %s (x%d)."), *S.Used.ToString(), *S.Wanted.ToString(), S.Count));
		}
		for (const FJamSolverReduction& R : Res.Reductions)
		{
			Lines.Add(FString::Printf(TEXT("Reduje %s de %d a %d."), *R.Role, R.Wanted, R.Got));
		}
		for (const FName& A : Res.Added)
		{
			Lines.Add(FString::Printf(TEXT("Agregué %s para que se mueva."), *A.ToString()));
		}
		if (Res.Trimmed.Num() > 0)
		{
			Lines.Add(FString::Printf(TEXT("Recorté %d piezas por el límite."), Res.Trimmed.Num()));
		}
		if (Res.Missing.Num() > 0)
		{
			TArray<FString> Items;
			for (const FJamSolverMissing& M : Res.Missing)
			{
				Items.Add(FString::Printf(TEXT("%d %s"), M.Count, *M.Type.ToString()));
			}
			Lines.Add(FString::Printf(TEXT("Te falta: %s."), *FString::Join(Items, TEXT(", "))));
		}
		if (Res.Unsupported.Num() > 0)
		{
			TArray<FString> Items;
			for (const FName& U : Res.Unsupported)
			{
				Items.Add(U.ToString());
			}
			Lines.Add(FString::Printf(TEXT("Aún no existe: %s."), *FString::Join(Items, TEXT(", "))));
		}
		if (Res.bBuildable && Res.EnergyCostPerSecond > 0.f)
		{
			Lines.Add(FString::Printf(TEXT("Gasta %.1f/s: dura %d s."), Res.EnergyCostPerSecond, FMath::FloorToInt32(Res.DurationSeconds)));
			if (Res.bLowEnergy)
			{
				Lines.Add(TEXT("Poca energía."));
			}
		}
		return FString::Join(Lines, TEXT(" "));
	}

	bool SolveImpl(const FKnowledge& K, const FJamSolverRequest& Req, FJamSolverResult& Res)
	{
		Res = FJamSolverResult();
		Res.bKnowledgeLoaded = true;
		Res.Orientation = FName(TEXT("upright"));

		const FKnTemplate* Requested = FindTemplate(K, Req.Choice.Archetype);
		if (!Requested)
		{
			Res.Note = FString::Printf(TEXT("solver: unknown archetype %s"), *Req.Choice.Archetype.ToString());
			return false;
		}
		Res.RequestedTemplate = Requested->Id;

		TArray<FName> Order;
		Order.Add(Requested->Id);
		for (const FName& F : Requested->Fallbacks)
		{
			if (FindTemplate(K, F))
			{
				Order.AddUnique(F);
			}
		}

		const FKnTemplate* Chosen = nullptr;
		FAttempt ChosenAttempt;
		bool bDegraded = false;
		TArray<FJamSolverMissing> RequestedMissing;
		for (int32 i = 0; i < Order.Num(); ++i)
		{
			const FKnTemplate* T = FindTemplate(K, Order[i]);
			FAttempt Att = TryTemplate(K, *T, Req, K.MaxParts);
			if (i == 0)
			{
				RequestedMissing = Att.Missing;
				for (const FName& U : Att.Unsupported)
				{
					Res.Unsupported.AddUnique(U);
				}
			}
			if (Att.bOk)
			{
				Chosen = T;
				ChosenAttempt = MoveTemp(Att);
				bDegraded = i > 0;
				break;
			}
		}

		if (!Chosen)
		{
			Res.Missing = RequestedMissing;
			Res.Message = BuildMessage(Res, *Requested);
			Res.Note = FString::Printf(TEXT("solver: template=%s not buildable missing=%d"), *Requested->Id.ToString(), Res.Missing.Num());
			return false;
		}

		Res.bBuildable = true;
		Res.Template = Chosen->Id;
		Res.bDegraded = bDegraded;
		Res.Substitutions = ChosenAttempt.Substitutions;
		Res.Reductions = ChosenAttempt.Reductions;
		Res.Trimmed = ChosenAttempt.Trimmed;
		Res.Added = ChosenAttempt.Added;
		for (const FName& U : ChosenAttempt.Unsupported)
		{
			Res.Unsupported.AddUnique(U);
		}
		// Si se degradó: primero lo que faltó para lo pedido; de lo demás, el máximo por tipo (no sumar dos veces la misma pieza).
		if (bDegraded)
		{
			for (const FJamSolverMissing& M : RequestedMissing)
			{
				AddMissing(Res.Missing, M.Type, M.Count);
			}
		}
		for (const FJamSolverMissing& M : ChosenAttempt.Missing)
		{
			int32 Have = 0;
			for (const FJamSolverMissing& X : Res.Missing)
			{
				if (X.Type == M.Type)
				{
					Have = X.Count;
				}
			}
			AddMissing(Res.Missing, M.Type, M.Count - Have);
		}

		FString Orientation = Req.Orientation.IsNone() ? Chosen->Orientation : Req.Orientation.ToString();
		if (Orientation.IsEmpty() || Orientation == TEXT("any"))
		{
			Orientation = TEXT("upright");
		}
		Res.Orientation = FName(*Orientation);
		Res.bAnchored = Chosen->bAnchored;

		Res.Recipe.Name = DisplayName(*Chosen);
		Res.Recipe.Parts = ChosenAttempt.Parts;
		float Reserve = 0.f;
		for (const FBuildRecipePart& P : Res.Recipe.Parts)
		{
			const FKnElement* E = FindElem(K, P.Type);
			FJamPartDef Def;
			if (!E || !UJamPartCatalog::Find(P.Type, Def))
			{
				continue; // no debería pasar: solo se colocan piezas usables
			}
			Res.MassKg += E->MassKg;
			Reserve += E->EnergyCapacity;
			// Se enciende todo lo que no es estructural/asiento/batería (mismo criterio que ComposeRecipe).
			if (Def.Kind != EJamPartKind::Structural && Def.Kind != EJamPartKind::Seat && Def.Kind != EJamPartKind::Battery)
			{
				Res.Recipe.Activate.Add(P.Id);
				Res.EnergyCostPerSecond += E->EnergyCostPerSecond;
			}
		}
		const float Bar = Req.Energy >= 0.f ? Req.Energy : K.EnergyStart;
		Res.EnergyAvailable = Bar + Reserve;
		if (Res.EnergyCostPerSecond > 0.f)
		{
			Res.DurationSeconds = Res.EnergyAvailable / Res.EnergyCostPerSecond;
			Res.bLowEnergy = Res.DurationSeconds < K.WarnDurationSeconds;
		}

		FString ValidateError;
		if (!UBuildRecipeLibrary::ValidateRecipe(Res.Recipe, nullptr, ValidateError))
		{
			// Red de seguridad: nunca entregar una receta que el ensamblador vaya a rechazar.
			UE_LOG(LogJamSolver, Warning, TEXT("solver produced an invalid recipe for %s: %s"), *Chosen->Id.ToString(), *ValidateError);
			Res.bBuildable = false;
			Res.Recipe = FBuildRecipe();
			Res.Note = FString::Printf(TEXT("solver: invalid recipe (%s)"), *ValidateError);
			return false;
		}

		Res.Message = BuildMessage(Res, *Requested);
		Res.Note = FString::Printf(TEXT("solver archetype=%s template=%s parts=%d subs=%d reduced=%d missing=%d trimmed=%d energy=%.1f/s"),
			*Req.Choice.Archetype.ToString(), *Chosen->Id.ToString(), Res.Recipe.Parts.Num(), Res.Substitutions.Num(), Res.Reductions.Num(),
			Res.Missing.Num(), Res.Trimmed.Num(), Res.EnergyCostPerSecond);
		return true;
	}

	bool ContainsAny(const FString& Text, std::initializer_list<const TCHAR*> Words)
	{
		for (const TCHAR* W : Words)
		{
			if (Text.Contains(W))
			{
				return true;
			}
		}
		return false;
	}
}

// ---------------------------------------------------------------------------------------------
// API pública
// ---------------------------------------------------------------------------------------------

FString UJamSolver::GetKnowledgePath()
{
	return FPaths::ProjectContentDir() / TEXT("Game/Building/Data/BuildKnowledge.json");
}

bool UJamSolver::LoadKnowledge(bool bForceReload)
{
	JamSolverImpl::FKnowledge& K = JamSolverImpl::Knowledge();
	if (K.bTried && !bForceReload)
	{
		return K.bLoaded;
	}

	const FString Path = GetKnowledgePath();
	FString Text;
	FString Error;
	if (!FFileHelper::LoadFileToString(Text, *Path))
	{
		K = JamSolverImpl::FKnowledge();
		K.bTried = true;
		UE_LOG(LogJamSolver, Warning, TEXT("BuildKnowledge.json not found at %s: using the table-based generator"), *Path);
		return false;
	}
	if (!LoadKnowledgeFromString(Text, Error))
	{
		UE_LOG(LogJamSolver, Warning, TEXT("BuildKnowledge.json invalid (%s): using the table-based generator"), *Error);
		return false;
	}
	UE_LOG(LogJamSolver, Log, TEXT("BuildKnowledge.json loaded: %d elements, %d templates"), K.Elements.Num(), K.Templates.Num());
	return true;
}

bool UJamSolver::LoadKnowledgeFromString(const FString& Json, FString& OutError)
{
	JamSolverImpl::FKnowledge Parsed;
	const bool bOk = JamSolverImpl::ParseKnowledge(Json, Parsed, OutError);
	JamSolverImpl::FKnowledge& K = JamSolverImpl::Knowledge();
	K = bOk ? Parsed : JamSolverImpl::FKnowledge();
	K.bTried = true;
	K.bLoaded = bOk;
	return bOk;
}

void UJamSolver::UnloadKnowledge()
{
	JamSolverImpl::FKnowledge& K = JamSolverImpl::Knowledge();
	K = JamSolverImpl::FKnowledge();
	K.bTried = true;
}

bool UJamSolver::HasTemplate(FName Archetype)
{
	return LoadKnowledge() && JamSolverImpl::FindTemplate(JamSolverImpl::Knowledge(), Archetype) != nullptr;
}

bool UJamSolver::IsInfiniteType(FName Type)
{
	if (!LoadKnowledge())
	{
		return Type == FName(TEXT("Seat"));
	}
	const JamSolverImpl::FKnElement* E = JamSolverImpl::FindElem(JamSolverImpl::Knowledge(), Type);
	return E && E->bInfinite;
}

bool UJamSolver::Solve(const FJamSolverRequest& Request, FJamSolverResult& OutResult)
{
	if (!LoadKnowledge())
	{
		OutResult = FJamSolverResult();
		OutResult.Note = TEXT("solver: no knowledge (BuildKnowledge.json missing or invalid)");
		return false;
	}
	return JamSolverImpl::SolveImpl(JamSolverImpl::Knowledge(), Request, OutResult);
}

TMap<FName, int32> UJamSolver::InventoryFromComponent(const UInventoryComponent* Inventory)
{
	TMap<FName, int32> Out;
	if (!Inventory)
	{
		return Out;
	}
	// Solo lectura del mapa público Items; no se toca la API del inventario (contrato compartido).
	for (const TPair<FName, int32>& KV : Inventory->Items)
	{
		FString Key = KV.Key.ToString();
		if (KV.Value > 0 && Key.RemoveFromStart(TEXT("Part.")) && !Key.IsEmpty())
		{
			Out.FindOrAdd(FName(*Key)) += KV.Value;
		}
	}
	return Out;
}

void UJamSolver::RefineChoiceFromText(const FString& Prompt, FJamBuildChoice& Choice)
{
	if (!LoadKnowledge())
	{
		return; // sin JSON el camino por tabla no conoce estas plantillas
	}
	using JamSolverImpl::ContainsAny;
	const FString L = Prompt.ToLower();

	const bool bVehicleWord = ContainsAny(L, { TEXT("carro"), TEXT("coche"), TEXT("camion"), TEXT("camión"), TEXT("tanque"), TEXT("tank"),
		TEXT("avion"), TEXT("avión"), TEXT("dron"), TEXT("helic"), TEXT("torreta"), TEXT("turret"), TEXT("fortaleza") });
	const bool bWheelWord = ContainsAny(L, { TEXT("rueda"), TEXT("llanta"), TEXT("wheel") });

	static const FName SpinnerName(TEXT("Spinner"));
	static const FName RollerName(TEXT("Roller"));
	static const FName AirshipName(TEXT("Airship"));
	static const FName HeadName(TEXT("Head"));

	if (HasTemplate(RollerName) && ContainsAny(L, { TEXT("rueda infernal"), TEXT("llanta infernal"), TEXT("rueda que rueda"), TEXT("rueda sola"), TEXT("roller") }))
	{
		Choice.Archetype = RollerName;
	}
	else if (HasTemplate(SpinnerName) && (ContainsAny(L, { TEXT("trompo"), TEXT("aspersor"), TEXT("spinner") })
		|| (bWheelWord && !bVehicleWord && ContainsAny(L, { TEXT("acostad"), TEXT("tumbad"), TEXT("gir"), TEXT("spin") }))))
	{
		Choice.Archetype = SpinnerName;
	}
	else if (HasTemplate(AirshipName) && (ContainsAny(L, { TEXT("dirigible"), TEXT("airship"), TEXT("zepel"), TEXT("globo aerost") })
		|| (!bVehicleWord && ContainsAny(L, { TEXT("globo"), TEXT("balloon") }))))
	{
		Choice.Archetype = AirshipName;
	}

	// "una torreta que apunte sola": pieza Head (apunta al enemigo más cercano).
	if (ContainsAny(L, { TEXT("apunt"), TEXT("auto-aim"), TEXT("autoaim"), TEXT("automat"), TEXT("automát"), TEXT("aims") }))
	{
		Choice.WantTypes.AddUnique(HeadName);
	}
}
