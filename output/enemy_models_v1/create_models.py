import bpy
import math
import os
from mathutils import Vector

outputDirectory = os.path.dirname(os.path.abspath(__file__))
bpy.ops.wm.read_factory_settings(use_empty=True)

def Material(name, color, roughness, metallic=0.0, transmission=0.0, emission=0.0):
    material = bpy.data.materials.new(name)
    material.diffuse_color = (*color, 1.0)
    material.use_nodes = True
    shader = material.node_tree.nodes.get('Principled BSDF')
    shader.inputs['Base Color'].default_value = (*color, 1.0)
    shader.inputs['Roughness'].default_value = roughness
    shader.inputs['Metallic'].default_value = metallic
    shader.inputs['Transmission Weight'].default_value = transmission
    shader.inputs['IOR'].default_value = 1.31
    shader.inputs['Emission Color'].default_value = (*color, 1.0)
    shader.inputs['Emission Strength'].default_value = emission
    return material

iceMaterial = Material('GlacierIce', (0.28, 0.64, 0.85), 0.20, 0.08, 0.55)
frostMaterial = Material('FrostedArmor', (0.53, 0.76, 0.88), 0.34, 0.1, 0.15)
blackMaterial = Material('UnknownOrganicMatter', (0.009, 0.019, 0.028), 0.24, 0.40)
coreMaterial = Material('AnomalyCore', (0.04, 0.55, 0.9), 0.16, 0.15, 0.1, 1.2)
floorMaterial = Material('StudioFloor', (0.055, 0.075, 0.10), 0.72)

def Sphere(name, location, scale, material, subdivisions=2):
    bpy.ops.mesh.primitive_ico_sphere_add(subdivisions=subdivisions, radius=1, location=location)
    meshObject = bpy.context.object
    meshObject.name = name
    meshObject.scale = scale
    meshObject.data.materials.append(material)
    if material == blackMaterial or material == coreMaterial:
        for polygon in meshObject.data.polygons:
            polygon.use_smooth = True
    return meshObject

def Crystal(name, root, tip, width, material):
    root = Vector(root)
    tip = Vector(tip)
    direction = (tip - root).normalized()
    tangent = direction.cross(Vector((0, 0, 1)))
    if tangent.length < 0.01:
        tangent = direction.cross(Vector((0, 1, 0)))
    tangent.normalize()
    bitangent = direction.cross(tangent).normalized()
    center = root.lerp(tip, 0.32)
    vertices = [tuple(root - direction * width * 0.22)]
    for index in range(5):
        angle = index * math.tau / 5
        point = center + tangent * math.cos(angle) * width + bitangent * math.sin(angle) * width * 0.5
        vertices.append(tuple(point))
    vertices.append(tuple(tip))
    faces = []
    for index in range(5):
        nextIndex = (index + 1) % 5
        faces.append((0, nextIndex + 1, index + 1))
        faces.append((index + 1, nextIndex + 1, 6))
    mesh = bpy.data.meshes.new(name)
    mesh.from_pydata(vertices, [], faces)
    mesh.update()
    meshObject = bpy.data.objects.new(name, mesh)
    bpy.context.collection.objects.link(meshObject)
    mesh.materials.append(material)
    return meshObject

def Tendril(name, points, radius):
    curve = bpy.data.curves.new(name, 'CURVE')
    curve.dimensions = '3D'
    curve.resolution_u = 5
    curve.bevel_depth = radius
    curve.bevel_resolution = 1
    spline = curve.splines.new('BEZIER')
    spline.bezier_points.add(len(points) - 1)
    for point, coordinate in zip(spline.bezier_points, points):
        point.co = coordinate
        point.handle_left_type = 'AUTO'
        point.handle_right_type = 'AUTO'
    curveObject = bpy.data.objects.new(name, curve)
    bpy.context.collection.objects.link(curveObject)
    curve.materials.append(blackMaterial)

def Core(radius, isSwarm):
    subdivisions = 2
    veinCount = 5
    if isSwarm:
        subdivisions = 1
        veinCount = 0
    Sphere('OrganicBody', (0, 0, 0), (radius, radius * 0.78, radius), blackMaterial, subdivisions)
    Sphere('ExposedCore', (0, -radius * 0.70, radius * 0.05), (radius * 0.45,) * 3, coreMaterial, subdivisions)
    for index in range(veinCount):
        angle = index * math.tau / veinCount
        Tendril('CoreVein', [(math.cos(angle) * radius * 0.35, -radius * 0.88, math.sin(angle) * radius * 0.35),
            (math.cos(angle + 0.2) * radius * 0.7, -radius * 0.60, math.sin(angle + 0.2) * radius * 0.7),
            (math.cos(angle + 0.35) * radius, 0, math.sin(angle + 0.35) * radius)], radius * 0.07)

