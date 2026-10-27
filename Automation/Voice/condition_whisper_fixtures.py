"""Deterministic offline ASR regressions; synthetic audio, never the user's mic."""
from pathlib import Path
import array
import json
import math
import random
import sys
import wave

ROOT = Path(__file__).resolve().parent / 'Fixtures' / 'Whisper'


def load(name):
    with wave.open(str(ROOT / name), 'rb') as source:
        assert source.getparams()[:3] == (1, 2, 16000)
        samples = array.array('h', source.readframes(source.getnframes()))
        if sys.byteorder != 'little':
            samples.byteswap()
        return samples.tolist()


def write(name, samples):
    data = array.array('h', (max(-32768, min(32767, round(x))) for x in samples))
    if sys.byteorder != 'little':
        data.byteswap()
    with wave.open(str(ROOT / name), 'wb') as output:
        output.setparams((1, 2, 16000, len(data), 'NONE', 'not compressed'))
        output.writeframes(data.tobytes())


samples = load('where_0.wav')
rms = math.sqrt(sum(x*x for x in samples) / len(samples))
rng = random.Random(6841)
# SNR 15 dB, fixed gain reduction, and short pre/post quiet intervals.
noise_rms = rms * 0.35 / 10**(15/20)
write('where_noisy.wav', [0]*4000 + [x*0.35+rng.gauss(0, noise_rms) for x in samples] + [0]*4000)
write('silence.wav', [0]*32000)
write('noise.wav', [rng.gauss(0, 300) for _ in range(32000)])
write('pop.wav', [16000 if 15000 <= n < 15016 else 0 for n in range(32000)])
(ROOT / 'conditioning.json').write_text(json.dumps({
    'source': 'where_0.wav', 'noisy_snr_db': 15, 'speech_gain': 0.35,
    'seed': 6841, 'synthetic_only': True,
    'note': 'Real speech through the player microphone remains a manual acceptance test.'
}, indent=2) + '\n', encoding='utf-8')
print('Created noisy speech, silence, random noise and an isolated pop.')
