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


def door_open():
    # Kol sesi + menteşe gıcırtısı
    x = silence(1.0)
    mix(x, click(1800, 0.06, 0.01, 0.5), 0.0, 0.7)
    mix(x, creak(0.8, 260, 420, 0.3), 0.08, 0.8)
    return x


def door_close():
    x = silence(0.9)
    mix(x, creak(0.35, 420, 300, 0.3), 0.0, 0.5)
    mix(x, thump(75, 0.3, 0.07), 0.32, 1.0)
    mix(x, click(2200, 0.05, 0.006, 0.6), 0.36, 0.6)
    return x


def door_bang():
    # Dışarıdan kapıya yüklenme: ağır darbe + kolun takırtısı + menteşe zorlanması
    x = silence(1.1)
    mix(x, thump(60, 0.4, 0.09), 0.0, 1.0)
    mix(x, lowpass(noise(0.25), 1200), 0.0, 0.25)
    for k in range(4):
        mix(x, click(1500 + 300 * k, 0.04, 0.006, 0.7), 0.05 + 0.09 * k, 0.5 - 0.08 * k)
    mix(x, creak(0.5, 200, 160, 0.4), 0.1, 0.45)
    return fade(x, 0.002, 0.2)


def door_burst():
    # Kapı zorla açılır: kırılma + duvara çarpma
    x = silence(1.4)
    mix(x, thump(50, 0.5, 0.12), 0.0, 1.0)
    mix(x, bandpass(noise(0.35), 300, 4000), 0.0, 0.6)
    mix(x, creak(0.25, 500, 900, 0.5), 0.02, 0.6)
    mix(x, thump(70, 0.35, 0.08), 0.28, 0.8)
    return fade(x, 0.002, 0.3)


def door_handle():
    # Kapı kolu aşağı-yukarı takırdar
    x = silence(0.5)
    for k in range(3):
        mix(x, click(2400 - 200 * k, 0.05, 0.005, 0.8), 0.0 + 0.12 * k, 0.8)
    return x


def breath_exhausted_loop():
    # Tükenmiş, derin ve hırıltılı nefes: her 1.1 sn'de bir sesli içe çekiş + ağır veriş (döngü, 4 nefes)
    per = 1.1
    cycles = 4
    n = int(SR * per * cycles)
    base = noise(per * cycles)
    inhale = bandpass(base, 1100, 4200)
    exhale = bandpass(base, 250, 1300)
    rasp = bandpass(noise(per * cycles), 90, 400)
    x = [0.0] * n
    pn = int(SR * per)
    for c in range(cycles):
        o = c * pn
        g = 0.9 + 0.1 * math.sin(c * 2.3)
        for i in range(pn):
            t = i / pn
            if t < 0.38:
                # Hızla içe çekiş (gasp): hızlı yükselen, sonda kesilen
                e = (t / 0.38) ** 0.6 * (1 - ((t / 0.38) ** 6))
                v = inhale[o + i] * e * 0.9 + rasp[o + i] * e * 0.5
            elif t < 0.46:
                v = 0.0
            else:
                u = (t - 0.46) / 0.54
                e = math.sin(math.pi * u) ** 1.1 * (1 - u * 0.3)
                v = exhale[o + i] * e * 1.1 + rasp[o + i] * e * 0.35
            x[o + i] = v * g
    return x


def breath_panic_loop():
    # Hızlı, panik halinde nefes alıp verme (~2.1 nefes/sn), döngü
    dur = 2.4
    n = int(SR * dur)
    base = noise(dur)
    inhale = bandpass(base, 900, 3800)
    exhale = bandpass(base, 300, 1600)
    x = [0.0] * n
    cycles = 5
    per = n // cycles
    for c in range(cycles):
        o = c * per
        for i in range(per):
            t = i / per
            if t < 0.42:
                e = math.sin(math.pi * t / 0.42) ** 1.5 * 0.55
                v = inhale[o + i] * e
            elif t < 0.5:
                v = 0.0
            else:
                e = math.sin(math.pi * (t - 0.5) / 0.5) ** 1.2
                v = exhale[o + i] * e
            x[o + i] = v * (0.85 + 0.15 * math.sin(c * 1.7))
    return x




