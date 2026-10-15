"""Generate five static Stage01 enemy appearances with a shared surface palette."""

import bpy
import bmesh
import math
import json
from pathlib import Path
from mathutils import Vector

kAssetDirectory = Path(__file__).resolve().parents[2] / 'resources/Models/Enemy/Ruins'
kEnemyIds = ['NormalEnemy', 'MoveEnemy', 'ArmoredEnemy', 'SwarmEnemy', 'PaintShooterEnemy']
kRadiusLimits = [1.32, 1.35, 1.04, 1.65, 1.32]
kRuntimeScales = [2.0, 2.0, 2.6, 0.58, 2.0]
assetObjects = []
materials = []
kPartNames = ['Body', 'Eye', 'Left', 'Right', 'Tail', 'Cannon', 'Tank']


def GetMotionPart(sceneObject, enemyId):
    name = sceneObject.name.split('.')[0]
    if enemyId == 'SwarmEnemy':
        if name in ['Tail', 'DorsalFin', 'NearFin']:
            return 'Tail'
        return 'Body'
    if name == 'EyeSlit':
        return 'Eye'
    if name in ['Cannon', 'Muzzle', 'HeavyCannon', 'HotMuzzle', 'CannonStone', 'SprayNozzle', 'PaintMuzzle']:
        return 'Cannon'
    if name in ['TankWindowFrame', 'PaintReservoir']:
        return 'Tank'
    if name in ['Stabilizer', 'SidePlate', 'SweptWing', 'WingArmor', 'BronzeClaw', 'ArticulatedArm', 'ArmStone']:
        centerX = sum((sceneObject.matrix_world @ vertex.co).x for vertex in sceneObject.data.vertices) / len(sceneObject.data.vertices)
        if centerX < 0:
            return 'Right'
        return 'Left'
    return 'Body'


def ExportMotionParts(model):
    report = {}
    partCollection = bpy.data.collections.new(model.name + '_Parts')
    bpy.context.scene.collection.children.link(partCollection)
    for partIndex, partName in enumerate(kPartNames):
        part = model.copy()
        part.data = model.data.copy()
        bpy.context.collection.objects.link(part)
        meshData = bmesh.new()
        meshData.from_mesh(part.data)
        layer = meshData.faces.layers.int.get('motionPart')
        removeFaces = [face for face in meshData.faces if face[layer] != partIndex]
        bmesh.ops.delete(meshData, geom=removeFaces, context='FACES')
        if not meshData.faces:
            meshData.free()
            bpy.data.objects.remove(part, do_unlink=True)
            continue
        meshData.to_mesh(part.data)
        meshData.free()
        bpy.ops.object.select_all(action='DESELECT')
        part.select_set(True)
        bpy.context.view_layer.objects.active = part
        bpy.ops.object.material_slot_remove_unused()
        part.name = model.name + '_' + partName
        part.data.calc_loop_triangles()
        report[partName] = len(part.data.loop_triangles)
        bpy.ops.wm.obj_export(filepath=str(kAssetDirectory / (part.name + '.obj')),
            export_selected_objects=True, export_materials=True, export_triangulated_mesh=True,
            forward_axis='NEGATIVE_Z', up_axis='Y', path_mode='RELATIVE')
        partCollection.objects.link(part)
        bpy.context.collection.objects.unlink(part)
        part.select_set(False)
        part.hide_render = True
        part.hide_set(True)
    bpy.ops.object.select_all(action='DESELECT')
    model.select_set(True)
    bpy.context.view_layer.objects.active = model
    return report


def AddMaterial(sceneObject, surfaceIndex):
    sceneObject.data.materials.append(materials[surfaceIndex])
    assetObjects.append(sceneObject)
    return sceneObject


def CreateBox(name, position, size, surfaceIndex, bevelWidth=0.04):
    bpy.ops.mesh.primitive_cube_add(size=1, location=position)
    sceneObject = bpy.context.object
    sceneObject.name = name
    sceneObject.dimensions = size
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    if bevelWidth > 0:
        bevel = sceneObject.modifiers.new('ChippedEdges', 'BEVEL')
        bevel.width = bevelWidth
        bevel.segments = 1
        bpy.ops.object.modifier_apply(modifier=bevel.name)
    return AddMaterial(sceneObject, surfaceIndex)


def CreateOrb(name, position, size, surfaceIndex, subdivisions=1):
    bpy.ops.mesh.primitive_ico_sphere_add(subdivisions=subdivisions, radius=1, location=position)
    sceneObject = bpy.context.object
    sceneObject.name = name
    sceneObject.scale = size
    return AddMaterial(sceneObject, surfaceIndex)


