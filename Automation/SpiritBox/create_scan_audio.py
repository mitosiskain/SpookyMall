"""Generate original, seamless radio static with no recordings or dependencies.

The bed is deliberately free of speech, tones, and abrupt scan gating. Circular
noise and periodic filter motion let the 12-second sound repeat without a fade.
The actor supplies the final quiet scan gain and mutes this bed during a reply.
"""
from pathlib import Path
import array
import hashlib
import json
import math
import random
import sys
import wave


ROOT = Path(__file__).resolve().parents[2] / "SourceAssets" / "SpiritBox"
RATE = 48000
DURATION = 12
SEED = 20261017
TARGET_RMS = 0.18
MAX_PEAK = 0.86


def generate_static():
    count = RATE * DURATION
    rng = random.Random(SEED)
    noise = array.array("f", (rng.uniform(-1.0, 1.0) for _ in range(count)))
    crackles = array.array("f", [0.0]) * count
    # A few quiet, short radio crackles. Wrap bursts so the source is circular.
    for _ in range(23):
        start = rng.randrange(count)
        frames = round(rng.uniform(0.002, 0.010) * RATE)
        strength = rng.uniform(0.045, 0.10)
        for offset in range(frames):
            envelope = math.sin(math.pi * offset / frames) ** 2
            crackles[(start + offset) % count] += (
                strength * envelope * rng.uniform(-1.0, 1.0)
            )

    low70 = low160 = body = hiss = air = 0.0
    alpha70 = 1.0 - math.exp(-2.0 * math.pi * 70.0 / RATE)
    alpha160 = 1.0 - math.exp(-2.0 * math.pi * 160.0 / RATE)
    alpha900 = 1.0 - math.exp(-2.0 * math.pi * 900.0 / RATE)
    alpha6000 = 1.0 - math.exp(-2.0 * math.pi * 6000.0 / RATE)
    samples = array.array("f")
    # The first circular pass settles the filter state. The second is retained.
    # Even the slowest filter forgets its initial state well before one loop.
    for take in range(2):
        for i, white in enumerate(noise):
            phase = 2.0 * math.pi * i / count
            cutoff = 2500.0 + 850.0 * math.sin(phase + 0.7) + 450.0 * math.sin(3.0 * phase + 2.1)
            alpha_sweep = 1.0 - math.exp(-2.0 * math.pi * cutoff / RATE)
            low70 += alpha70 * (white - low70)
            low160 += alpha160 * (white - low160)
            body += alpha900 * ((white - low70) - body)
            hiss += alpha_sweep * ((white - low160) - hiss)
            air += alpha6000 * (hiss - air)
            # Slow, irregular changes in hiss density rather than a fixed chop.
            envelope = 0.90 + 0.055 * math.sin(3.0 * phase + 1.3) + 0.035 * math.sin(7.0 * phase + 0.2)
            if take:
                samples.append((0.78 * air + 0.30 * body) * envelope + crackles[i])

    rms = math.sqrt(sum(value * value for value in samples) / count)
    peak = max(abs(value) for value in samples)
    gain = min(TARGET_RMS / rms, MAX_PEAK / peak)
    pcm = array.array("h", (round(value * gain * 32767.0) for value in samples))
    # Pick a naturally smooth point as the loop boundary. This neither inserts
    # silence nor crossfades away the scan texture near the ends of the file.
    seam = min(
        range(count),
        key=lambda i: abs(pcm[i] - pcm[i - 1])
        + abs((pcm[(i + 1) % count] - pcm[i]) - (pcm[i - 1] - pcm[i - 2])),
    )
    pcm = pcm[seam:] + pcm[:seam]
    return pcm


def main():
    ROOT.mkdir(parents=True, exist_ok=True)
    pcm = generate_static()
    path = ROOT / "SW_SpiritBox_Scan.wav"
    payload = array.array("h", pcm)
    if sys.byteorder != "little":
        payload.byteswap()
    with wave.open(str(path), "wb") as output:
        output.setnchannels(1)
        output.setsampwidth(2)
        output.setframerate(RATE)
        output.writeframes(payload.tobytes())

    rms = math.sqrt(sum(value * value for value in pcm) / len(pcm)) / 32768.0
    peak = max(abs(value) for value in pcm) / 32768.0
    adjacent_rms = math.sqrt(sum((pcm[i] - pcm[i - 1]) ** 2 for i in range(len(pcm))) / len(pcm)) / 32768.0
    metrics = {
        "rms": round(rms, 6),
        "rms_dbfs": round(20.0 * math.log10(rms), 2),
        "peak": round(peak, 6),
        "peak_dbfs": round(20.0 * math.log10(peak), 2),
        "clipped_samples": sum(abs(value) >= 32767 for value in pcm),
        "loop_seam_delta": round(abs(pcm[0] - pcm[-1]) / 32768.0, 6),
        "adjacent_delta_rms": round(adjacent_rms, 6),
    }
    provenance_path = ROOT / "provenance.json"
    provenance = json.loads(provenance_path.read_text(encoding="utf-8"))
    provenance["scan"] = {
        "file": path.name,
        "origin": "Original deterministic synthesized radio static; no sampled broadcasts, voices, or third-party recordings",
        "description": "Soft band-limited hiss with slow filter sweeps and subtle irregular crackles; seamless loop with no tonal hum or gated chopping",
        "sample_rate": RATE,
        "channels": 1,
        "bits_per_sample": 16,
        "duration_seconds": DURATION,
        "looping": True,
        "seed": SEED,
        "generator": "Automation/SpiritBox/create_scan_audio.py",
        "generator_revision": 2,
        "quality_checks": metrics,
    }
    integrity = {"file": path.name, "sha256": hashlib.sha256(path.read_bytes()).hexdigest(), "bytes": path.stat().st_size}
    provenance["integrity"] = [entry for entry in provenance["integrity"] if entry["file"] != path.name] + [integrity]
    provenance_path.write_text(json.dumps(provenance, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({"file": str(path), "duration_seconds": DURATION, **metrics, **integrity}, indent=2))


if __name__ == "__main__":
    main()
