import bpy
import math
import random
from pathlib import Path
from mathutils import Vector

outputPath = Path('C:/Projects/KohakuEngine/output/electric_gate_v1')
bpy.ops.object.select_all(action='SELECT')
bpy.ops.object.delete(use_global=False)

def MakeMaterial(name, color, metallic=0.0, roughness=0.45, emission=0.0):
    material = bpy.data.materials.new(name)
    material.diffuse_color = (*color, 1.0)
    material.use_nodes = True
    shader = material.node_tree.nodes.get('Principled BSDF')
    shader.inputs['Base Color'].default_value = (*color, 1.0)
    shader.inputs['Metallic'].default_value = metallic
    shader.inputs['Roughness'].default_value = roughness
    shader.inputs['Emission Color'].default_value = (*color, 1.0)
    shader.inputs['Emission Strength'].default_value = emission
    return material

armorMaterial = MakeMaterial('IvoryArmor', (0.72, 0.77, 0.76), 0.22)
darkMaterial = MakeMaterial('InnerGraphite', (0.025, 0.038, 0.047), 0.65)
metalMaterial = MakeMaterial('EmitterMetal', (0.23, 0.27, 0.28), 0.8)
warningMaterial = MakeMaterial('WarningOrange', (0.9, 0.11, 0.015), 0.2)
glowMaterial = MakeMaterial('EmitterGlow', (1.0, 0.045, 0.008), emission=5)
arcMaterial = MakeMaterial('ElectricOrange', (1.0, 0.065, 0.008), emission=9)
coreMaterial = MakeMaterial('ElectricHotCore', (1.0, 0.65, 0.24), emission=18)
floorMaterial = MakeMaterial('Backdrop', (0.037, 0.052, 0.065), roughness=0.85)

def AddBox(name, position, size, material, bevel=0.035):
    bpy.ops.mesh.primitive_cube_add(size=1, location=position)
    model = bpy.context.object
    model.name = name
    model.dimensions = size
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    model.data.materials.append(material)
    if bevel > 0:
        modifier = model.modifiers.new('EdgeHighlights', 'BEVEL')
        modifier.width = bevel
        modifier.segments = 1
        model.modifiers.new('WeightedNormals', 'WEIGHTED_NORMAL')
    return model

def AddArc(name, points, radius, material):
    curve = bpy.data.curves.new(name, 'CURVE')
    curve.dimensions = '3D'
    curve.bevel_depth = radius
    curve.bevel_resolution = 0
    curve.resolution_u = 1
    spline = curve.splines.new('POLY')
    spline.points.add(len(points) - 1)
    for point, position in zip(spline.points, points):
        point.co = (*position, 1.0)
    model = bpy.data.objects.new(name, curve)
    bpy.context.collection.objects.link(model)
    model.data.materials.append(material)
    return model

random.seed(27)
for side in [-1, 1]:
    centerX = side * 2.9
    AddBox('PillarBase', (centerX, 0, 0.58), (1.1, 1.0, 1.16), armorMaterial)
    AddBox('PillarBody', (centerX, 0.07, 2.85), (0.96, 0.86, 3.4), armorMaterial)
    AddBox('PillarCap', (centerX, 0.07, 4.6), (0.36, 0.4, 0.16), armorMaterial)
    AddBox('InnerEmitterRail', (centerX - side * 0.48, 0, 2.86), (0.1, 0.74, 3.18), darkMaterial)
    AddBox('BaseJoint', (centerX, -0.015, 1.19), (1.13, 1.03, 0.12), metalMaterial)
    for height in [2.2, 3.55]:
        AddBox('EmitterMount', (centerX - side * 0.57, 0, height), (0.24, 0.67, 0.7), armorMaterial)
        AddBox('EmitterSocket', (centerX - side * 0.73, 0, height), (0.12, 0.5, 0.48), darkMaterial)
        AddBox('LiveEmitter', (centerX - side * 0.81, 0, height), (0.08, 0.38, 0.36), glowMaterial)
        for stripeIndex in range(3):
            stripe = AddBox('WarningStripe', (centerX - 0.22 + stripeIndex * 0.2, -0.447, height + 0.48), (0.09, 0.018, 0.2), warningMaterial, 0)
            stripe.rotation_euler.y = -0.3

