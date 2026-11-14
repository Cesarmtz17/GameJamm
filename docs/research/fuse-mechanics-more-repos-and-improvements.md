# Fuse / Ultrahand mechanics: more repos and design improvements

Companion to `open-source-ultrahand-style-construction.md` (ComponentFuseMechanic and the 9 repos already listed there are not repeated). Only public sources and open-source code. Nothing from a commercial game was extracted or decompiled.

Evidence tags used below:
- **[code]** = I read the code in a local clone (scratchpad `more-repos/`, shallow clones, 2026-11-13).
- **[page]** = seen on a repo page / search snippet / `gh api` metadata only.
- **[inferred]** = my reasoning, not verified. **[memory]** = Unreal API knowledge from memory, not checked against 5.8 headers.

## 1. New repos found

Finding in one line: nobody has published a good Ultrahand clone. The useful pieces are small and scattered, and the best structural idea comes from a Besiege-like (creasiege), not from an Ultrahand demo.

| Repo | URL | License (how checked) | Engine | Mechanic | Use for us |
|---|---|---|---|---|---|
| ultrahandDemo (ranjansikand) | https://github.com/ranjansikand/ultrahandDemo | **None** (no LICENSE file, `gh api` license null) = all rights reserved | Unity C# | Grab, rotate, "attach" nearest body with runtime `FixedJoint` [code] | Concept only. Do not copy code |
| creasiege (BenPyton) | https://github.com/BenPyton/creasiege | MIT [LICENSE file present, `gh api` = MIT] | Unity C# | Besiege-like part tree built from `PartJoint` sockets, JSON save/load, destruction splits the tree [code] | Best reference for the data model. Reusable with MIT notice |
| Clingy (all-iver) | https://github.com/all-iver/clingy | MIT [LICENSE file present, `gh api` = MIT] | Unity C# | Runtime attachments library: strategy objects (physics joint, parenting, following) configured in the editor, joints created at runtime [code: README + strategy files] | Reference for "attachment = data asset that builds a joint" |
| OwnWar (Demindiro) | https://github.com/Demindiro/OwnWar | **AGPL-3.0** [LICENSE, `gh api`] | Godot 3 (GDScript + GDNative) | Block-based vehicles with chassis, wheels, wings, turrets, weapons folders [code: directory listing only] | Do not read for code (copyleft, viral over a network). Idea level only |
| godot-jolt-physics-joint (PRS-Organization) | https://github.com/PRS-Organization/godot-jolt-physics-joint | Apache-2.0 [LICENSE present, `gh api`] | Godot 4.6, Jolt | 5-axis arm built from `Generic6DOFJoint3D`, soft/hard limits, spring + motor servo [page: README; I did not read `robot_arm.gd`] | Evidence that a 6-DOF joint with limits and a motor covers hinge, slider and fixed |
| besiege-like (Alsctiho) | https://github.com/Alsctiho/besiege-like | **None** on the repo; bundles a proprietary "Godot MCP Pro" addon [code: addons/LICENSE] | Godot 4.x, Jolt | Core-block player assembles parts on six faces, assembly collision check built from per-part convex shapes plus swept rotor discs, preview-then-confirm [code: README, `assembly_collision.gd`]. No joint code found by grep | Concept only. The "preview, check, commit" flow is the takeaway |
| MGS.Machinery (mogoson) | https://github.com/mogoson/MGS.Machinery | Apache-2.0 [`gh api`] | Unity C# | Editor-time binding of machinery joints in a scene [page] | Not runtime assembly. Low value |
| godot-proposals #1423 (already cited) | https://github.com/godotengine/godot-proposals/issues/1423 | MIT (repo) | Godot | Request for a native weld joint | Confirms engines lack a "weld" primitive; we use a Fixed constraint or merge bodies |

Not usable / not found: Team Expotential assembly toolkit (UE forum, closed, license unknown, already cited). No Scrap Mechanic or Besiege clone with a license and working joints turned up in GitHub search ("ultrahand", "besiege-like", "scrap mechanic clone", "vehicle builder joints" returned nothing relevant beyond the rows above). Several one-star school "BesiegeLike" repos exist with no license; ignored.

## 2. What each approach actually does (verified from code)