def CreateRing(name, position, radius, tubeRadius, surfaceIndex, segments=12):
    bpy.ops.mesh.primitive_torus_add(major_segments=segments, minor_segments=4,
        major_radius=radius, minor_radius=tubeRadius, location=position, rotation=(math.pi * 0.5, 0, 0))
    sceneObject = bpy.context.object
    sceneObject.name = name
    return AddMaterial(sceneObject, surfaceIndex)


def CreateEye(position, radius):
    CreateRing('EyeFrame', position, radius, radius * 0.23, 1)
    eye = CreateOrb('HostileEye', (position[0], position[1] + 0.04, position[2]), (radius * 0.84, 0.14, radius * 0.84), 2, 2)
    CreateBox('EyeSlit', (position[0], position[1] + 0.195, position[2]), (radius * 0.20, 0.07, radius * 0.75), 1, 0.015)
    return eye


def CreateCrystal(position, radius=0.20):
    CreateOrb('LevitationCrystal', position, (radius, radius * 0.8, radius * 1.65), 3)


def CreateFin(name, points, surfaceIndex):
    center = Vector((0, 0, 0))
    for point in points:
        center += Vector(point)
    center /= len(points)
    vertices = list(points)
    vertices.extend([(center.x, center.y + 0.09, center.z), (center.x, center.y - 0.09, center.z)])
    faces = []
    for index in range(len(points)):
        nextIndex = (index + 1) % len(points)
        faces.extend([(index, nextIndex, len(points)), (nextIndex, index, len(points) + 1)])
    mesh = bpy.data.meshes.new(name + 'Mesh')
    mesh.from_pydata(vertices, [], faces)
    mesh.update()
    sceneObject = bpy.data.objects.new(name, mesh)
    bpy.context.collection.objects.link(sceneObject)
    return AddMaterial(sceneObject, surfaceIndex)


def CreateNormal():
    CreateOrb('GuardianBody', (0, 0, 0), (0.86, 0.68, 0.86), 1, 2)
    CreateEye((0, 0.66, 0), 0.43)
    for position, size, angle in [((-0.53, 0.26, 0.56), (0.66, 0.64, 0.42), -0.45),
            ((0.44, 0.18, 0.68), (0.68, 0.64, 0.44), 0.42),
            ((-0.44, 0.27, -0.56), (0.68, 0.64, 0.40), 0.4)]:
        armor = CreateBox('StoneArmor', position, size, 0, 0.10)
        armor.rotation_euler.y = angle
    for side in [-1, 1]:
        CreateBox('Stabilizer', (side * 1.0, 0, 0), (0.43, 0.28, 0.12), 1)
        CreateFin('SidePlate', [(side * 1.17, 0, -0.26), (side * 1.32, 0, 0), (side * 1.17, 0, 0.26)], 0)
    CreateCrystal((0, 0, -1.03))


def CreateGlider():
    CreateOrb('GliderBody', (0, 0, 0), (0.46, 0.66, 0.30), 1, 2)
    for side in [-1, 1]:
        CreateFin('SweptWing', [(side * 0.22, 0.28, 0.04), (side * 1.72, -0.30, 0.30),
            (side * 1.25, -0.57, -0.02), (side * 0.35, -0.42, -0.12)], 1)
        armor = CreateBox('WingArmor', (side * 0.78, -0.12, 0.10), (0.66, 0.37, 0.14), 0, 0.06)
        armor.rotation_euler.z = side * -0.20
    CreateFin('Tail', [(0, -0.30, 0.12), (0, -0.62, 0.60), (0, -0.74, 0.05)], 1)
    CreateEye((0, 0.63, 0.03), 0.23)
    CreateBox('Cannon', (0, 0.54, -0.26), (0.25, 0.66, 0.22), 1)
    CreateOrb('Muzzle', (0, 0.90, -0.26), (0.09, 0.025, 0.08), 2)
    CreateCrystal((0, -0.08, -0.47), 0.17)


def CreateArmored():
    CreateOrb('HeavyBody', (0, 0, -0.10), (0.80, 0.62, 0.56), 1, 2)
    CreateOrb('StoneShell', (0, -0.12, 0.47), (1.00, 0.76, 0.76), 0, 2)
    for side in [-1, 1]:
        CreateBox('ShellPanel', (side * 0.52, 0.40, 0.44), (0.52, 0.42, 0.65), 0, 0.11)
        CreateFin('BronzeClaw', [(side * 0.63, 0.30, -0.18), (side * 1.05, 0.52, -0.60),
            (side * 0.82, 0.70, -0.83), (side * 0.78, 0.43, -0.39)], 1)
    CreateEye((0, 0.63, 0.12), 0.24)
    CreateBox('HeavyCannon', (0, 0.74, -0.38), (0.63, 0.95, 0.48), 1, 0.10)
    CreateBox('CannonStone', (0, 0.58, -0.14), (0.67, 0.38, 0.22), 0)
    CreateBox('HotMuzzle', (0, 1.23, -0.38), (0.38, 0.04, 0.26), 2, 0.02)
    CreateCrystal((0, -0.10, -0.81), 0.19)


