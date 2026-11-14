#!/usr/bin/env python3
"""Prototipo ejecutable del solver por capacidades (espejo de Source/GameJamm/Building/JamSolver.cpp).

Uso (sin Unreal):  python3 -I Content/Game/Building/Data/solver_proto.py [-v] [--emit-cpp]
  -v          imprime el mensaje y las piezas de cada caso
  --emit-cpp  regenera Source/GameJamm/Building/Tests/JamSolverCases.inl (espejo para las pruebas de Unreal)

El modelo de decision solo ELIGE (arquetipo, extras, piezas deseadas). Este codigo decide lo posible:
llena los slots de la plantilla con el inventario, sustituye, reduce, valida que se mueva/ataque y lista
los materiales que faltan. El C++ es un port linea por linea de solve()/try_template(): si cambias uno,
cambia el otro y vuelve a correr este archivo.

Lee BuildKnowledge.json y, para no duplicar tablas, ExtraTable (JamAssembler.cpp) y el catalogo de piezas
(JamBuildPart.cpp). Tambien trae un espejo de JamLayout::Compute para comprobar que nada nace encimado.
"""
import json
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.normpath(os.path.join(HERE, "..", "..", "..", ".."))
KNOWLEDGE_PATH = os.path.join(HERE, "BuildKnowledge.json")
ASSEMBLER_CPP = os.path.join(REPO, "Source", "GameJamm", "Building", "JamAssembler.cpp")
PART_CPP = os.path.join(REPO, "Source", "GameJamm", "Building", "JamBuildPart.cpp")

MAX_PARTS = 21  # UJamAssembler::MaxParts
INFINITE = 1000000
ALL_FACES = ["Top", "Front", "Back", "Left", "Right", "Bottom"]
MOVE_CAPS = ["ground_drive", "spin", "thrust", "burst_thrust", "lift"]
LIFT_ROLES = ["lift", "hover_static"]
PASSIVE_KINDS = ["Structural", "Seat", "Battery"]  # no se "encienden"


# ------------------------------------------------------------------------------------------------
# Lectura de las tablas del C++ (solo para el prototipo; el C++ las tiene en memoria)
# ------------------------------------------------------------------------------------------------

def _read(path):
    with open(path, encoding="utf-8") as f:
        return f.read()


def parse_archetype_table(cpp_text):
    """Nombres de ArchetypeTable() en JamAssembler.cpp."""
    body = cpp_text.split("UJamBuildGenerator::ArchetypeTable()", 1)[1].split("return T;", 1)[0]
    return re.findall(r'\{\s*"(\w+)"\s*,\s*TEXT\(', body)


def parse_extra_table(cpp_text):
    """ExtraTable() -> [{key, part, sockets, flyer_sockets, allowed}] en el orden de la tabla."""
    body = cpp_text.split("UJamBuildGenerator::ExtraTable()", 1)[1].split("return T;", 1)[0]
    out = []
    names = lambda s: re.findall(r'TEXT\("([^"]*)"\)', s or "")
    arr = r'(N\(\{[^}]*\}\)|\{\})'
    row = re.compile(r'\{\s*"(\w+)"\s*,\s*TEXT\("[^"]*"\)\s*,\s*TEXT\("[^"]*"\)\s*,\s*("(\w+)"|NAME_None)\s*,\s*'
                     + arr + r'\s*,\s*' + arr + r'\s*,\s*' + arr)
    for m in row.finditer(body):
        out.append({"key": m.group(1), "part": m.group(3) or "", "sockets": names(m.group(4)),
                    "flyer_sockets": names(m.group(5)), "allowed": names(m.group(6))})
    return out


def parse_catalog(cpp_text):
    """Tabla de UJamPartCatalog -> {type: {kind, size, mass}}."""
    out = {}
    row = re.compile(r'Make\("(\w+)",\s*EJamPartKind::(\w+),\s*\w+,\s*FVector\(([^)]*)\),\s*([\d.]+)f')
    for m in row.finditer(cpp_text):
        size = [float(x) for x in m.group(3).split(",")]
        out[m.group(1)] = {"kind": m.group(2), "size": size, "mass": float(m.group(4))}
    return out


# ------------------------------------------------------------------------------------------------
# Conocimiento
# ------------------------------------------------------------------------------------------------

def load_knowledge(path=KNOWLEDGE_PATH):
    """-> (knowledge, "") o (None, error). Mismo criterio minimo que UJamSolver::LoadKnowledgeFromString."""
    try:
        k = json.loads(_read(path))
    except (OSError, ValueError) as e:
        return None, "cannot read knowledge: %s" % e
    if not isinstance(k, dict) or not k.get("elements") or not k.get("templates"):
        return None, "knowledge without elements/templates"
    for e in k["elements"]:
        if not e.get("type"):
            return None, "element without type"
    for t in k["templates"]:
        if not t.get("id"):
            return None, "template without id"
    return k, ""


def elem(kn, type_name):
    for e in kn["elements"]:
        if e["type"] == type_name:
            return e
    return None


def find_template(kn, name):
    """Por id de plantilla y, si no, por code_archetype (Fortress -> Wall)."""
    for t in kn["templates"]:
        if t["id"] == name:
            return t
    for t in kn["templates"]:
        if t.get("code_archetype") == name:
            return t
    return None


def provides_role(e, role):
    if e is None:
        return False
    if role in e.get("provides", []):
        return True
    if role == "weapon":
        return any(p.startswith("weapon_") for p in e.get("provides", []))
    return False


def is_weapon(e):
    return e is not None and (provides_role(e, "weapon") or "bomb" in e.get("provides", []))


def default_usable(kn):
    """Tipos que el solver puede usar: in_code. En C++ = existe en UJamPartCatalog."""
    return [e["type"] for e in kn["elements"] if e.get("in_code")]


# ------------------------------------------------------------------------------------------------
# Solver
# ------------------------------------------------------------------------------------------------

def new_request(archetype, extras=None, inventory=None, want_types=None, orientation="", energy=-1.0, unlimited=False):
    return {"archetype": archetype, "extras": list(extras or []), "want_types": list(want_types or []),
            "orientation": orientation, "inventory": dict(inventory or {}), "unlimited": unlimited, "energy": energy}


def primary(face):
    return face.split("_")[0] if face else ""


def secondary(face):
    parts = face.split("_") if face else []
    return parts[1] if len(parts) > 1 else ""


def with_primary(face, new_primary):
    sec = secondary(face)
    return new_primary + "_" + sec if sec and sec != new_primary else new_primary


def add_unique(lst, v):
    if v not in lst:
        lst.append(v)


def add_missing(missing, type_name, count):
    """missing = lista ordenada de [tipo, cantidad] (orden de aparicion = determinista)."""
    if count <= 0 or not type_name:
        return
    for m in missing:
        if m[0] == type_name:
            m[1] += count
            return
    missing.append([type_name, count])


def avail(kn, inv, unlimited, type_name):
    e = elem(kn, type_name)
    if unlimited or (e is not None and e.get("infinite")):
        return INFINITE
    return inv.get(type_name, 0)


def take(kn, inv, unlimited, type_name, count):
    e = elem(kn, type_name)
    if unlimited or (e is not None and e.get("infinite")):
        return
    inv[type_name] = inv.get(type_name, 0) - count


def slot_candidates(kn, usable, slot, wanted):
    """Orden de preferencia: deseados (si aplica), part_pref, piezas con la capacidad (orden del JSON) y sus sustitutos (1 nivel)."""
    raw = []
    for t in wanted:
        add_unique(raw, t)
    for t in slot.get("part_pref", []):
        add_unique(raw, t)
    for e in kn["elements"]:
        if provides_role(e, slot["role"]):
            add_unique(raw, e["type"])
    for t in list(raw):
        e = elem(kn, t)
        if e is not None:
            for s in e.get("substitutes", []):
                add_unique(raw, s)
    return [t for t in raw if t in usable]


def slot_faces(slot, n):
    """Caras de las n piezas colocadas. Si se redujo y las que quedan comparten borde ("Left_Front","Right_Front"), se centran."""
    faces = slot.get("faces")
    if not faces:
        return [slot.get("face", "Top")] * n
    kept = list(faces[:n])
    while len(kept) < n:
        kept.append(faces[-1])
    if 0 < n < len(faces):
        sec = secondary(kept[0])
        if sec and all(secondary(f) == sec for f in kept) and not all(secondary(f) == sec for f in faces):
            kept = [primary(f) for f in kept]
    return kept


