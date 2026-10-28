"""Create distant underwater landmarks, export assets, and render a depth preview."""
import bpy
import math
import random
import json
from pathlib import Path
from mathutils import Vector

kProjectDirectory = Path(__file__).resolve().parents[2]
kAssetDirectory = kProjectDirectory / 'resources/Models/Environment/Ocean/DistantRuins'
kPreviewDirectory = kAssetDirectory / 'previews'
kRandomSeed = 281127
random.seed(kRandomSeed)
assetParts = []
assetReports = []


def Material(name, color, emission=0.0):
    material = bpy.data.materials.new(name)
    material.diffuse_color = (*color, 1)
    material.use_nodes = True
    shader = material.node_tree.nodes['Principled BSDF']
    shader.inputs['Roughness'].default_value = 0.87
    shader.inputs['Emission Color'].default_value = (*color, 1)
    shader.inputs['Emission Strength'].default_value = emission
    texture = bpy.data.images.new(name + 'Texture', width=8, height=8)
    texture.pixels = [*color, 1] * 64
    texture.filepath_raw = str(kAssetDirectory / 'textures' / (name + '.png'))
    texture.file_format = 'PNG'
    texture.save()
    imageNode = material.node_tree.nodes.new('ShaderNodeTexImage')
    imageNode.image = texture
    material.node_tree.links.new(imageNode.outputs['Color'], shader.inputs['Base Color'])
    texture.pack()
    return material


def Finish(obj, name, material):
    obj.name = name
    obj.data.materials.append(material)
    assetParts.append(obj)
    return obj


def Block(name, center, size, material, bevelWidth=0.4):
    bpy.ops.mesh.primitive_cube_add(size=1, location=center)
    obj = bpy.context.object
    obj.scale = size
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    if bevelWidth > 0:
        bevel = obj.modifiers.new('WeatheredCorners', 'BEVEL')
        bevel.width = bevelWidth
        bevel.segments = 1
        bpy.ops.object.modifier_apply(modifier=bevel.name)
    return Finish(obj, name, material)


def Pillar(positionX, positionY, height, radius=4.2):
    Block('ColumnFoot', (positionX, positionY, 15), (radius * 3, radius * 3, 5), darkStone)
    for index in range(5):
        bottom = 17.5 + index * height / 5
        bpy.ops.mesh.primitive_cone_add(vertices=10, radius1=radius, radius2=radius * 0.95,
                                      depth=height / 5 - 0.35, location=(positionX, positionY, bottom + height / 10))
        Finish(bpy.context.object, 'ColumnDrum', stone)
    Block('ColumnCapital', (positionX, positionY, 18 + height), (radius * 3.3, radius * 3.3, 5), stone)


def Arch(centerY, springHeight, radius, thickness, depth, count):
    for index in range(count):
        angle = math.pi * (index + 0.5) / count
        obj = Block('PortalArchStone', (radius * math.cos(angle), centerY, springHeight + radius * math.sin(angle)),
                    (radius * math.pi / count * 0.94, depth, thickness), stone)
        obj.rotation_euler.y = math.pi * 0.5 - angle


def Rock(name, center, scale, material):
    bpy.ops.mesh.primitive_ico_sphere_add(subdivisions=1, radius=1, location=center)
    obj = bpy.context.object
    obj.scale = scale
    obj.rotation_euler = (random.uniform(-0.3, 0.3), random.uniform(-0.3, 0.3), random.uniform(-0.5, 0.5))
    return Finish(obj, name, material)


