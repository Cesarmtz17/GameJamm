"""Música de GameJamm compuesta por código: loops de aventura ligera (GDD §17). Originales, sin licencias.

Uso:
    blender --background --factory-startup --python gen_music.py -- <carpeta_salida>

- MUS_Menu_Loop: tranquilo (menú y pantalla final).
- MUS_Match_Loop: partida.
- MUS_Match_Intense_Loop: muerte súbita (mismo tema, más rápido, con más percusión).
Cada loop dura un número exacto de compases para repetirse sin corte (en UE: Sound Wave con Looping).
"""
import os
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import gen_sfx as sx  # noqa: E402

SR = sx.SR


def midi(n):
    return 440.0 * 2 ** ((n - 69) / 12)


# progresión I - V - vi - IV en Re mayor (D A Bm G)
CHORDS = [(62, 66, 69), (57, 61, 64), (59, 62, 66), (55, 59, 62)]
MELODY = [  # (nota midi o None, duración en corcheas) por acorde: 8 corcheas = 1 compás
    [(74, 2), (76, 1), (78, 1), (76, 2), (74, 2)],
    [(73, 2), (71, 2), (69, 4)],
    [(71, 1), (73, 1), (74, 2), (78, 2), (76, 2)],
    [(74, 2), (71, 2), (69, 2), (None, 2)],
]


def note(freq, d, kind, a=0.005, curve=3.0, vol=1.0):
    n = int(SR * d)
    return sx.tone(freq, d, kind)[:n] * sx.env(n, a=a, curve=curve) * vol


def kick(d):
    n = int(SR * d)
    x = sx.tone(sx.sweep(150, 45, d), d)[:n] * sx.env(n, a=0.001, curve=9)
    return x


def snare(d):
    n = int(SR * d)
    return sx.highpass(sx.noise_n(n), 1500) * sx.env(n, a=0.001, curve=14) * 0.6


def hat(d):
    n = int(SR * d)
    return sx.highpass(sx.noise_n(n), 6000) * sx.env(n, a=0.0005, curve=30) * 0.25


def place(buf, x, start):
    i = int(start)
    j = min(buf.size, i + x.size)
    buf[i:j] += x[: j - i]


def song(bpm, bars, intensity):
    eighth = 60 / bpm / 2
    total = int(SR * eighth * 8 * bars)
    bass = np.zeros(total)
    pads = np.zeros(total)
    lead = np.zeros(total)
    drums = np.zeros(total)
    for b in range(bars):
        root, third, fifth = CHORDS[b % 4]
        t0 = b * 8 * eighth * SR
        # bajo en corcheas (raíz / quinta)
        for k in range(8):
            f = midi(root - 24 + (7 if k % 4 == 2 else 0))
            place(bass, note(f, eighth * 0.95, "tri", curve=4, vol=0.55), t0 + k * eighth * SR)
        # pad del acorde
        for n_ in (root, third, fifth):
            place(pads, note(midi(n_), eighth * 8, "saw", a=0.15, curve=1.2, vol=0.08), t0)
        # arpegio
        if intensity >= 1:
            arp = [root, third, fifth, root + 12, fifth, third, root + 12, fifth]
            for k, n_ in enumerate(arp):
                place(lead, note(midi(n_ + 12), eighth * 0.8, "square", curve=6, vol=0.06), t0 + k * eighth * SR)
        # melodía (en la segunda mitad del loop)
        if b >= 4 or intensity == 0:
            pos = 0
            for n_, dur in MELODY[b % 4]:
                if n_ is not None:
                    place(lead, note(midi(n_), eighth * dur * 0.95, "tri", a=0.01, curve=2.5, vol=0.28),
                          t0 + pos * eighth * SR)
                pos += dur
        # batería
        for k in range(8):
            ts = t0 + k * eighth * SR
            if intensity == 0:
                if k == 0:
                    place(drums, kick(0.3) * 0.5, ts)
                continue
            if k in (0, 4) or (intensity == 2 and k in (3, 6)):
                place(drums, kick(0.3), ts)
            if k in (2, 6):
                place(drums, snare(0.2), ts)
            place(drums, hat(0.05) * (1.4 if intensity == 2 else 1), ts)
            if intensity == 2:
                place(drums, hat(0.05) * 0.7, ts + eighth * SR / 2)
    pads = sx.lowpass(pads, 2200)
    mix = bass + pads + lead + drums * (0.8 if intensity else 0.5)
    return mix


def save_stereo(path, x, gain=0.85):
    x = x / (np.max(np.abs(x)) + 1e-9) * gain
    import wave
    st = np.stack([x, x], axis=1)
    with wave.open(path, "wb") as w:
        w.setnchannels(2)
        w.setsampwidth(2)
        w.setframerate(SR)
        w.writeframes((st * 32767).astype(np.int16).tobytes())


def main():
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    out = os.path.abspath(argv[0] if argv else "Music")
    os.makedirs(out, exist_ok=True)
    for name, bpm, bars, inten in (("MUS_Menu_Loop", 84, 8, 0), ("MUS_Match_Loop", 112, 8, 1),
                                   ("MUS_Match_Intense_Loop", 140, 8, 2)):
        x = song(bpm, bars, inten)
        save_stereo(os.path.join(out, f"{name}.wav"), x)
        print(f"MUS {name:24s} {x.size / SR:5.1f}s bpm={bpm}")
    print("MUSIC_OK")


if __name__ == "__main__":
    main()