def adjust_face(kn, type_name, role, face):
    """Una pieza de sustento va en SU cara de sustento (globo arriba, ventilador abajo) aunque el slot diga otra."""
    e = elem(kn, type_name)
    if role in LIFT_ROLES and e is not None and e.get("lift_face") and primary(face) != e["lift_face"]:
        return with_primary(face, e["lift_face"])
    return face


def substitute_face(kn, wanted_type, used_type, face):
    """Cara de un sustituto de un extra: respeta la cara pedida salvo que no pueda ir ahi."""
    w = elem(kn, wanted_type)
    u = elem(kn, used_type)
    if u is None:
        return face
    if w is not None and w.get("lift_face") and u.get("lift_face") and primary(face) == w["lift_face"]:
        return with_primary(face, u["lift_face"])
    faces = u.get("faces", ["*"])
    if "*" in faces or primary(face) in faces or not faces:
        return face
    return faces[0]


def free_face(kn, parts, root_id, type_name):
    """Primera cara de la lista `faces` de la pieza que este libre en la raiz."""
    e = elem(kn, type_name)
    faces = list(e.get("faces", ["*"])) if e is not None else ["*"]
    if "*" in faces or not faces:
        faces = ALL_FACES
    used = [primary(p["socket"]) for p in parts if p["attach_to"] == root_id]
    for f in faces:
        if f not in used:
            return f
    return faces[0]


def count_type(parts, type_name):
    return len([p for p in parts if p["type"] == type_name])


def unique_id(parts, base):
    ids = [p["id"] for p in parts]
    if base not in ids:
        return base
    i = 2
    while "%s_%d" % (base, i) in ids:
        i += 1
    return "%s_%d" % (base, i)


def first_available(kn, inv, unlimited, cands, unit):
    for c in cands:
        if avail(kn, inv, unlimited, c) >= unit:
            return c
    return ""


def try_template(kn, extras_table, usable, tpl, req, max_parts):
    """Un intento con una plantilla. ok=False si un slot obligatorio no llega a su minimo o no cumple `requires`."""
    inv = dict(req["inventory"])
    unlimited = req["unlimited"]
    att = {"ok": False, "template": tpl["id"], "parts": [], "substitutions": [], "reductions": [], "missing": [],
           "unsupported": [], "trimmed": [], "added": [], "skipped": []}
    parts = att["parts"]

    # Lo que pidio el jugador: extras (orden de ExtraTable) y luego want_types (orden de llegada).
    want_order = []
    want_count = {}
    for ex in extras_table:
        if ex["key"] in req["extras"]:
            t = ex["part"] or ("BigWheel" if ex["key"] == "bigwheels" else "")
            if t:
                add_unique(want_order, t)
    for t in req["want_types"]:
        add_unique(want_order, t)
        want_count[t] = want_count.get(t, 0) + 1
    for t in want_order:
        if t not in usable:
            add_unique(att["unsupported"], t)
    fulfilled = {}  # tipo deseado -> piezas colocadas en su nombre por los slots

    slots = tpl.get("slots") or [{"role": "chassis", "count": 1}]
    root_idx = -1
    for i, s in enumerate(slots):
        if s.get("root"):
            root_idx = i
            break
    if root_idx < 0:
        for i, s in enumerate(slots):
            if s["role"] == "chassis":
                root_idx = i
                break
    if root_idx < 0:
        root_idx = 0
    order = [root_idx] + [i for i in range(len(slots)) if i != root_idx]

    anchored = bool(tpl.get("anchored"))
    root_id = ""
    role_first = {}
    role_counter = {}
    failed = False

    for si in order:
        slot = slots[si]
        role = slot["role"]
        is_root = si == root_idx
        optional = bool(slot.get("optional")) and not is_root
        count = 1 if is_root else int(slot.get("count", 1))
        wanted_for_role = [t for t in want_order if provides_role(elem(kn, t), role)]
        trigger = ""
        only_providers = False
        if optional:
            if role == "control":
                pass
            elif wanted_for_role:
                trigger = wanted_for_role[0]
            elif role == "anchor" and anchored:
                only_providers = True  # anclar solo con una pieza que ancle de verdad; si no hay, va suelta sin avisar
            else:
                continue
        minimum = 0 if optional else int(slot.get("min", count))
        if is_root:
            minimum = 1
        use_wanted = optional or bool(slot.get("prefer_wanted"))
        cands = slot_candidates(kn, usable, slot, wanted_for_role if use_wanted else [])
        if only_providers:
            cands = [t for t in cands if provides_role(elem(kn, t), role)]
        first_choice = cands[0] if cands else ""
        unit = 2 if slot.get("pairs") else 1

        placed = []
        while len(placed) + unit <= count and len(parts) + len(placed) + unit <= max_parts:
            t = first_available(kn, inv, unlimited, cands, unit)
            if not t:
                break
            take(kn, inv, unlimited, t, unit)
            placed.extend([t] * unit)
        n = len(placed)

        if n < minimum:
            failed = True
            if first_choice:
                # Lo que sobra del tipo preferido (p. ej. 1 rueda suelta de un par) ya no hay que pedirlo.
                add_missing(att["missing"], first_choice, max(1, minimum - n - avail(kn, inv, unlimited, first_choice)))
            else:
                raw = slot_candidates(kn, [e["type"] for e in kn["elements"]], slot, [])
                if raw:
                    add_unique(att["unsupported"], raw[0])
            if is_root:
                break
            continue
        if n == 0:
            if trigger:
                if trigger in usable:
                    add_missing(att["missing"], trigger, 1)
                fulfilled[trigger] = fulfilled.get(trigger, 0) + 1  # ya se reporto: que no lo repita la fase de extras
            continue
        if n < count and minimum > 0:
            att["reductions"].append({"role": role, "wanted": count, "got": n})
            add_missing(att["missing"], first_choice, max(1, count - n - avail(kn, inv, unlimited, first_choice)))

        faces = slot_faces(slot, n)
        parent = root_id
        if slot.get("on") and slot["on"] in role_first:
            parent = role_first[slot["on"]]
        for k, t in enumerate(placed):
            if is_root:
                pid = "body"
            elif role == "control":
                pid = unique_id(parts, "seat")
            else:
                idx = role_counter.get(role, 0)
                role_counter[role] = idx + 1
                pid = unique_id(parts, "%s%d" % (role, idx))
            part = {"id": pid, "type": t, "attach_to": "" if is_root else parent,
                    "socket": "" if is_root else adjust_face(kn, t, role, faces[k])}
            parts.append(part)
            if is_root:
                root_id = pid
            if role not in role_first:
                role_first[role] = pid

        seen = []
        for t in placed:
            if t in seen:
                continue
            seen.append(t)
            c = len([x for x in placed if x == t])
            if t != first_choice:
                att["substitutions"].append({"role": role, "wanted": first_choice, "used": t, "count": c})
                if not provides_role(elem(kn, t), role):
                    add_missing(att["missing"], first_choice, c)  # sustituto pasivo: lo ideal sigue faltando
            if trigger:
                pass
            elif slot.get("prefer_wanted") and t in want_order:
                fulfilled[t] = fulfilled.get(t, 0) + c
        if trigger:
            fulfilled[trigger] = fulfilled.get(trigger, 0) + n

    if failed or not root_id:
        return att

    # Capacidades obligatorias del arquetipo (`requires`): nunca devolver algo que no se mueva/ataque.
    requires = tpl.get("requires", [])
    if "move" in requires:
        can_move = any(any(c in elem(kn, p["type"]).get("provides", []) for c in MOVE_CAPS) for p in parts)
        if not can_move:
            thrusters = [e["type"] for e in kn["elements"] if "thrust" in e.get("provides", []) and e["type"] in usable]
            thrusters += [e["type"] for e in kn["elements"] if "burst_thrust" in e.get("provides", []) and e["type"] in usable
                          and e["type"] not in thrusters]
            t = first_available(kn, inv, unlimited, thrusters, 1) if len(parts) < max_parts else ""
            if not t:
                add_missing(att["missing"], thrusters[0] if thrusters else "", 1)
                return att
            take(kn, inv, unlimited, t, 1)
            parts.append({"id": unique_id(parts, "auto0"), "type": t, "attach_to": root_id, "socket": "Back"})
            att["added"].append(t)
    if "attack" in requires:
        if not any(is_weapon(elem(kn, p["type"])) for p in parts):
            weapons = [e["type"] for e in kn["elements"] if is_weapon(e) and e["type"] in usable]
            add_missing(att["missing"], weapons[0] if weapons else "", 1)
            return att

    # Extras elegidos (ExtraTable): una pieza por socket, con sustitutos; lo que no cabe se recorta.
    code_arch = tpl.get("code_archetype") or ""
    slot_parts = len(parts)  # las piezas de la plantilla; los extras no se deduplican entre si
    for ex in extras_table:
        if ex["key"] not in req["extras"] or not ex["part"]:
            continue
        t = ex["part"]
        if code_arch and ex["allowed"] and code_arch not in ex["allowed"]:
            att["skipped"].append(ex["key"])
            continue
        if fulfilled.get(t, 0) > 0 or t not in usable:
            continue
        socks = ex["flyer_sockets"] if (code_arch == "Flyer" and ex["flyer_sockets"]) else ex["sockets"]
        for i, sock in enumerate(socks):
            if any(p["type"] == t and primary(p["socket"]) == primary(sock) for p in parts[:slot_parts]):
                continue  # la plantilla ya trae esa pieza en esa cara
            if len(parts) >= max_parts:
                att["trimmed"].append(t)
                continue
            used = t
            if avail(kn, inv, unlimited, t) < 1:
                subs = [s for s in elem(kn, t).get("substitutes", []) if s in usable]
                used = first_available(kn, inv, unlimited, subs, 1)
            if not used:
                add_missing(att["missing"], t, 1)
                continue
            take(kn, inv, unlimited, used, 1)
            face = sock if used == t else substitute_face(kn, t, used, sock)
            parts.append({"id": unique_id(parts, "%s%d" % (ex["key"], i)), "type": used, "attach_to": root_id, "socket": face})
            if used != t:
                att["substitutions"].append({"role": ex["key"], "wanted": t, "used": used, "count": 1})

    # Piezas deseadas sueltas (want_<Tipo> de Jev): en la primera cara libre de su lista `faces`.
    seen = []
    want_idx = 0
    for t in req["want_types"]:
        if t in seen:
            continue
        seen.append(t)
        if t not in usable:
            continue
        need = want_count[t] - max(count_type(parts, t), fulfilled.get(t, 0))
        for _ in range(max(0, need)):
            if len(parts) >= max_parts:
                att["trimmed"].append(t)
                continue
            used = t
            if avail(kn, inv, unlimited, t) < 1:
                subs = [s for s in elem(kn, t).get("substitutes", []) if s in usable]
                used = first_available(kn, inv, unlimited, subs, 1)
            if not used:
                add_missing(att["missing"], t, 1)
                continue
            take(kn, inv, unlimited, used, 1)
            parts.append({"id": unique_id(parts, "want%d" % want_idx), "type": used, "attach_to": root_id,
                          "socket": free_face(kn, parts, root_id, used)})
            want_idx += 1
            if used != t:
                att["substitutions"].append({"role": "want", "wanted": t, "used": used, "count": 1})

    att["ok"] = True
    return att