def MakeTemple():
    for index in range(3):
        Block('Terrace', (0, 8, 2 + index * 4), (210 - index * 12, 142 - index * 10, 4), darkStone)
    for index in range(9):
        Block('GrandStair', (0, -98 + index * 5.5, index * 1.5), (76, 7, 3), stone, 0.2)
    for side in (-1, 1):
        for positionY in (-40, 22):
            Pillar(side * 80, positionY, 43)
            Pillar(side * 57, positionY, 43)
        Block('WingEntablature', (side * 68, -9, 67), (62, 90, 8), stone)
        Block('WingCornice', (side * 68, -9, 73), (68, 96, 4), trim)
        for layer in range(5):
            Block('PortalPier', (side * 38, -19, 20 + layer * 11), (13, 26, 10.5), stone)
        Block('PortalPlinth', (side * 38, -19, 15), (21, 35, 6), darkStone)
        Block('PortalInlay', (side * 31, -32.3, 47), (1.2, 0.6, 46), glow)
        Block('Buttress', (side * 46, 36, 46), (16, 38, 64), darkStone)
        for index in range(4):
            Block('BrokenParapet', (side * (46 + index * 15), 23, 79), (7, 8, 9), stone)
    Arch(-19, 70, 38, 12, 26, 17)
    Arch(-4, 70, 30, 5, 7, 15)
    Block('PortalThreshold', (0, -20, 14), (67, 34, 3), trim)
    Block('DarkInnerSanctum', (0, 44, 54), (64, 3, 79), abyss)
    Block('InnerDoorGlowLeft', (-20, 41.8, 58), (1, 0.5, 55), glow)
    Block('InnerDoorGlowRight', (20, 41.8, 58), (1, 0.5, 55), glow)
    Block('InnerDoorGlowTop', (0, 41.8, 86), (41, 0.5, 1), glow)
    Block('CrownStone', (0, -19, 117), (24, 29, 12), darkStone)
    Rock('CrownCrystal', (0, -22, 128), (5, 4, 11), glow)
    for index in range(13):
        side = 1
        if index % 2 == 0:
            side = -1
        Rock('FallenTempleMasonry', (side * random.uniform(70, 113), random.uniform(-80, 60), 1.2),
             (random.uniform(4, 9), random.uniform(4, 7), random.uniform(2, 5)), darkStone)


def MakeCliff():
    vertices = []
    faces = []
    rowCount = 16
    levelCount = 7
    for row in range(rowCount):
        forward = -150 + row * 20
        crest = 125 + 25 * math.sin(row * 0.7) + random.uniform(-10, 10)
        for level in range(levelCount):
            ratio = level / (levelCount - 1)
            horizontal = -30 + 58 * ratio + 8 * math.sin(row * 0.82 + level * 1.1)
            height = crest * ratio
            vertices.append((horizontal, forward + random.uniform(-5, 5), height))
    for row in range(rowCount - 1):
        for level in range(levelCount - 1):
            first = row * levelCount + level
            faces.extend([(first, first + 1, first + levelCount),
                          (first + 1, first + levelCount + 1, first + levelCount)])
    backStart = len(vertices)
    for row in range(rowCount):
        crest = vertices[row * levelCount + levelCount - 1]
        vertices.extend([(90, crest[1], 0), (90, crest[1], crest[2])])
    for row in range(rowCount - 1):
        first = row * levelCount
        back = backStart + row * 2
        faces.append((first + levelCount - 1, back + 1, back + 3, first + 2 * levelCount - 1))
        faces.append((back, back + 2, back + 3, back + 1))
        faces.append((first, first + levelCount, back + 2, back))
    firstCap = list(range(levelCount)) + [backStart + 1, backStart]
    lastCap = list(range((rowCount - 1) * levelCount, rowCount * levelCount))
    lastCap += [backStart + (rowCount - 1) * 2 + 1, backStart + (rowCount - 1) * 2]
    faces.append(tuple(reversed(firstCap)))
    faces.append(tuple(lastCap))
    mesh = bpy.data.meshes.new('StratifiedCliffMesh')
    mesh.from_pydata(vertices, [], faces)
    mesh.update()
    obj = bpy.data.objects.new('StratifiedCliff', mesh)
    scene.collection.objects.link(obj)
    Finish(obj, 'StratifiedCliff', rockStone)
    for row in range(12):
        for layer in range(3):
            Rock('RockShelf', (-20 + layer * 16, -135 + row * 25, 10 + layer * 24),
                 (random.uniform(15, 24), random.uniform(15, 22), random.uniform(9, 15)), rockStone)
    for index in range(6):
        Rock('BrokenBasaltSpire', (random.uniform(25, 60), -120 + index * 46, 120), (9, 12, random.uniform(28, 47)), darkStone)
    for index in range(4):
        Rock('CliffCrystal', (-36, -98 + index * 58, 12 + index * 8), (1.5, 2, 8), glow)


