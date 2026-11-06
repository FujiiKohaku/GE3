import bpy
import math
import json
from pathlib import Path
from mathutils import Vector

kOutputDirectory = Path(__file__).resolve().parent


def CreateMaterial(name, color, metallic=0.0, roughness=0.4, emissionStrength=0.0):
    material = bpy.data.materials.new(name)
    material.diffuse_color = (*color, 1.0)
    material.use_nodes = True
    shader = material.node_tree.nodes.get('Principled BSDF')
    shader.inputs['Base Color'].default_value = (*color, 1.0)
    shader.inputs['Metallic'].default_value = metallic
    shader.inputs['Roughness'].default_value = roughness
    shader.inputs['Emission Color'].default_value = (*color, 1.0)
    shader.inputs['Emission Strength'].default_value = emissionStrength
    return material


def CreateBox(name, location, dimensions, material, bevelWidth=0.04):
    bpy.ops.mesh.primitive_cube_add(size=1, location=location)
    meshObject = bpy.context.object
    meshObject.name = name
    meshObject.dimensions = dimensions
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    meshObject.data.materials.append(material)
    if bevelWidth > 0:
        modifier = meshObject.modifiers.new('EdgeBevel', 'BEVEL')
        modifier.width = bevelWidth
        modifier.segments = 2
        modifier = meshObject.modifiers.new('WeightedNormals', 'WEIGHTED_NORMAL')
    return meshObject


def CreateCylinder(name, location, radius, depth, material, vertices=8):
    bpy.ops.mesh.primitive_cylinder_add(vertices=vertices, radius=radius, depth=depth, location=location, rotation=(0, 0, math.pi / 8))
    meshObject = bpy.context.object
    meshObject.name = name
    meshObject.data.materials.append(material)
    modifier = meshObject.modifiers.new('EdgeBevel', 'BEVEL')
    modifier.width = 0.025
    modifier.segments = 2
    meshObject.modifiers.new('WeightedNormals', 'WEIGHTED_NORMAL')
    return meshObject


def CreateLine(name, start, end, thickness, material):
    midpoint = (Vector(start) + Vector(end)) / 2
    direction = Vector(end) - Vector(start)
    meshObject = CreateBox(name, midpoint, (thickness, thickness, direction.length), material, thickness / 3)
    meshObject.rotation_euler = direction.to_track_quat('Z', 'Y').to_euler()
    return meshObject


def AimAt(sceneObject, target):
    direction = Vector(target) - sceneObject.location
    sceneObject.rotation_euler = direction.to_track_quat('-Z', 'Y').to_euler()


def CreateLight(name, location, energy, color, size):
    lightData = bpy.data.lights.new(name, 'AREA')
    lightData.energy = energy
    lightData.color = color
    lightData.shape = 'DISK'
    lightData.size = size
    lightObject = bpy.data.objects.new(name, lightData)
    bpy.context.collection.objects.link(lightObject)
    lightObject.location = location
    AimAt(lightObject, (0, 0, 1.2))


