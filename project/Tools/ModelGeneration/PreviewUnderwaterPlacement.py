"""Render placed Stage01 props using the same OBJ data and scene transforms."""
import bpy
import json
import math
import sys
from pathlib import Path
from mathutils import Vector
from mathutils.bvhtree import BVHTree

kProjectDirectory = Path(__file__).resolve().parents[2]
kAssetDirectory = kProjectDirectory / 'resources/Models/Environment/Ocean/UnderwaterProps'
kPreviewDirectory = kAssetDirectory / 'previews'
kPropPrefixes = ('LowerRoute_Prop_', 'LowerRoute_Distant_')


def LoadMesh(path):
    vertices = []
    faces = []
    textureCoordinates = []
    faceTextureCoordinates = []
    for line in path.read_text(encoding='utf-8').splitlines():
        parts = line.split()
        if not parts:
            continue
        if parts[0] == 'v':
            # Engine import mirrors X, then preview maps engine X/Y/Z to X/Z/Y.
            vertices.append((-float(parts[1]), float(parts[3]), float(parts[2])))
        elif parts[0] == 'vt':
            textureCoordinates.append((float(parts[1]), float(parts[2])))
        elif parts[0] == 'f':
            face = []
            coordinates = []
            for corner in parts[1:]:
                indices = corner.split('/')
                face.append(int(indices[0]) - 1)
                if len(indices) > 1 and indices[1]:
                    coordinates.append(textureCoordinates[int(indices[1]) - 1])
                else:
                    coordinates.append((0, 0))
            faces.append(face)
            faceTextureCoordinates.append(coordinates)
    mesh = bpy.data.meshes.new(path.stem)
    mesh.from_pydata(vertices, [], faces)
    mesh.update()
    uvLayer = mesh.uv_layers.new(name='PaletteUv')
    for polygon, coordinates in zip(mesh.polygons, faceTextureCoordinates):
        for loopIndex, coordinate in zip(polygon.loop_indices, coordinates):
            uvLayer.data[loopIndex].uv = coordinate
    return mesh


def Material(name, texturePath, color):
    material = bpy.data.materials.new(name)
    material.use_nodes = True
    shader = material.node_tree.nodes['Principled BSDF']
    shader.inputs['Base Color'].default_value = (*color, 1)
    shader.inputs['Roughness'].default_value = 0.9
    if texturePath is not None:
        texture = material.node_tree.nodes.new('ShaderNodeTexImage')
        texture.image = bpy.data.images.load(str(texturePath))
        material.node_tree.links.new(texture.outputs['Color'], shader.inputs['Base Color'])
    return material


def RailHeight(distance, settings):
    branch = settings['rail']['lower_branch']
    ratio = (distance - branch['start_distance']) / (branch['end_distance'] - branch['start_distance'])
    transition = 1.0
    if ratio < 0.3:
        transition = ratio / 0.3
    elif ratio > 0.7:
        transition = (1 - ratio) / 0.3
    transition = max(0, min(1, transition))
    weight = transition ** 3 * (transition * (transition * 6 - 15) + 10)
    return settings['rail']['control_points'][0][1] + branch['offset'][1] * weight


def PointAt(obj, target):
    obj.rotation_euler = (Vector(target) - obj.location).to_track_quat('-Z', 'Y').to_euler()


bpy.ops.wm.read_factory_settings(use_empty=True)
scene = bpy.context.scene
scene.render.engine = 'CYCLES'
scene.cycles.samples = 24
scene.cycles.use_denoising = True
scene.render.resolution_x = 1400
scene.render.resolution_y = 820
scene.render.resolution_percentage = 100
scene.view_settings.view_transform = 'AgX'
scene.render.image_settings.file_format = 'PNG'
stage = json.loads((kProjectDirectory / 'resources/Scenes/stage01.json').read_text(encoding='utf-8'))
settings = json.loads((kProjectDirectory / 'resources/Stages/Stage01/settings.json').read_text(encoding='utf-8'))
paletteMaterial = Material('RuntimePalette', kAssetDirectory / 'Runtime/underwater_palette.png', (1, 1, 1))
sandMaterial = Material('ExistingSandFloor', kProjectDirectory / 'resources/Models/Environment/Ocean/textures/sand.png', (0.3, 0.4, 0.35))
meshCache = {}
propObjects = []
for entry in stage['scene']:
    name = entry.get('name', '')
    if not name.startswith(kPropPrefixes + ('LowerRoute_SandFloor',)):
        continue
    modelPath = kProjectDirectory / 'resources/Models' / entry['file_name']
    if entry['file_name'] not in meshCache:
        mesh = LoadMesh(modelPath)
        if name.startswith(kPropPrefixes):
            mesh.materials.append(paletteMaterial)
        else:
            mesh.materials.append(sandMaterial)
        meshCache[entry['file_name']] = mesh
    obj = bpy.data.objects.new(name, meshCache[entry['file_name']])
    scene.collection.objects.link(obj)
    obj.location = entry['transform']['translation']
    obj.scale = entry['transform']['scale']
    obj.rotation_euler.z = entry['transform']['rotation'][2]
    if name.startswith(kPropPrefixes):
        propObjects.append(obj)
