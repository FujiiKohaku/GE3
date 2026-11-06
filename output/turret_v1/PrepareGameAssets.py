from pathlib import Path
import math
import shutil
import struct
import zlib

kSourceDirectory = Path(__file__).resolve().parent / 'models'
kTargetDirectory = Path(__file__).resolve().parents[2] / 'project/resources/Models/Enemy/Turret'
kTargetDirectory.mkdir(parents=True, exist_ok=True)


def CreatePngChunk(chunkType, data):
    return struct.pack('>I', len(data)) + chunkType + data + struct.pack('>I', zlib.crc32(chunkType + data) & 0xffffffff)


def ToSrgb(value):
    if value <= 0.0031308:
        return round(255 * value * 12.92)
    return round(255 * (1.055 * math.pow(value, 1.0 / 2.4) - 0.055))


def WriteColorTexture(name, values):
    pixel = bytes([ToSrgb(float(value)) for value in values] + [255])
    png = b'\x89PNG\r\n\x1a\n'
    png += CreatePngChunk(b'IHDR', struct.pack('>IIBBBBB', 2, 2, 8, 6, 0, 0, 0))
    png += CreatePngChunk(b'IDAT', zlib.compress((b'\x00' + pixel * 2) * 2))
    png += CreatePngChunk(b'IEND', b'')
    (kTargetDirectory / (name + '.png')).write_bytes(png)


for partName in ['TurretBase', 'TurretYaw', 'TurretBarrel']:
    shutil.copyfile(kSourceDirectory / (partName + '.obj'), kTargetDirectory / (partName + '.obj'))
    outputLines = []
    materialName = ''
    for line in (kSourceDirectory / (partName + '.mtl')).read_text().splitlines():
        outputLines.append(line)
        if line.startswith('newmtl '):
            materialName = line.split()[1]
        if line.startswith('Kd '):
            WriteColorTexture(materialName, line.split()[1:4])
            outputLines.append('map_Kd ' + materialName + '.png')
    (kTargetDirectory / (partName + '.mtl')).write_text('\n'.join(outputLines) + '\n', encoding='utf-8')
print('Turret game assets written to', kTargetDirectory)
