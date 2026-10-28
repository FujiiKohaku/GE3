"""Create eight Stage01 underwater props and render their actual geometry."""
import bpy
import math
import random
import json
from pathlib import Path
from mathutils import Vector

kAssetDirectory = Path(__file__).resolve().parents[2] / 'resources/Models/Environment/Ocean/UnderwaterProps'
kRandomSeed = 281026
random.seed(kRandomSeed)
assetObjects = []
assetReports = []


def MakeMaterial(name, color, emission=0.0, metallic=0.0):
    material = bpy.data.materials.new(name)
    material.use_nodes = True
    material.diffuse_color = (*color, 1)
    shader = material.node_tree.nodes.get('Principled BSDF')
    shader.inputs['Base Color'].default_value = (*color, 1)
    shader.inputs['Roughness'].default_value = 0.76
    shader.inputs['Metallic'].default_value = metallic
    shader.inputs['Emission Color'].default_value = (*color, 1)
    shader.inputs['Emission Strength'].default_value = emission
    image = bpy.data.images.new(name + 'Texture', width=8, height=8)
    image.pixels = [*color, 1] * 64
    image.filepath_raw = str(kAssetDirectory / 'textures' / (name + '.png'))
    image.file_format = 'PNG'
    image.save()
    textureNode = material.node_tree.nodes.new('ShaderNodeTexImage')
    textureNode.image = image
    material.node_tree.links.new(textureNode.outputs['Color'], shader.inputs['Base Color'])
    return material


def Finish(obj, name, material):
    obj.name = name
    obj.data.materials.append(material)
    assetObjects.append(obj)
    return obj


def Rock(name, position, scale, material):
    bpy.ops.mesh.primitive_ico_sphere_add(subdivisions=1, radius=1, location=position)
    obj = bpy.context.object
    obj.scale = scale
    obj.rotation_euler = (random.random(), random.random(), random.random())
    return Finish(obj, name, material)


def Block(name, position, scale, material, rotation=0):
    bpy.ops.mesh.primitive_cube_add(size=1, location=position)
    obj = bpy.context.object
    obj.scale = scale
    obj.rotation_euler.z = rotation
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    bevel = obj.modifiers.new('WornEdges', 'BEVEL')
    bevel.width = 0.09
    bevel.segments = 1
    bpy.ops.object.modifier_apply(modifier=bevel.name)
    return Finish(obj, name, material)


def Rod(name, start, end, radius, material, tipRadius=None, sides=8):
    if tipRadius is None:
        tipRadius = radius
    direction = Vector(end) - Vector(start)
    bpy.ops.mesh.primitive_cone_add(vertices=sides, radius1=radius, radius2=tipRadius,
                                  depth=direction.length, location=(Vector(start) + Vector(end)) * 0.5)
    obj = bpy.context.object
    obj.rotation_euler = direction.to_track_quat('Z', 'Y').to_euler()
    return Finish(obj, name, material)


def Tube(name, points, radius, material, sides=6):
    vertices = []
    faces = []
    for pointIndex, point in enumerate(points):
        previousIndex = max(0, pointIndex - 1)
        nextIndex = min(len(points) - 1, pointIndex + 1)
        direction = (Vector(points[nextIndex]) - Vector(points[previousIndex])).normalized()
        side = direction.cross(Vector((0, 1, 0))).normalized()
        around = direction.cross(side).normalized()
        for cornerIndex in range(sides):
            angle = math.tau * cornerIndex / sides
            vertices.append(Vector(point) + radius * (side * math.cos(angle) + around * math.sin(angle)))
        if pointIndex > 0:
            for cornerIndex in range(sides):
                nextCorner = (cornerIndex + 1) % sides
                first = (pointIndex - 1) * sides
                second = pointIndex * sides
                faces.append((first + cornerIndex, first + nextCorner, second + nextCorner, second + cornerIndex))
    faces.append(tuple(reversed(range(sides))))
    faces.append(tuple((len(points) - 1) * sides + index for index in range(sides)))
    mesh = bpy.data.meshes.new(name + 'Mesh')
    mesh.from_pydata(vertices, [], faces)
    mesh.update()
    obj = bpy.data.objects.new(name, mesh)
    bpy.context.scene.collection.objects.link(obj)
    return Finish(obj, name, material)


