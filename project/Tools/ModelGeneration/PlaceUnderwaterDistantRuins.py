"""Install distant Stage01 scenery using the existing underwater texture palette."""
import importlib.util
import json
import math
from pathlib import Path
from PIL import Image

kProjectDirectory = Path(__file__).resolve().parents[2]
kAssetDirectory = kProjectDirectory / 'resources/Models/Environment/Ocean/DistantRuins'
kRuntimeDirectory = kAssetDirectory / 'Runtime'
kStagePath = kProjectDirectory / 'resources/Scenes/stage01.json'
kObjectPrefix = 'LowerRoute_Distant_'
kModelPrefix = 'Environment/Ocean/DistantRuins/Runtime/'
kSharedPalettePath = kProjectDirectory / 'resources/Models/Environment/Ocean/UnderwaterProps/Runtime/underwater_palette.png'


def LoadPlacementUtilities():
    path = Path(__file__).with_name('PlaceUnderwaterProps.py')
    specification = importlib.util.spec_from_file_location('UnderwaterPlacementUtilities', path)
    module = importlib.util.module_from_spec(specification)
    specification.loader.exec_module(module)
    return module


def ExportRuntime(assetId, utilities, paletteColors):
    vertices, normals, faces = utilities.ReadObj(kAssetDirectory / (assetId + '.obj'))
    materialNames = []
    colorMatches = {}
    for materialName, objectName, corners in faces:
        if materialName not in materialNames:
            materialNames.append(materialName)
            with Image.open(kAssetDirectory / 'textures' / (materialName + '.png')) as texture:
                color = texture.convert('RGB').getpixel((0, 0))
            bestIndex = 0
            bestError = float('inf')
            for paletteIndex, paletteColor in enumerate(paletteColors):
                error = sum((color[channel] - paletteColor[channel]) ** 2 for channel in range(3))
                if error < bestError:
                    bestIndex = paletteIndex
                    bestError = error
            colorMatches[materialName] = bestIndex
    lines = ['# Y up; importer mirrors X. One material; existing underwater palette.',
             'mtllib distant_shared_palette.mtl', 'o ' + assetId]
    for vertex in vertices:
        lines.append('v %.6f %.6f %.6f' % vertex)
    for normal in normals:
        lines.append('vn %.6f %.6f %.6f' % normal)
    for materialName in materialNames:
        lines.append('vt %.6f 0.5' % ((colorMatches[materialName] + 0.5) / len(paletteColors)))
    lines.extend(['usemtl DistantSharedPalette', 's off'])
    triangleCount = 0
    removedTriangleCount = 0
    for materialName, objectName, corners in faces:
        # The authored preview has a closed sanctuary wall; the live rail needs a through passage.
        if assetId == 'distant_temple' and materialName == 'UnlitRecess':
            removedTriangleCount += len(corners) - 2
            continue
        if assetId == 'distant_temple' and materialName == 'AncientTealSignal':
            if all(vertices[vertexIndex - 1][2] < -40.0 for vertexIndex, normalIndex in corners):
                removedTriangleCount += len(corners) - 2
                continue
        textureIndex = materialNames.index(materialName) + 1
        references = []
        for vertexIndex, normalIndex in corners:
            if normalIndex > 0:
                references.append('%d/%d/%d' % (vertexIndex, textureIndex, normalIndex))
            else:
                references.append('%d/%d' % (vertexIndex, textureIndex))
        lines.append('f ' + ' '.join(references))
        triangleCount += len(corners) - 2
    (kRuntimeDirectory / (assetId + '.obj')).write_text('\n'.join(lines) + '\n', encoding='utf-8')
    return {'triangles': triangleCount, 'removedWallTriangles': removedTriangleCount,
            'sharedPaletteIndices': colorMatches}