bpy.context.view_layer.update()
vertices = []
faces = []
for obj in propObjects:
    offset = len(vertices)
    vertices.extend(obj.matrix_world @ vertex.co for vertex in obj.data.vertices)
    faces.extend(tuple(offset + index for index in polygon.vertices) for polygon in obj.data.polygons)
tree = BVHTree.FromPolygons(vertices, faces)
blockedSamples = []
for distance in range(1200, 4201, 5):
    height = RailHeight(distance, settings)
    for horizontal in (-15, -7.5, 0, 7.5, 15):
        for vertical in (-12, 0, 12):
            point = Vector((horizontal, distance, height + vertical))
            nearest = tree.find_nearest(point, 2.0)
            if nearest[0] is not None:
                blockedSamples.append({'distance': distance, 'horizontal': horizontal, 'vertical': vertical,
                                       'clearance': nearest[3]})
report = {'checkedProps': len(propObjects), 'sampleCount': 601 * 15,
          'checkedHorizontalRange': [-15, 15], 'checkedVerticalRangeRelativeRail': [-12, 12],
          'requiredSurfaceClearance': 2.0, 'blockedSamples': blockedSamples,
          'previewUsesStageJsonAndRuntimeObj': True,
          'note': 'Blender preview lighting; not an in-game capture. Sparse surface sampling is not full collision verification.'}
(kAssetDirectory / 'placement_verification.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
print('PLACEMENT_CLEARANCE', len(blockedSamples), flush=True)
if '--verify-only' in sys.argv:
    raise SystemExit(0)
world = bpy.data.worlds.new('UnderwaterPreviewWorld')
world.use_nodes = True
world.node_tree.nodes['Background'].inputs['Color'].default_value = (0.035, 0.10, 0.15, 1)
world.node_tree.nodes['Background'].inputs['Strength'].default_value = 0.7
scene.world = world
sunData = bpy.data.lights.new('WaterSun', 'SUN')
sunData.energy = 2.0
sunData.color = (0.55, 0.86, 1.0)
sunData.angle = 0.2
sun = bpy.data.objects.new('WaterSun', sunData)
scene.collection.objects.link(sun)
sun.rotation_euler = (0.25, -0.4, -0.25)
cameraData = bpy.data.cameras.new('PlacementCamera')
camera = bpy.data.objects.new('PlacementCamera', cameraData)
scene.collection.objects.link(camera)
scene.camera = camera
cameraData.lens = 27
cameraData.clip_end = 1600
fillData = bpy.data.lights.new('CameraFill', 'AREA')
fillData.energy = 18000
fillData.shape = 'DISK'
fillData.size = 70
fillData.color = (0.25, 0.9, 0.8)
fill = bpy.data.objects.new('CameraFill', fillData)
scene.collection.objects.link(fill)
for previewId, eye, target in (
        ('placement_01_entry', (12, 1460, -63), (-20, 1600, -75)),
        ('placement_02_arch', (12, 2020, -77), (0, 2180, -83)),
        ('placement_03_bones', (8, 2580, -76), (0, 2780, -83))):
    camera.location = eye
    PointAt(camera, target)
    fill.location = (eye[0], eye[1] + 60, eye[2] + 35)
    PointAt(fill, target)
    scene.render.filepath = str(kPreviewDirectory / (previewId + '.png'))
    bpy.ops.render.render(write_still=True)
bpy.context.preferences.filepaths.save_version = 0
bpy.ops.wm.save_as_mainfile(filepath=str(kAssetDirectory / 'stage01_underwater_placement.blend'))
print('PLACEMENT_PREVIEW_COMPLETE', flush=True)