def Coral(position, size):
    root = Vector(position)
    for index in range(5):
        angle = index * 2.4
        tip = root + Vector((math.cos(angle) * size * 0.5, math.sin(angle) * size * 0.4, size * (0.65 + index * 0.1)))
        Rod('CoralBranch', root, tip, size * 0.09, coralMaterial, size * 0.045, 5)
        for side in (-1, 1):
            split = tip + Vector((size * 0.25 * side, 0, size * 0.23))
            Rod('CoralTip', tip, split, size * 0.05, coralTipMaterial, size * 0.025, 5)


def Crystal(position, height, radius=0.4):
    base = Vector(position)
    top = base + Vector((0, 0, height * 0.72))
    body = Rod('CrystalPrism', base, top, radius, crystalMaterial, radius * 0.88, 6)
    tip = Rod('CrystalTip', top, base + Vector((0, 0, height)), radius * 0.88, crystalTipMaterial, 0, 6)
    return body, tip


def ScatterRubble(count, radius):
    for index in range(count):
        angle = random.uniform(0, math.tau)
        distance = random.uniform(radius * 0.6, radius)
        size = random.uniform(0.18, 0.5)
        Rock('BrokenStone', (math.cos(angle) * distance, math.sin(angle) * distance, size * 0.2),
             (size * 1.5, size, size * 0.6), stoneMaterial)


def MakeArch():
    for side in (-1, 1):
        for layer in range(4):
            Block('ArchPier', (side * 3.4, 0, 0.48 + layer * 0.93), (1.35, 1.8, 0.87), stoneMaterial,
                  random.uniform(-0.025, 0.025))
        Block('ArchFoot', (side * 3.4, 0, 0.22), (1.9, 2.2, 0.45), darkStoneMaterial)
    for index in range(11):
        if index in (2, 3):
            continue
        angle = math.pi * (index + 0.5) / 11
        obj = Block('ArchVoussoir', (3.4 * math.cos(angle), 0, 3.2 + 3.4 * math.sin(angle)),
                    (0.89, 1.8, 1.2), stoneMaterial)
        obj.rotation_euler.y = math.pi * 0.5 - angle
    for index in range(3):
        Block('FallenArchStone', (3.0 + index * 0.65, -1.5, 0.3 + index * 0.12), (1.2, 0.9, 0.7), stoneMaterial, index * 0.6)
    Coral((-3.6, -0.8, 1.4), 1.0)
    Coral((3.8, 0.4, 0.3), 1.2)
    for index in range(9):
        Rock('ShellColony', (-3.9 + random.random(), -0.94, 0.8 + index * 0.38), (0.18, 0.1, 0.13), shellMaterial)
    ScatterRubble(12, 4.6)


def MakeCrystals():
    Rock('CrystalBed', (0, 0, 0.3), (3.8, 2.7, 0.65), darkStoneMaterial)
    for index in range(13):
        angle = index * 2.4
        distance = random.uniform(0, 2.7)
        height = random.uniform(1.0, 3.5)
        if index == 0:
            height = 5.3
            distance = 0
        Crystal((math.cos(angle) * distance, math.sin(angle) * distance, 0.35), height, height * 0.16)
    Coral((-2.7, -1.4, 0), 0.7)
    ScatterRubble(13, 3.8)


