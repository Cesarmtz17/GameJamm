#!/usr/bin/env python3
"""Prototipo ejecutable de la lógica pura de dispositivos (espejo de Source/GameJamm/Building/JamDeviceMath.cpp).

No hay compilador de UE en todas las máquinas del equipo: este archivo repite las mismas fórmulas y los mismos
casos que Source/GameJamm/Building/Tests/JamDeviceTests.cpp para poder verificarlos con solo Python.
También comprueba que la tabla de costos de energía de JamBuildPart.cpp diga lo mismo que BuildKnowledge.json.

Correr:  python3 -I Content/Game/Building/Data/devices_proto.py
"""
import json
import math
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", "..", "..", ".."))


# ---------- vectores mínimos ----------
def sub(a, b): return (a[0] - b[0], a[1] - b[1], a[2] - b[2])
def add(a, b): return (a[0] + b[0], a[1] + b[1], a[2] + b[2])
def mul(a, k): return (a[0] * k, a[1] * k, a[2] * k)
def dot(a, b): return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]
def cross(a, b): return (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])
def size(a): return math.sqrt(dot(a, a))
def norm(a):
    s = size(a)
    return mul(a, 1.0 / s) if s > 1e-8 else (0.0, 0.0, 0.0)
def clamp(x, lo, hi): return max(lo, min(hi, x))

UP = (0.0, 0.0, 1.0)
FWD = (1.0, 0.0, 0.0)
RIGHT = (0.0, 1.0, 0.0)


# ---------- espejo de JamDeviceMath.cpp ----------
def spend_energy(reserve, current, amount):
    """-> (reserve, current, ok). Reserva primero; si no alcanza, todo a 0 y False."""
    if amount <= 0:
        return reserve, current, True
    if reserve + current < amount:
        return 0.0, 0.0, False
    from_reserve = min(reserve, amount)
    return reserve - from_reserve, max(0.0, current - (amount - from_reserve)), True


def regen_energy(current, mx, time_since_spend, dt, regen_per_s, regen_delay):
    if time_since_spend < regen_delay or dt <= 0 or current >= mx:
        return current
    return min(mx, current + regen_per_s * dt)


def energy_cost(cost_per_s, drive_scale, dt):
    if cost_per_s <= 0 or dt <= 0:
        return 0.0
    return cost_per_s * min(abs(drive_scale), 1.5) * dt


def pickup_energy(item):
    return {"Energy.Small": 15.0, "Energy.Large": 40.0}.get(item, 0.0)


def are_hostile(a, b):
    return a != b


def is_in_cone(origin, direction, point, rng, half_angle_deg):
    to = sub(point, origin)
    dist = size(to)
    if dist > rng:
        return False
    if dist < 1.0:
        return True
    n = norm(direction)
    if size(n) == 0:
        return False
    return dot(to, n) / dist >= math.cos(math.radians(half_angle_deg))


def pick_nearest_target(origin, candidates, source_team, rng):
    """candidates: [(pos, team, alive)] -> índice o -1."""
    best, best_d2 = -1, rng * rng
    for i, (pos, team, alive) in enumerate(candidates):
        if not alive or not are_hostile(source_team, team):
            continue
        d = sub(pos, origin)
        d2 = dot(d, d)
        if d2 <= best_d2:
            best, best_d2 = i, d2
    return best


def signed_angle_around_axis(frm, to, axis):
    n = norm(axis)
    f = sub(frm, mul(n, dot(frm, n)))
    t = sub(to, mul(n, dot(to, n)))
    if size(n) == 0 or dot(f, f) < 1e-8 or dot(t, t) < 1e-8:
        return 0.0
    return math.atan2(dot(n, cross(f, t)), dot(f, t))


def aim_rate(err, kp, max_rate):
    return clamp(err * kp, -max_rate, max_rate)


def spin_accel(target, current, gain, max_accel, dt):
    k = gain
    if dt > 0:
        k = min(k, 0.8 / dt)
    return clamp(k * (target - current), -max_accel, max_accel)