def MakeTowers():
    for towerIndex, placement in enumerate(((-70, 18, 72), (-38, -12, 104), (0, 22, 132),
                                           (42, -8, 86), (75, 28, 115))):
        positionX, positionY, height = placement
        width = 15 + towerIndex % 3 * 3
        Block('TowerFoundation', (positionX, positionY, 4), (width * 1.6, width * 1.5, 8), darkStone)
        for level in range(6):
            Block('TowerStory', (positionX, positionY, 12 + level * (height - 18) / 6),
                  (width, width, (height - 18) / 6 - 0.4), stone, 0.25)
            Block('TowerBelt', (positionX, positionY, 19 + level * (height - 18) / 6),
                  (width + 2, width + 2, 2), trim, 0.15)
        Block('BrokenTowerCrown', (positionX - width * 0.15, positionY, height - 5),
              (width * 0.65, width, 12), darkStone)
        for side in (-1, 1):
            Block('TowerVerticalRecess', (positionX + side * width * 0.28, positionY - width * 0.51, height * 0.53),
                  (1.8, 0.6, height * 0.46), abyss, 0.0)
        if towerIndex in (1, 2, 4):
            Block('TowerSignalInlay', (positionX, positionY - width * 0.52, height * 0.65),
                  (0.7, 0.5, height * 0.18), glow, 0.0)
        bpy.ops.mesh.primitive_cone_add(vertices=4, radius1=width * 0.5, radius2=0,
                                      depth=width * 1.1, location=(positionX, positionY, height + 8))
        Finish(bpy.context.object, 'BrokenTowerSpire', stone)
        for index in range(3):
            Rock('TowerRubble', (positionX + random.uniform(-14, 14), positionY + random.uniform(-16, 16), 1),
                 (random.uniform(3, 6), 4, 3), darkStone)
    for side in (-1, 1):
        Block('BrokenSkyBridge', (side * 20, 14, 66), (30, 9, 5), darkStone)


def PointAt(obj, target):
    obj.rotation_euler = (Vector(target) - obj.location).to_track_quat('-Z', 'Y').to_euler()


def BuildAsset(assetId, label, buildFunction):
    global assetParts
    assetParts = []
    buildFunction()
    bpy.ops.object.select_all(action='DESELECT')
    for part in assetParts:
        part.select_set(True)
    bpy.context.view_layer.objects.active = assetParts[0]
    bpy.ops.object.join()
    obj = bpy.context.object
    obj.name = assetId
    scene.cursor.location = (0, 0, 0)
    bpy.ops.object.origin_set(type='ORIGIN_CURSOR')
    bpy.ops.object.transform_apply(location=False, rotation=True, scale=True)
    bpy.ops.wm.obj_export(filepath=str(kAssetDirectory / (assetId + '.obj')), export_selected_objects=True,
                          export_triangulated_mesh=True, forward_axis='NEGATIVE_Z', up_axis='Y', path_mode='RELATIVE')
    bpy.ops.export_scene.gltf(filepath=str(kAssetDirectory / (assetId + '.glb')), use_selection=True, export_animations=False)
    obj.data.calc_loop_triangles()
    bpy.context.view_layer.update()
    assetReports.append({'id': assetId, 'label': label, 'triangles': len(obj.data.loop_triangles),
                         'materials': len(obj.data.materials), 'dimensionsBlender': list(obj.dimensions)})
    return obj


def Instance(source, name, location, scale=(1, 1, 1), rotation=0):
    obj = bpy.data.objects.new(name, source.data)
    scene.collection.objects.link(obj)
    obj.location = location
    obj.scale = scale
    obj.rotation_euler.z = rotation
    return obj


def LoadForegroundArch():
    sourcePath = kProjectDirectory / 'resources/Models/Environment/Ocean/UnderwaterProps/01_broken_arch.obj'
    bpy.ops.object.select_all(action='DESELECT')
    bpy.ops.wm.obj_import(filepath=str(sourcePath), forward_axis='NEGATIVE_Z', up_axis='Y')
    imported = list(bpy.context.selected_objects)
    bpy.context.view_layer.objects.active = imported[0]
    bpy.ops.object.join()
    arch = bpy.context.object
    arch.name = 'PreviewOnly_ExistingForegroundArch'
    scene.cursor.location = (0, 0, 0)
    bpy.ops.object.origin_set(type='ORIGIN_CURSOR')
    arch.scale = (11, 11, 11)
    arch.location = (0, -135, -3)
    return arch