def MakeStatue():
    Block('BuriedShoulder', (0, 0.5, 0.5), (7.8, 2.5, 1.4), darkStoneMaterial)
    Block('AncientHead', (0, 0, 3.2), (3.7, 2.7, 4.6), stoneMaterial)
    Block('CrownBand', (0, 0, 5.5), (4.25, 3.0, 0.7), darkStoneMaterial)
    for side in (-1, 1):
        Block('Brow', (side * 0.95, -1.47, 4.05), (1.5, 0.5, 0.4), darkStoneMaterial, side * 0.12)
        Block('EyeSocket', (side * 0.95, -1.45, 3.6), (1.1, 0.15, 0.55), darkStoneMaterial)
        Block('GlowingEye', (side * 0.95, -1.56, 3.62), (0.65, 0.12, 0.16), crystalTipMaterial)
        Block('Ear', (side * 2.0, 0, 3.2), (0.6, 1.3, 1.4), darkStoneMaterial)
    Block('Nose', (0, -1.6, 3.1), (0.55, 0.8, 1.1), stoneMaterial)
    Block('Mouth', (0, -1.39, 2.1), (1.65, 0.12, 0.13), darkStoneMaterial)
    for index in range(5):
        Block('CrownTooth', ((index - 2) * 0.75, 0, 6.12), (0.48, 1.5, 0.8), stoneMaterial)
    Tube('FaceFracture', [(0.9, -1.38, 5.1), (0.45, -1.4, 4.55), (0.7, -1.42, 4.15)], 0.045, darkStoneMaterial)
    Rock('SandDrift', (-0.4, -0.5, 0.3), (4.8, 2.8, 0.9), sandMaterial)
    Coral((1.8, -0.7, 5.65), 1.1)
    Coral((-3, -0.5, 0.5), 1.0)
    ScatterRubble(13, 4.8)


def MakeRotor():
    Block('RotorPlinth', (0, 0, 0.35), (6.5, 3.5, 0.7), darkStoneMaterial)
    for side in (-1, 1):
        Block('RotorSupport', (side * 2.8, 0.15, 1.7), (0.95, 1.9, 2.6), stoneMaterial)
    bpy.ops.object.empty_add(location=(0, 0, 3.5))
    rotorRoot = bpy.context.object
    rotorRoot.name = 'AnimatedStoneRing'
    assetObjects.append(rotorRoot)
    for index in range(16):
        angle = math.tau * index / 16
        obj = Block('RingSegment', (2.5 * math.cos(angle), 0, 3.5 + 2.5 * math.sin(angle)),
                    (0.9, 0.8, 0.9), stoneMaterial)
        obj.rotation_euler.y = -angle
        obj.parent = rotorRoot
        obj.matrix_parent_inverse = rotorRoot.matrix_world.inverted()
        rune = Block('LuminousInlay', (2.5 * math.cos(angle), -0.43, 3.5 + 2.5 * math.sin(angle)),
                     (0.15, 0.08, 0.48), crystalMaterial)
        rune.rotation_euler.y = -angle
        rune.parent = rotorRoot
        rune.matrix_parent_inverse = rotorRoot.matrix_world.inverted()
    rotorRoot.rotation_euler.y = 0
    rotorRoot.keyframe_insert(data_path='rotation_euler', frame=1)
    rotorRoot.rotation_euler.y = math.tau
    rotorRoot.keyframe_insert(data_path='rotation_euler', frame=241)
    for curve in rotorRoot.animation_data.action.fcurves:
        for key in curve.keyframe_points:
            key.interpolation = 'LINEAR'
    Coral((-2.8, -1, 0.6), 1.1)
    ScatterRubble(10, 3.8)


def MakeSkeleton():
    for index in range(7):
        positionY = (index - 3) * 1.4
        radius = 3.4 - abs(index - 3) * 0.24
        for side in (-1, 1):
            points = []
            for step in range(11):
                angle = math.pi * 0.5 * step / 10
                points.append((side * radius * math.cos(angle), positionY, 0.4 + radius * 1.35 * math.sin(angle)))
            Tube('GiantRib', points, 0.24, boneMaterial, 7)
        Rock('Vertebra', (0, positionY, 0.33), (0.65, 0.7, 0.5), boneMaterial)
        Rod('SpinalCrest', (0, positionY, 0.5), (0, positionY, 1.1), 0.24, boneMaterial, 0.08)
    Coral((3.15, -2, 0), 1.1)
    ScatterRubble(12, 5)


