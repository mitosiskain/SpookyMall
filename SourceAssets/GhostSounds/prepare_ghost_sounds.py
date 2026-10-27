"""Prepare the checked-in CC0 recordings for Unreal.

Run with Python 3.12+ and numpy/soundfile installed. The project-local tools used
for this pass are in Saved/GhostAudioTools (ignored by Git):
    python -m pip install --target Saved/GhostAudioTools numpy soundfile
    python SourceAssets/GhostSounds/prepare_ghost_sounds.py

Reads the original downloaded recordings, averages their channels to mono,
removes DC, bandlimits/resamples to 48 kHz, and adds short edge fades. No pitch
shift, generated vocal, added reverb, or added music. Level adjustment never
exceeds a -6 dBFS sample peak. All outputs are nonlooping PCM16 WAV files.
"""

from __future__ import annotations

import hashlib
import io
import json
import math
from pathlib import Path
import sys
import wave
import zipfile

ROOT = Path(__file__).resolve().parent
PROJECT = ROOT.parents[1]
LOCAL_TOOLS = PROJECT / "Saved/GhostAudioTools"
if LOCAL_TOOLS.is_dir():
    sys.path.insert(0, str(LOCAL_TOOLS))

import numpy as np
import soundfile as sf


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def bandlimit(samples: np.ndarray, rate: int) -> np.ndarray:
    """A symmetric 161-tap Hann-windowed sinc lowpass before resampling."""
    cutoff = min(10000.0, 0.44 * min(rate, 48000))
    offsets = np.arange(161, dtype=np.float64) - 80
    kernel = (2 * cutoff / rate) * np.sinc(2 * cutoff / rate * offsets)
    kernel *= np.hanning(len(kernel))
    kernel /= np.sum(kernel)
    return np.convolve(samples, kernel, mode="same")


def read_source(original: str, member: str | None):
    path = ROOT / "Originals" / original
    if member:
        with zipfile.ZipFile(path) as archive:
            return sf.read(io.BytesIO(archive.read(member)), always_2d=True)
    return sf.read(path, always_2d=True)


SOURCES = [
    {
        "id": "qubodup_ghost_breath",
        "title": "Ghost breath",
        "author": "qubodup",
        "page_url": "https://opengameart.org/content/ghost-breath",
        "download_url": "https://opengameart.org/sites/default/files/ghostbreath.flac",
        "original_file": "Originals/ghostbreath_qubodup.flac",
        "license": "CC0-1.0",
        "license_url": "https://creativecommons.org/publicdomain/zero/1.0/",
        "selected": True,
    },
    {
        "id": "qubodup_ghost_moans",
        "title": "Ghost Monster Voice Moaning & Growling",
        "author": "qubodup",
        "page_url": "https://opengameart.org/content/ghost-monster-voice-moaning-growling",
        "download_url": "https://opengameart.org/sites/default/files/qubodup-GhostMoans.zip",
        "original_file": "Originals/qubodup-GhostMoans.zip",
        "license": "CC0-1.0",
        "license_url": "https://creativecommons.org/publicdomain/zero/1.0/",
        "selected": True,
    },
    {
        "id": "primbal_horror_breathing_candidate",
        "title": "horror breathing",
        "author": "primbal",
        "page_url": "https://opengameart.org/content/horror-breathing",
        "download_url": "https://opengameart.org/sites/default/files/horrorbreathing.zip",
        "original_file": "Originals/horrorbreathing_primbal.zip",
        "license": "CC0-1.0",
        "license_url": "https://creativecommons.org/publicdomain/zero/1.0/",
        "selected": False,
        "selection_note": "Research candidate only; strongly overdriven source (stereo RMS about 0.46). Not imported into Unreal.",
    },
]

