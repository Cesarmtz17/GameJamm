#!/usr/bin/env python3
"""Valida BuildKnowledge.json contra si mismo y contra las tablas del C++.

Uso:  python3 -I Content/Game/Building/Data/validate_knowledge.py
Sale con 1 si hay errores. Los avisos (WARN) no rompen: son diferencias de tuning entre el JSON y el catalogo.

Revisa: ids/tipos unicos, provides/faces/substitutes coherentes, costos de energia definidos, in_code == existe en
UJamPartCatalog (JamBuildPart.cpp), plantillas con code_archetype existente en ArchetypeTable (JamAssembler.cpp),
slots bien formados (roles que alguna pieza da, caras validas, pares, part_pref, on, fallbacks, requires),
piezas de ExtraTable presentes en el JSON y que cada arquetipo del codigo salga armable con el solver.
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))  # -I quita la carpeta del script del path
import solver_proto as sp  # noqa: E402

FACES = sp.ALL_FACES
REQUIRES = ["move", "attack"]
ORIENTATIONS = ["upright", "flat", "inverted", "any"]


def validate(kn, catalog, archetypes, extras_table):
    errors, warnings = [], []
    err = errors.append
    warn = warnings.append
    elements = kn["elements"]
    types = [e.get("type") for e in elements]

    # ---- elementos ----
    for field, values in (("id", [e.get("id") for e in elements]), ("type", types)):
        for v in sorted(set(values), key=str):
            if values.count(v) > 1:
                err("element %s duplicated: %s" % (field, v))
    all_caps = set()
    for e in elements:
        t = e.get("type", "?")
        for field in ("id", "type", "provides", "faces", "substitutes", "mass_kg", "in_code"):
            if field not in e:
                err("%s: missing field '%s'" % (t, field))
        if not isinstance(e.get("provides"), list) or not e.get("provides"):
            err("%s: provides must be a non-empty list" % t)
        all_caps.update(e.get("provides", []))
        faces = e.get("faces", [])
        if not faces:
            err("%s: faces is empty" % t)
        for f in faces:
            if f != "*" and f not in FACES:
                err("%s: unknown face '%s'" % (t, f))
        if "*" in faces and len(faces) > 1:
            err("%s: faces mixes '*' with named faces" % t)
        for s in e.get("substitutes", []):
            if s == t:
                err("%s: substitutes itself" % t)
            elif s not in types:
                err("%s: substitute '%s' is not an element" % (t, s))
        if e.get("substitutes", []) != list(dict.fromkeys(e.get("substitutes", []))):
            err("%s: duplicated substitutes" % t)
        cost = e.get("energy_cost_per_s")
        if not isinstance(cost, (int, float)) or isinstance(cost, bool) or cost < 0:
            err("%s: energy_cost_per_s must be a number >= 0 (got %r)" % (t, cost))
        if not isinstance(e.get("mass_kg"), (int, float)) or e.get("mass_kg", 0) <= 0:
            err("%s: mass_kg must be > 0" % t)
        if e.get("infinite") and cost:
            err("%s: infinite pieces must cost 0 energy" % t)
        if "lift_face" in e and e["lift_face"] not in ("Top", "Bottom"):
            err("%s: lift_face must be Top or Bottom" % t)
        in_catalog = t in catalog
        if bool(e.get("in_code")) != in_catalog:
            err("%s: in_code=%s but %s UJamPartCatalog (JamBuildPart.cpp)" % (t, e.get("in_code"), "exists in" if in_catalog else "is missing from"))
        if in_catalog and abs(catalog[t]["mass"] - float(e.get("mass_kg", 0))) > 0.01:
            warn("%s: mass_kg %s != catalog %s" % (t, e.get("mass_kg"), catalog[t]["mass"]))
    for t in catalog:
        if t not in types:
            err("catalog type '%s' has no element in the JSON" % t)
    seat = sp.elem(kn, "Seat")
    if seat is None or not seat.get("infinite") or "control" not in seat.get("provides", []):
        err("Seat must exist, be infinite and provide 'control'")

    # ---- energia ----
    energy = kn.get("energy", {})
    for field in ("max", "start", "warn_duration_s"):
        if not isinstance(energy.get(field), (int, float)):
            err("energy.%s must be a number" % field)
    if isinstance(kn.get("max_parts"), int) and kn["max_parts"] != sp.MAX_PARTS:
        warn("max_parts %s != UJamAssembler::MaxParts %d (el solver usa el menor)" % (kn["max_parts"], sp.MAX_PARTS))

    # ---- plantillas ----
    templates = kn["templates"]
    tids = [t.get("id") for t in templates]
    for v in set(tids):
        if tids.count(v) > 1:
            err("template id duplicated: %s" % v)
    usable = sp.default_usable(kn)
    for t in templates:
        tid = t.get("id", "?")
        ca = t.get("code_archetype")
        if ca is not None and ca not in archetypes:
            err("template %s: code_archetype '%s' is not in ArchetypeTable (JamAssembler.cpp)" % (tid, ca))
        if t.get("orientation") not in ORIENTATIONS:
            err("template %s: orientation '%s' unknown" % (tid, t.get("orientation")))
        for r in t.get("requires", []):
            if r not in REQUIRES:
                err("template %s: requires '%s' unknown" % (tid, r))
        if "requires" not in t or "fallbacks" not in t:
            err("template %s: needs 'requires' and 'fallbacks' (may be empty)" % tid)
        for f in t.get("fallbacks", []):
            if f == tid or f not in tids:
                err("template %s: fallback '%s' invalid" % (tid, f))
        slots = t.get("slots", [])
        if len([s for s in slots if s.get("root")]) > 1:
            err("template %s: more than one root slot" % tid)
        seen_roles = []
        for i, s in enumerate(slots):
            where = "template %s slot %d (%s)" % (tid, i, s.get("role"))
            role = s.get("role")
            if not role:
                err(where + ": no role")
                continue
            if role != "weapon" and role not in all_caps:
                err(where + ": no element provides this role")
            count = s.get("count", 1)
            if not isinstance(count, int) or count < 1:
                err(where + ": count must be >= 1")
                continue
            minimum = s.get("min", count)
            if minimum > count or minimum < 0:
                err(where + ": min out of range")
            if s.get("optional") and s.get("min", 0) > 0:
                err(where + ": optional with min > 0")
            if s.get("pairs") and (count % 2 or minimum % 2):
                err(where + ": pairs needs even count and min")
            if "faces" in s and len(s["faces"]) < count:  # mas caras que count = opciones; se usan las primeras
                err(where + ": faces has %d entries for count %d" % (len(s["faces"]), count))
            if "faces" in s and "face" in s:
                err(where + ": both face and faces")
            for f in s.get("faces", [s["face"]] if "face" in s else []):
                d, _ = sp.parse_socket(f)
                if d is None:
                    err(where + ": face '%s' is not a socket JamLayout understands" % f)
            for p in s.get("part_pref", []):
                e = sp.elem(kn, p)
                if e is None:
                    err(where + ": part_pref '%s' is not an element" % p)
                elif not sp.provides_role(e, role):
                    err(where + ": part_pref '%s' does not provide '%s'" % (p, role))
            if "on" in s and s["on"] not in seen_roles:
                err(where + ": 'on' must name the role of an earlier slot")
            seen_roles.append(role)
            # La primera opcion en codigo debe poder ir en la cara del slot.
            cands = sp.slot_candidates(kn, usable, s, [])
            is_root = s.get("root") or (role == "chassis" and not any(x.get("root") for x in slots))
            if cands and not is_root:
                for f in s.get("faces", [s.get("face", "Top")]):
                    for c in cands:
                        e = sp.elem(kn, c)
                        face = sp.primary(sp.adjust_face(kn, c, role, f))
                        if "*" not in e["faces"] and face not in e["faces"] and sp.provides_role(e, role) and c == cands[0]:
                            err(where + ": first choice '%s' cannot go on face '%s' (faces=%s)" % (c, face, e["faces"]))
                        if role in sp.LIFT_ROLES and "lift_face" not in e:
                            err(where + ": candidate '%s' needs lift_face" % c)
    for a in archetypes:
        if not any(t.get("code_archetype") == a for t in templates):
            err("ArchetypeTable '%s' has no template with that code_archetype" % a)

    # ---- ExtraTable ----
    for ex in extras_table:
        if ex["part"]:
            e = sp.elem(kn, ex["part"])
            if e is None:
                err("ExtraTable '%s': part '%s' is not an element" % (ex["key"], ex["part"]))
            elif not e.get("in_code"):
                err("ExtraTable '%s': part '%s' is not in_code" % (ex["key"], ex["part"]))
        for a in ex["allowed"]:
            if a not in archetypes:
                err("ExtraTable '%s': allowed archetype '%s' unknown" % (ex["key"], a))

    # ---- el solver arma cada arquetipo del codigo ----
    if not errors:
        for a in archetypes:
            res = sp.solve(kn, extras_table, catalog, sp.new_request(a, unlimited=True))
            if not res["buildable"]:
                err("archetype %s: solver cannot build it with unlimited inventory (%s)" % (a, res["note"]))
            else:
                for e in sp.validate_recipe(res["recipe"], catalog):
                    err("archetype %s: %s" % (a, e))
        for t in templates:
            res = sp.solve(kn, extras_table, catalog, sp.new_request(t["id"], unlimited=True))
            if not res["buildable"]:
                warn("template %s: not buildable yet with in_code parts (falta en catalogo: %s)" % (t["id"], ", ".join(res["unsupported"]) or "?"))
    return errors, warnings


def main():
    kn, e = sp.load_knowledge(sys.argv[1]) if len(sys.argv) > 1 else sp.load_knowledge()
    if kn is None:
        print("ERROR " + e)
        return 1
    cpp = sp._read(sp.ASSEMBLER_CPP)
    catalog = sp.parse_catalog(sp._read(sp.PART_CPP))
    archetypes = sp.parse_archetype_table(cpp)
    extras_table = sp.parse_extra_table(cpp)
    errors, warnings = validate(kn, catalog, archetypes, extras_table)
    for w in warnings:
        print("WARN  " + w)
    for x in errors:
        print("ERROR " + x)
    print("BuildKnowledge.json v%s: %d elementos (%d en codigo), %d plantillas, %d arquetipos, %d extras -> %d errores, %d avisos" % (
        kn.get("version"), len(kn["elements"]), len(sp.default_usable(kn)), len(kn["templates"]), len(archetypes), len(extras_table),
        len(errors), len(warnings)))
    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main())