def solve(kn, extras_table, catalog, req, usable=None):
    """Decisiones + inventario -> resultado (receta valida o lista de lo que falta). Determinista."""
    if usable is None:
        usable = default_usable(kn)
    max_parts = min(int(kn.get("max_parts", MAX_PARTS)), MAX_PARTS)
    res = {"buildable": False, "template": "", "requested_template": "", "degraded": False,
           "recipe": {"name": "", "parts": [], "activate": []},
           "substitutions": [], "reductions": [], "missing": [], "unsupported": [], "trimmed": [], "added": [],
           "orientation": "upright", "anchored": False, "energy_cost_per_s": 0.0, "energy_available": 0.0,
           "duration_s": -1.0, "low_energy": False, "mass_kg": 0.0, "message": "", "note": ""}
    tpl = find_template(kn, req["archetype"])
    if tpl is None:
        res["note"] = "solver: unknown archetype %s" % req["archetype"]
        return res
    res["requested_template"] = tpl["id"]

    order = [tpl["id"]]
    for f in tpl.get("fallbacks", []):
        if f not in order and find_template(kn, f) is not None:
            order.append(f)

    chosen = None
    requested_missing = []
    for i, tid in enumerate(order):
        t = find_template(kn, tid)
        att = try_template(kn, extras_table, usable, t, req, max_parts)
        if i == 0:
            requested_missing = att["missing"]
            for u in att["unsupported"]:
                add_unique(res["unsupported"], u)
        if att["ok"]:
            chosen = (t, att, i > 0)
            break

    if chosen is None:
        res["missing"] = requested_missing
        res["message"] = message_es(kn, res, tpl)
        res["note"] = "solver: template=%s not buildable missing=%d" % (tpl["id"], len(res["missing"]))
        return res

    t, att, degraded = chosen
    res["buildable"] = True
    res["template"] = t["id"]
    res["degraded"] = degraded
    res["substitutions"] = att["substitutions"]
    res["reductions"] = att["reductions"]
    res["trimmed"] = att["trimmed"]
    res["added"] = att["added"]
    for u in att["unsupported"]:
        add_unique(res["unsupported"], u)
    # Si se degrado: primero lo que falto para lo pedido; de lo demas, el maximo por tipo (no sumar dos veces la misma pieza).
    if degraded:
        for m in requested_missing:
            add_missing(res["missing"], m[0], m[1])
    for m in att["missing"]:
        have = 0
        for x in res["missing"]:
            if x[0] == m[0]:
                have = x[1]
        add_missing(res["missing"], m[0], m[1] - have)
    orientation = req["orientation"] or t.get("orientation", "upright")
    res["orientation"] = "upright" if orientation == "any" else orientation
    res["anchored"] = bool(t.get("anchored"))

    rec = res["recipe"]
    rec["name"] = display_name(t)
    energy = kn.get("energy", {})
    reserve = 0.0
    for p in att["parts"]:
        rec["parts"].append({"id": p["id"], "type": p["type"], "attach_to": p["attach_to"], "socket": p["socket"]})
        e = elem(kn, p["type"])
        res["mass_kg"] += float(e.get("mass_kg", 0))
        reserve += float(e.get("energy_capacity", 0))
        # Piezas simuladas en pruebas (aun fuera del catalogo C++): se encienden si gastan energia.
        kind = catalog[p["type"]]["kind"] if p["type"] in catalog else ("Device" if e.get("energy_cost_per_s", 0) > 0 else "Structural")
        if kind not in PASSIVE_KINDS:
            rec["activate"].append(p["id"])
            res["energy_cost_per_s"] += float(e.get("energy_cost_per_s", 0))
    bar = req["energy"] if req["energy"] >= 0 else float(energy.get("start", 60))
    res["energy_available"] = bar + reserve
    if res["energy_cost_per_s"] > 0:
        res["duration_s"] = res["energy_available"] / res["energy_cost_per_s"]
        res["low_energy"] = res["duration_s"] < float(energy.get("warn_duration_s", 15))
    res["message"] = message_es(kn, res, tpl)
    res["note"] = "solver archetype=%s template=%s parts=%d subs=%d reduced=%d missing=%d trimmed=%d energy=%.1f/s" % (
        req["archetype"], t["id"], len(rec["parts"]), len(res["substitutions"]), len(res["reductions"]),
        len(res["missing"]), len(res["trimmed"]), res["energy_cost_per_s"])
    return res


def display_name(tpl):
    name = tpl.get("es", tpl["id"]).split("/")[0].split(":")[0].strip()
    return name[:1].upper() + name[1:] if name else tpl["id"]


def message_es(kn, res, requested_tpl):
    """Mensaje del asistente (paso 6 y 7 del doc 13 §3.3)."""
    parts = []
    if res["buildable"]:
        parts.append("Armé: %s (%d piezas)." % (res["recipe"]["name"], len(res["recipe"]["parts"])))
        if res["degraded"]:
            parts.append("No alcanzó para %s." % display_name(requested_tpl))
    else:
        parts.append("No puedo armar %s con lo que tienes." % display_name(requested_tpl))
    for s in res["substitutions"]:
        parts.append("Usé %s en vez de %s (x%d)." % (s["used"], s["wanted"], s["count"]))
    for r in res["reductions"]:
        parts.append("Reduje %s de %d a %d." % (r["role"], r["wanted"], r["got"]))
    for a in res["added"]:
        parts.append("Agregué %s para que se mueva." % a)
    if res["trimmed"]:
        parts.append("Recorté %d piezas por el límite." % len(res["trimmed"]))
    if res["missing"]:
        parts.append("Te falta: %s." % ", ".join("%d %s" % (m[1], m[0]) for m in res["missing"]))
    if res["unsupported"]:
        parts.append("Aún no existe: %s." % ", ".join(res["unsupported"]))
    if res["buildable"] and res["energy_cost_per_s"] > 0:
        parts.append("Gasta %.1f/s: dura %d s." % (res["energy_cost_per_s"], int(res["duration_s"])))
        if res["low_energy"]:
            parts.append("Poca energía.")
    return " ".join(parts)


