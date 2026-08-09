#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Genera source/music_loop.h: musica di sottofondo e click sintetizzati.

- music_loop: int16 mono @11025 Hz, 4 battute (8 secondi) in SOL, stile
  shanty: melodia (onda triangolare) + basso (sine, ottava sotto).
- sfx_click: int16 mono @11025 Hz, 60 ms di onda quadra 880 Hz.

Il loop e' senza clic: ogni nota dura esattamente 5512 campioni (0.5 s a
11025 Hz) e il pattern torna all'inizio.
"""
import os
import math
import struct

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "..", "source", "music_loop.h")

RATE = 11025
NOTE_MS = 500
NOTE_LEN = RATE * NOTE_MS // 1000  # 5512 campioni per nota, esatto

# Frequenze (Hz) - SOL maggiore
F = {
    "G2": 98.0, "A2": 110.0, "B2": 123.47, "C3": 130.81, "D3": 146.83,
    "E3": 164.81, "F3": 174.61, "G3": 196.0, "A3": 220.0, "B3": 246.94,
    "C4": 261.63, "D4": 293.66, "E4": 329.63, "F4": 349.23, "G4": 392.0,
}

# 16 note = 4 battute
MELODY = [
    "G3", "G3", "A3", "G3", "C4", "B3", "G3", "G3",
    "G3", "G3", "A3", "G3", "D4", "C4", "B3", "A3",
    "G4", "E4", "C4", "B3", "A3", "B3", "G3", "G3",
    "F4", "D4", "C4", "B3", "G3", "G3", "A3", "G3",
]
BASS = ["G2", "G2", "A2", "A2", "C3", "C3", "B2", "B2",
        "G2", "G2", "A2", "A2", "D3", "D3", "C3", "C3",
        "G2", "G2", "C3", "C3", "A2", "A2", "G2", "G2",
        "F3", "F3", "C3", "C3", "G2", "G2", "A2", "G2"]


def tone(freq, nsamples, wave, vol):
    out = []
    for i in range(nsamples):
        t = float(i) / RATE
        ph = t * freq
        if wave == "tri":
            v = 2.0 * abs(2.0 * (ph - math.floor(ph + 0.5))) - 1.0
        else:
            v = math.sin(2.0 * math.pi * ph)
        out.append(v * vol)
    return out


def envelope(nsamples, attack, decay, sustain):
    env = []
    for i in range(nsamples):
        if i < attack:
            e = float(i) / attack
        elif i < attack + decay:
            e = sustain + (1.0 - sustain) * (1.0 - (i - attack) / decay)
        else:
            e = sustain
        env.append(e)
    return env


def build_music():
    n_notes = len(MELODY)
    total = n_notes * NOTE_LEN
    mix = [0.0] * total
    for n in range(n_notes):
        mel = tone(F[MELODY[n]], NOTE_LEN, "tri", 0.45)
        bass = tone(F[BASS[n]] / 1.0, NOTE_LEN, "sin", 0.35)
        env = envelope(NOTE_LEN, int(0.01 * RATE), int(0.12 * RATE), 0.8)
        base = n * NOTE_LEN
        for i in range(NOTE_LEN):
            v = mel[i] * env[i] + bass[i] * env[i]
            # dolce saturazione
            mix[base + i] = math.tanh(v) * 22000
    # fade di 5 ms in testa e in coda per un loop senza clic
    fade = int(0.005 * RATE)
    for i in range(fade):
        g = float(i) / fade
        mix[i] *= g
        mix[total - 1 - i] *= g
    return mix


def build_click():
    n = int(0.06 * RATE)
    env = envelope(n, int(0.005 * RATE), int(0.055 * RATE), 0.0)
    out = []
    for i in range(n):
        v = 1.0 if (i * 880.0 / RATE) % 1.0 < 0.5 else -1.0
        out.append(v * env[i] * 12000.0)
    return out


def emit(name, samples, lines):
    lines.append("static const short %s[] = {" % name)
    for i in range(0, len(samples), 12):
        chunk = samples[i:i + 12]
        lines.append("  " + ", ".join("%d" % v for v in chunk) + ",")
    lines.append("};")
    lines.append("#define %s_LEN %d" % (name.upper(), len(samples)))
    lines.append("")


def main():
    music = build_music()
    click = build_click()
    lines = []
    lines.append("/* Generato da tools/make_audio.py - NON MODIFICARE A MANO */")
    lines.append("#ifndef MUSIC_LOOP_H")
    lines.append("#define MUSIC_LOOP_H")
    lines.append("")
    lines.append("#define MUSIC_RATE %d" % RATE)
    lines.append("")
    emit("music_loop", [int(round(v)) for v in music], lines)
    emit("sfx_click", [int(round(v)) for v in click], lines)
    lines.append("#endif")
    with open(OUT, "w", encoding="ascii", newline="\n") as f:
        f.write("\n".join(lines) + "\n")
    print("Scritto: %s (%d campioni musica, %d click)" % (OUT, len(music), len(click)))


if __name__ == "__main__":
    main()
