// Arma de mano pedida (GDD 5.1): decision por palabras clave, sin clave ni red.
// Todo el vocabulario, los numeros y las piezas viven en Content/Game/Combat/Data/Weapons.json (ver su _doc).
#include "Combat/JamGunSpec.h"
#include "GameJamm.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Engine/StaticMesh.h"
#include "HAL/IConsoleManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace
{
	TAutoConsoleVariable<int32> CVarJamGunGenerated(TEXT("jam.Weapon.Generated"), 1,
		TEXT("1 = arma de mano pedida (GDD 5.1): 'un rifle que dispare hielo' te la pone en la mano. 0 = apagada: todo pedido va a construccion y Q no la ofrece."));

	typedef TSharedPtr<FJsonObject> FJamGunJson;

	FJamGunJson& JamGunCache()
	{
		static FJamGunJson Cached;
		return Cached;
	}

	/** Content/Game/Combat/Data/Weapons.json, leido una vez (JamGun::Reload lo suelta). Nunca devuelve nulo. */
	FJamGunJson JamGunConfig()
	{
		FJamGunJson& Cached = JamGunCache();
		if (!Cached.IsValid())
		{
			FString Text;
			const FString Path = FPaths::ProjectContentDir() / TEXT("Game/Combat/Data/Weapons.json");
			FJamGunJson Parsed;
			if (FFileHelper::LoadFileToString(Text, *Path) && FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Parsed) && Parsed.IsValid())
			{
				Cached = Parsed;
			}
			else
			{
				UE_LOG(LogJam, Warning, TEXT("Arma pedida: no pude leer %s; uso el vocabulario y los valores por defecto"), *Path);
				Cached = MakeShared<FJsonObject>();
			}
		}
		return Cached;
	}

	double JamGunNum(const FJamGunJson& O, const TCHAR* Field, double Default)
	{
		double V = Default;
		if (O.IsValid()) { O->TryGetNumberField(Field, V); }
		return V;
	}

	FString JamGunStr(const FJamGunJson& O, const TCHAR* Field, const FString& Default = FString())
	{
		FString V = Default;
		if (O.IsValid()) { O->TryGetStringField(Field, V); }
		return V;
	}

	bool JamGunBool(const FJamGunJson& O, const TCHAR* Field, bool Default)
	{
		bool V = Default;
		if (O.IsValid()) { O->TryGetBoolField(Field, V); }
		return V;
	}

	FVector JamGunVec(const FJamGunJson& O, const TCHAR* Field, const FVector& Default)
	{
		const TArray<TSharedPtr<FJsonValue>>* A = nullptr;
		if (!O.IsValid() || !O->TryGetArrayField(Field, A) || !A || A->Num() < 3)
		{
			return Default;
		}
		for (int32 I = 0; I < 3; ++I)
		{
			if (!(*A)[I].IsValid()) { return Default; }
		}
		return FVector((*A)[0]->AsNumber(), (*A)[1]->AsNumber(), (*A)[2]->AsNumber());
	}

	/** Objeto hijo, o nulo si no existe (los demas helpers aceptan nulo y devuelven el valor por defecto). */
	FJamGunJson JamGunObj(const FJamGunJson& O, const TCHAR* Field)
	{
		const TSharedPtr<FJsonObject>* Child = nullptr;
		if (O.IsValid() && O->TryGetObjectField(Field, Child) && Child && Child->IsValid())
		{
			return *Child;
		}
		return nullptr;
	}

	TArray<FJamGunJson> JamGunList(const FJamGunJson& O, const TCHAR* Field)
	{
		TArray<FJamGunJson> Out;
		const TArray<TSharedPtr<FJsonValue>>* A = nullptr;
		if (O.IsValid() && O->TryGetArrayField(Field, A) && A)
		{
			for (const TSharedPtr<FJsonValue>& V : *A)
			{
				const TSharedPtr<FJsonObject>* Item = nullptr;
				if (V.IsValid() && V->TryGetObject(Item) && Item && Item->IsValid()) { Out.Add(*Item); }
			}
		}
		return Out;
	}

	FJamGunJson JamGunById(const TArray<FJamGunJson>& List, const FString& Id)
	{
		if (!Id.IsEmpty())
		{
			for (const FJamGunJson& Item : List)
			{
				if (JamGunStr(Item, TEXT("id")).Equals(Id, ESearchCase::IgnoreCase)) { return Item; }
			}
		}
		return nullptr;
	}

	/**
	 * Padded = frase normalizada (" un rifle que dispare hielo "). La clave vale por inicio de palabra
	 * ("electri" vale para "electrico"); con '=' al inicio exige palabra(s) completas ("=un arma" no vale para "un armario").
	 */
	bool JamGunHasKey(const FString& Padded, const FString& RawKey)
	{
		const bool bExact = RawKey.StartsWith(TEXT("="));
		FString Key = JamGun::Normalize(RawKey); // " clave "
		if (Key.Len() < 3)
		{
			return false;
		}
		if (!bExact) { Key.LeftChopInline(1); } // " clave": puede seguir la palabra
		return Padded.Contains(Key, ESearchCase::CaseSensitive);
	}

	bool JamGunAnyOf(const TArray<FString>& Keys, const FString& Padded)
	{
		for (const FString& K : Keys)
		{
			if (JamGunHasKey(Padded, K)) { return true; }
		}
		return false;
	}

	bool JamGunAnyKey(const FJamGunJson& O, const TCHAR* Field, const FString& Padded)
	{
		TArray<FString> Keys;
		return O.IsValid() && O->TryGetStringArrayField(Field, Keys) && JamGunAnyOf(Keys, Padded);
	}

	FJamGunPiece JamGunPiece(const FJamGunJson& O, const FVector& Origin)
	{
		FJamGunPiece P;
		P.Mesh = JamGunStr(O, TEXT("mesh"));
		P.Size = JamGunVec(O, TEXT("size"), FVector::ZeroVector);
		P.Pos = Origin + JamGunVec(O, TEXT("pos"), FVector::ZeroVector);
		const FVector R = JamGunVec(O, TEXT("rot"), FVector::ZeroVector); // pitch, yaw, roll
		P.Rot = FRotator(R.X, R.Y, R.Z);
		P.bGlow = JamGunBool(O, TEXT("glow"), false);
		return P;
	}

	void JamGunPieces(const FJamGunJson& O, const FVector& Origin, TArray<FJamGunPiece>& Out)
	{
		for (const FJamGunJson& Item : JamGunList(O, TEXT("pieces")))
		{
			const FJamGunPiece P = JamGunPiece(Item, Origin);
			if (!P.Mesh.IsEmpty()) { Out.Add(P); }
		}
	}

	// Respaldo si Weapons.json no esta o viene vacio: lo minimo para que el pedido de arma siga funcionando.
	const TArray<FString>& JamGunFallbackKeys()
	{
		static const TArray<FString> Keys = { TEXT("pistol"), TEXT("rifle"), TEXT("escopeta"), TEXT("metralleta"), TEXT("lanzacohetes"), TEXT("=un arma"), TEXT("=una arma") };
		return Keys;
	}

	const TArray<FString>& JamGunFallbackExclude()
	{
		static const TArray<FString> Keys = { TEXT("carro"), TEXT("coche"), TEXT("tanque"), TEXT("torreta"), TEXT("vehic"), TEXT("dron"), TEXT("barco") };
		return Keys;
	}
}