# ------------------------------------------------------------------------------------------------
# Espejo de JamLayout (ParseSocket/Compute/Overlaps) para comprobar que nada nace encimado
# ------------------------------------------------------------------------------------------------

_TOKENS = {"top": (0, 0, 1), "up": (0, 0, 1), "above": (0, 0, 1), "upper": (0, 0, 1),
           "bottom": (0, 0, -1), "down": (0, 0, -1), "below": (0, 0, -1), "under": (0, 0, -1), "underneath": (0, 0, -1), "lower": (0, 0, -1),
           "front": (1, 0, 0), "forward": (1, 0, 0), "fwd": (1, 0, 0), "nose": (1, 0, 0),
           "back": (-1, 0, 0), "rear": (-1, 0, 0), "behind": (-1, 0, 0), "tail": (-1, 0, 0),
           "left": (0, -1, 0), "right": (0, 1, 0), "side": (0, 1, 0)}


def parse_socket(socket):
    s = socket.strip()
    if s.lower().startswith("attach_"):
        s = s[7:]
    s = s.replace("-", "_").replace(" ", "_")
    direction, shift = None, None
    for tok in [x for x in s.split("_") if x]:
        d = _TOKENS.get(tok.lower())
        if d is None:
            continue
        if direction is None:
            direction = d
        elif abs(sum(a * b for a, b in zip(d, direction))) < 0.5:
            shift = d
            break
    return direction, shift


def layout_overlaps(a, b, tol=0.5):
    return all(abs(a["c"][i] - b["c"][i]) < a["h"][i] + b["h"][i] - tol for i in range(3))


def layout_compute(recipe, catalog):
    placed = {}
    pending = list(recipe["parts"])
    progress = True
    while pending and progress:
        progress = False
        i = 0
        while i < len(pending):
            p = pending[i]
            half = [x * 0.5 for x in catalog[p["type"]]["size"]]
            item = {"c": [0.0, 0.0, 0.0], "h": half, "parent": p["attach_to"]}
            if p["attach_to"]:
                parent = placed.get(p["attach_to"])
                if parent is None:
                    i += 1
                    continue
                direction, shift = (0, 0, 1), None
                if p["socket"]:
                    d, s = parse_socket(p["socket"])
                    if d is not None:
                        direction, shift = d, s
                ad = [abs(x) for x in direction]
                reach = sum(ad[k] * parent["h"][k] for k in range(3)) + sum(ad[k] * half[k] for k in range(3))
                item["c"] = [parent["c"][k] + direction[k] * reach for k in range(3)]
                if shift is not None:
                    ash = [abs(x) for x in shift]
                    edge = sum(ash[k] * parent["h"][k] for k in range(3)) - sum(ash[k] * half[k] for k in range(3))
                    item["c"] = [item["c"][k] + shift[k] * max(0.0, edge) for k in range(3)]
                axis = 0 if ad[0] > 0.5 else (1 if ad[1] > 0.5 else 2)
                sign = 1.0 if direction[axis] > 0 else -1.0
                for _ in range(64):
                    moved = False
                    for key, q in placed.items():
                        if key == p["attach_to"] or not layout_overlaps(item, q):
                            continue
                        target = q["c"][axis] + sign * (q["h"][axis] + half[axis] + 1.0)
                        if sign * (target - item["c"][axis]) > 0:
                            item["c"][axis] = target
                            moved = True
                    if not moved:
                        break
            placed[p["id"]] = item
            pending.pop(i)
            progress = True
    return placed if not pending else None


def validate_recipe(recipe, catalog, max_parts=MAX_PARTS):
    """Espejo de UBuildRecipeLibrary::ValidateRecipe + sin piezas encimadas. -> lista de errores."""
    errs = []
    parts = recipe["parts"]
    if not parts:
        return ["no parts"]
    if len(parts) > max_parts:
        errs.append("too many parts %d" % len(parts))
    ids = [p["id"] for p in parts]
    if len(set(ids)) != len(ids):
        errs.append("duplicated ids")
    if len([p for p in parts if not p["attach_to"]]) != 1:
        errs.append("needs exactly one root")
    for p in parts:
        if p["type"] not in catalog:
            errs.append("unknown type %s" % p["type"])
        if p["attach_to"] and p["attach_to"] not in ids:
            errs.append("missing parent %s" % p["attach_to"])
        if p["attach_to"] and parse_socket(p["socket"])[0] is None:
            errs.append("bad socket %s" % p["socket"])
    for a in recipe["activate"]:
        if a not in ids:
            errs.append("activate unknown id %s" % a)
    if errs:
        return errs
    placed = layout_compute(recipe, catalog)
    if placed is None:
        return ["layout failed"]
    keys = sorted(placed)
    for i, a in enumerate(keys):
        for b in keys[i + 1:]:
            if placed[a]["parent"] != b and placed[b]["parent"] != a and layout_overlaps(placed[a], placed[b]):
                errs.append("%s overlaps %s" % (a, b))
    return errs


# ------------------------------------------------------------------------------------------------
# Casos de prueba: pedido en lenguaje natural ya traducido a (arquetipo, extras, deseados, inventario)
# ------------------------------------------------------------------------------------------------
# Campos esperados (todos opcionales):
#   buildable, template, degraded, types {tipo: n} (exacto), has {tipo: n} (minimo), parts (total),
#   sockets {id: (tipo, padre, cara)}, subs [(deseado, usado)], reduced [(rol, de, a)], missing {tipo: n},
#   unsupported [tipos], trimmed (cuantos), added [tipos], orientation, anchored, cost, duration, low_energy.
# `usable_extra` simula piezas que aun no estan en el catalogo C++ (Sled, Ball, Head...).

RICH = {"Plank": 10, "Panel": 10, "Wall": 10, "Block": 10, "Cylinder": 10, "Wheel": 10, "BigWheel": 10, "Spring": 10,
        "Fan": 10, "Rocket": 10, "Jet": 10, "Turbine": 10, "Wing": 10, "Sail": 10, "Balloon": 10, "HoverStone": 10,
        "Bomb": 10, "Battery": 10, "Flamethrower": 10, "Frost": 10, "Beam": 10, "Cannon": 10}

