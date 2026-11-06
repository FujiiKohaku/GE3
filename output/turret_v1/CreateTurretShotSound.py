from pathlib import Path
import math
import random
import struct
import wave

kSampleRate = 44100
kDurationSeconds = 0.32
kOutputPath = Path(__file__).resolve().parents[2] / 'project/resources/Audio/GameSfx/turret_shot.wav'
noiseGenerator = random.Random(724)
samples = []
phase = 0.0
filteredNoise = 0.0
for sampleIndex in range(round(kSampleRate * kDurationSeconds)):
    timeSeconds = sampleIndex / kSampleRate
    frequency = 58.0 + 150.0 * math.exp(-timeSeconds * 38.0)
    phase += math.tau * frequency / kSampleRate
    noise = noiseGenerator.uniform(-1.0, 1.0)
    filteredNoise = filteredNoise * 0.83 + noise * 0.17
    crack = noise * math.exp(-timeSeconds * 105.0) * 0.43
    thump = math.sin(phase) * math.exp(-timeSeconds * 19.0) * 0.75
    body = filteredNoise * math.exp(-timeSeconds * 16.0) * 0.68
    attack = min(1.0, timeSeconds / 0.0015)
    fade = min(1.0, (kDurationSeconds - timeSeconds) / 0.035)
    value = math.tanh((crack + thump + body) * 1.4) * 0.78 * attack * fade
    samples.append(struct.pack('<h', round(value * 32767)))
with wave.open(str(kOutputPath), 'wb') as outputFile:
    outputFile.setnchannels(1)
    outputFile.setsampwidth(2)
    outputFile.setframerate(kSampleRate)
    outputFile.writeframes(b''.join(samples))
print('Created', kOutputPath)