bool JamGun::IsEnabled()
{
	return CVarJamGunGenerated.GetValueOnAnyThread() != 0;
}

void JamGun::Reload()
{
	JamGunCache().Reset();
}

FString JamGun::Normalize(const FString& Text)
{
	FString Out;
	Out.Reserve(Text.Len() + 2);
	Out.AppendChar(TEXT(' '));
	bool bSpace = true;
	for (int32 I = 0; I < Text.Len(); ++I)
	{
		int32 C = int32(Text[I]);
		if (C >= 'A' && C <= 'Z') { C += 'a' - 'A'; }
		// Vocales con tilde o dieresis y la enie (mayusculas y minusculas), por codigo para que el fuente siga en ASCII.
		switch (C)
		{
		case 0xC0: case 0xC1: case 0xC2: case 0xC3: case 0xC4: case 0xE0: case 0xE1: case 0xE2: case 0xE3: case 0xE4: C = 'a'; break;
		case 0xC8: case 0xC9: case 0xCA: case 0xCB: case 0xE8: case 0xE9: case 0xEA: case 0xEB: C = 'e'; break;
		case 0xCC: case 0xCD: case 0xCE: case 0xCF: case 0xEC: case 0xED: case 0xEE: case 0xEF: C = 'i'; break;
		case 0xD2: case 0xD3: case 0xD4: case 0xD5: case 0xD6: case 0xF2: case 0xF3: case 0xF4: case 0xF5: case 0xF6: C = 'o'; break;
		case 0xD9: case 0xDA: case 0xDB: case 0xDC: case 0xF9: case 0xFA: case 0xFB: case 0xFC: C = 'u'; break;
		case 0xD1: case 0xF1: C = 'n'; break;
		default: break;
		}
		const bool bWord = (C >= 'a' && C <= 'z') || (C >= '0' && C <= '9');
		if (bWord)
		{
			Out.AppendChar(TCHAR(C));
			bSpace = false;
		}
		else if (!bSpace)
		{
			Out.AppendChar(TEXT(' '));
			bSpace = true;
		}
	}
	if (!bSpace) { Out.AppendChar(TEXT(' ')); }
	return Out;
}