def CreateSwarm():
    CreateFin('FishHull', [(-1.40, 0, 0), (0.30, 0.35, 0.40), (0.92, 0, 0), (0.30, -0.35, -0.28)], 1)
    CreateFin('Tail', [(0.75, 0, 0), (1.35, 0, 0.58), (1.15, 0, 0), (1.35, 0, -0.58)], 1)
    CreateFin('DorsalFin', [(-0.35, 0, 0.18), (0.1, 0, 0.90), (0.48, 0, 0.22)], 1)
    CreateFin('NearFin', [(-0.40, 0.18, 0), (0.40, 0.84, -0.40), (0.60, 0.22, -0.05)], 1)
    CreateBox('StonePlate', (0.06, 0.10, 0.33), (0.68, 0.44, 0.15), 0, 0.05)
    for side in [-1, 1]:
        CreateOrb('SmallEye', (-0.78, side * 0.20, 0.08), (0.16, 0.06, 0.16), 2)


def CreatePainter():
    CreateOrb('TankBody', (0, -0.10, 0.25), (0.76, 0.64, 0.76), 1, 2)
    CreateBox('TankWindowFrame', (-0.37, 0.47, 0.35), (0.77, 0.20, 0.82), 1, 0.12)
    CreateOrb('PaintReservoir', (-0.37, 0.60, 0.35), (0.31, 0.10, 0.35), 4, 1)
    CreateBox('TankLid', (0, -0.10, 0.95), (0.70, 0.68, 0.20), 0, 0.06)
    CreateEye((0.45, 0.47, 0.35), 0.24)
    for side in [-1, 1]:
        for depth in [-0.34, 0.35]:
            CreateFin('ArticulatedArm', [(side * 0.47, depth, -0.13), (side * 1.04, depth + 0.22, -0.38),
                (side * 1.17, depth + 0.35, -0.84), (side * 0.89, depth + 0.12, -0.55)], 1)
            armor = CreateBox('ArmStone', (side * 0.95, depth + 0.18, -0.50), (0.27, 0.35, 0.40), 0, 0.05)
            armor.rotation_euler.y = side * 0.40
    CreateBox('SprayNozzle', (0, 0.57, -0.32), (0.40, 0.90, 0.36), 1, 0.06)
    CreateOrb('PaintMuzzle', (0, 1.04, -0.32), (0.15, 0.04, 0.12), 4)
    CreateCrystal((0, -0.10, -0.84), 0.17)


def CreatePalette():
    for name, color, roughness, metallic, emission in [('RuinsStone', (0.65, 0.49, 0.29), 0.88, 0, 0),
            ('RuinsMetal', (0.085, 0.16, 0.18), 0.48, 0.65, 0),
            ('RuinsEye', (1.0, 0.20, 0.025), 0.35, 0, 1.8),
            ('RuinsCrystal', (0.025, 0.62, 0.76), 0.35, 0, 1.6),
            ('RuinsPaint', (0.63, 0.035, 0.55), 0.35, 0, 1.6)]:
        material = bpy.data.materials.new(name)
        material.use_nodes = True
        shader = material.node_tree.nodes.get('Principled BSDF')
        shader.inputs['Base Color'].default_value = (*color, 1)
        shader.inputs['Roughness'].default_value = roughness
        shader.inputs['Metallic'].default_value = metallic
        shader.inputs['Emission Color'].default_value = (*color, 1)
        shader.inputs['Emission Strength'].default_value = emission
        image = bpy.data.images.new(name + 'Texture', width=64, height=64)
        image.pixels = [*color, 1] * (64 * 64)
        image.filepath_raw = str(kAssetDirectory / 'textures' / (name + '.png'))
        image.file_format = 'PNG'
        image.save()
        texture = material.node_tree.nodes.new('ShaderNodeTexImage')
        texture.image = image
        material.node_tree.links.new(texture.outputs['Color'], shader.inputs['Base Color'])
        materials.append(material)