def wheel_scale(throttle, steer, side):
    t = clamp(throttle, -1, 1)
    s = clamp(steer, -1, 1)
    return t * (1 - 0.5 * abs(s)) if side * s > 0 else t


def fan_scale(lift_fan, throttle, lift):
    return clamp(1 + 0.5 * lift, 0.5, 1.5) if lift_fan else clamp(throttle, -1, 1)


def is_lift_direction(thrust, build_up):
    return dot(norm(thrust), norm(build_up)) > 0.7


def tilted_up(forward_flat, throttle, max_tilt_deg):
    tilt = math.radians(max_tilt_deg * clamp(throttle, -1, 1))
    return norm(add(UP, mul(norm(forward_flat), math.tan(tilt))))


def level_accel(up, desired, omega, kp, kd):
    u = norm(up)
    non_yaw = sub(omega, mul(u, dot(omega, u)))
    return sub(mul(cross(u, norm(desired)), kp), mul(non_yaw, kd))


def cooldown_ready(timer, dt, interval):
    """-> (timer, fired)"""
    timer += dt
    if timer >= interval:
        return 0.0, True
    return timer, False


SEAT_SKIPS = {"Structural", "Seat", "Battery", "Bomb"}
def seat_activates(kind): return kind not in SEAT_SKIPS

def refresh_slow_end(current_end, now, duration): return max(current_end, now + duration)


# ---------- casos (mismos nombres que JamDeviceTests.cpp) ----------
FAILS = []
def check(name, cond):
    print(("ok   " if cond else "FAIL ") + name)
    if not cond:
        FAILS.append(name)

def near(a, b, tol=1e-4): return abs(a - b) <= tol
def vnear(a, b, tol=1e-4): return all(near(x, y, tol) for x, y in zip(a, b))


def test_energy():
    check("Energy.SpendBarOnly", spend_energy(0, 60, 10) == (0, 50, True))
    check("Energy.ReserveFirst", spend_energy(30, 60, 10) == (20, 60, True))
    check("Energy.ReserveThenBar", spend_energy(5, 60, 10) == (0, 55, True))
    check("Energy.NotEnoughZeroes", spend_energy(1, 2, 10) == (0, 0, False))
    check("Energy.ZeroAmountFree", spend_energy(0, 0, 0) == (0, 0, True))
    check("Energy.RegenWaitsDelay", regen_energy(10, 100, 2.9, 1.0, 2, 3) == 10)
    check("Energy.RegenAfterDelay", near(regen_energy(10, 100, 3.0, 1.0, 2, 3), 12))
    check("Energy.RegenClampsMax", regen_energy(99.5, 100, 10, 1.0, 2, 3) == 100)
    check("Energy.CostFan1s", near(energy_cost(1.5, 1.0, 1.0), 1.5))
    check("Energy.CostThrottleZeroFree", energy_cost(1.5, 0.0, 1.0) == 0)
    check("Energy.CostReverseSameAsForward", near(energy_cost(0.2, -1.0, 0.5), 0.1))
    check("Energy.Pickups", pickup_energy("Energy.Small") == 15 and pickup_energy("Energy.Large") == 40 and pickup_energy("Part.Fan") == 0)

    # Simulación del ejemplo de la research 13 §6: carro con 4 ruedas + 1 ventilador = 2.3/s; barra llena dura ~43 s.
    reserve, cur, t, dt, ok = 0.0, 100.0, 0.0, 1.0 / 60.0, True
    while ok and t < 200:
        for cost in (0.2, 0.2, 0.2, 0.2, 1.5):
            reserve, cur, ok = spend_energy(reserve, cur, energy_cost(cost, 1.0, dt))
            if not ok:
                break
        t += dt
    check("Energy.CarLasts43s (%.1f s)" % t, 42.5 <= t <= 44.5)

    # Rueda Infernal (rueda + lanzallamas = 2.2/s) con una batería (reserva 30) y barra inicial 60: 90/2.2 = 40.9 s,
    # y la reserva se acaba antes de tocar la barra.
    reserve, cur, t, ok, bar_when_reserve_empty = 30.0, 60.0, 0.0, True, None
    while ok and t < 200:
        for cost in (0.2, 2.0):
            reserve, cur, ok = spend_energy(reserve, cur, energy_cost(cost, 1.0, dt))
            if not ok:
                break
        if reserve == 0 and bar_when_reserve_empty is None:
            bar_when_reserve_empty = cur
        t += dt
    check("Energy.BatteryFirst (bar %.2f when reserve empties)" % bar_when_reserve_empty, bar_when_reserve_empty > 59.9)
    check("Energy.InfernalWheelLasts41s (%.1f s)" % t, 40.4 <= t <= 41.4)

    # Tras vaciarse: 3 s sin recarga y luego +2/s.
    cur, since = 0.0, 0.0
    for _ in range(int(3.0 / dt) - 1):
        since += dt
        cur = regen_energy(cur, 100, since, dt, 2, 3)
    check("Energy.NoRegenBefore3s", cur == 0)
    for _ in range(int(5.0 / dt) + 2):
        since += dt
        cur = regen_energy(cur, 100, since, dt, 2, 3)
    check("Energy.Regen2PerSec (%.2f after ~5 s)" % cur, 9.9 <= cur <= 10.2)