def MakeVents():
    for index, position in enumerate(((-1.5, 0, 3.6), (1, 0.8, 5.0), (1.5, -1, 2.4))):
        positionX, positionY, height = position
        Rock('VentBase', (positionX, positionY, 0.35), (1.7, 1.5, 0.8), darkStoneMaterial)
        Rod('VentChimney', (positionX, positionY, 0.3), (positionX, positionY, height), 0.9,
            darkStoneMaterial, 0.48, 9)
        bpy.ops.mesh.primitive_torus_add(major_radius=0.43, minor_radius=0.16, major_segments=12,
                                      minor_segments=5, location=(positionX, positionY, height))
        Finish(bpy.context.object, 'MineralVentLip', mineralMaterial)
        Rod('DarkVentOpening', (positionX, positionY, height - 0.08), (positionX, positionY, height),
            0.32, blackMaterial)
        for bubbleIndex in range(6):
            bubble = Rock('PreviewBubble', (positionX + math.sin(bubbleIndex * 2) * 0.22,
                          positionY, height + 0.5 + bubbleIndex * 0.42), (0.11, 0.11, 0.16), bubbleMaterial)
            bubble['isPreviewEffect'] = True
    ScatterRubble(13, 3.5)


def MakeKelp():
    for index in range(15):
        baseX = random.uniform(-2.8, 2.8)
        baseY = random.uniform(-1.5, 1.5)
        height = random.uniform(2.6, 6.5)
        vertices = []
        faces = []
        for step in range(13):
            ratio = step / 12
            centerX = baseX + math.sin(ratio * 5 + index) * ratio * 0.7
            centerY = baseY + math.sin(ratio * 6 + index) * ratio * 0.3
            width = math.sin(math.pi * ratio) * 0.32 + 0.035
            vertices.extend([(centerX - width, centerY, height * ratio),
                             (centerX + width, centerY, height * ratio),
                             (centerX, centerY - 0.12, height * ratio)])
            if step > 0:
                first = (step - 1) * 3
                faces.extend([(first, first + 3, first + 5, first + 2),
                              (first + 2, first + 5, first + 4, first + 1)])
        mesh = bpy.data.meshes.new('KelpBladeMesh')
        mesh.from_pydata(vertices, [], faces)
        mesh.materials.append(kelpMaterial)
        mesh.materials.append(kelpTipMaterial)
        mesh.update()
        for polygon in mesh.polygons:
            if polygon.center.z > height * 0.7:
                polygon.material_index = 1
        mesh.update()
        obj = bpy.data.objects.new('SwayingKelpBlade', mesh)
        bpy.context.scene.collection.objects.link(obj)
        assetObjects.append(obj)
        for frame, tilt in ((1, -0.07), (61, 0.08), (121, -0.07)):
            obj.rotation_euler.y = tilt
            obj.keyframe_insert(data_path='rotation_euler', frame=frame)
        Rock('KelpHoldfast', (baseX, baseY, 0.1), (0.4, 0.35, 0.25), darkStoneMaterial)


def MakePillars():
    for index, placement in enumerate(((-2.5, 0.5, 5.7), (1.9, 1.1, 4.2), (0.2, -1, 1.8))):
        positionX, positionY, height = placement
        Block('PillarFoot', (positionX, positionY, 0.22), (2, 2, 0.44), darkStoneMaterial)
        for level in range(int(height / 0.85)):
            Rod('FlutedColumnDrum', (positionX, positionY, 0.4 + level * 0.85),
                (positionX, positionY, 1.2 + level * 0.85), 0.65, stoneMaterial, sides=10)
        Block('BrokenCapital', (positionX, positionY, height), (1.65, 1.65, 0.45), stoneMaterial, 0.12)
    Rod('FallenColumn', (-3.3, -1.6, 0.55), (2.3, -2.4, 0.65), 0.65, stoneMaterial, sides=10)
    Block('FallenCapital', (2.9, -2.5, 0.55), (1.7, 1.6, 0.8), darkStoneMaterial, 0.3)
    Coral((-2.7, 0.5, 5.95), 0.9)
    Coral((2, -1.5, 0.1), 0.7)
    ScatterRubble(15, 4)


def PointAt(obj, target):
    obj.rotation_euler = (Vector(target) - obj.location).to_track_quat('-Z', 'Y').to_euler()


