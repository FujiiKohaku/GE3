"""Create the stone/bronze rotor from the composited concept and install Stage01."""

import bpy
import math
import json
from pathlib import Path
from mathutils import Vector

kProjectDirectory = Path(__file__).resolve().parents[2]
kAssetDirectory = kProjectDirectory / 'resources/Models/Environment/Ocean/RuinsRotor'
kScenePath = kProjectDirectory / 'resources/Scenes/stage01.json'
kBarLength = 46.0
kHubDiameter = 7.2
assetObjects = []


def CreateMaterial(name, color, roughness, metallic, emission):
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
    return material


def AddMaterial(sceneObject, material):
    sceneObject.data.materials.append(material)
    assetObjects.append(sceneObject)
    return sceneObject


def CreateBox(name, position, size, material, bevelWidth=0.0):
    bpy.ops.mesh.primitive_cube_add(size=1, location=position)
    sceneObject = bpy.context.object
    sceneObject.name = name
    sceneObject.dimensions = size
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    if bevelWidth > 0:
        bevel = sceneObject.modifiers.new('WornCorners', 'BEVEL')
        bevel.width = bevelWidth
        bevel.segments = 1
        bpy.ops.object.modifier_apply(modifier=bevel.name)
    return AddMaterial(sceneObject, material)


def CreateRing(name, innerRadius, outerRadius, front, back, material, startAngle=0, endAngle=math.tau, segments=16):
    vertices = []
    isClosedRing = abs(endAngle - startAngle - math.tau) < 0.000001
    ringCount = segments + 1
    if isClosedRing:
        ringCount = segments
    for index in range(ringCount):
        angle = startAngle + (endAngle - startAngle) * index / segments
        for depth, radius in [(back, innerRadius), (back, outerRadius), (front, innerRadius), (front, outerRadius)]:
            vertices.append((math.cos(angle) * radius, depth, math.sin(angle) * radius))
    faces = []
    for index in range(segments):
        first = index * 4
        second = ((index + 1) % ringCount) * 4
        faces.extend([(first, second, second + 1, first + 1),
            (first + 2, first + 3, second + 3, second + 2),
            (first, first + 2, second + 2, second),
            (first + 1, second + 1, second + 3, first + 3)])
    if not isClosedRing:
        faces.extend([(0, 1, 3, 2), (segments * 4, segments * 4 + 2, segments * 4 + 3, segments * 4 + 1)])
    mesh = bpy.data.meshes.new(name + 'Mesh')
    mesh.from_pydata(vertices, [], faces)
    mesh.update()
    sceneObject = bpy.data.objects.new(name, mesh)
    bpy.context.collection.objects.link(sceneObject)
    return AddMaterial(sceneObject, material)


