"""Replace remaining Stage01 wall panels while preserving their passage bounds."""

import copy
import json
from pathlib import Path

kProjectDirectory = Path(__file__).resolve().parents[2]
kScenePath = kProjectDirectory / 'resources/Scenes/stage01.json'
kAssetDirectory = kProjectDirectory / 'resources/Models/Environment/Ocean/FloatingRuins'
kWallModel = 'Environment/Ocean/FloatingRuins/floating_slalom_wall_masonry.obj'
kGroundModel = 'Environment/Ocean/FloatingRuins/floating_earth_wall_ground.obj'
kWallNames = ['Gate02_Left', 'Gate02_Right', 'Gate02_LowCeiling',
    'Slalom02_RightWall', 'VerticalTunnel_Floor', 'VerticalTunnel_Ceiling',
    'OffsetGate_Left', 'OffsetGate_Right', 'OffsetGate_Top',
    'FinalGate_Left', 'FinalGate_Right', 'FinalGate_Bottom',
    'SecondActGate_Left', 'SecondActGate_Right', 'SecondActGate_Bottom',
    'SecondSlalom_Left', 'SecondSlalom_Right', 'SecondTunnel_Floor', 'SecondTunnel_Ceiling']
kCeilingNames = {'Gate02_LowCeiling', 'VerticalTunnel_Ceiling', 'OffsetGate_Top', 'SecondTunnel_Ceiling'}
kRoofSegments = [(-5.65, -4.40, 6.70), (-4.40, -2.90, 7.50),
    (-2.90, 0.30, 8.00), (0.30, 1.25, 7.35), (1.25, 3.70, 7.75),
    (3.70, 5.65, 6.45)]


def CreateBox(minimumX, maximumX, minimumHeight, maximumHeight, scale, minimumDepth, maximumDepth):
    return {'type': 'BOX',
        'center': [round(-(minimumX + maximumX) * 0.5 * scale[0], 6),
            round(-(minimumDepth + maximumDepth) * 0.5 * scale[1], 6),
            round((minimumHeight + maximumHeight) * 0.5 * scale[2], 6)],
        'size': [round((maximumX - minimumX) * scale[0], 6),
            round((maximumDepth - minimumDepth) * scale[1], 6),
            round((maximumHeight - minimumHeight) * scale[2], 6)]}


def ValidateInsidePanel(entry, original):
    originalPosition = original['transform']['translation']
    originalBox = original['collider']
    for box in entry['colliders']:
        for axis in range(3):
            center = entry['transform']['translation'][axis] + box['center'][axis]
            originalCenter = originalPosition[axis] + originalBox['center'][axis]
            if center - box['size'][axis] * 0.5 < originalCenter - originalBox['size'][axis] * 0.5 - 0.0001:
                raise RuntimeError('Wall collider exceeds original passage bounds: ' + entry['name'])
            if center + box['size'][axis] * 0.5 > originalCenter + originalBox['size'][axis] * 0.5 + 0.0001:
                raise RuntimeError('Wall collider exceeds original passage bounds: ' + entry['name'])


