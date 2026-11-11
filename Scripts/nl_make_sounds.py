# No Light After Midnight - eksik/ortak kullanılan olaylar için ayrı geçici sesler üretir ve içe aktarır.
# Sesler kodla sentezlenir (placeholder); final için aynı isimle gerçek kayıtlarla değiştirin.
# Çalıştırma: UnrealEditor-Cmd.exe NoLightAfterMidnight.uproject -ExecutePythonScript="<tam yol>\Scripts\nl_make_sounds.py" -unattended -nullrhi

import math
import os
import random
import struct
import wave

import unreal

SR = 44100
OUT_DIR = os.path.join(unreal.Paths.project_content_dir(), "Audio", "NL")
DEST = "/Game/Audio/NL"
rng = random.Random(7)


# ------------------------------------------------------------------ yardımcılar

def silence(sec):
    return [0.0] * int(SR * sec)


def mix(dst, src, at=0.0, gain=1.0):
    o = int(SR * at)
    need = o + len(src)
    if need > len(dst):
        dst.extend([0.0] * (need - len(dst)))
    for i, v in enumerate(src):
        dst[o + i] += v * gain
    return dst


def env_exp(n, tau):
    return [math.exp(-i / (SR * tau)) for i in range(n)]


def noise(sec):
    return [rng.uniform(-1, 1) for _ in range(int(SR * sec))]


def lowpass(x, cutoff):
    a = math.exp(-2 * math.pi * cutoff / SR)
    y, p = [], 0.0
    for v in x:
        p = (1 - a) * v + a * p
        y.append(p)
    return y


def highpass(x, cutoff):
    lp = lowpass(x, cutoff)
    return [a - b for a, b in zip(x, lp)]


def bandpass(x, lo, hi):
    return lowpass(highpass(x, lo), hi)


def tone(sec, f0, f1=None, shape="sine", vib=0.0, vib_rate=5.0):
    f1 = f0 if f1 is None else f1
    n, ph, y = int(SR * sec), 0.0, []
    for i in range(n):
        t = i / n
        f = f0 + (f1 - f0) * t
        f *= 1 + vib * math.sin(2 * math.pi * vib_rate * i / SR)
        ph += 2 * math.pi * f / SR
        if shape == "saw":
            v = 2 * ((ph / (2 * math.pi)) % 1.0) - 1
        elif shape == "square":
            v = 1.0 if math.sin(ph) >= 0 else -1.0
        else:
            v = math.sin(ph)
        y.append(v)
    return y


def apply_env(x, env):
    return [a * b for a, b in zip(x, env)]


def fade(x, fin=0.01, fout=0.05):
    n, a, b = len(x), int(SR * fin), int(SR * fout)
    y = list(x)
    for i in range(min(a, n)):
        y[i] *= i / a
    for i in range(min(b, n)):
        y[n - 1 - i] *= i / b
    return y


def click(freq, dur=0.05, tau=0.008, noise_amt=0.6):
    n = int(SR * dur)
    ping = apply_env(tone(dur, freq), env_exp(n, tau))
    burst = apply_env(highpass(noise(dur), 1500), env_exp(n, 0.002))
    return [p + noise_amt * b for p, b in zip(ping, burst)]


def thump(freq=70, dur=0.25, tau=0.06):
    n = int(SR * dur)
    body = apply_env(tone(dur, freq * 1.6, freq), env_exp(n, tau))
    hit = apply_env(lowpass(noise(dur), 600), env_exp(n, 0.015))
    return [b + 0.5 * h for b, h in zip(body, hit)]


def creak(dur, f0, f1, jitter=0.25):
    # Ahşap gıcırtısı: titreyen frekanslı testere dalga, bant geçiren filtre
    n, ph, y = int(SR * dur), 0.0, []
    cur = f0
    for i in range(n):
        if i % 220 == 0:
            cur = (f0 + (f1 - f0) * i / n) * (1 + rng.uniform(-jitter, jitter))
        ph += 2 * math.pi * cur / SR
        y.append(2 * ((ph / (2 * math.pi)) % 1.0) - 1)
    y = bandpass(y, 250, 2200)
    am = [0.5 + 0.5 * math.sin(2 * math.pi * 9 * i / SR + math.sin(i / 900.0)) for i in range(n)]
    return fade(apply_env(y, am), 0.03, 0.12)