def CreateBeacon(sideName, centerX, armorMaterial, darkMaterial, lightMaterial, coreMaterial):
    CreateCylinder(sideName + 'Foot', (centerX, 0, 0.12), 0.69, 0.24, darkMaterial)
    CreateCylinder(sideName + 'BaseArmor', (centerX, 0, 0.31), 0.59, 0.2, armorMaterial)
    CreateBox(sideName + 'Spine', (centerX, 0.07, 1.35), (0.58, 0.54, 1.95), darkMaterial)
    for direction in [-1, 1]:
        plate = CreateBox(sideName + 'Shoulder' + str(direction), (centerX + direction * 0.31, 0, 1.23), (0.18, 0.65, 1.72), armorMaterial)
        plate.rotation_euler.y = direction * math.radians(-5)
        CreateBox(sideName + 'Rail' + str(direction), (centerX + direction * 0.233, -0.303, 1.47), (0.036, 0.035, 1.3), lightMaterial, 0.008)
        CreateBox(sideName + 'FootLight' + str(direction), (centerX + direction * 0.32, -0.48, 0.3), (0.19, 0.035, 0.065), lightMaterial, 0.01)
    CreateBox(sideName + 'FrontInset', (centerX, -0.285, 1.32), (0.33, 0.045, 0.76), darkMaterial)
    for index in range(3):
        indicator = CreateBox(sideName + 'Indicator' + str(index), (centerX, -0.32, 1.12 + index * 0.17), (0.25, 0.035, 0.065), lightMaterial, 0.008)
        indicator.rotation_euler.y = math.radians(-18)
    CreateCylinder(sideName + 'CollarLower', (centerX, 0, 2.22), 0.44, 0.15, armorMaterial)
    CreateCylinder(sideName + 'Lantern', (centerX, 0, 2.55), 0.325, 0.54, coreMaterial)
    for direction in [-1, 1]:
        CreateBox(sideName + 'LanternFrame' + str(direction), (centerX + direction * 0.3, 0.02, 2.55), (0.055, 0.36, 0.56), darkMaterial, 0.015)
    CreateCylinder(sideName + 'CollarUpper', (centerX, 0, 2.88), 0.44, 0.13, lightMaterial)
    CreateCylinder(sideName + 'Cap', (centerX, 0, 2.99), 0.51, 0.14, armorMaterial)
    CreateCylinder(sideName + 'Crown', (centerX, 0, 3.09), 0.37, 0.08, darkMaterial)
    # A geometric record arrow and tray, repeated on both travel-facing sides.
    for faceDirection in [-1, 1]:
        faceY = faceDirection * 0.333
        CreateLine(sideName + 'RecordStem', (centerX, faceY, 2.73), (centerX, faceY, 2.49), 0.035, lightMaterial)
        CreateLine(sideName + 'ArrowLeft', (centerX - 0.1, faceY, 2.59), (centerX, faceY, 2.49), 0.035, lightMaterial)
        CreateLine(sideName + 'ArrowRight', (centerX + 0.1, faceY, 2.59), (centerX, faceY, 2.49), 0.035, lightMaterial)
        CreateLine(sideName + 'Tray', (centerX - 0.14, faceY, 2.4), (centerX + 0.14, faceY, 2.4), 0.035, lightMaterial)
        for direction in [-1, 1]:
            CreateLine(sideName + 'TrayEnd', (centerX + direction * 0.14, faceY, 2.4), (centerX + direction * 0.14, faceY, 2.48), 0.035, lightMaterial)