bool JamGun::IsWeaponPhrase(const FString& Phrase)
{
	if (!IsEnabled())
	{
		return false;
	}
	const FString P = Normalize(Phrase);
	if (P.Len() < 3)
	{
		return false;
	}
	const FJamGunJson Root = JamGunConfig();
	if (!Root->HasField(TEXT("keywords")) && !Root->HasField(TEXT("bodies")))
	{
		return !JamGunAnyOf(JamGunFallbackExclude(), P) && JamGunAnyOf(JamGunFallbackKeys(), P);
	}
	// Un pedido de construccion que menciona disparar ("un carro que dispare fuego") no es arma de mano.
	if (JamGunAnyKey(Root, TEXT("exclude"), P))
	{
		return false;
	}
	if (JamGunAnyKey(Root, TEXT("keywords"), P))
	{
		return true;
	}
	for (const FJamGunJson& Body : JamGunList(Root, TEXT("bodies")))
	{
		if (JamGunAnyKey(Body, TEXT("keywords"), P)) { return true; }
	}
	return false;
}

namespace
{
	// ---- PUNTO DE ENCHUFE DEL MODELO DE DECISION (OpenAI Decisions; ver UJamWeaponComponent::RequestGeneratedWeapon) ----
	// Con clave, el modelo solo ELIGE (CLAUDE.md 0.1): un "id" de bodies, un "id" de muzzles y los "id" de extras de
	// Weapons.json, a partir de la frase (JamGun::BuildDecisionRequest / ParseDecisionResponse). Esos ids entran aqui en
	// lugar de las busquedas por palabra de los pasos 1 a 3; el resto (numeros, limites, piezas) no cambia: el codigo
	// decide lo posible. ExtraIds nulo = sin modelo: todo por palabras (JamGun::Decide), que tambien es el respaldo.
	bool JamGunDecideImpl(const FString& Phrase, const FString& BodyId, const FString& MuzzleId, const TArray<FString>* ExtraIds, FJamGunSpec& Out)
	{
	if (!JamGun::IsWeaponPhrase(Phrase))
	{
		return false;
	}
	const FString P = JamGun::Normalize(Phrase);
	const FJamGunJson Root = JamGunConfig();
	const FJamGunJson Def = JamGunObj(Root, TEXT("defaults"));
	const TArray<FJamGunJson> Bodies = JamGunList(Root, TEXT("bodies"));
	const TArray<FJamGunJson> Muzzles = JamGunList(Root, TEXT("muzzles"));
	const TArray<FJamGunJson> Extras = JamGunList(Root, TEXT("extras"));

	// 1. Cuerpo: el id que eligio el modelo; si no hay (o no existe), el primero cuya palabra aparece; si no, el de defaults.
	FJamGunJson Body = JamGunById(Bodies, BodyId);
	if (!Body.IsValid())
	{
		for (const FJamGunJson& B : Bodies)
		{
			if (JamGunAnyKey(B, TEXT("keywords"), P)) { Body = B; break; }
		}
	}
	if (!Body.IsValid()) { Body = JamGunById(Bodies, JamGunStr(Def, TEXT("body"))); }
	if (!Body.IsValid() && Bodies.Num() > 0) { Body = Bodies[0]; }

	// 2. Boca (el efecto): la que se nombro; si no, la propia del cuerpo (lanzacohetes = bomba); si no, la de defaults.
	// Con modelo: la boca que eligio. Solo entra al nombre ("Rifle de hielo") si no es la propia del cuerpo ("Lanzacohetes", no "Lanzacohetes de bombas").
	FJamGunJson Muzzle = JamGunById(Muzzles, MuzzleId);
	bool bMuzzleNamed = Muzzle.IsValid() && !JamGunStr(Muzzle, TEXT("id")).Equals(JamGunStr(Body, TEXT("muzzle")), ESearchCase::IgnoreCase);
	if (!Muzzle.IsValid())
	{
		for (const FJamGunJson& M : Muzzles)
		{
			if (JamGunAnyKey(M, TEXT("keywords"), P)) { Muzzle = M; bMuzzleNamed = true; break; }
		}
	}
	if (!Muzzle.IsValid()) { Muzzle = JamGunById(Muzzles, JamGunStr(Body, TEXT("muzzle"))); }
	if (!Muzzle.IsValid()) { Muzzle = JamGunById(Muzzles, JamGunStr(Def, TEXT("muzzle"))); }
	if (!Muzzle.IsValid() && Muzzles.Num() > 0) { Muzzle = Muzzles[0]; }

	// Numeros de la boca (con los de defaults si falta un campo) por los multiplicadores del cuerpo.
	auto MuzzleNum = [&Muzzle, &Def](const TCHAR* Field, double Fallback) { return JamGunNum(Muzzle, Field, JamGunNum(Def, Field, Fallback)); };
	double Damage = MuzzleNum(TEXT("damage"), 12.0) * JamGunNum(Body, TEXT("damage_mult"), 1.0);
	double Interval = MuzzleNum(TEXT("interval"), 0.5) * JamGunNum(Body, TEXT("interval_mult"), 1.0);
	double Speed = MuzzleNum(TEXT("speed"), 4500.0) * JamGunNum(Body, TEXT("speed_mult"), 1.0);
	double Energy = MuzzleNum(TEXT("energy"), 4.0) * JamGunNum(Body, TEXT("energy_mult"), 1.0);
	double Range = MuzzleNum(TEXT("range"), 4500.0) * JamGunNum(Body, TEXT("range_mult"), 1.0);
	double Push = MuzzleNum(TEXT("push"), 250.0) * JamGunNum(Body, TEXT("push_mult"), 1.0);
	const double Gravity = MuzzleNum(TEXT("gravity"), 0.2);
	double Pellets = JamGunNum(Body, TEXT("pellets"), 1.0);
	double Spread = JamGunNum(Body, TEXT("spread_deg"), 0.0);
	double SpreadMult = 1.0;

	// 3. Extras: los que se nombraron y los que el cuerpo trae de fabrica (el francotirador trae mira).
	TArray<FString> BodyExtras;
	if (Body.IsValid()) { Body->TryGetStringArrayField(TEXT("extras"), BodyExtras); }
	TArray<FJamGunJson> Active;
	TArray<FString> NamedExtras;
	for (const FJamGunJson& E : Extras)
	{
		const FString Id = JamGunStr(E, TEXT("id"));
		const bool bBuiltIn = BodyExtras.Contains(Id);
		// Con modelo: los extras que marco con si (ParseDecisionResponse ya resolvio los negados por palabra). Sin modelo: por palabra.
		const bool bNamed = ExtraIds
			? ExtraIds->ContainsByPredicate([&Id](const FString& X) { return X.Equals(Id, ESearchCase::IgnoreCase); })
			: JamGunAnyKey(E, TEXT("keywords"), P);
		if (!bBuiltIn && !bNamed)
		{
			continue;
		}
		Active.Add(E);
		if (!bBuiltIn) { NamedExtras.Add(JamGunStr(E, TEXT("name"), Id)); }
		Damage *= JamGunNum(E, TEXT("damage_mult"), 1.0);
		Interval *= JamGunNum(E, TEXT("interval_mult"), 1.0);
		Speed *= JamGunNum(E, TEXT("speed_mult"), 1.0);
		Energy *= JamGunNum(E, TEXT("energy_mult"), 1.0);
		Range *= JamGunNum(E, TEXT("range_mult"), 1.0);
		Push *= JamGunNum(E, TEXT("push_mult"), 1.0);
		Pellets += JamGunNum(E, TEXT("pellets_add"), 0.0);
		Spread += JamGunNum(E, TEXT("spread_add"), 0.0);
		SpreadMult *= JamGunNum(E, TEXT("spread_mult"), 1.0);
	}

	// El codigo decide lo posible: limites para que ningun dato deje un arma rota.
	Out = FJamGunSpec();
	Out.Damage = float(FMath::Max(0.0, Damage));
	Out.Interval = float(FMath::Max(FMath::Max(0.02, JamGunNum(Def, TEXT("min_interval"), 0.06)), Interval));
	Out.Speed = float(FMath::Clamp(Speed, 300.0, 40000.0));
	Out.Gravity = float(FMath::Clamp(Gravity, 0.0, 4.0));
	Out.Energy = float(FMath::Max(0.0, Energy));
	Out.Range = float(FMath::Max(300.0, Range));
	Out.Push = float(FMath::Max(0.0, Push));
	const int32 MaxPellets = FMath::Clamp(FMath::RoundToInt(float(JamGunNum(Def, TEXT("max_pellets"), 12.0))), 1, 30);
	Out.Pellets = FMath::Clamp(FMath::RoundToInt(float(Pellets)), 1, MaxPellets);
	Out.SpreadDeg = float(FMath::Clamp(Spread * SpreadMult, 0.0, 60.0));
	Out.FanJitter = float(FMath::Clamp(JamGunNum(Def, TEXT("fan_jitter"), 0.25), 0.0, 1.0));
	Out.Effect = FName(*JamGunStr(Muzzle, TEXT("effect")));
	Out.bArea = JamGunBool(Muzzle, TEXT("area"), true);
	Out.EffectPellets = FMath::Max(0, FMath::RoundToInt(float(JamGunNum(Muzzle, TEXT("effect_pellets"), 0.0))));
	Out.Sound = JamGunStr(Muzzle, TEXT("sound"));
	const FVector Rgb = JamGunVec(Muzzle, TEXT("color"), FVector(1.0, 0.85, 0.5));
	Out.Color = FLinearColor(float(Rgb.X), float(Rgb.Y), float(Rgb.Z));

	// Nombre: "Rifle" + "de hielo" (solo si la boca se nombro) + extras nombrados.
	Out.BodyName = JamGunStr(Body, TEXT("name"), TEXT("Weapon"));
	Out.MuzzleName = JamGunStr(Muzzle, TEXT("name"));
	Out.ExtraNames = NamedExtras;
	Out.Name = Out.BodyName;
	const FString Suffix = JamGunStr(Muzzle, TEXT("suffix"));
	if (bMuzzleNamed && !Suffix.IsEmpty()) { Out.Name += TEXT(" ") + Suffix; }
	for (int32 I = 0; I < NamedExtras.Num(); ++I)
	{
		Out.Name += (I == 0 ? TEXT(" with ") : TEXT(" and ")) + NamedExtras[I];
	}

	// Piezas visuales: empunadura + cuerpo + boca (pegada a muzzle_pos del cuerpo) + extras (en los mounts del cuerpo).
	JamGunPieces(JamGunObj(Root, TEXT("grip")), FVector::ZeroVector, Out.Pieces);
	JamGunPieces(Body, FVector::ZeroVector, Out.Pieces);
	const FVector MuzzlePos = JamGunVec(Body, TEXT("muzzle_pos"), FVector(30.0, 0.0, 8.0));
	FJamGunPiece Tip = JamGunPiece(Muzzle, FVector::ZeroVector);
	Tip.Size *= FMath::Clamp(JamGunNum(Body, TEXT("muzzle_scale"), 1.0), 0.1, 10.0);
	const double TipLength = FMath::Max(0.0, Tip.Size.X);
	Tip.Pos += MuzzlePos + FVector(TipLength * 0.5, 0.0, 0.0);
	if (!Tip.Mesh.IsEmpty()) { Out.Pieces.Add(Tip); }
	Out.MuzzlePoint = MuzzlePos + FVector(TipLength, 0.0, 0.0);
	const FJamGunJson Mounts = JamGunObj(Body, TEXT("mounts"));
	for (const FJamGunJson& E : Active)
	{
		const FString Id = JamGunStr(E, TEXT("id"));
		JamGunPieces(E, JamGunVec(Mounts, *Id, JamGunVec(E, TEXT("mount"), FVector(0.0, 0.0, 12.0))), Out.Pieces);
	}
	if (Out.Pieces.Num() == 0)
	{
		// Sin datos: un canon en la mano, para que el arma se vea.
		FJamGunPiece Only;
		Only.Mesh = TEXT("SM_DEV_CANNON");
		Only.Size = FVector(40.0, 0.0, 0.0);
		Only.Pos = FVector(15.0, 0.0, 6.0);
		Out.Pieces.Add(Only);
		Out.MuzzlePoint = FVector(35.0, 0.0, 6.0);
	}

	// Proyectil: el de la boca, el de defaults o una bola que brilla.
	FJamGunJson Shot = JamGunObj(Muzzle, TEXT("projectile"));
	if (!Shot.IsValid()) { Shot = JamGunObj(Def, TEXT("projectile")); }
	if (Shot.IsValid())
	{
		Out.Projectile = JamGunPiece(Shot, FVector::ZeroVector);
	}
	if (Out.Projectile.Mesh.IsEmpty())
	{
		Out.Projectile.Mesh = TEXT("SM_PART_BALL");
		Out.Projectile.Size = FVector(10.0, 10.0, 10.0);
		Out.Projectile.bGlow = true;
	}

	// Mano y fogonazo.
	const FJamGunJson Hand = JamGunObj(Root, TEXT("hand"));
	Out.Socket = FName(*JamGunStr(Hand, TEXT("socket"), TEXT("hand_r")));
	Out.bAutoGrip = JamGunBool(Hand, TEXT("auto"), true);
	Out.HandLoc = JamGunVec(Hand, TEXT("loc"), FVector::ZeroVector);
	const FVector HandRot = JamGunVec(Hand, TEXT("rot"), FVector::ZeroVector);
	Out.HandRot = FRotator(HandRot.X, HandRot.Y, HandRot.Z);
	Out.HandScale = float(FMath::Clamp(JamGunNum(Hand, TEXT("scale"), 1.0), 0.05, 20.0));
	Out.LooseLoc = JamGunVec(Hand, TEXT("loose_loc"), FVector(45.0, 38.0, 10.0));
	Out.AimSeconds = float(FMath::Max(0.0, JamGunNum(Hand, TEXT("aim_seconds"), 1.2)));
	Out.FlashSeconds = float(FMath::Clamp(JamGunNum(Def, TEXT("flash_seconds"), 0.07), 0.0, 1.0));
	Out.FlashIntensity = float(FMath::Max(0.0, JamGunNum(Def, TEXT("flash_intensity"), 6000.0)));
	Out.FlashRadius = float(FMath::Max(10.0, JamGunNum(Def, TEXT("flash_radius"), 350.0)));
	Out.RecoilDistance = float(FMath::Clamp(JamGunNum(Def, TEXT("recoil_distance"), 2.5), 0.0, 10.0));
	Out.RecoilPitch = float(FMath::Clamp(JamGunNum(Def, TEXT("recoil_pitch"), 2.0), 0.0, 8.0));
	Out.RecoilSeconds = float(FMath::Clamp(JamGunNum(Def, TEXT("recoil_seconds"), 0.16), 0.0, 0.5));

	UE_LOG(LogJam, Log, TEXT("Arma pedida: \"%s\" -> %s (cuerpo %s, boca %s, efecto %s): dano %.1f x%d cada %.2f s, velocidad %.0f, gravedad %.2f, alcance %.0f, energia %.1f, %d piezas"),
		*Phrase, *Out.Name, *Out.BodyName, *Out.MuzzleName, *Out.Effect.ToString(), Out.Damage, Out.Pellets, Out.Interval, Out.Speed, Out.Gravity, Out.Range, Out.Energy, Out.Pieces.Num());
	return true;
	}
}