CLIPS = [
    ("SW_Ghost_InvisibleBreath.wav", "qubodup_ghost_breath", "ghostbreath_qubodup.flac", None, 0.085, 0.045, 0.12),
    ("SW_Ghost_InvisibleMoan.wav", "qubodup_ghost_moans", "qubodup-GhostMoans.zip", "qubodup-GhostMoans/wav/qubodup-GhostMoan01.wav", 0.095, 0.045, 0.12),
    ("SW_Ghost_HuntGrowl.wav", "qubodup_ghost_moans", "qubodup-GhostMoans.zip", "qubodup-GhostMoans/wav/qubodup-GhostMoan05.wav", 0.13, 0.025, 0.1),
]


def prepare() -> None:
    records = []
    for filename, source_id, original, member, target_rms, fade_in, fade_out in CLIPS:
        audio, source_rate = read_source(original, member)
        original_duration = len(audio) / source_rate
        samples = audio.mean(axis=1, dtype=np.float64)
        samples -= samples.mean()
        samples = bandlimit(samples, source_rate)
        count = round(len(samples) * 48000 / source_rate)
        samples = np.interp(np.arange(count) * source_rate / 48000, np.arange(len(samples)), samples)
        samples[:round(fade_in * 48000)] *= np.sin(np.linspace(0, math.pi / 2, round(fade_in * 48000))) ** 2
        samples[-round(fade_out * 48000):] *= np.cos(np.linspace(0, math.pi / 2, round(fade_out * 48000))) ** 2
        source_rms = float(np.sqrt(np.mean(samples ** 2)))
        source_peak = float(np.max(np.abs(samples)))
        gain = min(target_rms / max(source_rms, 1e-12), 0.5 / max(source_peak, 1e-12))
        pcm = np.rint(samples * gain * 32767).astype("<i2")
        output = ROOT / filename
        with wave.open(str(output), "wb") as handle:
            handle.setparams((1, 2, 48000, len(pcm), "NONE", "not compressed"))
            handle.writeframes(pcm.tobytes())
        restored = pcm.astype(np.float64) / 32768
        peak = float(np.max(np.abs(restored)))
        rms = float(np.sqrt(np.mean(restored ** 2)))
        records.append({
            "file": filename,
            "unreal_asset": "/Game/Audio/Ghost/Physical/" + filename.removesuffix(".wav"),
            "source_id": source_id,
            "archive_member": member,
            "sha256": sha256(output),
            "format": "WAV mono PCM16 48000 Hz",
            "duration_seconds": round(len(pcm) / 48000, 6),
            "source_duration_seconds": round(original_duration, 6),
            "source_rate_hz": source_rate,
            "source_channels": audio.shape[1],
            "source_trim_seconds": [0.0, round(original_duration, 6)],
            "peak_dbfs": round(20 * math.log10(peak), 3),
            "rms_dbfs": round(20 * math.log10(rms), 3),
            "digital_clipped_samples": int(np.count_nonzero(np.abs(pcm.astype(np.int32)) >= 32767)),
            "gain_db": round(20 * math.log10(gain), 3),
            "fade_in_seconds": fade_in,
            "fade_out_seconds": fade_out,
            "looping": False,
            "added_pitch_shift": False,
            "added_reverb": False,
        })
    for source in SOURCES:
        source["sha256"] = sha256(ROOT / source["original_file"])
    provenance = {
        "verified_date": "2026-10-17",
        "license_summary": "All original primary asset pages explicitly identify CC0. Commercial use and modification permitted; attribution not required. Voluntary credits preserved below.",
        "credits": ["Ghost breath and Ghost Monster Voice Moaning & Growling by qubodup, OpenGameArt.org, CC0 1.0."],
        "quality_note": "Selected using author descriptions and waveform/level checks. Not auditioned by the coding agent. Original ghost voices already contain artistic processing/tails. Mono preparation adds no reverb or pitch effect; subjective listening remains a PIE review item.",
        "preparation": "Deterministic channel averaging, DC removal, 161-tap lowpass, interpolation to 48 kHz, edge fades and RMS/peak-limited level adjustment. Full short source clips retained; no audio synthesized.",
        "sources": SOURCES,
        "clips": records,
    }
    (ROOT / "provenance.json").write_text(json.dumps(provenance, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(records, indent=2))


if __name__ == "__main__":
    prepare()