kAssetDirectory.mkdir(parents=True, exist_ok=True)
(kAssetDirectory / 'textures').mkdir(exist_ok=True)
kPreviewDirectory.mkdir(exist_ok=True)
bpy.ops.wm.read_factory_settings(use_empty=True)
scene = bpy.context.scene
scene.render.engine = 'CYCLES'
scene.cycles.samples = 24
scene.cycles.use_denoising = True
scene.render.image_settings.file_format = 'PNG'
scene.render.resolution_x = 1000
scene.render.resolution_y = 800
scene.render.resolution_percentage = 100
scene.view_settings.view_transform = 'AgX'
stone = Material('DistantBlueStone', (0.17, 0.30, 0.35))
darkStone = Material('DistantBasalt', (0.055, 0.12, 0.15))
rockStone = Material('DistantCliffStone', (0.10, 0.20, 0.23))
trim = Material('PatinatedBands', (0.19, 0.37, 0.37))
abyss = Material('UnlitRecess', (0.007, 0.025, 0.035))
glow = Material('AncientTealSignal', (0.05, 0.8, 0.65), 2.8)
temple = BuildAsset('distant_temple', '巨大な海底神殿', MakeTemple)
cliff = BuildAsset('distant_cliff_wall', '海底の断崖', MakeCliff)
towers = BuildAsset('distant_tower_cluster', '沈んだ塔群', MakeTowers)
assets = [temple, cliff, towers]
world = bpy.data.worlds.new('UnderwaterDistantWorld')
world.use_nodes = True
world.node_tree.nodes['Background'].inputs['Color'].default_value = (0.025, 0.09, 0.13, 1)
world.node_tree.nodes['Background'].inputs['Strength'].default_value = 0.6
scene.world = world
bpy.ops.mesh.primitive_plane_add(size=2, location=(0, 0, -2))
floor = bpy.context.object
floor.name = 'PreviewOnly_Ground'
floor.scale = (1600, 1600, 1)
floor.data.materials.append(Material('PreviewDeepSand', (0.09, 0.19, 0.20)))
sunData = bpy.data.lights.new('DiagonalWaterSun', 'SUN')
sunData.energy = 2.5
sunData.color = (0.58, 0.88, 1.0)
sunData.angle = 0.12
sun = bpy.data.objects.new('DiagonalWaterSun', sunData)
scene.collection.objects.link(sun)
sun.rotation_euler = (0.35, -0.42, -0.45)
fillData = bpy.data.lights.new('InspectionSoftbox', 'AREA')
fillData.energy = 1200000
fillData.size = 180
fillData.color = (0.35, 0.85, 0.8)
fill = bpy.data.objects.new('InspectionSoftbox', fillData)
scene.collection.objects.link(fill)
fill.location = (-90, -180, 230)
PointAt(fill, (0, 0, 40))
cameraData = bpy.data.cameras.new('DistantPreviewCamera')
camera = bpy.data.objects.new('DistantPreviewCamera', cameraData)
scene.collection.objects.link(camera)
scene.camera = camera
cameraData.clip_end = 4000
for obj in assets:
    for other in assets:
        other.hide_render = other != obj
    cameraData.type = 'ORTHO'
    center = Vector((0, 0, obj.dimensions.z * 0.43))
    span = max(obj.dimensions)
    camera.location = center + Vector((span * 0.9, -span * 1.5, span * 0.75))
    if obj == cliff:
        camera.location = center + Vector((-span * 0.9, -span * 1.5, span * 0.75))
    PointAt(camera, center)
    cameraData.ortho_scale = span * 1.45
    scene.render.filepath = str(kPreviewDirectory / (obj.name + '.png'))
    bpy.ops.render.render(write_still=True)
for obj in assets:
    obj.hide_render = True
templeView = Instance(temple, 'Preview_Temple_650UnitsAway', (0, 400, -2))
for side in (-1, 1):
    Instance(cliff, 'Preview_CliffNear', (side * 235, 160, 0), (side, 1.15, 1.15))
    Instance(cliff, 'Preview_CliffFar', (side * 260, 490, -7), (side * 1.15, 1.15, 1.5))
    Instance(towers, 'Preview_TowersMid', (side * 148, 270, -3), (0.72, 0.72, 0.72), side * 0.12)
    Instance(towers, 'Preview_TowersFar', (side * 190, 610, -5), (0.8, 0.8, 0.8), -side * 0.1)