bool JamGun::Decide(const FString& Phrase, FJamGunSpec& Out)
{
	return JamGunDecideImpl(Phrase, FString(), FString(), nullptr, Out);
}

bool JamGun::DecideWithIds(const FString& Phrase, const FString& BodyId, const FString& MuzzleId, const TArray<FString>& ExtraIds, FJamGunSpec& Out)
{
	return JamGunDecideImpl(Phrase, BodyId, MuzzleId, &ExtraIds, Out);
}

FString JamGun::BuildDecisionRequest(const FString& Phrase)
{
	// El modelo solo ELIGE entre los ids de Weapons.json. Los textos (bloque "decision", con estos por defecto) hablan de un
	// juguete y de efectos de juego: sin eso el modelo se niega ("refusal") a preguntas de armas.
	const FJamGunJson Root = JamGunConfig();
	const FJamGunJson Dec = JamGunObj(Root, TEXT("decision"));
	auto Describe = [](const FJamGunJson& O) -> FString
	{
		const FString D = JamGunStr(O, TEXT("ai"));
		return D.IsEmpty() ? JamGunStr(O, TEXT("name"), JamGunStr(O, TEXT("id"))) : D;
	};
	auto Option = [](const FString& Value, const FString& Description) -> TSharedPtr<FJsonValue>
	{
		const TSharedRef<FJsonObject> O = MakeShared<FJsonObject>();
		O->SetStringField(TEXT("value"), Value);
		if (!Description.IsEmpty()) { O->SetStringField(TEXT("description"), Description); }
		return MakeShared<FJsonValueObject>(O);
	};
	auto Question = [](const TCHAR* Type, const FString& Name, const FString& Instructions) -> TSharedRef<FJsonObject>
	{
		const TSharedRef<FJsonObject> Q = MakeShared<FJsonObject>();
		Q->SetStringField(TEXT("type"), Type);
		Q->SetStringField(TEXT("name"), Name);
		Q->SetStringField(TEXT("instructions"), Instructions);
		return Q;
	};
	auto Message = [](const TCHAR* Role, const FString& Content) -> TSharedPtr<FJsonValue>
	{
		const TSharedRef<FJsonObject> M = MakeShared<FJsonObject>();
		M->SetStringField(TEXT("role"), Role);
		M->SetStringField(TEXT("content"), Content);
		return MakeShared<FJsonValueObject>(M);
	};

	TArray<TSharedPtr<FJsonValue>> Questions;
	{
		TArray<TSharedPtr<FJsonValue>> Choices;
		for (const FJamGunJson& B : JamGunList(Root, TEXT("bodies"))) { Choices.Add(Option(JamGunStr(B, TEXT("id")), Describe(B))); }
		const TSharedRef<FJsonObject> Q = Question(TEXT("choice"), TEXT("body"),
			JamGunStr(Dec, TEXT("body"), TEXT("In this toy game, which toy body shape best matches the toy shooter the player asks for?")));
		Q->SetArrayField(TEXT("choices"), Choices);
		Questions.Add(MakeShared<FJsonValueObject>(Q));
	}
	{
		TArray<TSharedPtr<FJsonValue>> Choices;
		for (const FJamGunJson& M : JamGunList(Root, TEXT("muzzles"))) { Choices.Add(Option(JamGunStr(M, TEXT("id")), Describe(M))); }
		Choices.Add(Option(TEXT("Default"), JamGunStr(Dec, TEXT("muzzle_default"), TEXT("The player names no special effect: use the standard one for that toy body."))));
		const TSharedRef<FJsonObject> Q = Question(TEXT("choice"), TEXT("muzzle"),
			JamGunStr(Dec, TEXT("muzzle"), TEXT("In this toy game, which game effect should the toy shooter shoot? Choose Default when the player names no effect.")));
		Q->SetArrayField(TEXT("choices"), Choices);
		Questions.Add(MakeShared<FJsonValueObject>(Q));
	}
	const FString ExtraText = JamGunStr(Dec, TEXT("extra"), TEXT("In this toy game, does the player ask for this toy add-on on the toy shooter: %s? Answer yes only if the request mentions it."));
	for (const FJamGunJson& E : JamGunList(Root, TEXT("extras")))
	{
		Questions.Add(MakeShared<FJsonValueObject>(Question(TEXT("predicate"), TEXT("extra_") + JamGunStr(E, TEXT("id")), ExtraText.Replace(TEXT("%s"), *Describe(E)))));
	}

	TArray<TSharedPtr<FJsonValue>> Input;
	Input.Add(Message(TEXT("developer"), JamGunStr(Dec, TEXT("developer"),
		TEXT("You are the decision layer of a family-friendly toy video game. The player asks, in Spanish, for a toy shooter built from toy parts. It only shoots harmless cartoon game effects (game fire, game ice, game lightning, toy rockets); nothing is real and nobody can be hurt. Choose which toy parts the toy shooter uses. The game code sets every number and assembles it; you only choose."))));
	Input.Add(Message(TEXT("user"), JamGunStr(Dec, TEXT("user"), TEXT("Player request (toy shooter): ")) + Phrase));

	const TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetStringField(TEXT("model"), JamGunStr(Dec, TEXT("model"), TEXT("gpt-6-luna")));
	Body->SetArrayField(TEXT("input"), Input);
	Body->SetArrayField(TEXT("questions"), Questions);
	FString Out;
	FJsonSerializer::Serialize(Body, TJsonWriterFactory<>::Create(&Out));
	return Out;
}