for arcIndex, height in enumerate([2.2, 3.55]):
    points = []
    for pointIndex in range(19):
        positionX = -2.05 + 4.1 * pointIndex / 18
        offsetZ = 0.0
        offsetY = 0.0
        if pointIndex > 0 and pointIndex < 18:
            offsetZ = random.uniform(-0.16, 0.16)
            offsetY = random.uniform(-0.07, 0.07)
        points.append((positionX, offsetY, height + offsetZ))
    AddArc('DischargeOuter', points, 0.037, arcMaterial)
    AddArc('DischargeCore', points, 0.012, coreMaterial)
    for branchIndex in [4, 9, 14]:
        start = Vector(points[branchIndex])
        branchPoints = [start, start + Vector((0.17, -0.03, 0.2)), start + Vector((0.32, 0.02, 0.12)), start + Vector((0.49, 0, 0.34))]
        AddArc('DischargeBranch', branchPoints, 0.009, arcMaterial)
    for side in [-1, 1]:
        for sparkIndex in range(5):
            start = Vector((side * 2.05, 0, height))
            end = start + Vector((-side * random.uniform(0.07, 0.3), random.uniform(-0.2, 0.2), random.uniform(-0.28, 0.28)))
            AddArc('ContactSpark', [start, (start + end) / 2 + Vector((0, -0.04, 0.03)), end], 0.008, coreMaterial)

AddBox('StudioFloor', (0, 0, -0.14), (200, 200, 0.2), floorMaterial, 0)
scene = bpy.context.scene
scene.render.engine = 'CYCLES'
scene.cycles.samples = 32
scene.cycles.use_denoising = True
scene.world.color = (0.16, 0.16, 0.16)

def AddArea(name, position, energy, size, color):
    bpy.ops.object.light_add(type='AREA', location=position)
    light = bpy.context.object
    light.name = name
    light.data.energy = energy
    light.data.shape = 'DISK'
    light.data.size = size
    light.data.color = color
    light.rotation_euler = (Vector((0, 0, 2.2)) - light.location).to_track_quat('-Z', 'Y').to_euler()

AddArea('Key', (-4, -6, 9), 1300, 7, (0.82, 0.91, 1))
AddArea('Rim', (3, 4, 7), 1600, 5, (0.6, 0.8, 1))
AddArea('Fill', (5, -3, 4), 500, 4, (1, 0.66, 0.4))
bpy.ops.object.camera_add(location=(7.5, -20, 8.5))
camera = bpy.context.object
camera.rotation_euler = (Vector((0, 0, 2.3)) - camera.location).to_track_quat('-Z', 'Y').to_euler()
camera.data.type = 'ORTHO'
camera.data.ortho_scale = 10.4
scene.camera = camera
scene.render.resolution_x = 1440
scene.render.resolution_y = 1080
scene.render.resolution_percentage = 100
scene.view_settings.view_transform = 'AgX'
scene.use_nodes = True
nodes = scene.node_tree.nodes
nodes.clear()
renderNode = nodes.new('CompositorNodeRLayers')
glowNode = nodes.new('CompositorNodeGlare')
glowNode.glare_type = 'FOG_GLOW'
glowNode.quality = 'HIGH'
glowNode.threshold = 1.5
glowNode.size = 7
compositeNode = nodes.new('CompositorNodeComposite')
scene.node_tree.links.new(renderNode.outputs['Image'], glowNode.inputs['Image'])
scene.node_tree.links.new(glowNode.outputs['Image'], compositeNode.inputs['Image'])
scene.render.filepath = str(outputPath / 'ElectricGatePreview.png')
bpy.ops.wm.save_as_mainfile(filepath=str(outputPath / 'ElectricGate.blend'))
bpy.ops.render.render(write_still=True)
__result__ = {'preview': scene.render.filepath, 'blend': str(outputPath / 'ElectricGate.blend')}