CASES = [
    # --- carro ---
    dict(name="01 carro basico", prompt="hazme un carro", archetype="Car", inv={"Plank": 1, "Wheel": 4},
         expect=dict(buildable=True, template="Car", types={"Plank": 1, "Wheel": 4, "Seat": 1}, subs=[], reduced=[], missing={},
                     sockets={"body": ("Plank", "", ""), "ground_drive0": ("Wheel", "body", "Left_Front"),
                              "ground_drive3": ("Wheel", "body", "Right_Back"), "seat": ("Seat", "body", "Top")},
                     cost=0.8, duration=75.0, low_energy=False, orientation="upright")),
    dict(name="02 carro con cohete", prompt="un carro con cohete", archetype="Car", extras=["rocket"],
         inv={"Plank": 1, "Wheel": 4, "Rocket": 1},
         expect=dict(buildable=True, template="Car", types={"Plank": 1, "Wheel": 4, "Seat": 1, "Rocket": 1},
                     sockets={"rocket0": ("Rocket", "body", "Back")}, missing={}, cost=0.8)),
    dict(name="03 carro con cohete, sin cohete: usa ventilador", prompt="un carro con cohete", archetype="Car", extras=["rocket"],
         inv={"Plank": 1, "Wheel": 4, "Fan": 1},
         expect=dict(buildable=True, types={"Plank": 1, "Wheel": 4, "Seat": 1, "Fan": 1}, subs=[("Rocket", "Fan")],
                     sockets={"rocket0": ("Fan", "body", "Back")}, missing={}, cost=2.3)),
    dict(name="04 carro con cohete, sin cohete ni sustituto: pide material", prompt="un carro con cohete", archetype="Car",
         extras=["rocket"], inv={"Plank": 1, "Wheel": 4},
         expect=dict(buildable=True, types={"Plank": 1, "Wheel": 4, "Seat": 1}, missing={"Rocket": 1})),
    dict(name="05 carro con 2 ruedas: reduce en par y centra", prompt="un carrito", archetype="Car", inv={"Plank": 1, "Wheel": 2},
         expect=dict(buildable=True, template="Car", types={"Plank": 1, "Wheel": 2, "Seat": 1}, reduced=[("ground_drive", 4, 2)],
                     missing={"Wheel": 2}, sockets={"ground_drive0": ("Wheel", "body", "Left"), "ground_drive1": ("Wheel", "body", "Right")})),
    dict(name="06 carro con 3 ruedas: pares simetricos -> 2", prompt="un carro", archetype="Car", inv={"Plank": 1, "Wheel": 3},
         expect=dict(buildable=True, types={"Plank": 1, "Wheel": 2, "Seat": 1}, reduced=[("ground_drive", 4, 2)], missing={"Wheel": 1})),
    dict(name="07 carro sin Wheel: sustituye por BigWheel", prompt="un coche", archetype="Car", inv={"Plank": 1, "BigWheel": 4},
         expect=dict(buildable=True, types={"Plank": 1, "BigWheel": 4, "Seat": 1}, subs=[("Wheel", "BigWheel")], missing={})),
    dict(name="08 carro mixto: 2 Wheel + 2 BigWheel (un par de cada una)", prompt="un coche", archetype="Car",
         inv={"Plank": 1, "Wheel": 2, "BigWheel": 2},
         expect=dict(buildable=True, types={"Plank": 1, "Wheel": 2, "BigWheel": 2, "Seat": 1}, subs=[("Wheel", "BigWheel")],
                     reduced=[], sockets={"ground_drive0": ("Wheel", "body", "Left_Front"), "ground_drive2": ("BigWheel", "body", "Left_Back")})),
    dict(name="09 carro todoterreno (ruedas grandes)", prompt="un carro con ruedas grandes todoterreno", archetype="Car",
         extras=["bigwheels"], inv={"Plank": 1, "Wheel": 4, "BigWheel": 4},
         expect=dict(buildable=True, types={"Plank": 1, "BigWheel": 4, "Seat": 1}, subs=[], missing={})),
    dict(name="10 carro sin tabla: chasis sustituto (Panel)", prompt="un carro", archetype="Car", inv={"Panel": 1, "Wheel": 4},
         expect=dict(buildable=True, types={"Panel": 1, "Wheel": 4, "Seat": 1}, subs=[("Plank", "Panel")])),
    dict(name="11 carro sin ruedas: trineos + ventilador para que se mueva", prompt="un carro", archetype="Car",
         inv={"Plank": 1, "Sled": 4, "Fan": 1}, usable_extra=["Sled", "Ball"], skip_layout=True,
         expect=dict(buildable=True, template="Car", types={"Plank": 1, "Sled": 4, "Seat": 1, "Fan": 1}, subs=[("Wheel", "Sled")],
                     added=["Fan"], missing={"Wheel": 4}, sockets={"auto0": ("Fan", "body", "Back")})),
    dict(name="12 carro sin ruedas: pelotas (prefiere Ball antes que Sled)", prompt="un carro", archetype="Car",
         inv={"Plank": 1, "Ball": 4, "Sled": 4, "Rocket": 1}, usable_extra=["Sled", "Ball"], skip_layout=True,
         expect=dict(buildable=True, template="Car", types={"Plank": 1, "Ball": 4, "Seat": 1, "Rocket": 1}, subs=[("Wheel", "Ball")],
                     added=["Rocket"])),
    dict(name="13 carro sin ruedas con trineos pero sin empuje: no se mueve -> no se arma", prompt="un carro", archetype="Car",
         inv={"Plank": 1, "Sled": 4}, usable_extra=["Sled", "Ball"],
         expect=dict(buildable=False, missing={"Wheel": 4, "Fan": 1})),
    dict(name="14 carro sin ruedas pero con ventiladores: degrada a moto flotante", prompt="un carro", archetype="Car",
         inv={"Plank": 1, "Fan": 2},
         expect=dict(buildable=True, template="Hoverbike", degraded=True, types={"Plank": 1, "Fan": 2, "Seat": 1},
                     missing={"Wheel": 2}, sockets={"lift0": ("Fan", "body", "Bottom_Front"), "lift1": ("Fan", "body", "Bottom_Back")})),
    dict(name="15 carro sin nada util: pide materiales", prompt="un carro", archetype="Car", inv={"Plank": 1},
         expect=dict(buildable=False, parts=0, missing={"Wheel": 2})),
    dict(name="16 inventario vacio: pide chasis", prompt="un carro", archetype="Car", inv={},
         expect=dict(buildable=False, parts=0, missing={"Plank": 1})),
    # --- cohete ---
    dict(name="17 cohete", prompt="un cohete", archetype="Rocket", inv={"Cylinder": 1, "Rocket": 1},
         expect=dict(buildable=True, template="Rocket", types={"Cylinder": 1, "Rocket": 1}, cost=0.0, duration=-1.0,
                     sockets={"burst_thrust0": ("Rocket", "body", "Bottom")})),
    dict(name="18 cohete sin cohete: ventilador abajo", prompt="un misil", archetype="Rocket", inv={"Cylinder": 1, "Fan": 1},
         expect=dict(buildable=True, types={"Cylinder": 1, "Fan": 1}, subs=[("Rocket", "Fan")],
                     sockets={"burst_thrust0": ("Fan", "body", "Bottom")}, cost=1.5, duration=40.0)),
    dict(name="19 cohete sin cilindro: otro chasis", prompt="un cohete", archetype="Rocket", inv={"Plank": 1, "Rocket": 2},
         expect=dict(buildable=True, types={"Plank": 1, "Rocket": 1}, subs=[("Cylinder", "Plank")])),
    dict(name="20 cohete sin empuje: no se arma", prompt="un cohete", archetype="Rocket", inv={"Cylinder": 1},
         expect=dict(buildable=False, missing={"Rocket": 1})),
    # --- volador ---
    dict(name="21 volador", prompt="algo que vuele", archetype="Flyer", inv={"Plank": 1, "Fan": 2},
         expect=dict(buildable=True, template="Flyer", types={"Plank": 1, "Fan": 2, "Seat": 1}, cost=3.0, duration=20.0,
                     sockets={"lift0": ("Fan", "body", "Bottom_Front"), "lift1": ("Fan", "body", "Bottom_Back")})),
    dict(name="22 volador con ventilador extra de empuje", prompt="un avion con ventilador", archetype="Flyer", extras=["fan"],
         inv={"Plank": 1, "Fan": 3},
         expect=dict(buildable=True, types={"Plank": 1, "Fan": 3, "Seat": 1}, sockets={"thrust0": ("Fan", "body", "Back")}, cost=4.5,
                     low_energy=True)),
    dict(name="23 volador con 1 ventilador: degrada a moto flotante", prompt="un dron", archetype="Flyer", inv={"Plank": 1, "Fan": 1},
         expect=dict(buildable=True, template="Hoverbike", degraded=True, types={"Plank": 1, "Fan": 1, "Seat": 1},
                     reduced=[("lift", 2, 1)], missing={"Fan": 1}, sockets={"lift0": ("Fan", "body", "Bottom")})),
    dict(name="24 volador con globos: van arriba, no abajo", prompt="un volador", archetype="Flyer", inv={"Plank": 1, "Balloon": 2},
         expect=dict(buildable=True, template="Flyer", types={"Plank": 1, "Balloon": 2, "Seat": 1}, subs=[("Fan", "Balloon")],
                     sockets={"lift0": ("Balloon", "body", "Top_Front"), "lift1": ("Balloon", "body", "Top_Back")}, cost=0.0)),
    dict(name="25 volador sin sustento: degrada hasta carro", prompt="un helicoptero", archetype="Flyer", inv={"Plank": 1, "Wheel": 4},
         expect=dict(buildable=True, template="Car", degraded=True, types={"Plank": 1, "Wheel": 4, "Seat": 1}, missing={"Fan": 2})),
    dict(name="26 volador con canon (va abajo en Flyer)", prompt="un dron que dispara", archetype="Flyer", extras=["cannon"],
         inv={"Plank": 1, "Fan": 2, "Cannon": 1},
         expect=dict(buildable=True, types={"Plank": 1, "Fan": 2, "Seat": 1, "Cannon": 1}, sockets={"cannon0": ("Cannon", "body", "Bottom")})),
    # --- torreta ---
    dict(name="27 torreta", prompt="una torreta", archetype="Turret", inv={"Block": 1, "Cannon": 1},
         expect=dict(buildable=True, template="Turret", types={"Block": 1, "Cannon": 1}, anchored=True, cost=3.0, duration=20.0,
                     sockets={"weapon0": ("Cannon", "body", "Front")})),
    dict(name="28 torreta de fuego", prompt="una torreta lanzallamas", archetype="Turret", extras=["flame"],
         inv={"Block": 1, "Cannon": 1, "Flamethrower": 1},
         expect=dict(buildable=True, types={"Block": 1, "Flamethrower": 1}, subs=[], sockets={"weapon0": ("Flamethrower", "body", "Front")})),
    dict(name="29 torreta que apunta sola (Head existe)", prompt="una torreta que apunte sola", archetype="Turret",
         want=["Head"], inv={"Block": 1, "Cannon": 1, "Head": 1}, usable_extra=["Head"], skip_layout=True,
         expect=dict(buildable=True, types={"Block": 1, "Head": 1, "Cannon": 1}, unsupported=[],
                     sockets={"aim0": ("Head", "body", "Top"), "weapon0": ("Cannon", "aim0", "Front")}, cost=3.3)),
    dict(name="30 torreta que apunta sola (Head aun no esta en el codigo)", prompt="una torreta que apunte sola", archetype="Turret",
         want=["Head"], inv={"Block": 1, "Cannon": 1},
         expect=dict(buildable=True, types={"Block": 1, "Cannon": 1}, unsupported=["Head"], missing={},
                     sockets={"weapon0": ("Cannon", "body", "Front")})),
    dict(name="31 torreta que apunta sola sin cabeza en inventario: pide Head", prompt="una torreta que apunte sola", archetype="Turret",
         want=["Head"], inv={"Block": 1, "Cannon": 1}, usable_extra=["Head"],
         expect=dict(buildable=True, types={"Block": 1, "Cannon": 1}, missing={"Head": 1})),
    dict(name="32 torreta sin canon: usa laser", prompt="una torreta", archetype="Turret", inv={"Block": 1, "Beam": 1},
         expect=dict(buildable=True, types={"Block": 1, "Beam": 1}, subs=[("Cannon", "Beam")])),
    dict(name="33 torreta sin armas: no ataca -> no se arma", prompt="una torreta", archetype="Turret", inv={"Block": 2, "Plank": 3},
         expect=dict(buildable=False, missing={"Cannon": 1})),
    dict(name="34 torreta anclada con estaca", prompt="una torreta fija", archetype="Turret", inv={"Block": 1, "Cannon": 1, "Stake": 1},
         usable_extra=["Stake"], skip_layout=True,
         expect=dict(buildable=True, types={"Block": 1, "Cannon": 1, "Stake": 1}, sockets={"anchor0": ("Stake", "body", "Bottom")})),
    # --- tanque ---
    dict(name="35 tanque", prompt="un tanque", archetype="Tank", inv={"Plank": 1, "BigWheel": 4, "Cannon": 1},
         expect=dict(buildable=True, template="Tank", types={"Plank": 1, "BigWheel": 4, "Cannon": 1, "Seat": 1}, cost=3.8,
                     sockets={"weapon0": ("Cannon", "body", "Top_Front"), "seat": ("Seat", "body", "Top_Back")})),
    dict(name="36 tanque con muro al frente", prompt="un tanque blindado", archetype="Tank", extras=["wall"],
         inv={"Plank": 1, "BigWheel": 4, "Cannon": 1, "Wall": 1},
         expect=dict(buildable=True, types={"Plank": 1, "BigWheel": 4, "Cannon": 1, "Seat": 1, "Wall": 1},
                     sockets={"shield0": ("Wall", "body", "Front")})),
    dict(name="37 tanque blindado sin muro: usa panel", prompt="un tanque blindado", archetype="Tank", extras=["wall"],
         inv={"Plank": 1, "BigWheel": 4, "Cannon": 1, "Panel": 1},
         expect=dict(buildable=True, types={"Plank": 1, "BigWheel": 4, "Cannon": 1, "Seat": 1, "Panel": 1}, subs=[("Wall", "Panel")], missing={})),
    dict(name="38 tanque con ruedas chicas y lanzallamas", prompt="un tanque", archetype="Tank",
         inv={"Plank": 1, "Wheel": 4, "Flamethrower": 1},
         expect=dict(buildable=True, template="Tank", types={"Plank": 1, "Wheel": 4, "Flamethrower": 1, "Seat": 1},
                     subs=[("BigWheel", "Wheel"), ("Cannon", "Flamethrower")])),
    dict(name="39 tanque sin arma: degrada a carro", prompt="un tanque", archetype="Tank", inv={"Plank": 1, "BigWheel": 4},
         expect=dict(buildable=True, template="Car", degraded=True, types={"Plank": 1, "BigWheel": 4, "Seat": 1}, missing={"Cannon": 1})),
    dict(name="40 tanque sin ruedas: degrada a torreta", prompt="un tanque", archetype="Tank", inv={"Plank": 1, "Cannon": 1},
         expect=dict(buildable=True, template="Turret", degraded=True, types={"Plank": 1, "Cannon": 1}, missing={"BigWheel": 2})),
    dict(name="41 tanque con canon pedido: no duplica el canon", prompt="un tanque con cañon", archetype="Tank", extras=["cannon"],
         inv={"Plank": 1, "BigWheel": 4, "Cannon": 1},
         expect=dict(buildable=True, types={"Plank": 1, "BigWheel": 4, "Cannon": 1, "Seat": 1}, missing={})),
    # --- rueda acostada / rueda infernal ---
    dict(name="42 rueda acostada con dos lanzallamas que gira y quema", prompt="una rueda acostada con dos lanzallamas que gire y queme todo",
         archetype="Spinner", extras=["flame"], inv={"Wheel": 1, "Flamethrower": 2},
         expect=dict(buildable=True, template="Spinner", types={"Wheel": 1, "Flamethrower": 2}, orientation="flat", cost=4.2,
                     sockets={"body": ("Wheel", "", ""), "weapon0": ("Flamethrower", "body", "Front"), "weapon1": ("Flamethrower", "body", "Back")})),
    dict(name="43 rueda acostada con 1 lanzallamas: reduce", prompt="una rueda acostada con dos lanzallamas", archetype="Spinner",
         extras=["flame"], inv={"BigWheel": 1, "Flamethrower": 1},
         expect=dict(buildable=True, template="Spinner", types={"BigWheel": 1, "Flamethrower": 1}, reduced=[("weapon", 2, 1)],
                     subs=[("Wheel", "BigWheel")], missing={"Flamethrower": 1}, sockets={"weapon0": ("Flamethrower", "body", "Front")})),
    dict(name="44 rueda infernal (rueda que rueda sola con fuego)", prompt="una rueda infernal", archetype="Roller", extras=["flame"],
         inv={"Wheel": 1, "Flamethrower": 1},
         expect=dict(buildable=True, template="Roller", types={"Wheel": 1, "Flamethrower": 1}, orientation="upright", cost=2.2,
                     subs=[("BigWheel", "Wheel")], sockets={"weapon0": ("Flamethrower", "body", "Left")})),
    dict(name="45 rueda infernal de hielo: el arma pedida manda", prompt="una rueda que congele", archetype="Roller", extras=["frost"],
         inv={"BigWheel": 1, "Flamethrower": 1, "Frost": 1},
         expect=dict(buildable=True, types={"BigWheel": 1, "Frost": 1}, subs=[])),
    dict(name="46 rueda acostada sin armas: no ataca -> pide", prompt="una rueda que queme todo", archetype="Spinner", extras=["flame"],
         inv={"Wheel": 1},
         expect=dict(buildable=False, missing={"Flamethrower": 1})),
    dict(name="47 rueda acostada sin rueda: degrada a torreta", prompt="una rueda acostada con lanzallamas", archetype="Spinner",
         extras=["flame"], inv={"Block": 1, "Flamethrower": 2},
         expect=dict(buildable=True, template="Turret", degraded=True, types={"Block": 1, "Flamethrower": 1}, missing={"Wheel": 1})),
    # --- globo ---
    dict(name="48 globo con fuego debajo", prompt="un globo con fuego debajo", archetype="Airship", extras=["flame"],
         inv={"Plank": 1, "Balloon": 1, "Flamethrower": 1},
         expect=dict(buildable=True, template="Airship", types={"Plank": 1, "Flamethrower": 1, "Balloon": 1, "Seat": 1}, cost=2.0,
                     sockets={"heat0": ("Flamethrower", "body", "Top_Front"), "lift0": ("Balloon", "heat0", "Top_Front"),
                              "seat": ("Seat", "body", "Top_Back")})),
    dict(name="49 globo simple", prompt="un globo", archetype="Airship", inv={"Plank": 1, "Balloon": 1},
         expect=dict(buildable=True, types={"Plank": 1, "Balloon": 1, "Seat": 1}, sockets={"lift0": ("Balloon", "body", "Top_Front")}, cost=0.0)),
    dict(name="50 globo con fuego pero sin lanzallamas: pide", prompt="un globo con fuego debajo", archetype="Airship", extras=["flame"],
         inv={"Plank": 1, "Balloon": 1},
         expect=dict(buildable=True, types={"Plank": 1, "Balloon": 1, "Seat": 1}, missing={"Flamethrower": 1})),
    dict(name="51 globo sin globo: ventilador abajo", prompt="un dirigible", archetype="Airship", inv={"Plank": 1, "Fan": 1},
         expect=dict(buildable=True, template="Airship", types={"Plank": 1, "Fan": 1, "Seat": 1}, subs=[("Balloon", "Fan")],
                     sockets={"lift0": ("Fan", "body", "Bottom_Front")})),
    dict(name="52 carro con globo, sin globo: el ventilador sustituto va abajo", prompt="un carro con globo", archetype="Car",
         extras=["balloon"], inv={"Plank": 1, "Wheel": 4, "Fan": 1},
         expect=dict(buildable=True, subs=[("Balloon", "Fan")], sockets={"balloon0": ("Fan", "body", "Bottom")})),
    # --- hover / fortaleza ---
    dict(name="53 plataforma levitante", prompt="un hovercraft", archetype="Hover", inv={"Plank": 1, "HoverStone": 2},
         expect=dict(buildable=True, template="Hover", types={"Plank": 1, "HoverStone": 2, "Seat": 1}, cost=1.0)),
    dict(name="54 hover con 1 piedra: reduce", prompt="una plataforma flotante", archetype="Hover", inv={"Plank": 1, "HoverStone": 1},
         expect=dict(buildable=True, types={"Plank": 1, "HoverStone": 1, "Seat": 1}, reduced=[("hover_static", 2, 1)],
                     missing={"HoverStone": 1}, sockets={"hover_static0": ("HoverStone", "body", "Bottom")})),
    dict(name="55 fortaleza completa", prompt="una fortaleza", archetype="Fortress", inv={"Wall": 3, "Block": 1},
         expect=dict(buildable=True, template="Wall", types={"Wall": 3, "Block": 1}, anchored=True, cost=0.0, duration=-1.0,
                     sockets={"shield0": ("Wall", "body", "Left"), "weight0": ("Block", "body", "Top_Back")})),
    dict(name="56 fortaleza pobre: un solo muro, sin pedir de mas", prompt="una barricada", archetype="Fortress", inv={"Wall": 1},
         expect=dict(buildable=True, types={"Wall": 1}, reduced=[], missing={})),
    dict(name="57 fortaleza sin muros: panel", prompt="un bunker", archetype="Fortress", inv={"Panel": 1},
         expect=dict(buildable=True, types={"Panel": 1}, subs=[("Wall", "Panel")])),
    # --- limite de piezas, Seat gratis, energia, libres ---
    dict(name="58 mas de 21 piezas: recorta", prompt="un carro con 30 baterias", archetype="Car", want=["Battery"] * 30,
         inv={"Plank": 1, "Wheel": 4, "Battery": 30},
         expect=dict(buildable=True, parts=21, types={"Plank": 1, "Wheel": 4, "Seat": 1, "Battery": 15}, trimmed=15, missing={})),
    dict(name="59 carro con todos los extras e inventario rico: no pasa de 21", prompt="un carro con todo", archetype="Car",
         extras=["fan", "jet", "turbine", "rocket", "flame", "cannon", "balloon", "wing", "hover", "wall", "frost", "beam", "bomb",
                 "spring", "sail", "bigwheels"], inv=RICH,
         expect=dict(buildable=True, parts=21, trimmed=2, has={"BigWheel": 4, "Seat": 1, "Fan": 1, "Cannon": 1})),
    dict(name="60 Seat gratis e infinito: no esta en el inventario", prompt="un carro", archetype="Car", inv={"Plank": 1, "Wheel": 4},
         expect=dict(buildable=True, has={"Seat": 1}, missing={})),
    dict(name="61 Seat no se descuenta aunque se pidan varios", prompt="un carro con tres asientos", archetype="Car",
         want=["Seat", "Seat", "Seat"], inv={"Plank": 1, "Wheel": 4},
         expect=dict(buildable=True, types={"Plank": 1, "Wheel": 4, "Seat": 3}, missing={}, cost=0.8)),
    dict(name="62 poca energia: avisa", prompt="un tanque", archetype="Tank", inv={"Plank": 1, "BigWheel": 4, "Cannon": 1}, energy=20.0,
         expect=dict(buildable=True, cost=3.8, low_energy=True)),
    dict(name="63 bateria pegada suma reserva", prompt="un tanque con bateria", archetype="Tank", want=["Battery"],
         inv={"Plank": 1, "BigWheel": 4, "Cannon": 1, "Battery": 1}, energy=20.0,
         expect=dict(buildable=True, has={"Battery": 1}, cost=3.8, duration=50.0 / 3.8, low_energy=True)),
    dict(name="64 ejemplo del doc: 4 ruedas + ventilador = 2.3/s, 43 s con barra llena", prompt="un carro con ventilador", archetype="Car",
         extras=["fan"], inv={"Plank": 1, "Wheel": 4, "Fan": 1}, energy=100.0,
         expect=dict(buildable=True, cost=2.3, duration=100.0 / 2.3, low_energy=False)),
    dict(name="65 libre: lo que pidas sobre un chasis", prompt="una tabla con un resorte y un cañon", archetype="Free",
         want=["Spring", "Cannon"], inv={"Plank": 1, "Spring": 1, "Cannon": 1},
         expect=dict(buildable=True, template="Free", types={"Plank": 1, "Spring": 1, "Cannon": 1},
                     sockets={"want0": ("Spring", "body", "Top"), "want1": ("Cannon", "body", "Front")})),
    dict(name="66 pieza deseada que no existe en el catalogo: se reporta, no rompe", prompt="un carro con espejo", archetype="Car",
         want=["Mirror"], inv={"Plank": 1, "Wheel": 4, "Mirror": 1},
         expect=dict(buildable=True, types={"Plank": 1, "Wheel": 4, "Seat": 1}, unsupported=["Mirror"], missing={})),
    dict(name="67 planeador sin alas: degrada a volador", prompt="un planeador", archetype="Glider", inv={"Plank": 1, "Fan": 2},
         expect=dict(buildable=True, template="Flyer", degraded=True, missing={"Wing": 2})),
    dict(name="68 planeador", prompt="un planeador", archetype="Glider", extras=["wing"], inv={"Plank": 1, "Wing": 2},
         expect=dict(buildable=True, template="Glider", types={"Plank": 1, "Wing": 2, "Seat": 1})),
    dict(name="69 arquetipo desconocido: no se arma (el llamador usa el camino viejo)", prompt="???", archetype="Submarine", inv=RICH,
         expect=dict(buildable=False, parts=0)),
    dict(name="70 inventario ilimitado (sin inventario del jugador): tanque completo", prompt="un tanque", archetype="Tank", unlimited=True,
         expect=dict(buildable=True, types={"Plank": 1, "BigWheel": 4, "Cannon": 1, "Seat": 1}, subs=[], reduced=[], missing={})),
]