bool JamGun::ParseDecisionResponse(const FString& ResponseJson, const FString& Phrase, FString& OutBodyId, FString& OutMuzzleId, TArray<FString>& OutExtraIds, FString& OutNote)
{
	OutBodyId.Reset();
	OutMuzzleId.Reset();
	OutExtraIds.Reset();
	FJamGunJson Response;
	const TArray<TSharedPtr<FJsonValue>>* Answers = nullptr;
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(ResponseJson), Response) || !Response.IsValid() || !Response->TryGetArrayField(TEXT("answers"), Answers) || !Answers)
	{
		OutNote = TEXT("response missing answers");
		return false;
	}
	const FJamGunJson Root = JamGunConfig();
	const double MinConf = JamGunNum(JamGunObj(Root, TEXT("decision")), TEXT("min_confidence"), 0.6);
	const TArray<FJamGunJson> Extras = JamGunList(Root, TEXT("extras"));
	const FString P = Normalize(Phrase);
	int32 Answered = 0, Refused = 0;
	for (const TSharedPtr<FJsonValue>& V : *Answers)
	{
		const TSharedPtr<FJsonObject>* Item = nullptr;
		if (!V.IsValid() || !V->TryGetObject(Item) || !Item || !Item->IsValid())
		{
			continue;
		}
		const FJamGunJson A = *Item;
		const FString Type = JamGunStr(A, TEXT("type"));
		const FString Name = JamGunStr(A, TEXT("name"));
		if (Type == TEXT("choice"))
		{
			++Answered;
			// Con poca confianza, o "Default" en la boca, el id queda vacio: decide la palabra clave (o lo propio del cuerpo).
			const FString Picked = JamGunStr(A, TEXT("choice"));
			if (JamGunNum(A, TEXT("confidence"), 0.0) < MinConf) { continue; }
			if (Name == TEXT("body")) { OutBodyId = Picked; }
			else if (Name == TEXT("muzzle") && !Picked.Equals(TEXT("Default"), ESearchCase::IgnoreCase)) { OutMuzzleId = Picked; }
		}
		else if (Type == TEXT("predicate"))
		{
			++Answered;
			if (Name.StartsWith(TEXT("extra_")) && JamGunNum(A, TEXT("probability"), 0.0) > 0.5) { OutExtraIds.AddUnique(Name.RightChop(6)); }
		}
		else
		{
			// Negativa del modelo en esta pregunta: sin respuesta. Un extra negado lo decide su palabra clave; cuerpo y boca quedan vacios.
			++Refused;
			if (Name.StartsWith(TEXT("extra_")) && JamGunAnyKey(JamGunById(Extras, Name.RightChop(6)), TEXT("keywords"), P)) { OutExtraIds.AddUnique(Name.RightChop(6)); }
		}
	}
	OutNote = FString::Printf(TEXT("answered=%d refused=%d"), Answered, Refused);
	return Answered > 0; // todo negado: decide la palabra clave
}