def test_cone_and_teams():
    o = (0, 0, 0)
    check("Cone.Ahead", is_in_cone(o, FWD, (300, 0, 0), 400, 25))
    check("Cone.OutOfRange", not is_in_cone(o, FWD, (401, 0, 0), 400, 25))
    check("Cone.Behind", not is_in_cone(o, FWD, (-100, 0, 0), 400, 25))
    check("Cone.InsideEdge24deg", is_in_cone(o, FWD, (300 * math.cos(math.radians(24)), 300 * math.sin(math.radians(24)), 0), 400, 25))
    check("Cone.OutsideEdge26deg", not is_in_cone(o, FWD, (300 * math.cos(math.radians(26)), 300 * math.sin(math.radians(26)), 0), 400, 25))
    check("Cone.AtOrigin", is_in_cone(o, FWD, (0.2, 0, 0), 400, 25))
    check("Cone.UnnormalizedDir", is_in_cone(o, (10, 0, 0), (300, 0, 0), 400, 25))
    # Si la base gira, el cono gira: mismo punto, dirección rotada 90°.
    check("Cone.RotatesWithBase", not is_in_cone(o, RIGHT, (300, 0, 0), 400, 25) and is_in_cone(o, RIGHT, (0, 300, 0), 400, 25))
    check("Team.Hostile", are_hostile("Red", "Blue") and are_hostile("Red", "Neutral") and are_hostile("Neutral", "Blue"))
    check("Team.NoFriendlyFire", not are_hostile("Red", "Red") and not are_hostile("Neutral", "Neutral"))
    # Daño de llama: 15/s en ticks de 0.25 s -> 3.75 por tick; 100 de vida = 27 ticks = 6.75 s.
    hp, ticks = 100.0, 0
    while hp > 0:
        hp -= 15 * 0.25
        ticks += 1
    check("Flame.Kills100hpIn27Ticks", ticks == 27)