def CreateBar(materials, hasHub):
    assetObjects.clear()
    stone, bronze, danger, crystal = materials
    for side in [-1, 1]:
        CreateBox('BronzeSpine', (side * 13.15, 0, 0), (19.70, 1.60, 1.80), bronze, 0.14)
        for index in range(3):
            center = side * (6.65 + index * 5.75)
            CreateBox('StoneArmor', (center, 0.90, 0), (5.48, 1.10, 1.32), stone, 0.12)
            # Geometric inscriptions are raised, never coplanar with the stone face.
            rune = CreateBox('BronzeRune', (center, 1.49, 0), (0.42, 0.12, 0.42), bronze, 0.03)
            rune.rotation_euler.y = math.pi * 0.25
        for center in [4.20, 10.0, 16.0, 21.9]:
            CreateBox('Binding', (side * center, 0.25, 0), (0.48, 1.94, 1.88), bronze, 0.06)
        for edge in [-1, 1]:
            CreateBox('DangerEdge', (side * 13.40, 1.08, edge * 0.86), (18.70, 0.16, 0.13), danger, 0.025)
        CreateBox('TipArmor', (side * 22.35, 0.65, 0), (1.30, 1.90, 1.82), bronze, 0.20)
        CreateBox('TipStone', (side * 22.35, 1.65, 0), (0.90, 0.22, 1.24), stone, 0.10)
    if hasHub:
        CreateRing('BronzeHub', 2.10, 3.60, 0.72, -0.72, bronze)
        for index in range(4):
            angle = index * math.pi * 0.5 + 0.10
            CreateRing('HubStone', 2.42, 3.38, 1.08, 0.76, stone, angle, angle + math.pi * 0.5 - 0.20, 3)
        CreateRing('CoreGlow', 1.82, 2.08, 0.95, 0.75, crystal, segments=12)
        bpy.ops.mesh.primitive_ico_sphere_add(subdivisions=1, radius=1, location=(0, 1.32, 0))
        core = bpy.context.object
        core.name = 'LevitationCrystal'
        core.scale = (1.74, 0.90, 1.74)
        AddMaterial(core, crystal)
    bpy.ops.object.select_all(action='DESELECT')
    for sceneObject in assetObjects:
        sceneObject.select_set(True)
    bpy.context.view_layer.objects.active = assetObjects[0]
    bpy.ops.object.join()
    model = bpy.context.object
    model.name = 'RuinsRotorBar'
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    bpy.ops.object.mode_set(mode='EDIT')
    bpy.ops.mesh.select_all(action='SELECT')
    bpy.ops.mesh.normals_make_consistent(inside=False)
    bpy.ops.uv.smart_project(island_margin=0.02)
    bpy.ops.object.mode_set(mode='OBJECT')
    # Encode the four surface classes in disjoint UV bands; texture wrapping is unchanged.
    for face in model.data.polygons:
        material = model.data.materials[face.material_index]
        surfaceIndex = materials.index(material)
        for loopIndex in face.loop_indices:
            model.data.uv_layers.active.data[loopIndex].uv.x += surfaceIndex * 2
    model.data.calc_loop_triangles()
    return model, len(model.data.loop_triangles)


def InstallRotors(triangleCounts):
    sceneText = kScenePath.read_text(encoding='utf8')
    originalData = json.loads(sceneText)
    names = {'RotatingLaser_BarA': True, 'RotatingLaser_BarB': False,
        'FinalDefenseLaser_A': True, 'FinalDefenseLaser_B': False}
    replacements = {}
    for original in originalData['scene']:
        name = original['name']
        if name not in names:
            continue
        entry = json.loads(json.dumps(original))
        hasHub = names[name]
        fileName = 'ruins_rotor_bar.obj'
        if hasHub:
            fileName = 'ruins_rotor_hub_bar.obj'
        scale = original['collider']['size'][0] / kBarLength
        entry['file_name'] = 'Environment/Ocean/RuinsRotor/' + fileName
        entry['transform']['scale'] = [scale, scale, scale]
        if hasHub:
            entry['colliders'] = [{'type': 'BOX', 'center': [0, 0, 0],
                'size': [kHubDiameter * scale, 6.0, kHubDiameter * scale]}]
        replacements[name] = entry
    if len(replacements) != 4:
        raise RuntimeError('Expected four rotating bars.')
    for line in sceneText.splitlines():
        for name, entry in replacements.items():
            if '"' + name + '"' in line:
                sceneText = sceneText.replace(line, '    ' + json.dumps(entry) + ',', 1)
                break
    updatedData = json.loads(sceneText)
    originalOtherObjects = [entry for entry in originalData['scene'] if entry['name'] not in names]
    updatedOtherObjects = [entry for entry in updatedData['scene'] if entry['name'] not in names]
    if originalOtherObjects != updatedOtherObjects:
        raise RuntimeError('Unrelated stage objects changed.')
    for original in originalData['scene']:
        if original['name'] not in names:
            continue
        updated = replacements[original['name']]
        for key in ['collider', 'gimmick', 'hazard']:
            if updated[key] != original[key]:
                raise RuntimeError('Existing hazard behavior changed.')
    kScenePath.write_text(sceneText, encoding='utf8')
    report = {'triangles': triangleCounts, 'materials': 4, 'nativeBarLength': kBarLength,
        'hubDiameter': kHubDiameter, 'installedBars': list(replacements.values()),
        'surfaceUvBands': ['stone', 'bronze', 'orangeEmission', 'cyanEmission']}
    (kAssetDirectory / 'asset_info.json').write_text(json.dumps(report, indent=2), encoding='utf8')
    print('ROTOR_RESULT ' + json.dumps({'triangles': triangleCounts, 'installedBars': len(replacements)}), flush=True)


