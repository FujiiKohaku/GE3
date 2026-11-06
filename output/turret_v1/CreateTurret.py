import bpy
import math
import json
from pathlib import Path
from mathutils import Vector, Matrix

kOutputDirectory = Path(__file__).resolve().parent
kModelDirectory = kOutputDirectory / 'models'
kModelDirectory.mkdir(parents=True, exist_ok=True)


def CreateMaterial(name, color, metallic=0.0, emission=0.0):
    material = bpy.data.materials.new(name)
    material.diffuse_color = (*color, 1.0)
    material.use_nodes = True
    shader = material.node_tree.nodes.get('Principled BSDF')
    shader.inputs['Base Color'].default_value = (*color, 1.0)
    shader.inputs['Metallic'].default_value = metallic
    shader.inputs['Roughness'].default_value = 0.36
    shader.inputs['Emission Color'].default_value = (*color, 1.0)
    shader.inputs['Emission Strength'].default_value = emission
    return material


def CreateBox(name, location, dimensions, material, bevel=0.04):
    bpy.ops.mesh.primitive_cube_add(size=1.0, location=location)
    obj = bpy.context.object
    obj.name = name
    obj.dimensions = dimensions
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    if bevel > 0.0:
        modifier = obj.modifiers.new('EdgeChamfer', 'BEVEL')
        modifier.width = bevel
        modifier.segments = 1
        bpy.ops.object.modifier_apply(modifier=modifier.name)
    obj.data.materials.append(material)
    return obj


def CreateCylinder(name, location, radius, depth, material, vertices=12, rotation=(0.0, 0.0, 0.0)):
    bpy.ops.mesh.primitive_cylinder_add(vertices=vertices, radius=radius, depth=depth, location=location, rotation=rotation)
    obj = bpy.context.object
    obj.name = name
    obj.data.materials.append(material)
    return obj


def CreateTube(name, centerY, length, outerRadius, innerRadius, centerZ, material):
    vertices = []
    faces = []
    for yValue, radius in [(centerY - length * 0.5, outerRadius), (centerY + length * 0.5, outerRadius),
                           (centerY - length * 0.5, innerRadius), (centerY + length * 0.5, innerRadius)]:
        for index in range(8):
            angle = math.tau * index / 8.0 + math.pi / 8.0
            vertices.append((radius * math.cos(angle), yValue, centerZ + radius * math.sin(angle)))
    for index in range(8):
        nextIndex = (index + 1) % 8
        faces.extend([(index, nextIndex, 8 + nextIndex, 8 + index),
                      (16 + index, 24 + index, 24 + nextIndex, 16 + nextIndex),
                      (index, 16 + index, 16 + nextIndex, nextIndex),
                      (8 + index, 8 + nextIndex, 24 + nextIndex, 24 + index)])
    mesh = bpy.data.meshes.new(name)
    mesh.from_pydata(vertices, [], [tuple(reversed(face)) for face in faces])
    mesh.update()
    obj = bpy.data.objects.new(name, mesh)
    bpy.context.collection.objects.link(obj)
    obj.data.materials.append(material)
    return obj


def JoinPart(objects, name, pivot):
    bpy.ops.object.select_all(action='DESELECT')
    for obj in objects:
        obj.select_set(True)
    bpy.context.view_layer.objects.active = objects[0]
    bpy.ops.object.join()
    obj = bpy.context.object
    obj.name = name
    bpy.context.scene.cursor.location = pivot
    bpy.ops.object.origin_set(type='ORIGIN_CURSOR')
    return obj


def SetParent(obj, parent):
    worldMatrix = obj.matrix_world.copy()
    obj.parent = parent
    obj.matrix_world = worldMatrix


def AimAt(obj, point):
    obj.rotation_euler = (Vector(point) - obj.location).to_track_quat('-Z', 'Y').to_euler()


def AddArea(name, location, power, size, color):
    bpy.ops.object.light_add(type='AREA', location=location)
    light = bpy.context.object
    light.name = name
    light.data.energy = power
    light.data.shape = 'DISK'
    light.data.size = size
    light.data.color = color
    AimAt(light, (0.0, -0.5, 0.8))