def test_targeting():
    c = [((1000, 0, 0), "Blue", True), ((500, 0, 0), "Red", True), ((300, 0, 0), "Blue", False), ((800, 0, 0), "Neutral", True), ((5000, 0, 0), "Blue", True)]
    check("Target.NearestHostileAlive", pick_nearest_target((0, 0, 0), c, "Red", 2000) == 3)
    check("Target.SkipsAllyAndDead", pick_nearest_target((0, 0, 0), c[:3], "Red", 2000) == 0)
    check("Target.NoneInRange", pick_nearest_target((0, 0, 0), c, "Red", 700) == -1)
    check("Target.NeutralTurretHitsTeams", pick_nearest_target((0, 0, 0), c, "Neutral", 2000) == 1)
    check("Target.Empty", pick_nearest_target((0, 0, 0), [], "Red", 2000) == -1)

    check("Aim.Left90", near(signed_angle_around_axis(FWD, RIGHT, UP), math.pi / 2))
    check("Aim.Right90", near(signed_angle_around_axis(FWD, (0, -1, 0), UP), -math.pi / 2))
    check("Aim.IgnoresHeight", near(signed_angle_around_axis(FWD, (1, 1, 5), UP), math.pi / 4))
    check("Aim.ParallelToAxisIsZero", signed_angle_around_axis(FWD, UP, UP) == 0)
    check("Aim.RateSaturates", aim_rate(2.0, 6, 3) == 3 and aim_rate(-2.0, 6, 3) == -3 and near(aim_rate(0.1, 6, 3), 0.6))

    # Lazo cerrado: el signo del ángulo coincide con la regla omega x r, así que la Head converge al objetivo.
    aim, target, dt, omega = FWD, norm((-1.0, 1.0, 0.0)), 1.0 / 60.0, 0.0
    for _ in range(240):
        err = signed_angle_around_axis(aim, target, UP)
        omega += spin_accel(aim_rate(err, 6, 3), omega, 30, 200, dt) * dt
        aim = norm(add(aim, mul(cross(mul(UP, omega), aim), dt)))
    check("Aim.ConvergesIn4s (err %.4f rad)" % abs(signed_angle_around_axis(aim, target, UP)), abs(signed_angle_around_axis(aim, target, UP)) < 0.02)


def test_spin_and_seat():
    check("Spin.Proportional", near(spin_accel(10, 8, 30, 200, 1 / 60), 60))
    check("Spin.Saturates", spin_accel(10, 0, 30, 200, 1 / 60) == 200)
    check("Spin.LowFpsGainLimited", near(spin_accel(1, 0, 30, 200, 0.1), 8))
    # Rueda raíz: de 0 a 10 rad/s sin pasarse.
    w, dt, peak = 0.0, 1 / 60, 0.0
    for _ in range(120):
        w += spin_accel(10, w, 30, 200, dt) * dt
        peak = max(peak, w)
    check("Spin.RootWheelReachesTarget (%.3f rad/s, peak %.3f)" % (w, peak), near(w, 10, 0.01) and peak <= 10.0001)
    # Igual a 10 fps (ganancia limitada): estable.
    w = 0.0
    for _ in range(40):
        w += spin_accel(10, w, 30, 200, 0.1) * 0.1
    check("Spin.StableAt10Fps (%.3f)" % w, near(w, 10, 0.05))

    check("Seat.WheelStraight", wheel_scale(1, 0, -1) == 1 and wheel_scale(1, 0, 1) == 1)
    check("Seat.WheelTurnRightSlowsRight", near(wheel_scale(1, 1, 1), 0.5) and wheel_scale(1, 1, -1) == 1)
    check("Seat.WheelTurnLeftSlowsLeft", near(wheel_scale(1, -1, -1), 0.5) and wheel_scale(1, -1, 1) == 1)
    check("Seat.WheelReverse", wheel_scale(-1, 0, 1) == -1)
    check("Seat.RootWheelSideZero", wheel_scale(0.5, 1, 0) == 0.5)
    check("Seat.FanThrust", fan_scale(False, 0.5, 1) == 0.5 and fan_scale(False, -2, 0) == -1)
    check("Seat.FanLift", fan_scale(True, 0, 0) == 1 and fan_scale(True, 0, 1) == 1.5 and fan_scale(True, 1, -1) == 0.5)
    check("Seat.LiftDirection", is_lift_direction(UP, UP) and not is_lift_direction(FWD, UP) and not is_lift_direction((0, 0, -1), UP))
    t = tilted_up(FWD, 1, 15)
    check("Seat.TiltForward15deg", near(math.degrees(math.atan2(t[0], t[2])), 15) and near(t[1], 0))
    check("Seat.NoTiltWithoutThrottle", vnear(tilted_up(FWD, 0, 15), UP))
    check("Seat.LevelZeroWhenLevel", vnear(level_accel(UP, UP, (0, 0, 3), 25, 8), (0, 0, 0)))
    # Inclinado hacia +Y (roll): el torque debe devolver Up hacia el vertical -> d(Up)/dt = accel x Up apunta a -Y.
    tilted = norm((0, 0.5, 1))
    a = level_accel(tilted, UP, (0, 0, 0), 25, 8)
    check("Seat.LevelRestores", cross(a, tilted)[1] < 0)
    check("Seat.LevelDampsRoll", level_accel(UP, UP, (2, 0, 0), 25, 8) == (-16, 0, 0))
    # Lazo cerrado del nivelado: de 30° de inclinación a < 1° en 2 s.
    up, omega, dt = norm((0, math.tan(math.radians(30)), 1)), (0.0, 0.0, 0.0), 1 / 60
    for _ in range(120):
        omega = add(omega, mul(level_accel(up, UP, omega, 25, 8), dt))
        up = norm(add(up, mul(cross(omega, up), dt)))
    ang = math.degrees(math.acos(clamp(dot(up, UP), -1, 1)))
    check("Seat.LevelConverges (%.2f deg)" % ang, ang < 1.0)

    check("Seat.Activates", seat_activates("Fan") and seat_activates("Wheel") and seat_activates("Flame") and seat_activates("Stake"))
    check("Seat.DoesNotActivate", not seat_activates("Structural") and not seat_activates("Seat") and not seat_activates("Battery") and not seat_activates("Bomb"))