def Main():
    (kAssetDirectory / 'textures').mkdir(parents=True, exist_ok=True)
    (kAssetDirectory / 'previews').mkdir(exist_ok=True)
    bpy.ops.object.select_all(action='SELECT')
    bpy.ops.object.delete(use_global=False)
    materials = [CreateMaterial('RotorSandstone', (0.64, 0.49, 0.30), 0.88, 0, 0),
        CreateMaterial('RotorBronze', (0.16, 0.23, 0.21), 0.48, 0.65, 0),
        CreateMaterial('RotorDanger', (1.0, 0.16, 0.015), 0.35, 0, 3.5),
        CreateMaterial('RotorCrystal', (0.035, 0.72, 0.82), 0.25, 0, 2.5)]
    triangleCounts = {}
    models = []
    for hasHub in [True, False]:
        model, triangleCount = CreateBar(materials, hasHub)
        fileName = 'ruins_rotor_bar'
        if hasHub:
            fileName = 'ruins_rotor_hub_bar'
        model.name = fileName
        triangleCounts[fileName] = triangleCount
        bpy.ops.wm.obj_export(filepath=str(kAssetDirectory / (fileName + '.obj')),
            export_selected_objects=True, export_materials=True, export_triangulated_mesh=True,
            forward_axis='NEGATIVE_Z', up_axis='Y', path_mode='RELATIVE')
        models.append(model)
    models[1].rotation_euler.y = math.pi * 0.5
    for model in models:
        model.rotation_euler.y += math.pi * 0.22
    scene = bpy.context.scene
    scene.render.engine = 'CYCLES'
    scene.cycles.samples = 24
    scene.cycles.use_denoising = True
    scene.render.resolution_x = 1200
    scene.render.resolution_y = 1000
    scene.render.resolution_percentage = 100
    scene.render.image_settings.file_format = 'PNG'
    scene.view_settings.view_transform = 'AgX'
    scene.world.color = (0.18, 0.25, 0.29)
    for name, position, energy, size in [('Key', (-15, 18, 30), 18000, 22), ('Fill', (20, 12, -10), 11000, 18)]:
        lightData = bpy.data.lights.new(name, 'AREA')
        lightData.energy = energy
        lightData.shape = 'DISK'
        lightData.size = size
        light = bpy.data.objects.new(name, lightData)
        scene.collection.objects.link(light)
        light.location = position
        light.rotation_euler = (-light.location).to_track_quat('-Z', 'Y').to_euler()
    cameraData = bpy.data.cameras.new('PreviewCamera')
    camera = bpy.data.objects.new('PreviewCamera', cameraData)
    scene.collection.objects.link(camera)
    camera.location = (9, 70, 10)
    camera.rotation_euler = (-camera.location).to_track_quat('-Z', 'Y').to_euler()
    cameraData.type = 'ORTHO'
    cameraData.ortho_scale = 53
    scene.camera = camera
    scene.render.filepath = str(kAssetDirectory / 'previews/ruins_rotor.png')
    bpy.ops.wm.save_as_mainfile(filepath=str(kAssetDirectory / 'ruins_rotor.blend'))
    bpy.ops.render.render(write_still=True)
    InstallRotors(triangleCounts)


Main()