def Main():
    bpy.ops.object.select_all(action='SELECT')
    bpy.ops.object.delete(use_global=False)
    armorMaterial = CreateMaterial('PearlArmor', (0.56, 0.65, 0.7), 0.65, 0.32)
    darkMaterial = CreateMaterial('GraphiteFrame', (0.027, 0.046, 0.065), 0.65, 0.36)
    lightMaterial = CreateMaterial('CyanEmission', (0.025, 0.8, 1.0), 0.15, 0.25, 4.5)
    coreMaterial = CreateMaterial('LanternCore', (0.015, 0.22, 0.27), 0.3, 0.22, 1.5)
    CreateBeacon('Left', -4.1, armorMaterial, darkMaterial, lightMaterial, coreMaterial)
    CreateBeacon('Right', 4.1, armorMaterial, darkMaterial, lightMaterial, coreMaterial)
    boundaryMaterial = CreateMaterial('BoundaryTransparent', (0.025, 0.65, 0.8), 0, 0.5, 0.6)
    boundaryShader = boundaryMaterial.node_tree.nodes.get('Principled BSDF')
    boundaryShader.inputs['Alpha'].default_value = 0.045
    boundaryMaterial.diffuse_color = (0.025, 0.65, 0.8, 0.045)
    boundaryMaterial.surface_render_method = 'BLENDED'
    boundary = CreateBox('LightBoundary', (0, 0, 1.42), (7.52, 0.008, 2.75), boundaryMaterial, 0)
    boundary['purpose'] = 'Visual only. Autosave trigger should be implemented separately.'
    CreateBox('ThresholdLight', (0, 0, 0.035), (7.55, 0.038, 0.025), lightMaterial, 0.008)
    for centerX in [-3.75, 3.75]:
        CreateBox('BoundaryEdge', (centerX, 0, 1.42), (0.02, 0.02, 2.74), lightMaterial, 0.004)
    assetObjects = list(bpy.context.scene.objects)
    assetCollection = bpy.data.collections.new('CheckpointAsset')
    bpy.context.scene.collection.children.link(assetCollection)
    for meshObject in assetObjects:
        for collection in list(meshObject.users_collection):
            collection.objects.unlink(meshObject)
        assetCollection.objects.link(meshObject)
    bpy.ops.object.select_all(action='DESELECT')
    for meshObject in assetObjects:
        meshObject.select_set(True)
    bpy.ops.export_scene.gltf(filepath=str(kOutputDirectory / 'Checkpoint.glb'), use_selection=True, export_apply=True)
    dependencyGraph = bpy.context.evaluated_depsgraph_get()
    triangleCount = 0
    for meshObject in assetObjects:
        evaluatedObject = meshObject.evaluated_get(dependencyGraph)
        evaluatedMesh = evaluatedObject.to_mesh()
        evaluatedMesh.calc_loop_triangles()
        triangleCount += len(evaluatedMesh.loop_triangles)
        evaluatedObject.to_mesh_clear()
    (kOutputDirectory / 'ModelInfo.json').write_text(json.dumps({'triangles': triangleCount, 'meshObjects': len(assetObjects), 'widthMeters': 9.6, 'heightMeters': 3.13, 'clearPassageMeters': 7.0}, indent=2), encoding='utf-8')
    floorMaterial = CreateMaterial('PreviewFloor', (0.035, 0.052, 0.078), 0.25, 0.45)
    CreateBox('PreviewFloor', (0, 1, -0.16), (200, 200, 0.25), floorMaterial)
    seamMaterial = CreateMaterial('FloorSeam', (0.012, 0.021, 0.034), 0.15, 0.7)
    for index in range(-5, 9):
        CreateBox('FloorJoint', (0, index * 2.0, -0.028), (25, 0.022, 0.006), seamMaterial, 0)
    for centerX in [-5.2, 5.2]:
        CreateBox('RouteGuide', (centerX, 4, -0.022), (0.035, 45, 0.008), coreMaterial, 0)
    scene = bpy.context.scene
    scene.world.use_nodes = True
    scene.world.node_tree.nodes['Background'].inputs[0].default_value = (0.055, 0.085, 0.13, 1)
    scene.world.node_tree.nodes['Background'].inputs[1].default_value = 0.45
    CreateLight('KeyLight', (0, -5, 9), 2300, (0.75, 0.86, 1), 8)
    CreateLight('RimLight', (1, 4, 6), 2600, (0.23, 0.7, 1), 7)
    CreateLight('SoftFill', (-7, -1, 4), 1300, (1, 0.8, 0.6), 6)
    cameraData = bpy.data.cameras.new('PreviewCamera')
    camera = bpy.data.objects.new('PreviewCamera', cameraData)
    scene.collection.objects.link(camera)
    camera.location = (10, -16, 7)
    cameraData.type = 'ORTHO'
    cameraData.ortho_scale = 14.5
    AimAt(camera, (0, 0, 1.35))
    scene.camera = camera
    scene.render.engine = 'CYCLES'
    scene.cycles.samples = 32
    scene.cycles.use_denoising = True
    scene.render.resolution_x = 1500
    scene.render.resolution_y = 900
    scene.render.resolution_percentage = 100
    scene.view_settings.view_transform = 'AgX'
    scene.use_nodes = True
    nodes = scene.node_tree.nodes
    nodes.clear()
    renderLayer = nodes.new('CompositorNodeRLayers')
    glow = nodes.new('CompositorNodeGlare')
    glow.glare_type = 'FOG_GLOW'
    glow.threshold = 1.5
    glow.quality = 'MEDIUM'
    composite = nodes.new('CompositorNodeComposite')
    scene.node_tree.links.new(renderLayer.outputs['Image'], glow.inputs['Image'])
    scene.node_tree.links.new(glow.outputs['Image'], composite.inputs['Image'])
    scene.render.image_settings.file_format = 'PNG'
    scene.render.filepath = str(kOutputDirectory / 'CheckpointPreview.png')
    bpy.ops.wm.save_as_mainfile(filepath=str(kOutputDirectory / 'Checkpoint.blend'))
    bpy.ops.render.render(write_still=True)
    camera.location = (0, -15, 3.5)
    cameraData.type = 'PERSP'
    cameraData.lens = 37
    AimAt(camera, (0, 0, 1.4))
    scene.render.filepath = str(kOutputDirectory / 'CheckpointFront.png')
    bpy.ops.render.render(write_still=True)
    print('CHECKPOINT_COMPLETE triangles=' + str(triangleCount))


Main()
