"""Install the supplied mountain model at Stage01's first gate; verify its tunnel.

Run with Blender. The OBJ geometry is preserved. Kd colors become tiny textures
because the game importer consumes image textures rather than OBJ diffuse colors.
"""
import bpy
import json
import math
from pathlib import Path
from mathutils import Matrix, Vector
from mathutils.bvhtree import BVHTree

kProjectDirectory = Path(__file__).resolve().parents[2]
kAssetDirectory = kProjectDirectory / 'resources/Models/Environment/MountainRuins'
kStagePath = kProjectDirectory / 'resources/Scenes/stage01.json'
kScale = 9.5
kHorizontalScale = 14.0
kProgress = 440.0
kHeight = -30.0
kObjectName = 'Stage01_MountainRuins'
kRemovedNames = {'Gate01_FloatingRuins_ground', 'Gate01_FloatingRuins_masonry'}


def LoadGeometry():
    vertices = []
    faces = []
    for line in (kAssetDirectory / 'rock_mountain_ruins.obj').read_text().splitlines():
        parts = line.split()
        if not parts:
            continue
        if parts[0] == 'v':
            # Engine import mirrors X; then the scene turns the mountain 180 degrees.
            vertices.append(Vector((float(parts[1]), float(parts[2]), -float(parts[3]))))
        if parts[0] == 'f':
            faces.append([int(part.split('/')[0]) - 1 for part in parts[1:]])
    return vertices, faces


def CreateColorTextures():
    textureDirectory = kAssetDirectory / 'textures'
    textureDirectory.mkdir(exist_ok=True)
    materialPath = kAssetDirectory / 'rock_mountain_ruins.mtl'
    output = []
    materialName = ''
    textureCount = 0
    for line in materialPath.read_text().splitlines():
        if line.startswith('map_Kd '):
            continue
        output.append(line)
        if line.startswith('newmtl '):
            materialName = line.split(maxsplit=1)[1]
        if line.startswith('Kd '):
            color = [float(value) for value in line.split()[1:4]]
            textureName = materialName + '.png'
            image = bpy.data.images.new(materialName + 'Color', width=16, height=16)
            image.pixels = [*color, 1.0] * 256
            image.filepath_raw = str(textureDirectory / textureName)
            image.file_format = 'PNG'
            image.save()
            output.append('map_Kd textures/' + textureName)
            textureCount += 1
    materialPath.write_text('\n'.join(output) + '\n', encoding='utf8')
    return textureCount


def CreateColliders(vertices, tree):
    minimum = Vector(tuple(min(point[axis] for point in vertices) for axis in range(3)))
    maximum = Vector(tuple(max(point[axis] for point in vertices) for axis in range(3)))
    # Narrow vertical strips trace the arch rather than filling the opening with one box.
    boxes = []
    columnWidth = 0.75
    count = math.ceil((maximum.x - minimum.x) / columnWidth)
    depth = maximum.z - minimum.z
    for column in range(count):
        left = minimum.x + column * columnWidth
        right = min(left + columnWidth, maximum.x)
        lowest = maximum.y
        highest = minimum.y
        hasSurface = False
        for horizontalSample in range(7):
            positionX = left + (right - left) * horizontalSample / 6
            for row in range(math.ceil((maximum.y - minimum.y) / 0.1) + 1):
                height = minimum.y + row * 0.1
                hit = tree.ray_cast(Vector((positionX, height, minimum.z - 1)), Vector((0, 0, 1)), depth + 2)[0]
                if hit is not None:
                    lowest = min(lowest, height)
                    highest = max(highest, height)
                    hasSurface = True
        if not hasSurface:
            continue
        lowest = max(minimum.y, lowest - 0.10)
        highest = min(maximum.y, highest + 0.10)
        # Collider offsets/sizes are world-space values in the scene file: X, Z, Y.
        boxes.append({'type': 'BOX', 'center': [round((left + right) * kHorizontalScale * 0.5, 5),
            round((minimum.z + maximum.z) * kScale * 0.5, 5), round((lowest + highest) * kScale * 0.5, 5)],
            'size': [round((right - left) * kHorizontalScale, 5), round(depth * kScale, 5),
                round((highest - lowest) * kScale, 5)]})
    return boxes, minimum, maximum


def VerifyPassage(tree, boxes):
    nearestDistance = 1000.0
    sampleCount = 0
    # Wider than the normal straight flight line: a +/-16 horizontal, +/-5 vertical corridor.
    for positionX in [-16.0, -8.0, 0.0, 8.0, 16.0]:
        for height in [-5.0, 0.0, 5.0]:
            for progress in range(370, 521, 2):
                localPoint = Vector((positionX / kHorizontalScale, (height - kHeight) / kScale,
                    (progress - kProgress) / kScale))
                nearest = tree.find_nearest(localPoint)
                if nearest[0] is not None:
                    # The minimum scale gives a conservative world-space distance bound.
                    clearance = nearest[3] * min(kScale, kHorizontalScale)
                    nearestDistance = min(nearestDistance, clearance)
                    if clearance <= 1.5:
                        raise RuntimeError('Mesh intersects flight corridor: ' + str(localPoint))
                for box in boxes:
                    center = box['center']
                    size = box['size']
                    point = [positionX, progress - kProgress, height - kHeight]
                    squaredDistance = 0.0
                    for axis in range(3):
                        distance = max(0.0, abs(point[axis] - center[axis]) - size[axis] * 0.5)
                        squaredDistance += distance * distance
                    if squaredDistance <= 1.5 * 1.5:
                        raise RuntimeError('Collider blocks flight corridor')
                sampleCount += 1
    return sampleCount, nearestDistance