def ExportAsset(assetId, label, buildFunction):
    global assetObjects
    assetObjects = []
    buildFunction()
    scene.frame_set(1)
    bpy.context.view_layer.update()
    collection = bpy.data.collections.new(assetId)
    scene.collection.children.link(collection)
    for obj in assetObjects:
        for owner in list(obj.users_collection):
            owner.objects.unlink(obj)
        collection.objects.link(obj)
    bpy.ops.object.select_all(action='DESELECT')
    for obj in assetObjects:
        obj.select_set(True)
    bpy.ops.export_scene.gltf(filepath=str(kAssetDirectory / (assetId + '.glb')), use_selection=True,
                              export_animations=True, export_frame_range=True)
    for obj in assetObjects:
        if obj.get('isPreviewEffect', False):
            obj.select_set(False)
    bpy.ops.wm.obj_export(filepath=str(kAssetDirectory / (assetId + '.obj')), export_selected_objects=True,
                          forward_axis='NEGATIVE_Z', up_axis='Y', export_triangulated_mesh=True,
                          path_mode='RELATIVE')
    triangleCount = 0
    for obj in assetObjects:
        if obj.type == 'MESH':
            obj.data.calc_loop_triangles()
            triangleCount += len(obj.data.loop_triangles)
    corners = []
    for obj in assetObjects:
        if obj.type == 'MESH':
            corners.extend(obj.matrix_world @ Vector(corner) for corner in obj.bound_box)
    minimum = Vector(tuple(min(point[axis] for point in corners) for axis in range(3)))
    maximum = Vector(tuple(max(point[axis] for point in corners) for axis in range(3)))
    center = (minimum + maximum) * 0.5
    span = max(maximum.x - minimum.x, maximum.y - minimum.y, maximum.z - minimum.z)
    camera.location = center + Vector((span * 0.95, -span * 1.65, span * 0.85))
    PointAt(camera, center)
    camera.data.ortho_scale = span * 1.45
    floor.scale = (span * 1.4, span * 1.4, 1)
    scene.render.filepath = str(kAssetDirectory / 'previews' / (assetId + '.png'))
    bpy.ops.render.render(write_still=True)
    for obj in assetObjects:
        obj.hide_render = True
    assetReports.append({'id': assetId, 'label': label, 'triangles': triangleCount,
                         'dimensionsBlender': list(maximum - minimum)})


