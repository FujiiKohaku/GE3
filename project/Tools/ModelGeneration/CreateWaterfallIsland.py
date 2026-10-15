"""Create an editable Stage01 mountain island and a real geometry preview.

Blender Z is height; the flight passage runs along Blender Y. No scene install.
"""

import bpy
import bmesh
import json
import math
import random
from pathlib import Path
from mathutils import Vector

kAssetDirectory = Path(__file__).resolve().parents[2] / 'resources/Models/Environment/Ocean/WaterfallIsland'
kRandomSeed = 27015
randomEngine = random.Random(kRandomSeed)


def CreateMaterial(name, color, alpha=1.0):
    material = bpy.data.materials.new(name)
    material.use_nodes = True
    material.diffuse_color = (*color, alpha)
    shader = material.node_tree.nodes.get('Principled BSDF')
    shader.inputs['Base Color'].default_value = (*color, 1.0)
    shader.inputs['Roughness'].default_value = 0.87
    shader.inputs['Alpha'].default_value = alpha
    image = bpy.data.images.new(name + 'Color', width=16, height=16)
    image.pixels = [*color, alpha] * 256
    image.filepath_raw = str(kAssetDirectory / 'textures' / (name + '.png'))
    image.file_format = 'PNG'
    image.save()
    texture = material.node_tree.nodes.new('ShaderNodeTexImage')
    texture.image = image
    material.node_tree.links.new(texture.outputs['Color'], shader.inputs['Base Color'])
    return material


def CreateMesh(name, vertices, faces, materials):
    mesh = bpy.data.meshes.new(name + 'Mesh')
    mesh.from_pydata(vertices, [], faces)
    mesh.update()
    model = bpy.data.objects.new(name, mesh)
    bpy.context.collection.objects.link(model)
    for material in materials:
        mesh.materials.append(material)
    editableMesh = bmesh.new()
    editableMesh.from_mesh(mesh)
    bmesh.ops.recalc_face_normals(editableMesh, faces=list(editableMesh.faces))
    bmesh.ops.triangulate(editableMesh, faces=list(editableMesh.faces))
    editableMesh.to_mesh(mesh)
    editableMesh.free()
    for face in mesh.polygons:
        face.material_index = randomEngine.randrange(len(materials))
    return model


def CreateBox(name, position, size, material):
    bpy.ops.mesh.primitive_cube_add(size=1.0, location=position)
    model = bpy.context.object
    model.name = name
    model.scale = size
    model.data.materials.append(material)
    return model


def CreateCliff(name, position, radius, height, materials, grassMaterial):
    sideCount = 9
    angles = [2.0 * math.pi * index / sideCount for index in range(sideCount)]
    radii = [radius * randomEngine.uniform(0.88, 1.12) for index in range(sideCount)]
    vertices = []
    for level, factor in [(-3.0, 1.10), (height * 0.35, 1.05), (height * 0.8, 0.94), (height, 0.85)]:
        for index, angle in enumerate(angles):
            vertices.append((position[0] + radii[index] * factor * math.cos(angle),
                position[1] + radii[index] * factor * math.sin(angle) * 0.88,
                level + randomEngine.uniform(-0.7, 0.7)))
    faces = []
    for ringIndex in range(3):
        for index in range(sideCount):
            nextIndex = (index + 1) % sideCount
            faces.append((ringIndex * sideCount + index, ringIndex * sideCount + nextIndex,
                (ringIndex + 1) * sideCount + nextIndex, (ringIndex + 1) * sideCount + index))
    faces.append(tuple(reversed(range(sideCount))))
    faces.append(tuple(range(3 * sideCount, 4 * sideCount)))
    cliff = CreateMesh(name, vertices, faces, materials)
    cliff.data.materials.append(grassMaterial)
    for face in cliff.data.polygons:
        if face.normal.z > 0.75 and face.center.z > height * 0.92:
            face.material_index = len(materials)
    return cliff


