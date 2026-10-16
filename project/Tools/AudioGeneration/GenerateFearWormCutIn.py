"""Generate the short water-breach cut-in impact using only the Python standard library."""
import math
import pathlib
import random
import struct
import wave

kSampleRate = 44100
kDurationSeconds = 0.65


def Generate(outputPath):
    randomSource = random.Random(42)
    samples = bytearray()
    filteredNoise = 0.0
    phase = 0.0
    for sampleIndex in range(int(kSampleRate * kDurationSeconds)):
        elapsedSeconds = sampleIndex / kSampleRate
        progress = elapsedSeconds / kDurationSeconds
        filteredNoise = filteredNoise * 0.88 + randomSource.uniform(-1.0, 1.0) * 0.12
        frequency = 48.0 + 115.0 * math.exp(-elapsedSeconds * 15.0)
        phase += math.tau * frequency / kSampleRate
        attack = min(elapsedSeconds / 0.008, 1.0)
        tail = (1.0 - progress) ** 2
        impact = math.sin(phase) * math.exp(-elapsedSeconds * 10.0) * 0.65
        whoosh = filteredNoise * (0.75 + math.sin(math.pi * progress)) * tail
        electric = math.sin(math.tau * (1300.0 * elapsedSeconds - 550.0 * elapsedSeconds ** 2))
        electric *= math.exp(-elapsedSeconds * 22.0) * 0.08
        sample = math.tanh((impact + whoosh + electric) * attack) * 0.80
        samples.extend(struct.pack("<h", int(sample * 32767)))
    outputPath.parent.mkdir(parents=True, exist_ok=True)
    with wave.open(str(outputPath), "wb") as audioFile:
        audioFile.setnchannels(1)
        audioFile.setsampwidth(2)
        audioFile.setframerate(kSampleRate)
        audioFile.writeframes(samples)


if __name__ == "__main__":
    projectDirectory = pathlib.Path(__file__).resolve().parents[2]
    Generate(projectDirectory / "resources/Audio/GameSfx/fear_worm_cut_in.wav")