modelNames = ['NormalEnemy', 'MoveEnemy', 'ArmoredEnemy', 'PaintShooterEnemy', 'SwarmEnemy']
displayLocations = [(-4.8, 1.8, 1.8), (0, 1.8, 1.8), (4.8, 1.8, 1.8), (-2.6, -3.0, 1.8), (3.0, -3.0, 1.8)]
modelStatistics = []
for modelIndex, modelName in enumerate(modelNames):
    collection = bpy.data.collections.new(modelName)
    bpy.context.scene.collection.children.link(collection)
    layerCollection = bpy.context.view_layer.layer_collection.children[modelName]
    bpy.context.view_layer.active_layer_collection = layerCollection
    radius = 0.55
    if modelIndex == 2:
        radius = 0.9
    if modelIndex == 3:
        radius = 0.85
    if modelIndex == 4:
        radius = 0.35
    Core(radius, modelIndex == 4)
    if modelIndex == 0:
        for index in range(3):
            angle = index * math.tau / 3 + math.pi / 2
            Crystal('IceFin', (math.cos(angle) * 0.4, 0.1, math.sin(angle) * 0.4),
                (math.cos(angle) * 1.8, 0.6, math.sin(angle) * 1.8), 0.48, iceMaterial)
        Crystal('Nose', (0, -0.35, -0.1), (0, -1.2, -0.25), 0.25, iceMaterial)
    if modelIndex == 1:
        for side in [-1, 1]:
            Crystal('SweptWing', (side * 0.4, 0, 0), (side * 2.0, 0.9, 0.45), 0.7, iceMaterial)
            Tendril('WingVein', [(side * 0.2, -0.1, 0), (side * 0.9, 0.1, 0.15), (side * 1.6, 0.65, 0.4)], 0.055)
        Crystal('Tail', (0, 0.25, 0), (0, 1.75, 0.15), 0.3, iceMaterial)
    if modelIndex == 2:
        for index in range(6):
            angle = index * math.tau / 6
            Crystal('ArmorPlate', (math.cos(angle) * 0.48, -0.05, math.sin(angle) * 0.48),
                (math.cos(angle) * 1.38, 0.5, math.sin(angle) * 1.38), 0.76, frostMaterial)
        Sphere('RearShell', (0, 0.45, 0), (1.0, 0.65, 1.0), frostMaterial, 1)
    if modelIndex == 3:
        Sphere('IceSac', (0, 0.28, 0.1), (1.0, 0.9, 1.0), iceMaterial)
        for index in range(3):
            angle = index * math.tau / 3
            Crystal('SacPlate', (math.cos(angle) * 0.7, 0.1, math.sin(angle) * 0.7),
                (math.cos(angle) * 1.45, 0.4, math.sin(angle) * 1.45), 0.4, frostMaterial)
        bpy.ops.mesh.primitive_torus_add(major_segments=16, minor_segments=6, location=(0, -1.0, 0), rotation=(math.pi / 2, 0, 0), major_radius=0.30, minor_radius=0.13)
        bpy.context.object.name = 'OrganicNozzle'
        bpy.context.object.data.materials.append(blackMaterial)
        Sphere('MouthCavity', (0, -0.97, 0), (0.22, 0.03, 0.22), blackMaterial)
    if modelIndex == 4:
        Crystal('ShardBody', (0, 0, -0.15), (0, 0.5, 1.0), 0.45, iceMaterial)
        Crystal('SmallFin', (-0.15, 0.1, 0), (-0.75, 0.5, -0.5), 0.22, iceMaterial)
        Crystal('SmallFin', (0.15, 0.1, 0), (0.65, 0.5, -0.5), 0.22, iceMaterial)
    bpy.ops.object.select_all(action='DESELECT')
    for meshObject in list(collection.objects):
        meshObject.select_set(True)
    bpy.context.view_layer.objects.active = list(collection.objects)[0]
    bpy.ops.object.convert(target='MESH')
    # One mesh object per enemy; material slots preserve ice, body and core.
    bpy.ops.object.join()
    bpy.context.object.name = modelName
    triangleCount = 0
    for meshObject in collection.objects:
        meshObject.data.calc_loop_triangles()
        triangleCount += len(meshObject.data.loop_triangles)
    modelStatistics.append({'model': modelName, 'triangles': triangleCount, 'objects': len(collection.objects)})
    bpy.ops.export_scene.gltf(filepath=os.path.join(outputDirectory, modelName + '.glb'), use_selection=True)
    bpy.ops.wm.obj_export(filepath=os.path.join(outputDirectory, modelName + '.obj'), export_selected_objects=True, forward_axis='NEGATIVE_Y', up_axis='Z')
    for meshObject in collection.objects:
        meshObject.location += Vector(displayLocations[modelIndex])

bpy.context.view_layer.active_layer_collection = bpy.context.view_layer.layer_collection
bpy.ops.mesh.primitive_plane_add(size=200)
bpy.context.object.data.materials.append(floorMaterial)
world = bpy.data.worlds.new('StudioWorld')
world.use_nodes = True
world.node_tree.nodes['Background'].inputs[0].default_value = (0.16, 0.20, 0.26, 1)
world.node_tree.nodes['Background'].inputs[1].default_value = 0.4
bpy.context.scene.world = world
for location, energy, size in [((-5,-6,10),1800,8), ((7,2,8),2200,7), ((0,5,5),1400,6)]:
    bpy.ops.object.light_add(type='AREA', location=location)
    lightObject = bpy.context.object
    lightObject.data.energy = energy
    lightObject.data.shape = 'DISK'
    lightObject.data.size = size
    lightObject.rotation_euler = (Vector((0,0,1)) - lightObject.location).to_track_quat('-Z','Y').to_euler()
bpy.ops.object.camera_add(location=(4,-20,13))
cameraObject = bpy.context.object
cameraObject.rotation_euler = (Vector((0,-0.3,1.3)) - cameraObject.location).to_track_quat('-Z','Y').to_euler()
cameraObject.data.type = 'ORTHO'
cameraObject.data.ortho_scale = 15.5
scene = bpy.context.scene
scene.camera = cameraObject
scene.render.engine = 'CYCLES'
scene.cycles.samples = 32
scene.cycles.use_denoising = True
scene.render.resolution_x = 1600
scene.render.resolution_y = 1100
scene.render.resolution_percentage = 100
scene.render.image_settings.file_format = 'PNG'
scene.render.filepath = os.path.join(outputDirectory, 'preview.png')
bpy.ops.wm.save_as_mainfile(filepath=os.path.join(outputDirectory, 'EnemyModels.blend'))
bpy.ops.render.render(write_still=True)
__result__ = {'models': modelStatistics, 'preview': scene.render.filepath}