# ------------------------------------------------------------------ ortam (atmosfer)

def _slow_curve(n, rate_hz, lo, hi, seed):
    # Yavaş, rastgele dalgalanan zarf (rüzgâr şiddeti gibi)
    r = random.Random(seed)
    step = int(SR / max(rate_hz, 0.01))
    pts = [r.uniform(lo, hi) for _ in range(n // step + 3)]
    pts[-1] = pts[0]
    out = []
    for i in range(n):
        k, f = divmod(i, step)
        t = f / step
        t = t * t * (3 - 2 * t)
        out.append(pts[k] + (pts[k + 1] - pts[k]) * t)
    return out


def _loopable(x, xf=1.0):
    # Sonu başa yumuşakça bindirir: döngü dikişsiz olur
    m = int(SR * xf)
    body = x[:-m]
    tail = x[-m:]
    for i in range(m):
        t = i / m
        body[i] = body[i] * t + tail[i] * (1 - t)
    return body


def amb_wind_gust_loop():
    # Sert rüzgâr: uğultu + ıslık bandı, şiddeti yavaşça kabarıp söner
    dur = 24.0
    n = int(SR * dur)
    base = lowpass(lowpass(noise(dur), 380), 380)
    whistle = bandpass(noise(dur), 700, 1300)
    gust = _slow_curve(n, 0.18, 0.15, 1.0, 11)
    y = [b * (0.4 + 0.9 * g) + w * 0.35 * g * g for b, w, g in zip(base, whistle, gust)]
    return _loopable(y, 1.5)


def amb_waves_loop():
    # Kıyıya vuran dalgalar: her ~5 sn bir kabarma + çekilirken hışırtı
    dur = 16.0
    n = int(SR * dur)
    rumble = lowpass(noise(dur), 250)
    hiss = bandpass(noise(dur), 1500, 5000)
    y = [0.0] * n
    starts = [0.0, 5.3, 10.4]
    for s in starts:
        o = int(SR * s)
        for i in range(int(SR * 6.0)):
            j = (o + i) % n
            t = i / SR
            swell = (t / 1.4) ** 2 if t < 1.4 else math.exp(-(t - 1.4) / 1.1)
            wash = math.exp(-((t - 2.2) / 1.6) ** 2)
            y[j] += rumble[j] * swell * 1.2 + hiss[j] * wash * 0.35
    return _loopable(y, 1.0)


def amb_interior_hum_loop():
    # Kapalı oda: alçak elektrik vızıltısı + hafif hava sesi
    dur = 10.0
    hum = [0.6 * a + 0.25 * b + 0.12 * c for a, b, c in zip(tone(dur, 50), tone(dur, 100), tone(dur, 150))]
    air = lowpass(noise(dur), 220)
    wob = _slow_curve(int(SR * dur), 0.4, 0.8, 1.0, 21)
    y = [(h * 0.25 + a * 0.9) * w for h, a, w in zip(hum, air, wob)]
    return _loopable(y, 0.8)


def dist_branch_snap():
    # Yakınlarda kırılan dal: keskin çatırtı + birkaç küçük kırılma
    y = silence(1.2)
    crack = apply_env(bandpass(noise(0.12), 900, 6000), env_exp(int(SR * 0.12), 0.012))
    mix(y, crack, 0.05, 1.0)
    for k in range(5):
        mix(y, click(rng.uniform(900, 2500), 0.04, 0.006, 0.9), 0.1 + k * rng.uniform(0.03, 0.09), rng.uniform(0.2, 0.5))
    mix(y, thump(110, 0.3, 0.05), 0.06, 0.4)
    return fade(y, 0.002, 0.2)


def dist_metal_clank():
    # Uzakta düşen metal: inharmonik çınlama, uzun sönüm
    dur = 3.0
    n = int(SR * dur)
    y = [0.0] * n
    for f, a, tau in [(412, 1.0, 0.9), (1033, 0.6, 0.6), (1777, 0.45, 0.4), (2620, 0.3, 0.25), (3391, 0.2, 0.18)]:
        y = [v + s for v, s in zip(y, apply_env(tone(dur, f), [a * e for e in env_exp(n, tau)]))]
    hit = apply_env(highpass(noise(0.05), 2000), env_exp(int(SR * 0.05), 0.004))
    mix(y, hit, 0.0, 0.8)
    mix(y, [v * 0.5 for v in y[:int(SR * 1.5)]], 0.42, 0.35)   # ikinci sekme
    return fade(y, 0.001, 0.4)


def dist_footsteps():
    # Uzakta çakıl/toprakta birkaç hızlı adım, sonra durur
    y = silence(3.2)
    t = 0.1
    for k in range(rng.randint(5, 7)):
        step = apply_env(bandpass(noise(0.16), 300, 3500), env_exp(int(SR * 0.16), 0.035))
        mix(y, step, t, rng.uniform(0.6, 1.0))
        mix(y, thump(60, 0.15, 0.03), t, 0.3)
        t += rng.uniform(0.32, 0.42)
    return fade(y, 0.002, 0.3)


def dist_whisper():
    # Anlaşılmayan fısıltı: nefesli gürültü, hece gibi açılıp kapanan formantlar
    dur = 2.6
    n = int(SR * dur)
    br = bandpass(noise(dur), 1200, 6000)
    y = []
    for i, v in enumerate(br):
        t = i / SR
        syl = max(0.0, math.sin(2 * math.pi * 3.1 * t + math.sin(t * 5.0))) ** 2
        env = math.sin(math.pi * min(t / dur, 1.0))
        y.append(v * syl * env)
    formant = bandpass(y, 1800, 3200)
    y = [a * 0.6 + b * 0.8 for a, b in zip(y, formant)]
    return fade(y, 0.05, 0.3)


def dist_moan():
    # Uzaktan, boğuk bir inleme (insan mı değil mi belli değil)
    dur = 3.4
    n = int(SR * dur)
    v = tone(dur, 140, 95, shape="saw", vib=0.03, vib_rate=4.2)
    v = lowpass(lowpass(v, 700), 700)
    env = [math.sin(math.pi * (i / n)) ** 1.5 for i in range(n)]
    breath = lowpass(noise(dur), 900)
    y = [a * e + b * e * 0.2 for a, b, e in zip(v, breath, env)]
    return fade(y, 0.1, 0.4)


def dist_tree_creak():
    # Rüzgârda gıcırdayan ağaç gövdesi
    y = silence(2.6)
    mix(y, creak(1.6, 120, 85, 0.15), 0.1, 1.0)
    mix(y, creak(0.7, 95, 140, 0.2), 1.6, 0.6)
    return fade(lowpass(y, 1500), 0.05, 0.3)


def dist_foghorn():
    # Çok uzakta bir gemi düdüğü / sis düdüğü
    dur = 5.0
    n = int(SR * dur)
    v = [a + 0.6 * b for a, b in zip(tone(dur, 87, shape="saw"), tone(dur, 87.6, shape="saw"))]
    v = lowpass(lowpass(v, 320), 320)
    env = [min(1.0, (i / n) / 0.15) * min(1.0, (1 - i / n) / 0.35) for i in range(n)]
    y = [a * e for a, e in zip(v, env)]
    mix(y, [s * 0.3 for s in y[:int(SR * 3.5)]], 0.9, 0.5)   # yankı
    return fade(y, 0.2, 0.8)


def dist_knock():
    # Ahşaba üç vuruş
    y = silence(1.6)
    for k, t in enumerate([0.05, 0.38, 0.71]):
        mix(y, thump(120, 0.22, 0.04), t, 0.9)
        mix(y, click(700, 0.05, 0.01, 0.4), t, 0.4)
    return fade(lowpass(y, 2500), 0.002, 0.2)



def amb_glass_wind_loop():
    # Fenerin içi: camlara bastıran boğuk rüzgâr, aralıklardan ince ıslık, arada camda tıkırtı
    dur = 20.0
    n = int(SR * dur)
    press = lowpass(lowpass(noise(dur), 260), 260)
    gust = _slow_curve(n, 0.22, 0.2, 1.0, 31)
    whistle = tone(dur, 1150, 1150, vib=0.035, vib_rate=0.7)
    wgate = _slow_curve(n, 0.3, 0.0, 1.0, 37)
    y = [p * (0.5 + 0.8 * g) + w * 0.05 * max(0.0, wg - 0.45) * g for p, g, w, wg in zip(press, gust, whistle, wgate)]
    r = random.Random(41)
    for _ in range(26):
        at = r.uniform(0.2, dur - 0.3)
        mix(y, click(r.uniform(2800, 5200), 0.03, 0.004, 0.8), at, r.uniform(0.04, 0.12))
    return _loopable(y, 1.5)


def glass_gust_hit():
    # Cama çarpan sert rüzgâr: boğuk "vump", ardından camın/çerçevenin titreşip tıkırdaması
    y = silence(1.8)
    n = int(SR * 0.9)
    whump = lowpass(lowpass(noise(0.9), 200), 200)
    env = [((i / n) / 0.25) ** 2 if i / n < 0.25 else math.exp(-((i / n) - 0.25) / 0.2) for i in range(n)]
    mix(y, apply_env(whump, env), 0.0, 1.0)
    t = 0.18
    amp = 0.55
    while t < 1.3:
        mix(y, click(rng.uniform(3000, 6000), 0.03, 0.003, 0.9), t, amp)
        mix(y, click(rng.uniform(600, 900), 0.04, 0.01, 0.3), t + 0.004, amp * 0.4)
        t += rng.uniform(0.025, 0.06)
        amp *= 0.93
    mix(y, creak(0.5, 160, 120, 0.2), 0.25, 0.25)
    return fade(y, 0.01, 0.3)


def power_down():
    # Elektrik kesildi (binanın içinden): ağır röle "klank", lambaların uğultusu pesleşip söner, son bir çıtırtı
    x = silence(2.2)
    mix(x, thump(55, 0.4, 0.09), 0.0, 1.3)
    mix(x, click(500, 0.07, 0.02, 1.0), 0.0, 0.7)
    n = int(SR * 1.8)
    hum = apply_env(tone(1.8, 100, 28, "saw"), env_exp(n, 0.55))
    mix(x, lowpass(hum, 700), 0.04, 0.55)
    buzz = apply_env(tone(1.2, 120, 50, "square"), env_exp(int(SR * 1.2), 0.25))
    mix(x, bandpass(buzz, 200, 1600), 0.04, 0.12)
    for t in (0.35, 0.9, 1.5):
        mix(x, click(rng.uniform(1500, 2600), 0.02, 0.004, 0.8), t, 0.25)
    return fade(x, 0.002, 0.3)


def power_up():
    # Elektrik geldi: röle "klak", uğultu yükselip oturur, lambalar tıkırdayarak yanar
    x = silence(1.6)
    mix(x, thump(70, 0.3, 0.06), 0.0, 1.0)
    mix(x, click(800, 0.06, 0.015, 1.0), 0.0, 0.7)
    up = tone(1.4, 35, 100, "saw")
    env = [min(1.0, i / (SR * 0.6)) * (1.0 - max(0.0, (i - SR * 0.9) / (SR * 0.5))) for i in range(len(up))]
    mix(x, lowpass(apply_env(up, env), 700), 0.03, 0.5)
    for t in (0.25, 0.32, 0.5, 0.56):
        mix(x, click(rng.uniform(1800, 3000), 0.02, 0.004, 0.7), t, 0.3)
    return fade(x, 0.002, 0.2)


# ------------------------------------------------------------------ hava (yağmur, gök gürültüsü)

def _mix_wrap(dst, src, at, gain):
    # Döngü sesleri için: sona taşan kısım başa sarılır (dikiş olmaz)
    n = len(dst)
    o = int(SR * at)
    for i, v in enumerate(src):
        dst[(o + i) % n] += v * gain


def amb_rain_loop():
    # Dışarıda sağanak: geniş hışırtı + gövde + yere/yapraklara düşen sık damlacıklar
    dur = 20.0
    n = int(SR * dur)
    hiss = bandpass(noise(dur), 1200, 7000)
    body = lowpass(lowpass(noise(dur), 900), 900)
    swell = _slow_curve(n, 0.25, 0.8, 1.0, 31)
    y = [(h * 0.55 + b * 1.6) * s for h, b, s in zip(hiss, body, swell)]
    t = 0.0
    while t < dur:
        d = click(rng.uniform(2500, 7000), 0.015, 0.0018, 0.8)
        _mix_wrap(y, d, t, rng.uniform(0.04, 0.22))
        t += rng.expovariate(140.0)
    return y


def amb_rain_roof_loop():
    # Bina içi: yağmurun çatıya/sacına vuruşu - boğuk tıkırtılar + alçak uğultu
    dur = 16.0
    n = int(SR * dur)
    rumble = lowpass(lowpass(noise(dur), 300), 300)
    swell = _slow_curve(n, 0.3, 0.75, 1.0, 41)
    y = [r * 2.2 * s for r, s in zip(rumble, swell)]
    t = 0.0
    while t < dur:
        d = lowpass(click(rng.uniform(250, 900), 0.03, 0.006, 0.5), 1800)
        _mix_wrap(y, d, t, rng.uniform(0.05, 0.25))
        t += rng.expovariate(90.0)
    # Arada oluktan damlayan tek tük iri damlalar
    t = 0.7
    while t < dur:
        _mix_wrap(y, click(rng.uniform(900, 1400), 0.06, 0.015, 0.2), t, 0.35)
        t += rng.uniform(0.6, 1.8)
    return y


def _thunder(dur, bumps, crack, seed):
    r = random.Random(seed)
    n = int(SR * dur)
    rumble = lowpass(lowpass(noise(dur), 140), 140)
    grit = bandpass(noise(dur), 180, 900)
    env = [0.0] * n
    for (c, w, a) in bumps:
        for i in range(n):
            t = i / SR
            env[i] += a * math.exp(-((t - c) / w) ** 2)
    tail = [math.exp(-max(0.0, i / SR - bumps[-1][0]) / 1.6) for i in range(n)]
    flick = _slow_curve(n, 9.0, 0.55, 1.0, seed + 1)
    y = [(ru * 3.0 + g * 0.35 * f) * e * tl for ru, g, e, tl, f in zip(rumble, grit, env, tail, flick)]
    if crack:
        # Yakın şimşek: keskin çatırtı, sonra yırtılan gökyüzü
        cr = noise(1.2)
        cr = [v * math.exp(-i / (SR * 0.09)) for i, v in enumerate(highpass(cr, 600))]
        mix(y, cr, 0.0, 1.4)
        rip = bandpass(noise(1.4), 300, 2500)
        am = [abs(math.sin(i / SR * r.uniform(28, 40) * math.pi)) * math.exp(-i / (SR * 0.45)) for i in range(len(rip))]
        mix(y, apply_env(rip, am), 0.05, 0.7)
    return fade(y, 0.002 if crack else 0.3, 1.2)


def thunder_near():
    return _thunder(7.0, [(0.25, 0.35, 1.0), (1.0, 0.6, 0.8), (2.3, 0.9, 0.55), (3.8, 1.0, 0.35)], True, 51)


def thunder_far():
    return _thunder(8.0, [(0.9, 0.7, 0.7), (2.2, 0.9, 1.0), (3.9, 1.1, 0.6), (5.2, 1.0, 0.3)], False, 61)


def thunder_far2():
    return _thunder(7.0, [(0.6, 0.5, 0.9), (1.6, 0.7, 0.6), (3.0, 1.2, 0.8)], False, 71)


# ------------------------------------------------------------------ açılış: bot

def boat_engine_loop():
    # Küçük dıştan takma motor: hızlı "pat-pat" vuruşlar + egzoz hırıltısı + suyun çağıltısı
    dur = 6.0
    n = int(SR * dur)
    y = [0.0] * n
    rate = 14.0   # vuruş/sn
    t = 0.0
    while t < dur:
        _mix_wrap(y, thump(rng.uniform(55, 70), 0.09, 0.025), t, rng.uniform(0.7, 1.0))
        t += 1.0 / rate * rng.uniform(0.93, 1.07)
    rasp = bandpass(noise(dur), 300, 1400)
    am = [0.5 + 0.5 * math.sin(2 * math.pi * rate * i / SR) for i in range(n)]
    water = bandpass(noise(dur), 500, 3000)
    slosh = _slow_curve(n, 1.2, 0.4, 1.0, 81)
    y = [a * 0.9 + r * m * 0.3 + w * s * 0.18 for a, r, m, w, s in zip(y, rasp, am, water, slosh)]
    return _loopable(y, 0.5)


def boat_engine_stop():
    # Gaz kesilir: vuruşlar seyrelir, son bir öksürük, sessizlik
    y = silence(2.0)
    t, gap = 0.0, 1.0 / 13.0
    while t < 1.3:
        mix(y, thump(rng.uniform(50, 65), 0.1, 0.03), t, max(0.15, 1.0 - t * 0.6))
        t += gap
        gap *= 1.18
    mix(y, lowpass(noise(0.25), 900), 1.35, 0.25)
    return fade(y, 0.002, 0.3)


def boat_dock_bump():
    # Bot ahşap iskeleye yaslanır: boğuk tok darbe, tahta gıcırtısı, suyun şapırtısı
    y = silence(2.2)
    mix(y, thump(48, 0.5, 0.11), 0.0, 1.3)
    mix(y, click(380, 0.08, 0.025, 0.6), 0.0, 0.6)
    mix(y, creak(0.9, 140, 95, 0.3), 0.15, 0.45)
    mix(y, apply_env(bandpass(noise(1.2), 400, 2500), env_exp(int(SR * 1.2), 0.35)), 0.05, 0.3)
    return fade(y, 0.002, 0.3)


def boat_step_off():
    # Islak tahtaya ağır adım + iskelenin hafif gıcırtısı
    y = silence(1.0)
    mix(y, thump(70, 0.25, 0.04), 0.0, 1.0)
    mix(y, apply_env(bandpass(noise(0.3), 800, 4000), env_exp(int(SR * 0.3), 0.04)), 0.0, 0.4)
    mix(y, creak(0.5, 180, 130, 0.25), 0.08, 0.25)
    return fade(y, 0.002, 0.2)


SOUNDS = {
    "boat_engine_loop": boat_engine_loop,
    "boat_engine_stop": boat_engine_stop,
    "boat_dock_bump": boat_dock_bump,
    "boat_step_off": boat_step_off,
    "amb_rain_loop": amb_rain_loop,
    "amb_rain_roof_loop": amb_rain_roof_loop,
    "thunder_near": thunder_near,
    "thunder_far": thunder_far,
    "thunder_far2": thunder_far2,
    "power_down": power_down,
    "power_up": power_up,
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
    "door_open": door_open,
    "door_close": door_close,
    "door_bang": door_bang,
    "door_burst": door_burst,
    "door_handle": door_handle,
    "breath_panic_loop": breath_panic_loop,
    "breath_exhausted_loop": breath_exhausted_loop,
    "amb_wind_gust_loop": amb_wind_gust_loop,
    "amb_waves_loop": amb_waves_loop,
    "amb_interior_hum_loop": amb_interior_hum_loop,
    "dist_branch_snap": dist_branch_snap,
    "dist_metal_clank": dist_metal_clank,
    "dist_footsteps": dist_footsteps,
    "dist_whisper": dist_whisper,
    "dist_moan": dist_moan,
    "dist_tree_creak": dist_tree_creak,
    "dist_foghorn": dist_foghorn,
    "dist_knock": dist_knock,
    "amb_glass_wind_loop": amb_glass_wind_loop,
    "glass_gust_hit": glass_gust_hit,
}

# Sadece bazı sesleri üretmek için: NL_SOUNDS=ad1,ad2 (boşsa hepsi). Diğer assetler yeniden içe aktarılmaz.
_only = [s.strip() for s in os.environ.get("NL_SOUNDS", "").split(",") if s.strip()]
if _only:
    SOUNDS = {k: v for k, v in SOUNDS.items() if k in _only}

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
    if a and name.endswith("_loop"):
        a.set_editor_property("looping", True)
        unreal.EditorAssetLibrary.save_loaded_asset(a)
    lines.append("%s: %s (%.2fs)" % (name, "OK" if a else "MISSING", a.get_editor_property("duration") if a else 0))
with open(os.path.join(unreal.Paths.project_saved_dir(), "NLSounds.log"), "w", encoding="utf-8") as f:
    f.write("\n".join(lines))