def check_case(case, res, catalog):
    exp = case["expect"]
    errs = []
    parts = res["recipe"]["parts"]
    types = {}
    for p in parts:
        types[p["type"]] = types.get(p["type"], 0) + 1
    by_id = {p["id"]: p for p in parts}

    def eq(label, got, want):
        if got != want:
            errs.append("%s: got %r, expected %r" % (label, got, want))

    def close(label, got, want):
        if abs(got - want) > 0.01:
            errs.append("%s: got %.3f, expected %.3f" % (label, got, want))

    for key in ("buildable", "template", "degraded", "orientation", "anchored", "low_energy", "unsupported", "added"):
        if key in exp:
            eq(key, res[key], exp[key])
    if "types" in exp:
        eq("types", types, exp["types"])
    for t, n in exp.get("has", {}).items():
        if types.get(t, 0) < n:
            errs.append("has %s: got %d, expected >= %d" % (t, types.get(t, 0), n))
    if "parts" in exp:
        eq("parts", len(parts), exp["parts"])
    for pid, (t, parent, face) in exp.get("sockets", {}).items():
        p = by_id.get(pid)
        if p is None:
            errs.append("socket: no part '%s' (ids: %s)" % (pid, sorted(by_id)))
        else:
            eq("part " + pid, (p["type"], p["attach_to"], p["socket"]), (t, parent, face))
    if "subs" in exp:
        eq("subs", [(s["wanted"], s["used"]) for s in res["substitutions"]], exp["subs"])
    if "reduced" in exp:
        eq("reduced", [(r["role"], r["wanted"], r["got"]) for r in res["reductions"]], exp["reduced"])
    if "missing" in exp:
        eq("missing", {m[0]: m[1] for m in res["missing"]}, exp["missing"])
    if "trimmed" in exp:
        eq("trimmed", len(res["trimmed"]), exp["trimmed"])
    if "cost" in exp:
        close("cost", res["energy_cost_per_s"], exp["cost"])
    if "duration" in exp:
        close("duration", res["duration_s"], exp["duration"])

    # Invariantes de toda receta devuelta.
    if res["buildable"]:
        if not case.get("skip_layout"):
            errs.extend("recipe: " + e for e in validate_recipe(res["recipe"], catalog))
        if len(parts) > MAX_PARTS:
            errs.append("more than MaxParts")
        if not case.get("unlimited"):
            for t, n in types.items():
                if t != "Seat" and n > case.get("inv", {}).get(t, 0):
                    errs.append("uses %d %s but inventory has %d" % (n, t, case.get("inv", {}).get(t, 0)))
    else:
        if parts:
            errs.append("not buildable but returned parts")
    if not res["message"] and res["requested_template"]:
        errs.append("empty message")
    return errs