**ultrahandDemo, `Blocks.cs` (197 lines) [code]**
- Every block holds one outgoing `FixedJoint` plus a list of incoming joints from neighbors.
- `Attach()` takes `OverlapSphere` radius 2, picks the nearest other rigidbody, adds a `FixedJoint` to itself, and tells the neighbor to remember it.
- Bug visible in code: after choosing `obj` it registers the joint on `results[1]`, not on `obj`. That is the wrong neighbor whenever the nearest body is not slot 1. It also never checks `obj != null`, so a miss adds a joint to nothing.
- "Grab a whole assembly" is done by recursing through the joint lists and calling `Grab(false)` on each neighbor. Held motion sets `rigidbody.velocity` toward a point in front of the camera. Gravity off, rotation frozen while held.
- Detach destroys joints and updates both lists. There is no socket, no alignment, no clip check, and the graph lives in two lists per block (easy to desync).

**creasiege, `Part.cs` / `PartJoint.cs` / `VehicleEditorController.cs` [code]**
- Sockets are child objects (`PartJoint`) with a sphere trigger collider on a dedicated "Joints" layer. A part has a list of them; joint 0 is the default "root" joint that attaches to the parent.
- `PartJoint.AttachTo` links two sockets both ways, disables both colliders so they cannot be picked twice, and records the target pose (`other.rotation * Euler(0,180,0)`, i.e. sockets face each other).
- `Part.AttachTo` then solves the part transform from the socket: `rot = jointRot * Euler(0,0,angle) * inverse(localRot)`, `pos = jointPos - rot * localPos`. It also supports cycling which socket is the root and an `angle` spin about the socket axis.
- The structure is a tree: each part has exactly one parent socket. Parts are plain child objects of one rigidbody; attaching adds the part's mass to the root rigidbody (`rb.mass += mass`). There are no physics joints at all [code].
- Save format is `JSONVehicle { version, name, parts[] }`; each `JSONPart` carries the prefab name, `partTargetId` and `constraintJoint` (socket index on the parent) [code, struct seen; JSONPart fields beyond these not read]. Load = instantiate prefab, `AttachTo(GetPartByID(target).GetJointFromID(socket))`. This is a parts-plus-sockets recipe, close to what we want for LLM output.
- Damage: `Part.Destroy()` detaches the part, then moves each child subtree into a new `Debris` rigidbody and recomputes center of mass. That is a structural-split mechanic with no physics joints involved.

**Clingy [code, README + strategy signatures]**
- An `AttachStrategy` is a ScriptableObject. The Physics strategy takes anchor / connected anchor / axis / swing axis from "param selectors" that read either object's attach points. The joint component is created at runtime from that description. Chain / One-to-One / Many-to-One variants exist. Takeaway: joint parameters live in a data asset, not in the code that decides to connect.

**besiege-like (Godot) [code]**
- `assembly_collision.gd` builds one convex shape per mesh (cached) and swaps rotating parts for a swept cylinder, so a preview can be rejected before committing. Matches the "overlap clip check" idea in ComponentFuseMechanic but uses real part shapes and handles moving parts. The README says preview does not change the real loadout until confirm.

## 3. Comparison

| Aspect | ComponentFuseMechanic | ultrahandDemo | creasiege | Clingy | besiege-like |
|---|---|---|---|---|---|
| Connectivity authority | Chaos constraints [report] | Two joint lists per block | Part tree (sockets) | Attachment objects | Assembly data |
| Joint type | Constraint actor (config not read here) | Always Fixed | None (one rigid body) | Any Unity joint via data | None found |
| Socket metadata | Name substring | None | Pose + parent id + index | Attach points with params | Face index |
| Clip check | Overlap query | None | Trigger colliders only | None | Shape + swept rotor |
| Persistence | None | None | JSON (id + socket index) | Events only | Save/restore |
| Break / split | Sweep to find constraints | Destroy joints | Subtree to debris body | `Detach()` | n/a |

Takeaways [inferred from the table]:
- Everyone who ships a stable result uses a **graph or tree as the authority**; the engine joint is a derived output (or absent). That matches the existing report's recommendation.
- creasiege's "one rigid body + added mass" is the cheapest robust build, but loses articulation. We want both: rigid clusters for fixed joints, real constraints only for hinge/slider/motor.
- Nobody here has a structural-stress model. That is new work for us (see 4.4).

## 4. Proposed improvements to our design

Context: our repo already has `Source/GameJamm/Building` and `Core` folders. I did not read them. Contracts in CLAUDE.md (`UHealthComponent`, `EJamTeam`, `UInventoryComponent`, recipe JSON) must not change without asking, so everything below is additive.