bpy.ops.object.select_all(action='SELECT')
bpy.ops.object.delete(use_global=False)
graphite = CreateMaterial('Graphite', (0.045, 0.065, 0.085), 0.45)
armor = CreateMaterial('ArmorGrey', (0.19, 0.25, 0.29), 0.5)
orange = CreateMaterial('SignalOrange', (0.95, 0.27, 0.045), 0.2)
steel = CreateMaterial('MachinedSteel', (0.39, 0.48, 0.52), 0.65)
dark = CreateMaterial('BoreBlack', (0.008, 0.014, 0.019), 0.05)
cyan = CreateMaterial('StatusCyan', (0.04, 0.85, 0.92), 0.15, 2.2)

baseParts = [CreateCylinder('Foundation', (0.0, 0.0, 0.17), 1.04, 0.34, graphite, 8),
             CreateCylinder('BaseArmor', (0.0, 0.0, 0.38), 0.83, 0.18, armor, 8),
             CreateCylinder('Bearing', (0.0, 0.0, 0.52), 0.54, 0.15, steel, 16)]
for index in range(4):
    angle = math.pi * 0.25 + index * math.pi * 0.5
    xValue = math.cos(angle) * 0.87
    yValue = math.sin(angle) * 0.87
    foot = CreateBox('AnchorFoot', (xValue, yValue, 0.16), (0.56, 0.82, 0.32), armor, 0.08)
    foot.rotation_euler.z = angle - math.pi * 0.5
    baseParts.append(foot)
    cap = CreateBox('FootOrange', (xValue, yValue, 0.33), (0.39, 0.5, 0.065), orange, 0.025)
    cap.rotation_euler.z = angle - math.pi * 0.5
    baseParts.append(cap)
base = JoinPart(baseParts, 'TurretBase', (0.0, 0.0, 0.0))

yawParts = [CreateCylinder('YawRing', (0.0, 0.0, 0.65), 0.65, 0.19, graphite, 12)]
for side in [-1.0, 1.0]:
    yawParts.append(CreateBox('Yoke', (side * 0.51, 0.0, 0.93), (0.21, 0.71, 0.62), armor, 0.06))
    yawParts.append(CreateCylinder('PitchJoint', (side * 0.64, 0.0, 1.12), 0.205, 0.09, orange, 12, (0.0, math.pi * 0.5, 0.0)))
    yawParts.append(CreateCylinder('JointHub', (side * 0.695, 0.0, 1.12), 0.1, 0.035, graphite, 10, (0.0, math.pi * 0.5, 0.0)))
yaw = JoinPart(yawParts, 'TurretYaw', (0.0, 0.0, 0.62))

barrelParts = [CreateBox('Receiver', (0.0, 0.02, 1.15), (0.79, 1.12, 0.64), graphite, 0.09),
               CreateBox('UpperArmor', (0.0, 0.1, 1.49), (0.79, 0.94, 0.12), orange, 0.04),
               CreateBox('RearCap', (0.0, 0.62, 1.16), (0.65, 0.17, 0.49), armor, 0.04),
               CreateBox('FrontShield', (0.0, -0.53, 1.15), (0.94, 0.23, 0.74), orange, 0.07),
               CreateBox('Sensor', (0.0, -0.67, 1.44), (0.24, 0.035, 0.075), cyan, 0.012),
               CreateTube('Barrel', -1.33, 1.4, 0.19, 0.11, 1.15, steel),
               CreateTube('BarrelShroud', -0.91, 0.49, 0.27, 0.192, 1.15, armor),
               CreateTube('MuzzleBrake', -2.03, 0.4, 0.32, 0.145, 1.15, graphite),
               CreateTube('MuzzleLip', -2.245, 0.05, 0.29, 0.145, 1.15, orange),
               CreateCylinder('BoreInterior', (0.0, -1.68, 1.15), 0.11, 0.015, dark, 12, (math.pi * 0.5, 0.0, 0.0))]