def Main():
    (kAssetDirectory / 'textures').mkdir(parents=True, exist_ok=True)
    (kAssetDirectory / 'previews').mkdir(exist_ok=True)
    bpy.ops.object.select_all(action='SELECT')
    bpy.ops.object.delete(use_global=False)
    CreatePalette()
    report = {}
    builders = [CreateNormal, CreateGlider, CreateArmored, CreateSwarm, CreatePainter]
    for index, builder in enumerate(builders):
        assetObjects.clear()
        builder()
        bpy.context.view_layer.update()
        for sceneObject in assetObjects:
            partName = GetMotionPart(sceneObject, kEnemyIds[index])
            attribute = sceneObject.data.attributes.new('motionPart', 'INT', 'FACE')
            for element in attribute.data:
                element.value = kPartNames.index(partName)
        bpy.ops.object.select_all(action='DESELECT')
        for sceneObject in assetObjects:
            sceneObject.select_set(True)
        bpy.context.view_layer.objects.active = assetObjects[0]
        bpy.ops.object.join()
        model = bpy.context.object
        model.name = kEnemyIds[index]
        bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
        radius = max(vertex.co.length for vertex in model.data.vertices)
        for vertex in model.data.vertices:
            vertex.co *= kRadiusLimits[index] / radius
        bpy.ops.object.mode_set(mode='EDIT')
        bpy.ops.mesh.select_all(action='SELECT')
        bpy.ops.mesh.normals_make_consistent(inside=False)
        bpy.ops.uv.smart_project(island_margin=0.02)
        bpy.ops.object.mode_set(mode='OBJECT')
        for polygon in model.data.polygons:
            surfaceIndex = materials.index(model.data.materials[polygon.material_index])
            for loopIndex in polygon.loop_indices:
                model.data.uv_layers.active.data[loopIndex].uv.x += surfaceIndex * 2
        model.data.calc_loop_triangles()
        triangleCount = len(model.data.loop_triangles)
        parts = ExportMotionParts(model)
        if sum(parts.values()) != triangleCount:
            raise RuntimeError('Part splitting changed the triangle count.')
        bpy.ops.wm.obj_export(filepath=str(kAssetDirectory / (model.name + '.obj')),
            export_selected_objects=True, export_materials=True, export_triangulated_mesh=True,
            forward_axis='NEGATIVE_Z', up_axis='Y', path_mode='RELATIVE')
        report[model.name] = {'triangles': triangleCount, 'materials': len(model.data.materials),
            'modelRadius': kRadiusLimits[index], 'runtimeScale': kRuntimeScales[index],
            'worldRadius': kRadiusLimits[index] * kRuntimeScales[index], 'parts': parts}
        position = [(4, 0, 2.4), (0, 0, 2.4), (-4, 0, 2.4), (2, 0, -1.6), (-2, 0, -1.6)][index]
        model.location = position
        if index == 2:
            model.scale = (1.25, 1.25, 1.25)
        if index == 3:
            model.scale = (0.65, 0.65, 0.65)
    scene = bpy.context.scene
    scene.render.engine = 'CYCLES'
    scene.cycles.samples = 24
    scene.cycles.use_denoising = True
    scene.render.resolution_x = 1500
    scene.render.resolution_y = 1000
    scene.render.resolution_percentage = 100
    scene.render.image_settings.file_format = 'PNG'
    scene.world.color = (0.18, 0.27, 0.34)
    for name, position, energy, size in [('Key', (-5, 8, 10), 2300, 8), ('Fill', (7, 5, 0), 1400, 7)]:
        lightData = bpy.data.lights.new(name, 'AREA')
        lightData.energy = energy
        lightData.size = size
        light = bpy.data.objects.new(name, lightData)
        scene.collection.objects.link(light)
        light.location = position
        light.rotation_euler = (Vector((0, 0, 0)) - light.location).to_track_quat('-Z', 'Y').to_euler()
    cameraData = bpy.data.cameras.new('PreviewCamera')
    camera = bpy.data.objects.new('PreviewCamera', cameraData)
    scene.collection.objects.link(camera)
    camera.location = (2.5, 18, 5.5)
    camera.rotation_euler = (Vector((0, 0, 0.4)) - camera.location).to_track_quat('-Z', 'Y').to_euler()
    cameraData.type = 'ORTHO'
    cameraData.ortho_scale = 12.6
    scene.camera = camera
    scene.render.filepath = str(kAssetDirectory / 'previews/ruins_enemies.png')
    bpy.ops.wm.save_as_mainfile(filepath=str(kAssetDirectory / 'ruins_enemies.blend'))
    bpy.ops.render.render(write_still=True)
    (kAssetDirectory / 'asset_info.json').write_text(json.dumps(report, indent=2), encoding='utf8')
    print('RUINS_ENEMIES_RESULT ' + json.dumps(report), flush=True)


Main()
