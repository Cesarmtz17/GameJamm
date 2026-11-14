"""Efectos de sonido de GameJamm sintetizados por código (originales, sin licencias de terceros).

Uso (con el Python de Blender, que trae numpy):
    blender --background --factory-startup --python gen_sfx.py -- <carpeta_salida>

Cubre la tabla de feedback y audio del GDD (§17). WAV mono 16-bit 44.1 kHz, nombres SFX_<evento>.wav listos para UE.
"""
import os
import sys
import wave

import numpy as np

SR = 44100
rng = np.random.default_rng(7)


def t(d):
    return np.linspace(0, d, int(SR * d), endpoint=False)


def env(n, a=0.005, r=None, curve=4.0):
    """Envolvente ataque rápido + caída exponencial."""
    x = np.ones(n)
    na = max(1, int(SR * a))
    x[:na] = np.linspace(0, 1, na)
    tail = np.linspace(0, 1, n - na)
    x[na:] = np.exp(-curve * tail)
    return x


def tone(freq, d, kind="sine"):
    tt = t(d)
    f = np.asarray(freq) if np.ndim(freq) else np.full(tt.size, freq)
    ph = 2 * np.pi * np.cumsum(f) / SR
    if kind == "square":
        return np.sign(np.sin(ph))
    if kind == "saw":
        return 2 * ((ph / (2 * np.pi)) % 1) - 1
    if kind == "tri":
        return 2 * np.abs(2 * ((ph / (2 * np.pi)) % 1) - 1) - 1
    return np.sin(ph)


def noise(d):
    return rng.uniform(-1, 1, int(round(SR * d)))


def noise_n(n):
    return rng.uniform(-1, 1, int(n))


def lowpass(x, cutoff):
    a = np.exp(-2 * np.pi * cutoff / SR)
    y = np.empty_like(x)
    acc = 0.0
    for i, v in enumerate(x):
        acc = (1 - a) * v + a * acc
        y[i] = acc
    return y


def highpass(x, cutoff):
    return x - lowpass(x, cutoff)


def sweep(f0, f1, d):
    return np.geomspace(f0, f1, int(SR * d))


def mix(*parts):
    n = max(p.size for p in parts)
    out = np.zeros(n)
    for p in parts:
        out[:p.size] += p
    return out


def pad(x, d):
    return np.concatenate([np.zeros(int(SR * d)), x])