def InstallStage(entry):
    text = kStagePath.read_text(encoding='utf8')
    before = json.loads(text)
    lines = []
    isInstalled = False
    for line in text.splitlines():
        isRemoved = False
        for name in kRemovedNames | {kObjectName}:
            if '"' + name + '"' in line:
                if not isInstalled:
                    lines.append('    ' + json.dumps(entry, ensure_ascii=False) + ',')
                    isInstalled = True
                isRemoved = True
                break
        if not isRemoved:
            lines.append(line)
    if not isInstalled:
        raise RuntimeError('First gate / mountain placement was not found')
    afterText = '\n'.join(lines) + '\n'
    after = json.loads(afterText)
    unaffectedBefore = [item for item in before['scene'] if item['name'] not in kRemovedNames | {kObjectName}]
    unaffectedAfter = [item for item in after['scene'] if item['name'] != kObjectName]
    if unaffectedBefore != unaffectedAfter:
        raise RuntimeError('Unrelated stage objects changed')
    kStagePath.write_text(afterText, encoding='utf8')


def RenderPreview():
    bpy.ops.object.select_all(action='SELECT')
    bpy.ops.object.delete(use_global=False)
    bpy.ops.wm.obj_import(filepath=str(kAssetDirectory / 'rock_mountain_ruins.obj'),
        forward_axis='NEGATIVE_Z', up_axis='Y')
    for model in list(bpy.context.selected_objects):
        # Reflect world-space Blender Y, preserving the OBJ importer's axis rotation.
        placement = Matrix.Translation((0, -kProgress, kHeight))
        model.matrix_world = placement @ Matrix.Diagonal((kHorizontalScale, -kScale, kScale, 1)) @ model.matrix_world
        highestPoint = max((model.matrix_world @ vertex.co).z for vertex in model.data.vertices)
        if highestPoint < 120:
            raise RuntimeError('Preview axis conversion placed the mountain below sea level')
    scene = bpy.context.scene
    scene.render.engine = 'CYCLES'
    scene.cycles.samples = 24
    scene.cycles.use_denoising = True
    scene.render.resolution_x = 1440
    scene.render.resolution_y = 810
    scene.render.resolution_percentage = 100
    scene.world.use_nodes = True
    background = scene.world.node_tree.nodes.get('Background')
    background.inputs['Color'].default_value = (0.65, 0.81, 0.90, 1)
    background.inputs['Strength'].default_value = 0.8
    lightData = bpy.data.lights.new('PreviewSun', 'SUN')
    lightData.energy = 2.0
    light = bpy.data.objects.new('PreviewSun', lightData)
    scene.collection.objects.link(light)
    light.rotation_euler = (0.4, -0.5, -0.3)
    oceanMaterial = bpy.data.materials.new('PreviewOcean_NotExported')
    oceanMaterial.use_nodes = True
    shader = oceanMaterial.node_tree.nodes.get('Principled BSDF')
    shader.inputs['Base Color'].default_value = (0.035, 0.44, 0.54, 1)
    shader.inputs['Roughness'].default_value = 0.3
    bpy.ops.mesh.primitive_plane_add(size=4000, location=(0, 0, -30))
    bpy.context.object.data.materials.append(oceanMaterial)
    cameraData = bpy.data.cameras.new('PlacementCamera')
    camera = bpy.data.objects.new('PlacementCamera', cameraData)
    scene.collection.objects.link(camera)
    camera.location = (0, 30, 3)
    cameraData.sensor_fit = 'VERTICAL'
    cameraData.sensor_height = 24
    cameraData.lens = 24 / (2 * math.tan(0.45 * 0.5))
    cameraData.clip_end = 5000
    camera.rotation_euler = (Vector((0, -kProgress, 3)) - camera.location).to_track_quat('-Z', 'Y').to_euler()
    scene.camera = camera
    previewDirectory = kAssetDirectory / 'previews'
    previewDirectory.mkdir(exist_ok=True)
    scene.render.filepath = str(previewDirectory / 'stage01_mountain_ruins.png')
    bpy.ops.wm.save_as_mainfile(filepath=str(kAssetDirectory / 'stage01_mountain_ruins_preview.blend'))
    bpy.ops.render.render(write_still=True)


def Main():
    vertices, faces = LoadGeometry()
    tree = BVHTree.FromPolygons(vertices, faces)
    boxes, minimum, maximum = CreateColliders(vertices, tree)
    sampleCount, clearance = VerifyPassage(tree, boxes)
    textureCount = CreateColorTextures()
    entry = {'name': kObjectName, 'type': 'MESH',
        'file_name': 'Environment/MountainRuins/rock_mountain_ruins.obj',
        'transform': {'translation': [0.0, kProgress, kHeight],
            'rotation': [0.0, 0.0, -math.pi], 'scale': [kHorizontalScale, kScale, kScale]}, 'colliders': boxes}
    InstallStage(entry)
    report = {'triangles': sum(len(face) - 2 for face in faces), 'materialTextures': textureCount,
        'colliderBoxes': len(boxes), 'dimensions': [(maximum.x - minimum.x) * kHorizontalScale,
            (maximum.y - minimum.y) * kScale, (maximum.z - minimum.z) * kScale],
        'enginePosition': [0, kHeight, kProgress], 'engineYawRadians': math.pi,
        'engineScale': [kHorizontalScale, kScale, kScale],
        'passageSamples': sampleCount, 'minimumMeshClearance': clearance,
        'verifiedCorridor': {'x': [-16, 16], 'y': [-5, 5], 'z': [370, 520]},
        'gameplayVerified': False, 'containsWaterfall': False}
    (kAssetDirectory / 'placement_info.json').write_text(json.dumps(report, indent=2), encoding='utf8')
    print('MOUNTAIN_PLACEMENT ' + json.dumps(report), flush=True)
    RenderPreview()


Main()