kAssetDirectory.mkdir(parents=True, exist_ok=True)
(kAssetDirectory / 'textures').mkdir(exist_ok=True)
(kAssetDirectory / 'previews').mkdir(exist_ok=True)
bpy.ops.wm.read_factory_settings(use_empty=True)
scene = bpy.context.scene
scene.render.engine = 'CYCLES'
scene.cycles.samples = 24
scene.cycles.use_denoising = True
scene.render.resolution_x = 900
scene.render.resolution_y = 760
scene.render.resolution_percentage = 100
scene.render.image_settings.file_format = 'PNG'
scene.render.film_transparent = False
scene.view_settings.view_transform = 'AgX'
scene.frame_end = 241
scene.render.fps = 30
stoneMaterial = MakeMaterial('AbyssStone', (0.20, 0.31, 0.36))
darkStoneMaterial = MakeMaterial('WeatheredStone', (0.09, 0.16, 0.19))
sandMaterial = MakeMaterial('PaleSand', (0.35, 0.40, 0.36))
coralMaterial = MakeMaterial('PurpleCoral', (0.33, 0.13, 0.32))
coralTipMaterial = MakeMaterial('CoralTips', (0.63, 0.32, 0.48))
shellMaterial = MakeMaterial('ShellIvory', (0.60, 0.62, 0.51))
boneMaterial = MakeMaterial('AncientBone', (0.57, 0.64, 0.56))
crystalMaterial = MakeMaterial('TealCrystal', (0.025, 0.65, 0.58), 0.6, 0.25)
crystalTipMaterial = MakeMaterial('CrystalGlow', (0.20, 0.92, 0.76), 2.0, 0.15)
kelpMaterial = MakeMaterial('DeepKelp', (0.025, 0.23, 0.19))
kelpTipMaterial = MakeMaterial('KelpTips', (0.10, 0.55, 0.34), 0.3)
mineralMaterial = MakeMaterial('VentMinerals', (0.58, 0.40, 0.17))
blackMaterial = MakeMaterial('VentInterior', (0.009, 0.015, 0.02))
bubbleMaterial = MakeMaterial('BubblePreview', (0.20, 0.55, 0.64), 0.2, 0.45)
bpy.ops.mesh.primitive_plane_add(size=2, location=(0, 0, -0.18))
floor = bpy.context.object
floor.name = 'PreviewGround'
floor.data.materials.append(MakeMaterial('PreviewGround', (0.035, 0.075, 0.095)))
world = bpy.data.worlds.new('AbyssStudio')
world.use_nodes = True
world.node_tree.nodes['Background'].inputs['Color'].default_value = (0.07, 0.14, 0.20, 1)
world.node_tree.nodes['Background'].inputs['Strength'].default_value = 0.6
scene.world = world
for name, location, energy, color, size in (
        ('SoftKey', (-6, -8, 14), 2600, (0.63, 0.85, 1.0), 9),
        ('TurquoiseRim', (5, 6, 10), 3200, (0.28, 1.0, 0.85), 7),
        ('WarmFill', (7, -4, 6), 1100, (1.0, 0.76, 0.58), 7)):
    lightData = bpy.data.lights.new(name, 'AREA')
    lightData.energy = energy
    lightData.color = color
    lightData.shape = 'DISK'
    lightData.size = size
    light = bpy.data.objects.new(name, lightData)
    scene.collection.objects.link(light)
    light.location = location
    PointAt(light, (0, 0, 2))
cameraData = bpy.data.cameras.new('PreviewCamera')
camera = bpy.data.objects.new('PreviewCamera', cameraData)
scene.collection.objects.link(camera)
cameraData.type = 'ORTHO'
cameraData.lens = 50
scene.camera = camera
for assetId, label, buildFunction in (
        ('01_broken_arch', '崩れた石のアーチ', MakeArch),
        ('02_crystal_cluster', '発光する結晶群', MakeCrystals),
        ('03_buried_colossus', '半分埋まった巨像', MakeStatue),
        ('04_sunken_rotor', '沈んだ遺跡の回転装置', MakeRotor),
        ('05_giant_ribcage', '巨大な肋骨・骨格', MakeSkeleton),
        ('06_hydrothermal_vents', '熱水噴出口', MakeVents),
        ('07_tall_kelp', '背の高い海藻群', MakeKelp),
        ('08_collapsed_pillars', '崩落した石柱群', MakePillars)):
    ExportAsset(assetId, label, buildFunction)
for collection in scene.collection.children:
    if collection.name.startswith('0'):
        assetIndex = int(collection.name[:2]) - 1
        offset = Vector(((assetIndex % 4) * 15, (assetIndex // 4) * 18, 0))
        for obj in collection.objects:
            obj.hide_render = False
            if obj.parent is None:
                obj.location += offset
floor.location = (22.5, 9, -0.18)
floor.scale = (50, 35, 1)
sunData = bpy.data.lights.new('GallerySun', 'SUN')
sunData.energy = 1.4
sunData.angle = 0.3
sun = bpy.data.objects.new('GallerySun', sunData)
scene.collection.objects.link(sun)
sun.rotation_euler = (0.4, -0.5, -0.3)
scene.camera.location = (62, -73, 70)
PointAt(scene.camera, (22.5, 9, 2))
scene.camera.data.ortho_scale = 72
scene.frame_set(1)
bpy.ops.wm.save_as_mainfile(filepath=str(kAssetDirectory / 'underwater_props.blend'))
(kAssetDirectory / 'asset_info.json').write_text(json.dumps(assetReports, ensure_ascii=False, indent=2), encoding='utf-8')
print('UNDERWATER_PROPS_COMPLETE', json.dumps(assetReports, ensure_ascii=False))