def save(path, x, gain=0.9):
    x = x / (np.max(np.abs(x)) + 1e-9) * gain
    fade = min(200, x.size)
    x[-fade:] *= np.linspace(1, 0, fade)
    with wave.open(path, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(SR)
        w.writeframes((x * 32767).astype(np.int16).tobytes())


# ---------------------------------------------------------------- sonidos (GDD §17)
def pop():                       # cada pieza que arma el asistente
    d = 0.12
    return tone(sweep(900, 300, d), d) * env(int(SR * d), curve=8) + 0.2 * lowpass(noise(d), 3000) * env(int(SR * d), curve=20)


def wood_thunk():                # unir piezas / golpe en madera
    d = 0.25
    body = tone(sweep(220, 110, d), d) * env(int(SR * d), curve=10)
    knock = lowpass(noise(d), 1800) * env(int(SR * d), a=0.001, curve=30)
    return mix(body, 0.8 * knock)


def power_on():
    d = 0.45
    return mix(tone(sweep(120, 600, d), d, "saw") * 0.3 * env(int(SR * d), a=0.05, curve=2),
               tone(sweep(240, 1200, d), d) * 0.4 * env(int(SR * d), a=0.05, curve=3),
               pad(tone(1400, 0.06) * env(int(SR * 0.06), curve=6), 0.0) * 0.4)


def power_off():
    d = 0.5
    return tone(sweep(700, 80, d), d, "saw") * 0.5 * env(int(SR * d), a=0.01, curve=3)


def motor_loop():                # zumbido de motor, se puede repetir sin corte
    d = 2.0
    tt = t(d)
    base = 55
    wob = 1 + 0.03 * np.sin(2 * np.pi * 4 * tt)
    x = 0.6 * tone(base * wob, d, "saw") + 0.3 * tone(base * 2 * wob, d, "square") + 0.15 * lowpass(noise(d), 400)
    return lowpass(x, 900)


def no_energy():
    d = 0.7
    x = tone(sweep(400, 60, d), d, "square") * env(int(SR * d), a=0.01, curve=2.5)
    return lowpass(x, 1500)


def fire_loop():
    d = 2.0
    crackle = np.zeros(int(SR * d))
    for _ in range(60):
        i = rng.integers(0, crackle.size - 400)
        crackle[i:i + 400] += rng.uniform(0.3, 1) * noise_n(400) * env(400, a=0.0005, curve=25)
    roar = lowpass(noise(d), 500) * 0.8
    return mix(roar, 0.5 * highpass(crackle, 2000))


def ice():
    d = 0.8
    shimmer = sum(tone(f, d) * env(int(SR * d), a=0.002, curve=c) for f, c in
                  ((2093, 5), (2637, 6), (3136, 7), (4186, 8)))
    crack = highpass(noise(0.08), 3000) * env(int(SR * 0.08), curve=15)
    return mix(0.6 * shimmer, crack)


def shock():
    d = 0.6
    buzz = tone(60, d, "square") * 0.4
    zap = np.zeros(int(SR * d))
    for _ in range(25):
        i = rng.integers(0, zap.size - 600)
        zap[i:i + 600] += highpass(noise_n(600), 2500) * env(600, a=0.0003, curve=20)
    return mix(buzz, zap) * env(int(SR * d), a=0.005, curve=2)


def explosion():
    d = 1.6
    boom = lowpass(noise(d), 300) * env(int(SR * d), a=0.002, curve=4)
    sub = tone(sweep(90, 30, d), d) * env(int(SR * d), a=0.002, curve=5)
    debris = highpass(noise(d), 1500) * env(int(SR * d), a=0.002, curve=9) * 0.4
    return mix(boom, 0.9 * sub, debris)


def cannon():
    d = 0.9
    return mix(lowpass(noise(d), 600) * env(int(SR * d), a=0.001, curve=8),
               tone(sweep(150, 50, d), d) * env(int(SR * d), a=0.001, curve=7))


def hit():
    d = 0.2
    return mix(lowpass(noise(d), 2500) * env(int(SR * d), a=0.001, curve=25),
               tone(sweep(300, 120, d), d) * 0.6 * env(int(SR * d), curve=15))


def sword_swing():
    d = 0.35
    w = highpass(noise(d), 600)
    shape = np.sin(np.linspace(0, np.pi, w.size)) ** 2
    return lowpass(w, 4000) * shape


def tower_shot():                # cristal de la atalaya
    d = 0.5
    return mix(tone(sweep(1800, 600, d), d, "tri") * env(int(SR * d), curve=5),
               0.3 * highpass(noise(0.05), 4000) * env(int(SR * 0.05), curve=10))


def tower_destroyed():
    d = 2.5
    crumble = np.zeros(int(SR * d))
    for _ in range(40):
        i = rng.integers(0, int(crumble.size * 0.8))
        n = rng.integers(2000, 8000)
        n = min(n, crumble.size - i)
        crumble[i:i + n] += lowpass(noise_n(n), rng.uniform(300, 1200)) * env(n, a=0.002, curve=8) * rng.uniform(.3, 1)
    return mix(explosion() * 0.8, crumble)


def fanfare():
    notes = [523.25, 659.25, 783.99, 1046.5]
    out = np.zeros(0)
    for i, f in enumerate(notes):
        d = 0.14 if i < 3 else 0.6
        n = (tone(f, d, "square") * 0.35 + tone(f * 2, d) * 0.2) * env(int(SR * d), a=0.005, curve=3 if i == 3 else 6)
        out = np.concatenate([out, n])
    return lowpass(out, 5000)


def loot_pickup():               # "tin"
    d = 0.35
    a = tone(1318.5, 0.08) * env(int(SR * 0.08), curve=6)
    b = tone(1975.5, d) * env(int(SR * d), curve=6)
    return np.concatenate([a, b])


def error_soft():                # mochila llena
    d = 0.18
    one = tone(220, d, "square") * env(int(SR * d), curve=6)
    return lowpass(np.concatenate([one, np.zeros(int(SR * 0.05)), one]), 1200)


def death():
    d = 0.9
    return mix(lowpass(noise(0.15), 400) * env(int(SR * 0.15), curve=10),
               tone(sweep(200, 40, d), d) * env(int(SR * d), a=0.005, curve=3))


def respawn():
    d = 0.8
    return mix(tone(sweep(200, 1600, d), d) * env(int(SR * d), a=0.3, curve=3),
               0.3 * tone(sweep(400, 3200, d), d, "tri") * env(int(SR * d), a=0.3, curve=4))


def chest_open():
    d = 0.6
    creak = tone(sweep(180, 260, 0.4), 0.4, "saw") * 0.3 * env(int(SR * 0.4), a=0.02, curve=2)
    creak = lowpass(creak, 1200)
    return np.concatenate([creak, wood_thunk()[: int(SR * 0.2)] * 0.6, loot_pickup()[: int(SR * 0.3)] * 0.5])


def ui_click():
    d = 0.05
    return tone(1200, d) * env(int(SR * d), a=0.001, curve=12)


def wind_loop():                 # ambiente de islas
    d = 4.0
    tt = t(d)
    gust = 0.6 + 0.4 * np.sin(2 * np.pi * 0.25 * tt) ** 2
    return lowpass(noise(d), 700) * gust


SFX = {
    "SFX_Build_Pop": pop, "SFX_Build_Attach": wood_thunk, "SFX_Power_On": power_on, "SFX_Power_Off": power_off,
    "SFX_Motor_Loop": motor_loop, "SFX_Energy_Empty": no_energy, "SFX_Fire_Loop": fire_loop, "SFX_Ice": ice,
    "SFX_Shock": shock, "SFX_Explosion": explosion, "SFX_Cannon": cannon, "SFX_Hit": hit,
    "SFX_Sword_Swing": sword_swing, "SFX_Tower_Shot": tower_shot, "SFX_Tower_Destroyed": tower_destroyed,
    "SFX_Fanfare": fanfare, "SFX_Loot_Pickup": loot_pickup, "SFX_Backpack_Full": error_soft, "SFX_Death": death,
    "SFX_Respawn": respawn, "SFX_Chest_Open": chest_open, "SFX_UI_Click": ui_click, "SFX_Amb_Wind_Loop": wind_loop,
}


def main():
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    out = os.path.abspath(argv[0] if argv else "SFX")
    os.makedirs(out, exist_ok=True)
    for name, fn in SFX.items():
        x = fn()
        save(os.path.join(out, f"{name}.wav"), x)
        print(f"SFX {name:22s} {x.size / SR:5.2f}s")
    print("SFX_OK", len(SFX))


if __name__ == "__main__":
    main()