def run_case(kn, extras_table, catalog, case):
    req = new_request(case["archetype"], case.get("extras"), case.get("inv"), case.get("want"),
                      case.get("orientation", ""), case.get("energy", -1.0), case.get("unlimited", False))
    usable = default_usable(kn) + case.get("usable_extra", [])
    res = solve(kn, extras_table, catalog, req, usable)
    again = solve(kn, extras_table, catalog, req, usable)
    errs = check_case(case, res, catalog)
    if json.dumps(res, sort_keys=True) != json.dumps(again, sort_keys=True):
        errs.append("not deterministic")
    return res, errs


def run_combo_sweep(kn, extras_table, catalog, archetypes):
    """Espejo de la prueba ComposeAllCombos: cada arquetipo del codigo x (sin extras, cada extra, todos) con inventario ilimitado."""
    errs = []
    n = 0
    keys = [e["key"] for e in extras_table]
    for a in archetypes:
        for combo in [[]] + [[k] for k in keys] + [keys]:
            n += 1
            res = solve(kn, extras_table, catalog, new_request(a, combo, unlimited=True))
            if not res["buildable"]:
                errs.append("%s %s: not buildable (%s)" % (a, combo, res["note"]))
                continue
            for e in validate_recipe(res["recipe"], catalog):
                errs.append("%s %s: %s" % (a, combo, e))
    # Y todas las plantillas del JSON, solas y con cada extra, tambien deben salir validas.
    for t in kn["templates"]:
        for combo in [[]] + [[k] for k in keys]:
            n += 1
            res = solve(kn, extras_table, catalog, new_request(t["id"], combo, unlimited=True))
            if not res["buildable"]:
                # Plantillas cuyo slot obligatorio aun no tiene pieza en el catalogo (Catapult no; Glider si tiene Wing).
                if res["missing"] or res["unsupported"]:
                    continue
                errs.append("template %s %s: not buildable (%s)" % (t["id"], combo, res["note"]))
                continue
            for e in validate_recipe(res["recipe"], catalog):
                errs.append("template %s %s: %s" % (t["id"], combo, e))
    return n, errs


