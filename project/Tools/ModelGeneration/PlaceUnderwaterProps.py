"""Prepare single-material runtime meshes and place Stage01 underwater scenery."""
import json
import math
from pathlib import Path
from PIL import Image

kProjectDirectory = Path(__file__).resolve().parents[2]
kAssetDirectory = kProjectDirectory / 'resources/Models/Environment/Ocean/UnderwaterProps'
kRuntimeDirectory = kAssetDirectory / 'Runtime'
kStagePath = kProjectDirectory / 'resources/Scenes/stage01.json'
kObjectPrefix = 'LowerRoute_Prop_'
kModelPrefix = 'Environment/Ocean/UnderwaterProps/Runtime/'
kPlacements = (
    ('03_buried_colossus', -44, 1550, 4.2, 0.12),
    ('01_broken_arch', 0, 2150, 10.0, 0.0),
    ('01_broken_arch', 0, 3260, 10.0, 0.0),
    ('04_sunken_rotor', 48, 1920, 4.5, -0.2),
    ('04_sunken_rotor', -48, 3510, 4.0, 0.2),
    ('05_giant_ribcage', 0, 2740, 14.0, 0.0),
    ('08_collapsed_pillars', 46, 1750, 4.0, 0.15),
    ('08_collapsed_pillars', -46, 2350, 4.0, -0.1),
    ('08_collapsed_pillars', 46, 3070, 4.0, 0.2),
    ('08_collapsed_pillars', -46, 3730, 3.5, -0.15),
    ('06_hydrothermal_vents', -38, 1850, 3.0, 0.3),
    ('06_hydrothermal_vents', 38, 2490, 3.0, -0.2),
    ('06_hydrothermal_vents', -38, 2940, 3.4, 0.1),
    ('06_hydrothermal_vents', 38, 3630, 2.8, -0.3),
)


def ReadObj(path):
    vertices = []
    normals = []
    faces = []
    materialName = ''
    objectName = ''
    for line in path.read_text(encoding='utf-8').splitlines():
        parts = line.split()
        if not parts:
            continue
        if parts[0] == 'v':
            vertices.append(tuple(float(value) for value in parts[1:4]))
        elif parts[0] == 'vn':
            normals.append(tuple(float(value) for value in parts[1:4]))
        elif parts[0] == 'usemtl':
            materialName = parts[1]
        elif parts[0] == 'o':
            objectName = parts[1]
        elif parts[0] == 'f':
            corners = []
            for reference in parts[1:]:
                indices = reference.split('/')
                normalIndex = 0
                if len(indices) > 2 and indices[2]:
                    normalIndex = int(indices[2])
                corners.append((int(indices[0]), normalIndex))
            faces.append((materialName, objectName, corners))
    return vertices, normals, faces


def ExportRuntime(assetId, vertices, normals, faces, materialNames, pivotHeight=0.0):
    lines = ['# Single material mesh; Y up; engine importer mirrors X.',
             'mtllib underwater_palette.mtl', 'o ' + assetId]
    for positionX, positionY, positionZ in vertices:
        lines.append('v %.6f %.6f %.6f' % (positionX, positionY - pivotHeight, positionZ))
    for normal in normals:
        lines.append('vn %.6f %.6f %.6f' % normal)
    for materialIndex in range(len(materialNames)):
        lines.append('vt %.6f 0.5' % ((materialIndex + 0.5) / len(materialNames)))
    lines.extend(['usemtl UnderwaterPalette', 's off'])
    triangleCount = 0
    for materialName, objectName, corners in faces:
        references = []
        textureIndex = materialNames.index(materialName) + 1
        for vertexIndex, normalIndex in corners:
            if normalIndex > 0:
                references.append('%d/%d/%d' % (vertexIndex, textureIndex, normalIndex))
            else:
                references.append('%d/%d' % (vertexIndex, textureIndex))
        lines.append('f ' + ' '.join(references))
        triangleCount += len(corners) - 2
    (kRuntimeDirectory / (assetId + '.obj')).write_text('\n'.join(lines) + '\n', encoding='utf-8')
    return triangleCount


def PrepareModels():
    kRuntimeDirectory.mkdir(exist_ok=True)
    sourcePaths = sorted(kAssetDirectory.glob('0*.obj'))
    sourceModels = {}
    materialNames = []
    for path in sourcePaths:
        vertices, normals, faces = ReadObj(path)
        sourceModels[path.stem] = (vertices, normals, faces)
        for materialName, objectName, corners in faces:
            if materialName not in materialNames:
                materialNames.append(materialName)
    palette = Image.new('RGB', (len(materialNames) * 32, 32))
    for materialIndex, materialName in enumerate(materialNames):
        with Image.open(kAssetDirectory / 'textures' / (materialName + '.png')) as texture:
            color = texture.convert('RGB').getpixel((0, 0))
        palette.paste(color, (materialIndex * 32, 0, (materialIndex + 1) * 32, 32))
    palette.save(kRuntimeDirectory / 'underwater_palette.png')
    (kRuntimeDirectory / 'underwater_palette.mtl').write_text(
        'newmtl UnderwaterPalette\nKd 1 1 1\nKa 1 1 1\nKs 0 0 0\nNs 1\nd 1\nillum 2\nmap_Kd underwater_palette.png\n',
        encoding='utf-8')
    modelReports = {}
    for assetId, model in sourceModels.items():
        vertices, normals, faces = model
        if assetId == '04_sunken_rotor':
            ringFaces = []
            baseFaces = []
            for face in faces:
                if face[1].startswith(('RingSegment', 'LuminousInlay')):
                    ringFaces.append(face)
                else:
                    baseFaces.append(face)
            modelReports[assetId + '_base'] = ExportRuntime(assetId + '_base', vertices, normals, baseFaces, materialNames)
            modelReports[assetId + '_ring'] = ExportRuntime(assetId + '_ring', vertices, normals, ringFaces, materialNames, 3.5)
        else:
            modelReports[assetId] = ExportRuntime(assetId, vertices, normals, faces, materialNames)
    return modelReports