def write(name, x, peak=0.85):
    m = max(1e-6, max(abs(v) for v in x))
    x = [v / m * peak for v in x]
    path = os.path.join(OUT_DIR, name + ".wav")
    with wave.open(path, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(SR)
        w.writeframes(b"".join(struct.pack("<h", int(max(-1, min(1, v)) * 32767)) for v in x))
    return path


# ------------------------------------------------------------------ sesler

def flashlight_on():
    x = silence(0.2)
    mix(x, click(3200, 0.06, 0.006), 0.0)
    mix(x, click(4200, 0.04, 0.004, 0.3), 0.025, 0.6)
    return x


def flashlight_off():
    x = silence(0.18)
    mix(x, click(1500, 0.07, 0.01, 0.8), 0.0)
    return x


def flashlight_empty():
    # Pil biterken titreyen vızıltı, sonra sessizleşir
    dur = 0.8
    buzz = tone(dur, 120, 90, "square")
    buzz = lowpass(buzz, 900)
    n = len(buzz)
    gate, g = [], 1.0
    for i in range(n):
        if i % 600 == 0:
            g = 1.0 if rng.random() > 0.35 else 0.0
        gate.append(g * math.exp(-i / (SR * 0.35)))
    x = apply_env(buzz, gate)
    mix(x, click(900, 0.06, 0.01), dur - 0.06, 0.5)
    return fade(x, 0.005, 0.05)


def fuse_trip():
    # Kıvılcım çıtırtısı + ağır şalter düşmesi + sönen elektrik uğultusu
    x = silence(1.2)
    for _ in range(26):
        t = rng.uniform(0.0, 0.32)
        d = rng.uniform(0.004, 0.02)
        s = apply_env(highpass(noise(d), 2500), env_exp(int(SR * d), d / 3))
        mix(x, s, t, rng.uniform(0.4, 1.0))
    mix(x, thump(60, 0.35, 0.08), 0.3, 1.2)
    mix(x, click(700, 0.08, 0.02, 1.0), 0.3, 0.8)
    hum = apply_env(tone(0.8, 100, 60, "saw"), env_exp(int(SR * 0.8), 0.18))
    mix(x, lowpass(hum, 500), 0.33, 0.35)
    return fade(x, 0.002, 0.1)


def charger_undock():
    # Yuvadan sıyırıp çıkarma: yükselen sürtünme + çıt
    x = silence(0.35)
    slide = bandpass(noise(0.18), 800, 4000)
    slide = apply_env(slide, [math.sin(math.pi * i / len(slide)) for i in range(len(slide))])
    mix(x, slide, 0.0, 0.5)
    mix(x, click(2600, 0.05, 0.006), 0.17)
    return x


def drowned_warning():
    # Uzaktan gelen, aşağı kayan inilti ve nefes; çığlıktan farklı, daha boğuk ve tehditkâr
    dur = 2.6
    n = int(SR * dur)
    moan = [0.0] * n
    for h, g in [(1, 1.0), (2, 0.45), (3, 0.25), (5, 0.1)]:
        mix(moan, tone(dur, 118 * h, 82 * h, vib=0.012, vib_rate=4.3), 0.0, g)
    moan = lowpass(moan, 1100)
    shape = [math.sin(math.pi * i / n) ** 1.5 for i in range(n)]
    moan = apply_env(moan, shape)
    breath = apply_env(bandpass(noise(dur), 300, 1800), [0.35 * math.sin(math.pi * i / n) for i in range(n)])
    x = [a + b for a, b in zip(moan, breath)]
    return fade(x, 0.05, 0.3)


def drowned_leave():
    # Uzaklaşan su damlaları ve çekilen dalga
    dur = 2.2
    x = silence(dur)
    wash = apply_env(lowpass(noise(dur), 700), [math.exp(-i / (SR * 0.7)) for i in range(int(SR * dur))])
    mix(x, wash, 0.0, 0.6)
    t, g = 0.05, 1.0
    while t < dur - 0.2:
        d = 0.06
        drip = apply_env(tone(d, rng.uniform(1400, 2200), rng.uniform(500, 800)), env_exp(int(SR * d), 0.015))
        mix(x, drip, t, g)
        t += rng.uniform(0.12, 0.3)
        g *= 0.78
    return fade(x, 0.02, 0.3)


def drowned_return():
    # Derinden yükselen gürültü ve fısıltı: geri döndü
    dur = 2.4
    n = int(SR * dur)
    swell = [i / n for i in range(n)]
    rumble = apply_env(lowpass(noise(dur), 160), swell)
    sub = apply_env(tone(dur, 38, 46), swell)
    whisper = bandpass(noise(dur), 2000, 5000)
    wenv = [0.25 * (0.5 + 0.5 * math.sin(2 * math.pi * 3.1 * i / SR)) * swell[i] for i in range(n)]
    whisper = apply_env(whisper, wenv)
    x = [a * 1.2 + b * 0.8 + c for a, b, c in zip(rumble, sub, whisper)]
    return fade(x, 0.2, 0.08)


def runner_retreat():
    # Uzaklaşan koşu adımları
    x = silence(2.2)
    t, g = 0.0, 1.0
    while t < 1.9:
        mix(x, lowpass(thump(90, 0.12, 0.03), 900), t, g)
        t += 0.2
        g *= 0.82
    return fade(x, 0.005, 0.2)


def chair_sit():
    x = silence(0.8)
    mix(x, thump(85, 0.2, 0.05), 0.0, 0.9)
    mix(x, creak(0.6, 380, 300), 0.1, 0.7)
    return x


def chair_stand():
    # Sandalye geri itilirken sürtünme + gıcırtı
    x = silence(0.9)
    scrape = bandpass(noise(0.45), 400, 2500)
    am = [math.sin(math.pi * i / len(scrape)) * (0.6 + 0.4 * math.sin(2 * math.pi * 31 * i / SR)) for i in range(len(scrape))]
    mix(x, apply_env(scrape, am), 0.15, 0.7)
    mix(x, creak(0.5, 300, 470), 0.0, 0.6)
    return x


def player_death():
    # İki kalp atışı ve sönen boğuk uğultu
    x = silence(2.0)
    mix(x, thump(55, 0.25, 0.07), 0.0, 1.0)
    mix(x, thump(50, 0.25, 0.07), 0.28, 0.8)
    drone = apply_env(lowpass(tone(1.6, 70, 40, "saw"), 300), env_exp(int(SR * 1.6), 0.6))
    mix(x, drone, 0.35, 0.5)
    return fade(x, 0.005, 0.3)


SOUNDS = {
    "flashlight_on": flashlight_on,
    "flashlight_off": flashlight_off,
    "flashlight_empty": flashlight_empty,
    "fuse_trip": fuse_trip,
    "charger_undock": charger_undock,
    "drowned_warning": drowned_warning,
    "drowned_leave": drowned_leave,
    "drowned_return": drowned_return,
    "runner_retreat": runner_retreat,
    "chair_sit": chair_sit,
    "chair_stand": chair_stand,
    "player_death": player_death,
}

tasks = []
for name, fn in SOUNDS.items():
    path = write(name, fn())
    t = unreal.AssetImportTask()
    t.filename = path
    t.destination_path = DEST
    t.destination_name = name
    t.automated = True
    t.replace_existing = True
    t.save = True
    tasks.append(t)

unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)

lines = []
for name in SOUNDS:
    a = unreal.load_asset(DEST + "/" + name)
    lines.append("%s: %s (%.2fs)" % (name, "OK" if a else "MISSING", a.get_editor_property("duration") if a else 0))
with open(os.path.join(unreal.Paths.project_saved_dir(), "NLSounds.log"), "w", encoding="utf-8") as f:
    f.write("\n".join(lines))
