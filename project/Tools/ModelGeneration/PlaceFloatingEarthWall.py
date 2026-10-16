"""Install one shaded masonry object and a separate floating ground object."""

import json
import math
from pathlib import Path

kProjectDirectory = Path(__file__).resolve().parents[2]
kAssetDirectory = kProjectDirectory / 'resources/Models/Environment/Ocean/FloatingRuins'
kScenePath = kProjectDirectory / 'resources/Scenes/stage01.json'
kScale = [6.0, 2.0, 4.0]
kTranslation = [0.0, 240.0, -7.0]
kArchBoxCount = 12


def CreateBox(minimumX, maximumX, minimumHeight, maximumHeight, minimumDepth=-0.06, maximumDepth=0.78):
    return {
        'type': 'BOX',
        # OBJ export maps Blender Y to -Z; the engine additionally mirrors OBJ X.
        'center': [round(-(minimumX + maximumX) * 0.5 * kScale[0], 5),
            round(-(minimumDepth + maximumDepth) * 0.5 * kScale[1], 5),
            round((minimumHeight + maximumHeight) * 0.5 * kScale[2], 5)],
        'size': [round((maximumX - minimumX) * kScale[0], 5),
            round((maximumDepth - minimumDepth) * kScale[1], 5),
            round((maximumHeight - minimumHeight) * kScale[2], 5)],
    }


def Main():
    wallBoxes = []
    for minimumX, maximumX, roofHeight in [(-5.65, -4.8, 3.38), (-4.8, -1.4, 3.98),
            (1.4, 3.6, 3.98), (3.6, 4.6, 2.82), (4.6, 5.65, 2.30)]:
        wallBoxes.append(CreateBox(minimumX, maximumX, 0.54, roofHeight))
    for index in range(kArchBoxCount):
        minimumX = -1.4 + 2.8 * index / kArchBoxCount
        maximumX = -1.4 + 2.8 * (index + 1) / kArchBoxCount
        nearestX = min(abs(minimumX), abs(maximumX))
        archHeight = 2.01 + math.sqrt(max(0.0, 1.96 - nearestX * nearestX))
        wallBoxes.append(CreateBox(minimumX, maximumX, archHeight, 3.98))
    entries = []
    triangleCount = 0
    wallTriangleCount = 0
    for partName in ['ground', 'masonry']:
        fileName = 'floating_earth_wall_' + partName + '.obj'
        modelPath = kAssetDirectory / fileName
        if not modelPath.exists():
            raise RuntimeError('Generate the solid wall model before installing.')
        partTriangles = sum(line.startswith('f ') for line in modelPath.read_text(encoding='utf8').splitlines())
        triangleCount += partTriangles
        entry = {
            'name': 'Gate01_FloatingRuins_' + partName,
            'type': 'MESH',
            'file_name': 'Environment/Ocean/FloatingRuins/' + fileName,
            'transform': {'translation': kTranslation, 'rotation': [0.0, 0.0, 0.0], 'scale': kScale},
        }
        if partName == 'ground':
            entry['collider'] = CreateBox(-6.35, 6.35, -4.1, 0.90, -2.65, 2.65)
        else:
            entry['colliders'] = wallBoxes
            wallTriangleCount = partTriangles
        entries.append(entry)
    for box in wallBoxes[5:]:
        left = (box['center'][0] - box['size'][0] * 0.5) / kScale[0]
        right = (box['center'][0] + box['size'][0] * 0.5) / kScale[0]
        bottom = (box['center'][2] - box['size'][2] * 0.5) / kScale[2]
        for sampleIndex in range(21):
            positionX = left + (right - left) * sampleIndex / 20
            openingHeight = 2.01 + math.sqrt(max(0.0, 1.96 - positionX * positionX))
            if bottom + 0.00001 < openingHeight:
                raise RuntimeError('An arch box obstructs the opening.')
    sceneText = kScenePath.read_text(encoding='utf8')
    sceneData = json.loads(sceneText)
    oldGateNames = {'Gate01_Left', 'Gate01_Right', 'Gate01_Top'}
    for entry in sceneData['scene']:
        if entry['name'].startswith('Gate01_FloatingRuins'):
            oldGateNames.add(entry['name'])
    oldGateLines = []
    for sceneLine in sceneText.splitlines():
        if any('"' + name + '"' in sceneLine for name in oldGateNames):
            oldGateLines.append(sceneLine)
    if not oldGateLines:
        raise RuntimeError('Gate01 was not found.')
    replacement = '\n'.join('    ' + json.dumps(entry) + ',' for entry in entries)
    sceneText = sceneText.replace(oldGateLines[0], replacement, 1)
    for oldGateLine in oldGateLines[1:]:
        sceneText = sceneText.replace(oldGateLine + '\n', '', 1)
    updatedData = json.loads(sceneText)
    originalOtherObjects = [entry for entry in sceneData['scene'] if entry['name'] not in oldGateNames]
    updatedOtherObjects = [entry for entry in updatedData['scene'] if not entry['name'].startswith('Gate01_FloatingRuins')]
    if originalOtherObjects != updatedOtherObjects:
        raise RuntimeError('Unrelated scene objects changed.')
    kScenePath.write_text(sceneText, encoding='utf8')
    reportPath = kAssetDirectory / 'asset_info.json'
    report = json.loads(reportPath.read_text(encoding='utf8'))
    report['objTriangles'] = triangleCount
    report['masonryTriangles'] = wallTriangleCount
    report['placedInStage'] = True
    report['wallColliderCount'] = len(wallBoxes)
    report['stagePlacement'] = {'stage': 'stage01', 'gate': 'Gate01', 'parts': entries}
    reportPath.write_text(json.dumps(report, indent=2), encoding='utf8')
    print(json.dumps({'objTriangles': triangleCount, 'masonryTriangles': wallTriangleCount,
        'wallColliders': len(wallBoxes), 'renderObjects': len(entries), 'archOpeningValidated': True}))


Main()