def Main():
    utilities = LoadPlacementUtilities()
    kRuntimeDirectory.mkdir(exist_ok=True)
    with Image.open(kSharedPalettePath) as palette:
        palette = palette.convert('RGB')
        assert palette.width % 32 == 0
        paletteColors = [palette.getpixel((index * 32 + 16, 16)) for index in range(palette.width // 32)]
    (kRuntimeDirectory / 'distant_shared_palette.mtl').write_text(
        'newmtl DistantSharedPalette\nKd 1 1 1\nKa 1 1 1\nKs 0 0 0\nNs 1\nd 1\nillum 2\n'
        'map_Kd ../../UnderwaterProps/Runtime/underwater_palette.png\n', encoding='utf-8')
    models = {}
    for assetId in ('distant_temple', 'distant_cliff_wall', 'distant_tower_cluster'):
        models[assetId] = ExportRuntime(assetId, utilities, paletteColors)
    sourceText = kStagePath.read_text(encoding='utf-8')
    stage = json.loads(sourceText)
    retainedObjects = []
    floorObjects = []
    for obj in stage['scene']:
        if not obj.get('name', '').startswith(kObjectPrefix):
            retainedObjects.append(obj)
        if obj.get('name', '').startswith('LowerRoute_SandFloor'):
            floorObject = dict(obj)
            floorObject['vertices'] = utilities.ReadObj(kProjectDirectory / 'resources/Models' / obj['file_name'])[0]
            floorObjects.append(floorObject)
    placements = [('Temple', 'distant_temple', 0, 2580, [1.1, 0.75, 0.53], math.pi)]
    for sideName, horizontal, yaw in (('Left', -210, 0.0), ('Right', 210, math.pi)):
        for index, forward in enumerate((1800, 2600, 3400)):
            placements.append(('Cliff_%s%02d' % (sideName, index + 1), 'distant_cliff_wall',
                               horizontal, forward, [1.3, 2.65, 0.4], yaw))
        for index, forward in enumerate((2380, 3140)):
            towerHorizontal = horizontal / 210 * 110
            placements.append(('Towers_%s%02d' % (sideName, index + 1), 'distant_tower_cluster',
                               towerHorizontal, forward, [0.5, 0.8, 0.38], math.pi))
    installed = []
    triangleCount = 0
    for name, assetId, horizontal, forward, scale, yaw in placements:
        height = utilities.GroundHeight(horizontal, forward, floorObjects) - 1.0
        obj = {'name': kObjectPrefix + name, 'type': 'MESH', 'file_name': kModelPrefix + assetId + '.obj',
               'transform': {'translation': [horizontal, forward, round(height, 5)],
                             'rotation': [0.0, 0.0, round(yaw, 6)], 'scale': scale}}
        installed.append(obj)
        triangleCount += models[assetId]['triangles']
    retainedLines = [line for line in sourceText.splitlines() if kObjectPrefix not in line]
    sceneLine = retainedLines.index('  "scene": [')
    newLines = ['    ' + json.dumps(obj, ensure_ascii=False) + ',' for obj in installed]
    retainedLines[sceneLine + 1:sceneLine + 1] = newLines
    output = '\n'.join(retainedLines) + '\n'
    parsed = json.loads(output)
    assert [obj for obj in parsed['scene'] if not obj.get('name', '').startswith(kObjectPrefix)] == retainedObjects
    assert len({obj.get('name') for obj in parsed['scene']}) == len(parsed['scene'])
    kStagePath.write_text(output, encoding='utf-8')
    report = {'sceneObjects': len(installed), 'trianglesAllPlacements': triangleCount,
              'newTextureDescriptors': 0, 'sharedPalette': str(kSharedPalettePath.relative_to(kProjectDirectory)),
              'models': models, 'objects': installed, 'collidersAdded': 0,
              'templePassage': 'Runtime sanctuary wall and rear door trim removed; preview source retained'}
    (kAssetDirectory / 'placement_info.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
    print(json.dumps({'sceneObjects': len(installed), 'trianglesAllPlacements': triangleCount,
                      'newTextureDescriptors': 0}, ensure_ascii=False))


if __name__ == '__main__':
    Main()