### 4.1 Socket graph as the source of truth
Keep ComponentFuseMechanic's socket search for finding candidates, but store connectivity in our own data:
```
FJamPartNode   { FGuid Id; TWeakObjectPtr<UPrimitiveComponent> Comp; FName PartType; float Mass; float MaxLoad; }
FJamSocketDef  { FName Name; FName Tag /*"Bolt","Plate","AxleM","AxleF"*/; FVector Axis; EJamJointKind Allowed; }
FJamEdge       { FGuid A, B; FName SocketA, SocketB; EJamJointKind Kind; FJamJointParams P; }
```
- `UJamAssemblyGraph` (plain UObject owned by the root actor) holds `TMap<FGuid,FJamPartNode>` and `TArray<FJamEdge>`.
- Replace the name-substring test with a tag compatibility table (data asset: `Tag -> compatible Tags + allowed joint kinds`). Authoring stays cheap: one socket name convention `Jam_<Tag>_<Index>` parsed once at load.
- Detach = remove edge, destroy that edge's constraint, then run connected-components; each component becomes its own rigid cluster (see 4.2). No sweeps. This fixes the "sweep to find constraints" weakness already in the first report.
- creasiege's socket pose solve (`pos = jointPos - rot*localPos`, spin about the socket axis via `angle`) is a good snap routine: implement as `FTransform SolveSnap(FJamSocket A_world, FJamSocketDef B_local, float SpinDeg)`. Re-derive it, it is 3 lines.

### 4.2 Typed joints (fixed / hinge / slider / motor)
Map one enum to one Chaos constraint configuration [memory: names from `FConstraintInstance`, not checked against 5.8]:

| Kind | Constraint setup | Notes |
|---|---|---|
| Fixed | All linear and angular locked | **Do not create a constraint at all when avoidable.** Merge the cluster instead: attach the child component to the cluster root (`AttachToComponent` + `KeepWorld`) so it is one rigid body. Cheaper, no jitter, and mirrors creasiege's single-body result. Use a locked constraint only for parts that must stay separate bodies (e.g. breakable under load) |
| Hinge | Linear locked, Twist free (or limited), Swing locked, optional angular limit | Axis comes from the socket `Axis` |
| Slider | Linear X free within limit, others locked, angular locked | |
| Motor | Hinge + angular velocity drive (target RPM, max torque) | Motor is a hinge with a drive. Expose torque as `EditDefaultsOnly` |

