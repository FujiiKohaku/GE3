import bpy
import math
import struct
import zlib
from pathlib import Path

targetDirectory = Path('C:/Projects/KohakuEngine/project/resources/Models/Environment/ElectricGate')
targetDirectory.mkdir(parents=True, exist_ok=True)
bpy.ops.wm.open_mainfile(filepath='C:/Projects/KohakuEngine/output/electric_gate_v1/ElectricGate.blend')

def CreateChunk(chunkType, data):
    return struct.pack('>I', len(data)) + chunkType + data + struct.pack('>I', zlib.crc32(chunkType + data) & 0xffffffff)

def ToSrgb(value):
    if value <= 0.0031308:
        return round(255 * value * 12.92)
    return round(255 * (1.055 * math.pow(value, 1 / 2.4) - 0.055))

for partName in ['ElectricGateBody', 'ElectricGateEmitters']:
    bpy.ops.object.select_all(action='DESELECT')
    for model in bpy.context.scene.objects:
        if model.type != 'MESH' or model.name == 'StudioFloor':
            continue
        isEmitter = model.name.startswith('LiveEmitter')
        if partName == 'ElectricGateEmitters' and isEmitter:
            model.select_set(True)
        if partName == 'ElectricGateBody' and not isEmitter:
            model.select_set(True)
    bpy.ops.wm.obj_export(filepath=str(targetDirectory / (partName + '.obj')), export_selected_objects=True, forward_axis='NEGATIVE_Z', up_axis='Y', export_triangulated_mesh=True)
    materialPath = targetDirectory / (partName + '.mtl')
    materialLines = []
    materialName = ''
    for line in materialPath.read_text().splitlines():
        materialLines.append(line)
        if line.startswith('newmtl '):
            materialName = line.split()[1]
        if line.startswith('Kd '):
            color = bytes([ToSrgb(float(value)) for value in line.split()[1:4]] + [255])
            png = b'\x89PNG\r\n\x1a\n'
            png += CreateChunk(b'IHDR', struct.pack('>IIBBBBB', 2, 2, 8, 6, 0, 0, 0))
            png += CreateChunk(b'IDAT', zlib.compress((b'\x00' + color * 2) * 2))
            png += CreateChunk(b'IEND', b'')
            (targetDirectory / (materialName + '.png')).write_bytes(png)
            materialLines.append('map_Kd ' + materialName + '.png')
    materialPath.write_text('\n'.join(materialLines) + '\n', encoding='utf-8')
print('Exported electric gate assets:', targetDirectory)