def CreateMountain(materials, grassMaterial):
    # A concave, closed extruded profile: the tunnel is real empty space.
    outline = [(-72, -4), (-70, 19), (-60, 27), (-58, 43), (-43, 49),
        (-37, 67), (-27, 72), (-22, 88), (-12, 96), (-3, 93), (4, 82),
        (17, 85), (28, 73), (34, 66), (46, 60), (53, 42), (65, 35), (73, 15), (74, -4), (14, -4), (14, 12)]
    for index in range(1, 13):
        angle = math.pi * index / 12
        outline.append((14 * math.cos(angle), 12 + 14 * math.sin(angle)))
    outline.append((-14, -4))
    vertices = []
    for depth in [-1.0, 39.0]:
        for positionX, height in outline:
            vertices.append((positionX, depth, height))
    count = len(outline)
    faces = [tuple(reversed(range(count))), tuple(range(count, count * 2))]
    for index in range(count):
        nextIndex = (index + 1) % count
        faces.append((index, nextIndex, nextIndex + count, index + count))
    mountain = CreateMesh('MountainWithFlightTunnel', vertices, faces, materials)
    mountain.data.materials.append(grassMaterial)
    for face in mountain.data.polygons:
        if face.normal.z > 0.7:
            face.material_index = len(materials)
    return mountain


def CreateArch(name, center, radius, thickness, depth, materials):
    vertices = []
    for depthOffset in [-depth * 0.5, depth * 0.5]:
        for ringRadius in [radius, radius + thickness]:
            for index in range(13):
                angle = math.pi * index / 12
                vertices.append((center[0] + ringRadius * math.cos(angle), center[1] + depthOffset,
                    center[2] + ringRadius * math.sin(angle)))
    faces = []
    for index in range(12):
        faces.extend([(index, index + 1, index + 14, index + 13),
            (index + 26, index + 39, index + 40, index + 27),
            (index, index + 26, index + 27, index + 1),
            (index + 13, index + 14, index + 40, index + 39)])
    faces.extend([(0, 13, 39, 26), (12, 38, 51, 25)])
    return CreateMesh(name, vertices, faces, materials)


def CreateFall(name, centerX, topHeight, bottomHeight, topDepth, bottomDepth, width, materials):
    vertices = []
    columns = 20
    rows = 10
    for row in range(rows + 1):
        progress = row / rows
        depth = topDepth + (bottomDepth - topDepth) * math.sqrt(progress)
        height = topHeight + (bottomHeight - topHeight) * progress
        for column in range(columns + 1):
            fraction = column / columns
            positionX = centerX + (fraction - 0.5) * width * (1.0 + progress * 0.08)
            ripple = 0.06 * math.sin(column * 2.2 + row * 0.7)
            vertices.append((positionX, depth + ripple, height))
    faces = []
    for row in range(rows):
        for column in range(columns):
            start = row * (columns + 1) + column
            faces.append((start, start + 1, start + columns + 2, start + columns + 1))
    model = CreateMesh(name, vertices, faces, materials)
    for face in model.data.polygons:
        column = min(vertexIndex % (columns + 1) for vertexIndex in face.vertices)
        stripeMaterials = [0, 1, 0, 0, 2, 1, 0, 1, 0, 0, 1, 2, 0, 0, 1, 0, 1, 0, 2, 0]
        face.material_index = stripeMaterials[column % len(stripeMaterials)]
        face.use_smooth = True
    return model


def JoinModels(name, models):
    bpy.ops.object.select_all(action='DESELECT')
    for model in models:
        model.select_set(True)
    bpy.context.view_layer.objects.active = models[0]
    bpy.ops.object.transform_apply(location=False, rotation=True, scale=True)
    bpy.ops.object.join()
    model = bpy.context.object
    model.name = name
    bpy.context.scene.cursor.location = (0, 0, 0)
    bpy.ops.object.origin_set(type='ORIGIN_CURSOR')
    bpy.ops.object.mode_set(mode='EDIT')
    bpy.ops.mesh.select_all(action='SELECT')
    bpy.ops.uv.smart_project(island_margin=0.02)
    bpy.ops.object.mode_set(mode='OBJECT')
    return model


def PointAt(model, target):
    model.rotation_euler = (Vector(target) - model.location).to_track_quat('-Z', 'Y').to_euler()