def test_cooldown_and_status():
    timer, shots = 0.0, 0
    for _ in range(48):  # 6 s en pasos de 0.125 s (exacto en binario)
        timer, fired = cooldown_ready(timer, 0.125, 1.5)
        shots += fired
    check("Cooldown.Cannon4ShotsIn6s (%d)" % shots, shots == 4)
    timer, fired = cooldown_ready(0.0, 0.1, 1.5)
    check("Cooldown.NotReadyEarly", not fired and near(timer, 0.1))
    check("Slow.Refreshes", refresh_slow_end(0, 10, 3) == 13 and refresh_slow_end(13, 11, 3) == 14)
    check("Slow.NeverShortens", refresh_slow_end(20, 10, 3) == 20)


def test_cost_table_matches_json():
    """La tabla EnergyCostFor de JamBuildPart.cpp debe decir lo mismo que energy_cost_per_s del JSON (tipos del catálogo)."""
    with open(os.path.join(HERE, "BuildKnowledge.json"), encoding="utf-8") as f:
        knowledge = json.load(f)
    with open(os.path.join(ROOT, "Source", "GameJamm", "Building", "JamBuildPart.cpp"), encoding="utf-8") as f:
        cpp = f.read()
    block = cpp[cpp.index("EnergyCostFor(FName Type)"):cpp.index("FJamPartDef Make(")]
    cpp_costs = {m.group(1): float(m.group(2)) for m in re.finditer(r'TEXT\("(\w+)"\),\s*([0-9.]+)f', block)}
    catalog = set(re.findall(r'Make\("(\w+)",', cpp))
    json_costs = {e["type"]: float(e.get("energy_cost_per_s", 0.0)) for e in knowledge["elements"]}
    bad = []
    for t in sorted(catalog):
        if t in json_costs and not near(json_costs[t], cpp_costs.get(t, 0.0)):
            bad.append("%s json=%s cpp=%s" % (t, json_costs[t], cpp_costs.get(t, 0.0)))
    check("Costs.CatalogMatchesJson (%d tipos) %s" % (len(catalog), "; ".join(bad)), not bad and len(catalog) >= 20)
    energy = knowledge["energy"]
    check("Costs.EnergyBlockMatchesComponentDefaults", (energy["max"], energy["start"], energy["regen_per_s"], energy["regen_delay_s"]) == (100, 60, 2.0, 3.0)
          and energy["pickups"] == {"Energy.Small": 15, "Energy.Large": 40})
    missing = sorted(t for t in catalog if t in json_costs and not next(e for e in knowledge["elements"] if e["type"] == t)["in_code"])
    check("Costs.CatalogTypesMarkedInCode %s" % missing, not missing)


if __name__ == "__main__":
    test_energy()
    test_cone_and_teams()
    test_targeting()
    test_spin_and_seat()
    test_cooldown_and_status()
    test_cost_table_matches_json()
    print("\n%d fallas" % len(FAILS))
    sys.exit(1 if FAILS else 0)