def GroundHeight(horizontal, forward, floorObjects):
    # Sample the actual existing sand-floor mesh, including its scene scale.
    for floorObject in reversed(floorObjects):
        translation = floorObject['transform']['translation']
        scale = floorObject['transform']['scale']
        vertices = floorObject['vertices']
        rowCount = len(vertices) // 25
        localForward = (forward - translation[1]) / scale[1]
        segmentLength = vertices[-1][2]
        if localForward < 0 or localForward > segmentLength:
            continue
        column = ((horizontal - translation[0]) / scale[0] + 120.0) / 10.0
        row = localForward / segmentLength * (rowCount - 1)
        columnIndex = min(23, max(0, math.floor(column)))
        rowIndex = min(rowCount - 2, max(0, math.floor(row)))
        columnRatio = column - columnIndex
        rowRatio = row - rowIndex
        first = rowIndex * 25 + columnIndex
        height00 = vertices[first][1]
        height10 = vertices[first + 1][1]
        height01 = vertices[first + 25][1]
        height11 = vertices[first + 26][1]
        if columnRatio + rowRatio <= 1:
            height = height00 + (height10 - height00) * columnRatio + (height01 - height00) * rowRatio
        else:
            height = height11 + (height01 - height11) * (1 - columnRatio) + (height10 - height11) * (1 - rowRatio)
        return translation[2] + height * scale[2]
    raise ValueError('No sand floor at distance ' + str(forward))


def MakeObject(name, assetId, horizontal, forward, height, scale, yaw):
    return {'name': kObjectPrefix + name, 'type': 'MESH', 'file_name': kModelPrefix + assetId + '.obj',
            'transform': {'translation': [round(horizontal, 5), round(forward, 5), round(height, 5)],
                          'rotation': [0.0, 0.0, round(yaw, 6)], 'scale': [scale, scale, scale]}}


def Main():
    modelReports = PrepareModels()
    sourceText = kStagePath.read_text(encoding='utf-8')
    stage = json.loads(sourceText)
    floorObjects = []
    for obj in stage['scene']:
        if obj.get('name', '').startswith('LowerRoute_SandFloor'):
            floorObject = dict(obj)
            floorObject['vertices'] = ReadObj(kProjectDirectory / 'resources/Models' / obj['file_name'])[0]
            floorObjects.append(floorObject)
    placements = list(kPlacements)
    for index in range(12):
        horizontal = 38.0
        if index % 2 == 0:
            horizontal = -38.0
        placements.append(('02_crystal_cluster', horizontal, 1430 + index * 230, 2.4 + (index % 3) * 0.2, index * 0.37))
    for index in range(14):
        horizontal = 50.0
        if index % 2 == 0:
            horizontal = -50.0
        placements.append(('07_tall_kelp', horizontal, 1480 + index * 190, 3.0 + (index % 3) * 0.3, index * 0.29))
    objects = []
    for placementIndex, placement in enumerate(placements):
        assetId, horizontal, forward, scale, yaw = placement
        height = GroundHeight(horizontal, forward, floorObjects) - 0.8
        yaw += math.pi
        name = '%02d_%s' % (placementIndex + 1, assetId[3:])
        if assetId == '04_sunken_rotor':
            objects.append(MakeObject(name + '_Base', assetId + '_base', horizontal, forward, height, scale, yaw))
            ring = MakeObject(name + '_Ring', assetId + '_ring', horizontal, forward, height + 3.5 * scale, scale, yaw)
            ring['gimmick'] = {'type': 'ROTATION', 'speed': 0.18, 'axis': [0.0, 1.0, 0.0], 'range': [0.0, 0.0, 0.0]}
            objects.append(ring)
        else:
            objects.append(MakeObject(name, assetId, horizontal, forward, height, scale, yaw))
    # Keep the rest of the hand-authored scene byte-for-byte when installing or re-running.
    retainedLines = []
    for line in sourceText.splitlines():
        if kObjectPrefix not in line:
            retainedLines.append(line)
    sceneLine = retainedLines.index('  "scene": [')
    newLines = []
    for obj in objects:
        newLines.append('    ' + json.dumps(obj, ensure_ascii=False) + ',')
    retainedLines[sceneLine + 1:sceneLine + 1] = newLines
    output = '\n'.join(retainedLines) + '\n'
    parsed = json.loads(output)
    names = [obj.get('name') for obj in parsed['scene']]
    assert len(names) == len(set(names)), 'Duplicate object names'
    kStagePath.write_text(output, encoding='utf-8')
    triangleCount = sum(modelReports[Path(obj['file_name']).stem] for obj in objects)
    report = {'placements': len(placements), 'sceneObjects': len(objects), 'trianglesAllPlacements': triangleCount,
              'runtimeMeshCount': len(modelReports), 'models': modelReports, 'objects': objects,
              'placementCoordinateOrder': ['horizontal', 'forward', 'height'], 'collidersAdded': 0,
              'ringRotationRadiansPerSecond': 0.18, 'kelpAnimation': 'Static OBJ scenery'}
    (kAssetDirectory / 'placement_info.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
    print(json.dumps({key: value for key, value in report.items() if key not in ('objects', 'models')}, ensure_ascii=False))


if __name__ == '__main__':
    Main()