- Always set "disable collision between constrained bodies" for hinge/slider neighbors, otherwise adjacent parts fight [memory].
- Create the constraint **once**, after the snap finishes. Do not copy the break-and-recreate-per-tick loop (the report's main objection).
- Joint creation helper in one place: `FJamJointFactory::Build(const FJamEdge&) -> UPhysicsConstraintComponent*`. All code paths (player fuse, LLM spec, load game) call it, so behavior cannot diverge.
- Clingy's lesson: put per-kind defaults in a DataAsset (`UJamJointProfile`) so design tunes stiffness, limits and break force in BP without recompiling.

### 4.3 Clip check
- Keep the overlap query, but ignore the pair being joined (the report flags self-rejection as probable). Use the parts' real collision shapes, not a capsule, as besiege-like does with cached convex shapes. [inferred: UE has `ComponentOverlapMulti` already used by the repo.]
- Preview-then-commit: draw a ghost at the snapped pose, run the check on the ghost, commit only on confirm. Cancel leaves the world untouched (same flow as besiege-like).

### 4.4 Structural limits (jam-sized)
No repo above has this; everything here is my design [inferred]. Keep it a number check, not a solver.
- Per-part `Mass`, per-edge `MaxLoad` (in Newtons, from the profile).
- On any graph change, run a BFS from the cluster's anchor part (heaviest or the part touching ground/vehicle root), accumulating downstream mass. If `downstreamMass * g > MaxLoad` for an edge, it is overloaded.
- Cheap hard limits that cover most abuse: max parts per cluster (e.g. 40), max graph depth (e.g. 8), max chain length of hinges, max total mass for a motor. Enforce at attach time and show why the placement is rejected.
- Runtime breaking: use the constraint's own break threshold (`SetLinearBreakable` / `SetAngularBreakable` style APIs) [memory] and listen for the broken event, then call the graph's remove-edge. The earlier report cites a UE 5.0.2 forum claim that the broken event sometimes did not fire; do not rely on it alone. As a fallback, poll constraint force on a slow timer (0.2 s) for overloaded edges.
- Debris on split: like creasiege `Part.Destroy()`, each connected component becomes its own body; recompute center of mass and mass for the new clusters.

### 4.5 Networking-lite
Matches the CLAUDE.md rule: minimal, authority on the server, no deep investment. [inferred]
- Server owns `UJamAssemblyGraph`. Replicate only the edge list (`FFastArraySerializer` of `FJamEdge`: 2 Guids, 2 names, kind, 1 float). Clients rebuild constraints locally from edges and treat physics as local, snapped by a periodic root-transform correction.
- Requests are RPCs: `Server_RequestFuse(PartA, SocketA, PartB, SocketB, Kind)` and `Server_RequestDetach(EdgeId)`. The server re-runs validation (4.1, 4.3, 4.4) and replies by changing the replicated array. Clients never trust their own snap.
- Do not attempt client-side prediction or physics replication for the jam. Single-player vs bots comes first.

### 4.6 LLM-spec-driven assembly (validated JSON)
Reuse creasiege's idea (parts + `targetId` + socket index) but with typed joints and a strict validator. Proposed recipe, versioned, **new file, not the existing recipe JSON contract**:
```json
{ "version": 1, "name": "cart",
  "parts":  [ {"id":"p0","type":"Plank"}, {"id":"p1","type":"Wheel"}, {"id":"p2","type":"Wheel"} ],
  "joints": [ {"kind":"fixed","a":"p0","sa":"Jam_Axle_0","b":"p1","sb":"Jam_Axle_0"},
              {"kind":"hinge","a":"p0","sa":"Jam_Axle_1","b":"p2","sb":"Jam_Axle_0","limit":null} ] }
```
Pipeline: `LLM text -> JSON -> schema validate -> graph validate -> spawn -> build`. Validate in C++ before spawning anything:
1. Schema: version, ids unique, `type` in the part catalog, joint `kind` in the enum, numeric ranges clamped, max sizes (parts, joints).
2. Referential: every `a`/`b` exists, `sa`/`sb` are real sockets on that part type, tags compatible, `kind` allowed by both sockets, each socket used at most once.
3. Graph: connected (or declared multiple clusters), no cycles unless every edge is hinge/slider/motor (cycles of fixed joints over-constrain), within the limits in 4.4.
4. Geometry: solve the snap transforms over a spanning tree, run the clip check on the resulting layout in a throwaway (no physics) pass. Reject on overlap.
5. Only then spawn actors and call `FJamJointFactory::Build`.
- On rejection return a short machine-readable error list (`{"path":"joints[1].sb","reason":"socket_in_use"}`) so the LLM can retry. Cap retries at 2.
- Treat the LLM output as untrusted input: no part types or paths it invents are ever loaded; the catalog is an allowlist. Never let spec text choose asset paths or call functions.
- Save/load uses the same JSON, which gives free test fixtures.

### 4.7 Ordered work list (for a jam deadline of 15 Nov 2026)
Today is 13 Nov and the feature freeze is Sat 15 Nov 12:00 CDMX, so do only the first three and treat the rest as post-jam:
1. Fix the 4 small defects already listed in the first report, plus `Fixed = merge the child to the cluster` instead of a constraint. [Smallest change, biggest stability gain.]
2. Create-once constraint after snap; delete the per-tick loop.
3. Graph + edge removal for detach (`UJamAssemblyGraph`), no replication.
4. Hinge and motor profile (one DataAsset).
5. JSON spec validator (steps 1-3 above only; skip geometry pass).
6. Structural limits, networking, geometry validation: after the jam.

## 5. Gaps and uncertainty
- I did not read ComponentFuseMechanic again; statements about it come from the earlier report and the task brief.
- UE joint API names (`SetLinearBreakable`, drive APIs, disable-collision flag) are from memory, not checked against UE 5.8 headers. Verify in the editor before relying on them.
- Clingy and OwnWar were not read beyond README, filenames and strategy signatures. godot-jolt-physics-joint: README only.
- `ultrahandDemo` and `besiege-like` have no license, so under default copyright they cannot be reused; the earlier report's rule (rewrite from the description, never copy expression) applies.
- Structural-limit, networking and spec-schema sections are design proposals, not tested. No code was compiled or run.
- The `results[1]` bug in ultrahandDemo is a reading of the code, not a reproduced failure.