FTransform JamGun::FitPiece(const UStaticMesh* Mesh, const FJamGunPiece& Piece)
{
	FVector Scale(1.0);
	FVector Center = FVector::ZeroVector;
	if (Mesh)
	{
		// Mide la malla real: cada eje con medida se escala a esa medida; los que van en 0 conservan la proporcion.
		const FBox Box = Mesh->GetBoundingBox();
		const FVector Have = Box.GetSize();
		Center = Box.GetCenter();
		double Keep = 1.0;
		for (int32 I = 0; I < 3; ++I)
		{
			if (Piece.Size[I] > 0.0 && Have[I] > KINDA_SMALL_NUMBER) { Keep = Piece.Size[I] / Have[I]; break; }
		}
		for (int32 I = 0; I < 3; ++I)
		{
			Scale[I] = (Piece.Size[I] > 0.0 && Have[I] > KINDA_SMALL_NUMBER) ? Piece.Size[I] / Have[I] : Keep;
		}
	}
	const FQuat Rot = Piece.Rot.Quaternion();
	return FTransform(Rot, Piece.Pos - Rot.RotateVector(Scale * Center), Scale);
}

UStaticMesh* JamGun::LoadMesh(const FString& NameOrPath)
{
	if (NameOrPath.IsEmpty())
	{
		return nullptr;
	}
	const FString Path = NameOrPath.StartsWith(TEXT("/")) ? NameOrPath : FString::Printf(TEXT("/Game/Game/Building/Parts/%s.%s"), *NameOrPath, *NameOrPath);
	UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, *Path);
	if (!Mesh)
	{
		UE_LOG(LogJam, Warning, TEXT("Arma pedida: no existe la malla %s (Weapons.json)"), *Path);
	}
	return Mesh;
}

FString JamGun::Number(float Value)
{
	const float Whole = FMath::RoundToFloat(Value);
	return FMath::IsNearlyEqual(Value, Whole, 0.05f) ? FString::FromInt(int32(Whole)) : FString::Printf(TEXT("%.1f"), Value);
}