def CreateReplacement(original):
    position = original['transform']['translation']
    size = original['collider']['size']
    originalCenter = original['collider']['center']
    if original['transform']['rotation'] != [0.0, 0.0, 0.0]:
        raise RuntimeError('Unexpected rotated wall: ' + original['name'])
    scale = [size[0] / 11.30, size[1] / 2.0, size[2] / 7.46]
    bottom = position[2] + originalCenter[2] - size[2] * 0.5
    translation = [position[0] + originalCenter[0], position[1] + originalCenter[1], bottom - 0.54 * scale[2]]
    wall = copy.deepcopy(original)
    wall['file_name'] = kWallModel
    wall['transform'] = {'translation': translation, 'rotation': [0.0, 0.0, 0.0], 'scale': scale}
    wall.pop('collider')
    wall['colliders'] = []
    for minimumX, maximumX, roofHeight in kRoofSegments:
        wall['colliders'].append(CreateBox(minimumX, maximumX, 0.54, roofHeight, scale, -1.0, 1.0))
    ValidateInsidePanel(wall, original)
    entries = [wall]
    if original['name'] not in kCeilingNames:
        groundScale = [size[0] / 12.70, size[1] / 5.30, 3.0]
        ground = {'name': original['name'] + '_FloatingRuinsGround', 'type': 'MESH', 'file_name': kGroundModel,
            'transform': {'translation': [translation[0], translation[1], bottom - 0.54 * groundScale[2]],
                'rotation': [0.0, 0.0, 0.0], 'scale': groundScale},
            'collider': CreateBox(-6.35, 6.35, -4.1, 0.54, groundScale, -2.65, 2.65)}
        # Ground is below the old panel and stays within its X/forward footprint.
        groundBox = ground['collider']
        for axis in [0, 1]:
            if groundBox['size'][axis] > size[axis] + 0.0001:
                raise RuntimeError('Ground expands the passage footprint.')
        groundTop = ground['transform']['translation'][2] + groundBox['center'][2] + groundBox['size'][2] * 0.5
        if groundTop > bottom + 0.0001:
            raise RuntimeError('Ground intrudes above the old panel bottom.')
        entries.append(ground)
    return entries


def Main():
    sceneText = kScenePath.read_text(encoding='utf8')
    originalData = json.loads(sceneText)
    originalByName = {}
    for entry in originalData['scene']:
        if entry['name'] in kWallNames:
            originalByName[entry['name']] = entry
    if len(originalByName) != len(kWallNames):
        raise RuntimeError('The expected Stage01 wall panels were not found.')
    reportPath = kAssetDirectory / 'stage01_wall_replacements.json'
    if all(entry['file_name'] == kWallModel for entry in originalByName.values()):
        if not reportPath.exists():
            raise RuntimeError('Replacement source dimensions are unavailable.')
        previousReport = json.loads(reportPath.read_text(encoding='utf8'))
        originalByName = {}
        for entry in previousReport['originalPanels']:
            originalByName[entry['name']] = entry
        # Reapply from the saved original dimensions, replacing prior generated grounds.
        for line in sceneText.splitlines():
            for name in kWallNames:
                if '"' + name + '_FloatingRuinsGround"' in line:
                    sceneText = sceneText.replace(line + '\n', '', 1)
                    break
    replacements = {}
    newEntries = []
    for name in kWallNames:
        original = originalByName[name]
        if original['file_name'] != 'Environment/Block/block.obj':
            raise RuntimeError('Unexpected source model: ' + name)
        entries = CreateReplacement(original)
        replacements[name] = entries
        newEntries.extend(entries)
    for line in sceneText.splitlines():
        for name, entries in replacements.items():
            if '"' + name + '"' in line:
                replacement = '\n'.join('    ' + json.dumps(entry) + ',' for entry in entries)
                sceneText = sceneText.replace(line, replacement, 1)
                break
    updatedData = json.loads(sceneText)
    touchedNames = set(kWallNames)
    for entry in newEntries:
        touchedNames.add(entry['name'])
    originalOtherObjects = [entry for entry in originalData['scene'] if entry['name'] not in touchedNames]
    updatedOtherObjects = [entry for entry in updatedData['scene'] if entry['name'] not in touchedNames]
    if originalOtherObjects != updatedOtherObjects:
        raise RuntimeError('Unrelated stage objects changed.')
    for entry in newEntries:
        if not (kProjectDirectory / 'resources/Models' / entry['file_name']).exists():
            raise RuntimeError('Replacement model is missing.')
    report = {'wallCount': len(kWallNames), 'groundCount': len(newEntries) - len(kWallNames),
        'wallColliderCount': len(kWallNames) * len(kRoofSegments),
        'originalPanels': list(originalByName.values()), 'replacements': newEntries,
        'passageBoundsValidated': True}
    reportPath.write_text(json.dumps(report, indent=2), encoding='utf8')
    kScenePath.write_text(sceneText, encoding='utf8')
    print(json.dumps({'replacedWalls': report['wallCount'], 'addedGrounds': report['groundCount'],
        'wallColliders': report['wallColliderCount'], 'passageBoundsValidated': True}))


Main()