# ------------------------------------------------------------------------------------------------
# Espejo para las pruebas de Unreal: los mismos casos, con el resultado exacto de este prototipo
# ------------------------------------------------------------------------------------------------

CASES_INL = os.path.join(REPO, "Source", "GameJamm", "Building", "Tests", "JamSolverCases.inl")


def _cpp_str(text):
    return 'TEXT("%s")' % text.replace("\\", "\\\\").replace('"', '\\"')


def emit_cpp_cases(kn, extras_table, catalog):
    """Tabla C++ (Tests/JamSolverCases.inl) que JamSolverTests.cpp compara campo por campo con UJamSolver::Solve.
    Se omiten los casos que simulan piezas que aun no estan en el catalogo (usable_extra)."""
    lines = ["// GENERADO por Content/Game/Building/Data/solver_proto.py --emit-cpp. No editar a mano.",
             "// Cada fila: entrada del solver + resultado exacto del prototipo Python (espejo)."]
    n = 0
    for case in CASES:
        if case.get("usable_extra"):
            continue
        res, _ = run_case(kn, extras_table, catalog, case)
        rec = res["recipe"]
        inv = case.get("inv") or {}
        fields = [
            _cpp_str(case["name"]),
            _cpp_str(case["archetype"]),
            _cpp_str(",".join(case.get("extras") or [])),
            _cpp_str(",".join(case.get("want") or [])),
            _cpp_str(",".join("%s:%d" % (t, inv[t]) for t in inv)),
            "true" if case.get("unlimited") else "false",
            "%.4ff" % case.get("energy", -1.0),
            "true" if res["buildable"] else "false",
            _cpp_str(res["template"]),
            _cpp_str("|".join("%s:%s:%s:%s" % (p["id"], p["type"], p["attach_to"], p["socket"]) for p in rec["parts"])),
            _cpp_str(",".join(rec["activate"])),
            _cpp_str("|".join("%s:%s>%s:%d" % (x["role"], x["wanted"], x["used"], x["count"]) for x in res["substitutions"])),
            _cpp_str("|".join("%s:%d:%d" % (x["role"], x["wanted"], x["got"]) for x in res["reductions"])),
            _cpp_str(",".join("%s:%d" % (m[0], m[1]) for m in res["missing"])),
            _cpp_str(",".join(res["unsupported"])),
            "%d" % len(res["trimmed"]),
            _cpp_str(res["orientation"]),
            "%.4ff" % res["energy_cost_per_s"],
            "%.4ff" % res["duration_s"],
        ]
        lines.append("{ " + ", ".join(fields) + " },")
        n += 1
    return "\n".join(lines) + "\n", n


def main(argv):
    verbose = "-v" in argv
    kn, err = load_knowledge()
    if kn is None:
        print("FAIL: " + err)
        return 1
    cpp = _read(ASSEMBLER_CPP)
    extras_table = parse_extra_table(cpp)
    archetypes = parse_archetype_table(cpp)
    catalog = parse_catalog(_read(PART_CPP))
    if len(extras_table) < 10 or len(archetypes) < 5 or len(catalog) < 10:
        print("FAIL: could not parse C++ tables (extras=%d archetypes=%d catalog=%d)" % (len(extras_table), len(archetypes), len(catalog)))
        return 1

    failed = 0
    for case in CASES:
        res, errs = run_case(kn, extras_table, catalog, case)
        status = "ok  " if not errs else "FAIL"
        print("%s %s" % (status, case["name"]))
        if verbose or errs:
            print("       \"%s\" -> %s" % (case["prompt"], res["message"]))
            if verbose:
                print("       " + ", ".join("%s:%s@%s.%s" % (p["id"], p["type"], p["attach_to"] or "-", p["socket"] or "-") for p in res["recipe"]["parts"]))
        for e in errs:
            print("       - " + e)
        failed += 1 if errs else 0

    n, errs = run_combo_sweep(kn, extras_table, catalog, archetypes)
    print("%s barrido de combinaciones (%d recetas con inventario ilimitado)" % ("ok  " if not errs else "FAIL", n))
    for e in errs[:40]:
        print("       - " + e)
    failed += 1 if errs else 0

    text, n_cpp = emit_cpp_cases(kn, extras_table, catalog)
    if "--emit-cpp" in argv:
        with open(CASES_INL, "w", encoding="utf-8", newline="\n") as f:
            f.write(text)
        print("ok   escrito %s (%d casos)" % (os.path.relpath(CASES_INL, REPO), n_cpp))
    else:
        current = _read(CASES_INL).replace("\r\n", "\n") if os.path.exists(CASES_INL) else ""
        fresh = current == text
        print("%s espejo C++ al dia: Tests/JamSolverCases.inl (%d casos)%s" % (
            "ok  " if fresh else "FAIL", n_cpp, "" if fresh else " -> corre con --emit-cpp"))
        failed += 0 if fresh else 1

    bad, _ = load_knowledge(os.path.join(HERE, "no-existe.json"))
    print("%s JSON ausente -> load falla (el llamador usa el camino actual)" % ("ok  " if bad is None else "FAIL"))
    failed += 0 if bad is None else 1

    print("\n%d casos, %d fallas" % (len(CASES) + 3, failed))
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