for side in [-1.0, 1.0]:
    barrelParts.append(CreateBox('CheekArmor', (side * 0.423, 0.13, 1.17), (0.11, 0.7, 0.4), armor, 0.025))
    for index in range(3):
        barrelParts.append(CreateBox('CoolingVent', (side * 0.487, 0.02 + index * 0.15, 1.18), (0.015, 0.065, 0.22), dark, 0.0))
pitch = JoinPart(barrelParts, 'TurretBarrel', (0.0, 0.0, 1.12))
SetParent(yaw, base)
SetParent(pitch, yaw)
bpy.ops.object.empty_add(type='ARROWS', location=(0.0, -2.28, 1.15))
muzzle = bpy.context.object
muzzle.name = 'MuzzlePoint'
SetParent(muzzle, pitch)

# OBJは各パーツの回転軸が原点になるローカル座標で書き出す。
for obj in [base, yaw, pitch]:
    bpy.ops.object.select_all(action='DESELECT')
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    savedMatrix = obj.matrix_world.copy()
    savedParent = obj.parent
    obj.parent = None
    obj.matrix_world = Matrix.Identity(4)
    bpy.ops.wm.obj_export(filepath=str(kModelDirectory / (obj.name + '.obj')), export_selected_objects=True,
                          forward_axis='NEGATIVE_Z', up_axis='Y', export_materials=True, export_triangulated_mesh=True)
    obj.parent = savedParent
    obj.matrix_world = savedMatrix

bpy.ops.object.select_all(action='DESELECT')
for obj in [base, yaw, pitch, muzzle]:
    obj.select_set(True)
bpy.ops.export_scene.gltf(filepath=str(kModelDirectory / 'Turret.glb'), export_format='GLB', use_selection=True)

scene = bpy.context.scene
scene.render.engine = 'CYCLES'
scene.cycles.samples = 48
scene.cycles.use_denoising = True
scene.render.resolution_x = 1280
scene.render.resolution_y = 960
scene.render.resolution_percentage = 100
scene.world.color = (0.12, 0.12, 0.12)
scene.view_settings.view_transform = 'AgX'
floorMaterial = CreateMaterial('StudioFloor', (0.055, 0.075, 0.095))
CreateBox('StudioGround', (0.0, 0.0, -0.11), (200.0, 200.0, 0.2), floorMaterial, 0.0)
AddArea('Key', (2.0, -4.0, 6.0), 950.0, 5.0, (0.8, 0.91, 1.0))
AddArea('Fill', (-4.0, -1.0, 3.0), 700.0, 4.0, (0.6, 0.8, 1.0))
AddArea('Rim', (1.0, 4.0, 5.0), 1300.0, 3.0, (1.0, 0.65, 0.35))
bpy.ops.object.camera_add(location=(5.4, -7.4, 4.4))
camera = bpy.context.object
camera.name = 'PreviewCamera'
camera.data.type = 'ORTHO'
camera.data.ortho_scale = 5.6
AimAt(camera, (0.0, -0.4, 0.8))
scene.camera = camera
yaw.rotation_euler.z = math.radians(-12.0)
pitch.rotation_euler.x = math.radians(-8.0)
bpy.ops.wm.save_as_mainfile(filepath=str(kOutputDirectory / 'Turret.blend'))
scene.render.filepath = str(kOutputDirectory / 'TurretPreview.png')
bpy.ops.render.render(write_still=True)

triangleCount = 0
for obj in [base, yaw, pitch]:
    obj.data.calc_loop_triangles()
    triangleCount += len(obj.data.loop_triangles)
metadata = {'triangles': triangleCount, 'parts': ['TurretBase', 'TurretYaw', 'TurretBarrel'],
            'muzzle': 'MuzzlePoint', 'blenderForward': '-Y', 'objForward': '+Z', 'objUp': '+Y',
            'yawPivotBlender': [0, 0, 0.62], 'pitchPivotRelativeToYawBlender': [0, 0, 0.5]}
(kOutputDirectory / 'ModelInfo.json').write_text(json.dumps(metadata, indent=2), encoding='utf-8')
print('TURRET_COMPLETE', json.dumps(metadata))