foreground = LoadForegroundArch()
fill.location = (-90, -240, 180)
fillData.energy = 2000000
PointAt(fill, (0, 140, 30))
sunData.energy = 3.5
world.node_tree.nodes['Background'].inputs['Strength'].default_value = 0.8
waterMaterial = bpy.data.materials.new('PreviewOnly_BoundedWater')
waterMaterial.use_nodes = True
waterMaterial.node_tree.nodes.clear()
waterOutput = waterMaterial.node_tree.nodes.new('ShaderNodeOutputMaterial')
volume = waterMaterial.node_tree.nodes.new('ShaderNodeVolumeScatter')
volume.inputs['Color'].default_value = (0.22, 0.65, 0.76, 1)
volume.inputs['Density'].default_value = 0.0014
volume.inputs['Anisotropy'].default_value = 0.3
waterMaterial.node_tree.links.new(volume.outputs['Volume'], waterOutput.inputs['Volume'])
bpy.ops.mesh.primitive_cube_add(size=1, location=(0, 250, 120))
water = bpy.context.object
water.name = 'PreviewOnly_BoundedWater'
water.scale = (1400, 1600, 600)
water.data.materials.append(waterMaterial)
for index, placement in enumerate(((-100, 160, 260), (85, 350, 280), (160, 470, 300))):
    lightData = bpy.data.lights.new('PreviewOnly_LightShaft', 'SPOT')
    lightData.energy = 4500000
    lightData.color = (0.38, 0.90, 1.0)
    lightData.spot_size = 0.30
    lightData.spot_blend = 0.65
    lightData.shadow_soft_size = 4
    light = bpy.data.objects.new(lightData.name, lightData)
    scene.collection.objects.link(light)
    light.location = placement
    PointAt(light, (placement[0] - 90, placement[1] + 150, 0))
cameraData.type = 'PERSP'
cameraData.lens = 30
camera.location = (8, -260, 36)
PointAt(camera, (0, 400, 59))
scene.render.resolution_x = 1680
scene.render.resolution_y = 1000
scene.cycles.samples = 32
scene.view_settings.exposure = 0.7
scene.render.filepath = str(kPreviewDirectory / 'distant_ruins_atmosphere.png')
bpy.ops.render.render(write_still=True)
foreground.hide_render = True
camera.location = (120, -120, 115)
PointAt(camera, (0, 400, 55))
cameraData.lens = 35
scene.render.filepath = str(kPreviewDirectory / 'distant_ruins_open_view.png')
bpy.ops.render.render(write_still=True)
foreground.hide_render = False
camera.location = (8, -260, 36)
PointAt(camera, (0, 400, 59))
cameraData.lens = 30
scene.render.filepath = str(kPreviewDirectory / 'distant_ruins_atmosphere.png')
for obj in assets:
    obj.hide_set(True)
bpy.ops.object.select_all(action='DESELECT')
templeView.select_set(True)
bpy.context.view_layer.objects.active = templeView
bpy.context.preferences.filepaths.save_version = 0
for image in bpy.data.images:
    if image.source == 'FILE' and image.filepath:
        image.pack()
bpy.ops.wm.save_as_mainfile(filepath=str(kAssetDirectory / 'underwater_distant_ruins.blend'))
report = {'assets': assetReports, 'gamePlacementChanged': False,
          'previewCameraBlender': [8, -260, 36], 'previewTempleBaseBlender': [0, 400, -2],
          'previewFogDensity': 0.0014, 'previewLighting': 'Blender Cycles; not an in-game capture',
          'previewOnlyObjects': ['Existing foreground arch', 'Sand plane', 'Lights and fog'],
          'exportAxes': 'OBJ Y up, forward -Z; engine importer mirrors X',
          'emissionNote': 'Blender/GLB emission; game OBJ emission requires material setup'}
(kAssetDirectory / 'asset_info.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
print('DISTANT_RUINS_COMPLETE', json.dumps(report, ensure_ascii=False), flush=True)