def Main():
    kAssetDirectory.mkdir(parents=True, exist_ok=True)
    (kAssetDirectory / 'textures').mkdir(exist_ok=True)
    (kAssetDirectory / 'previews').mkdir(exist_ok=True)
    bpy.ops.object.select_all(action='SELECT')
    bpy.ops.object.delete(use_global=False)
    rocks = [CreateMaterial('IslandRockWarm', (0.48, 0.39, 0.29)),
        CreateMaterial('IslandRockLight', (0.53, 0.45, 0.35)),
        CreateMaterial('IslandRockShade', (0.41, 0.40, 0.35))]
    grass = CreateMaterial('IslandMoss', (0.32, 0.46, 0.18))
    stones = [CreateMaterial('IslandSandstone', (0.72, 0.66, 0.46)),
        CreateMaterial('IslandStoneShade', (0.58, 0.54, 0.40))]
    waters = [CreateMaterial('IslandWater', (0.12, 0.61, 0.70), 0.72),
        CreateMaterial('IslandWaterPale', (0.36, 0.80, 0.85), 0.78),
        CreateMaterial('IslandWaterWhite', (0.76, 0.94, 0.95), 0.85)]
    foamMaterial = CreateMaterial('IslandFoam', (0.75, 0.93, 0.94), 0.82)
    terrain = [CreateMountain(rocks, grass)]
    for index, values in enumerate([(-57, -6, 17, 29), (-45, -1, 16, 48), (-30, 0, 13, 64),
        (-21, 20, 14, 84), (49, -1, 19, 44), (34, 6, 17, 63), (22, 24, 13, 78),
        (68, 12, 13, 24), (-70, 7, 13, 16), (-30, -18, 12, 27), (32, -14, 14, 30),
        (-58, 29, 16, 36), (58, 32, 17, 45)]):
        positionX, depth, radius, height = values
        terrain.append(CreateCliff('TerracedCliff' + str(index), (positionX, depth), radius, height, rocks, grass))
    ruins = [CreateArch('TunnelStoneArch', (0, -2.6, 12), 14.3, 2.4, 3.0, stones)]
    for side in [-1, 1]:
        ruins.append(CreateBox('TunnelPier', (side * 15.5, -2.6, 5), (2.5, 3, 14), stones[0]))
    # Ruined aqueduct across the summit terrace, with genuine arch openings.
    for centerX in [-30, -18, -6, 6, 18, 30]:
        ruins.append(CreateArch('AqueductArch', (centerX, -3.5, 57), 4.5, 1.2, 3.0, stones))
        ruins.append(CreateBox('AqueductPier', (centerX - 5.25, -3.5, 53.5), (1.5, 3, 9), stones[0]))
    ruins.append(CreateBox('AqueductChannel', (0, -3.5, 63.3), (73, 4, 1.2), stones[0]))
    for index, values in enumerate([(-29, 1, 64, 18), (27, 12, 72, 19), (-13, 20, 91, 15), (49, 7, 43, 12)]):
        positionX, depth, base, height = values
        for course in range(int(height / 2.5)):
            width = 6.0
            if course == int(height / 2.5) - 1:
                width = 6.8
            ruins.append(CreateBox('TowerCourse' + str(index), (positionX, depth, base + course * 2.5),
                (width, 5.5, 2.43), stones[course % 2]))
        for offset in [-2, 2]:
            battlementHeight = base + (int(height / 2.5) - 1) * 2.5 + 2.30
            ruins.append(CreateBox('BrokenBattlement', (positionX + offset, depth, battlementHeight), (1.6, 5.5, 2.2), stones[0]))
    water = []
    for index, values in enumerate([(0, 84, 64, -3, -5, 17), (0, 63.8, 32, -6, -12, 23),
        (0, 31.8, 0, -12, -17, 25), (-44, 48, 28, -12, -20, 9),
        (-31, 27, 0, -27, -30, 9), (49, 43, 0, -18, -24, 10)]):
        water.append(CreateFall('Cascade' + str(index), *values, waters))
    foam = []
    for centerX, depth, radius in [(0, -19, 15), (-31, -31, 6), (49, -25, 7)]:
        for index in range(12):
            angle = math.tau * index / 12
            bpy.ops.mesh.primitive_ico_sphere_add(subdivisions=1, radius=1,
                location=(centerX + math.cos(angle) * radius, depth + math.sin(angle) * radius * 0.35, 0.3))
            model = bpy.context.object
            model.scale = (3.0, 1.8, 0.22)
            model.data.materials.append(foamMaterial)
            foam.append(model)
    models = [JoinModels('WaterfallIsland_Terrain', terrain), JoinModels('WaterfallIsland_Ruins', ruins),
        JoinModels('WaterfallIsland_Water', water), JoinModels('WaterfallIsland_Foam', foam)]
    triangleCounts = {}
    for model in models:
        model.data.calc_loop_triangles()
        triangleCounts[model.name] = len(model.data.loop_triangles)
        bpy.ops.object.select_all(action='DESELECT')
        model.select_set(True)
        bpy.context.view_layer.objects.active = model
        bpy.ops.wm.obj_export(filepath=str(kAssetDirectory / (model.name + '.obj')), export_selected_objects=True,
            export_triangulated_mesh=True, export_materials=True, forward_axis='NEGATIVE_Z', up_axis='Y', path_mode='RELATIVE')
    bpy.ops.object.select_all(action='DESELECT')
    for model in models:
        model.select_set(True)
    bpy.ops.export_scene.gltf(filepath=str(kAssetDirectory / 'waterfall_island.glb'), export_format='GLB', use_selection=True)
    scene = bpy.context.scene
    scene.render.engine = 'CYCLES'
    scene.cycles.samples = 24
    scene.cycles.use_denoising = True
    scene.render.resolution_x = 1440
    scene.render.resolution_y = 900
    scene.render.resolution_percentage = 100
    scene.world.use_nodes = True
    background = scene.world.node_tree.nodes.get('Background')
    background.inputs['Color'].default_value = (0.62, 0.80, 0.89, 1)
    background.inputs['Strength'].default_value = 0.65
    scene.view_settings.view_transform = 'AgX'
    oceanMaterial = bpy.data.materials.new('PreviewOcean_NotExported')
    oceanMaterial.use_nodes = True
    shader = oceanMaterial.node_tree.nodes.get('Principled BSDF')
    shader.inputs['Base Color'].default_value = (0.055, 0.42, 0.52, 1)
    shader.inputs['Roughness'].default_value = 0.28
    noise = oceanMaterial.node_tree.nodes.new('ShaderNodeTexNoise')
    noise.inputs['Scale'].default_value = 1.3
    bump = oceanMaterial.node_tree.nodes.new('ShaderNodeBump')
    bump.inputs['Strength'].default_value = 0.3
    bump.inputs['Distance'].default_value = 0.3
    oceanMaterial.node_tree.links.new(noise.outputs['Fac'], bump.inputs['Height'])
    oceanMaterial.node_tree.links.new(bump.outputs['Normal'], shader.inputs['Normal'])
    bpy.ops.mesh.primitive_plane_add(size=4000, location=(0, 0, -0.3))
    bpy.context.object.name = 'PreviewOcean_NotExported'
    bpy.context.object.data.materials.append(oceanMaterial)
    lightData = bpy.data.lights.new('Daylight', 'SUN')
    lightData.energy = 3.0
    lightData.angle = math.radians(15)
    light = bpy.data.objects.new('Daylight', lightData)
    scene.collection.objects.link(light)
    light.rotation_euler = (math.radians(25), math.radians(-30), math.radians(-25))
    fillData = bpy.data.lights.new('OceanFill', 'AREA')
    fillData.energy = 95000
    fillData.shape = 'DISK'
    fillData.size = 130
    fill = bpy.data.objects.new('OceanFill', fillData)
    scene.collection.objects.link(fill)
    fill.location = (0, -110, 110)
    PointAt(fill, (0, 0, 40))
    cameraData = bpy.data.cameras.new('PreviewCamera')
    camera = bpy.data.objects.new('PreviewCamera', cameraData)
    scene.collection.objects.link(camera)
    camera.location = (108, -232, 95)
    cameraData.lens = 40
    cameraData.clip_end = 5000
    PointAt(camera, (0, 4, 40))
    scene.camera = camera
    scene.render.image_settings.file_format = 'PNG'
    scene.render.filepath = str(kAssetDirectory / 'previews/waterfall_island.png')
    report = {'triangles': triangleCounts, 'totalTriangles': sum(triangleCounts.values()), 'meshObjects': len(models),
        'materials': 10, 'tunnelWidth': 28, 'tunnelArchHeight': 26, 'installedInStage': False,
        'waterAnimationImplemented': False, 'units': 'Blender Z up, OBJ Y up', 'seed': kRandomSeed}
    (kAssetDirectory / 'asset_info.json').write_text(json.dumps(report, indent=2), encoding='utf8')
    bpy.ops.wm.save_as_mainfile(filepath=str(kAssetDirectory / 'waterfall_island.blend'))
    bpy.ops.render.render(write_still=True)
    camera.location = (0, -232, 29)
    cameraData.lens = 39
    PointAt(camera, (0, 0, 43))
    scene.render.filepath = str(kAssetDirectory / 'previews/waterfall_island_approach.png')
    bpy.ops.render.render(write_still=True)
    print('WATERFALL_ISLAND_RESULT ' + json.dumps(report))


Main()
