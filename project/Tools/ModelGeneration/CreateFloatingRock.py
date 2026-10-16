import bpy
import bmesh
import math
import random
import json
from pathlib import Path
from mathutils import Vector

kAssetDirectory = Path(__file__).resolve().parents[2] / 'resources/Models/Environment/Ocean/FloatingRock'
kPreviewDirectory = kAssetDirectory / 'previews'
kRandomSeed = 1742
randomEngine = random.Random(kRandomSeed)


def CreateMaterial(name, color, roughness, emissionStrength=0.0):
    material = bpy.data.materials.new(name)
    material.use_nodes = True
    shader = material.node_tree.nodes.get('Principled BSDF')
    shader.inputs['Base Color'].default_value = (*color, 1.0)
    shader.inputs['Roughness'].default_value = roughness
    if emissionStrength > 0.0:
        shader.inputs['Emission Color'].default_value = (*color, 1.0)
        shader.inputs['Emission Strength'].default_value = emissionStrength
    # Actual image files keep the OBJ independent of Blender shader nodes.
    imageSize = 64
    image = bpy.data.images.new(name + 'Texture', width=imageSize, height=imageSize)
    pixels = []
    for pixelIndex in range(imageSize * imageSize):
        shade = randomEngine.uniform(0.94, 1.06)
        for component in color:
            pixels.append(min(1.0, component * shade))
        pixels.append(1.0)
    image.pixels = pixels
    image.filepath_raw = str(kAssetDirectory / 'textures' / (name + '.png'))
    image.file_format = 'PNG'
    image.save()
    texture = material.node_tree.nodes.new('ShaderNodeTexImage')
    texture.image = image
    material.node_tree.links.new(texture.outputs['Color'], shader.inputs['Base Color'])
    material.diffuse_color = (*color, 1.0)
    return material


def AssignMaterials(meshObject, materials):
    for material in materials:
        meshObject.data.materials.append(material)


def CreateShard(name, position, scale, materials):
    bpy.ops.mesh.primitive_ico_sphere_add(subdivisions=1, radius=1.0, location=position)
    shard = bpy.context.object
    shard.name = name
    shard.scale = scale
    shard.rotation_euler = (randomEngine.uniform(-1.0, 1.0), randomEngine.uniform(-1.0, 1.0), randomEngine.uniform(-2.0, 2.0))
    for vertex in shard.data.vertices:
        vertex.co *= randomEngine.uniform(0.82, 1.15)
    AssignMaterials(shard, materials)
    for face in shard.data.polygons:
        face.material_index = randomEngine.randrange(len(materials))
    return shard


def CreateCrystal(name, position, radius, length, material):
    vertices = []
    for ringIndex in range(2):
        ringRadius = radius
        ringHeight = 0.0
        if ringIndex == 1:
            ringRadius *= 0.72
            ringHeight = -length * 0.64
        for cornerIndex in range(5):
            angle = cornerIndex * math.tau / 5.0
            vertices.append((math.cos(angle) * ringRadius, math.sin(angle) * ringRadius, ringHeight))
    vertices.append((0.0, 0.0, -length))
    faces = [(4, 3, 2, 1, 0)]
    for cornerIndex in range(5):
        nextIndex = (cornerIndex + 1) % 5
        faces.append((cornerIndex, nextIndex, nextIndex + 5, cornerIndex + 5))
        faces.append((cornerIndex + 5, nextIndex + 5, 10))
    mesh = bpy.data.meshes.new(name + 'Mesh')
    mesh.from_pydata(vertices, [], faces)
    mesh.update()
    crystal = bpy.data.objects.new(name, mesh)
    bpy.context.collection.objects.link(crystal)
    crystal.location = position
    crystal.rotation_euler = (randomEngine.uniform(-0.28, 0.28), randomEngine.uniform(-0.28, 0.28), randomEngine.random() * math.tau)
    AssignMaterials(crystal, [material])
    return crystal


def PointAt(sceneObject, target):
    sceneObject.rotation_euler = (Vector(target) - sceneObject.location).to_track_quat('-Z', 'Y').to_euler()


def AddAreaLight(name, location, energy, size, color):
    data = bpy.data.lights.new(name, 'AREA')
    data.energy = energy
    data.shape = 'DISK'
    data.size = size
    data.color = color
    sceneObject = bpy.data.objects.new(name, data)
    bpy.context.collection.objects.link(sceneObject)
    sceneObject.location = location
    PointAt(sceneObject, (0.0, 0.0, 0.0))


def Main():
    kAssetDirectory.mkdir(parents=True, exist_ok=True)
    kPreviewDirectory.mkdir(exist_ok=True)
    (kAssetDirectory / 'textures').mkdir(exist_ok=True)
    bpy.ops.object.select_all(action='SELECT')
    bpy.ops.object.delete(use_global=False)
    materials = [
        CreateMaterial('Sandstone', (0.56, 0.39, 0.23), 0.88),
        CreateMaterial('SunlitStone', (0.77, 0.61, 0.40), 0.90),
        CreateMaterial('WeatheredStone', (0.36, 0.28, 0.19), 0.94),
        CreateMaterial('FractureInterior', (0.15, 0.16, 0.14), 0.98),
    ]
    crystalMaterial = CreateMaterial('SeaCrystal', (0.055, 0.52, 0.43), 0.32, 0.28)
    paleCrystalMaterial = CreateMaterial('CrystalFacet', (0.18, 0.73, 0.61), 0.30, 0.18)
    bpy.ops.mesh.primitive_ico_sphere_add(subdivisions=3, radius=1.0)
    baseObject = bpy.context.object
    for vertex in baseObject.data.vertices:
        variation = randomEngine.uniform(0.90, 1.10)
        vertex.co.x *= 1.42 * variation
        vertex.co.y *= 1.12 * variation
        vertex.co.z *= 1.13 * variation
        if vertex.co.z < -0.15:
            vertex.co.x *= 0.86
            vertex.co.y *= 0.90
    baseMesh = baseObject.data.copy()
    bpy.data.objects.remove(baseObject, do_unlink=True)
    seeds = [Vector(value) for value in [(-0.55, -0.42, 0.48), (0.58, -0.30, 0.45), (-0.08, 0.58, 0.55), (-0.54, 0.28, -0.42), (0.53, 0.36, -0.35), (0.05, -0.52, -0.46)]]
    assetObjects = []
    for seedIndex, seed in enumerate(seeds):
        mesh = baseMesh.copy()
        meshObject = bpy.data.objects.new('RockChunk' + str(seedIndex + 1), mesh)
        bpy.context.collection.objects.link(meshObject)
        AssignMaterials(meshObject, materials)
        editableMesh = bmesh.new()
        editableMesh.from_mesh(mesh)
        for face in editableMesh.faces:
            face.material_index = randomEngine.choices([0, 1, 2], weights=[6, 3, 1])[0]
        for otherIndex, otherSeed in enumerate(seeds):
            if seedIndex == otherIndex:
                continue
            geometry = list(editableMesh.verts) + list(editableMesh.edges) + list(editableMesh.faces)
            bmesh.ops.bisect_plane(editableMesh, geom=geometry, dist=0.00001,
                plane_co=(seed + otherSeed) * 0.5, plane_no=(otherSeed - seed).normalized(),
                clear_outer=True, clear_inner=False)
            boundaryEdges = []
            for edge in editableMesh.edges:
                if edge.is_boundary:
                    boundaryEdges.append(edge)
            if boundaryEdges:
                filled = bmesh.ops.holes_fill(editableMesh, edges=boundaryEdges, sides=0)
                for face in filled['faces']:
                    face.material_index = 3
        center = Vector((0.0, 0.0, 0.0))
        for vertex in editableMesh.verts:
            center += vertex.co
        center /= max(1, len(editableMesh.verts))
        for vertex in editableMesh.verts:
            vertex.co = center + (vertex.co - center) * 0.965 + seed.normalized() * 0.052
        bmesh.ops.recalc_face_normals(editableMesh, faces=list(editableMesh.faces))
        editableMesh.to_mesh(mesh)
        editableMesh.free()
        assetObjects.append(meshObject)
    bpy.data.meshes.remove(baseMesh)
    assetObjects.append(CreateShard('CrystalCore', (0.0, 0.0, -0.05), (0.77, 0.65, 0.72), [crystalMaterial, paleCrystalMaterial]))
    for index, position in enumerate([(-1.62, -0.12, 0.20), (1.63, 0.05, 0.04), (-1.30, 0.72, -0.45), (1.10, -0.72, -0.67), (-0.42, -0.38, -1.42), (0.77, 0.60, 1.26)]):
        shardSize = randomEngine.uniform(0.13, 0.23)
        assetObjects.append(CreateShard('FloatingFragment' + str(index + 1), position,
            (shardSize * 1.25, shardSize, shardSize * 0.82), materials[:3]))
    for index, position in enumerate([(-0.42, -0.25, -0.78), (0.12, -0.42, -0.86), (0.43, 0.10, -0.72)]):
        assetObjects.append(CreateCrystal('Crystal' + str(index + 1), position, 0.14, 0.48 + index * 0.09, crystalMaterial))
    bpy.ops.object.select_all(action='DESELECT')
    for sceneObject in assetObjects:
        sceneObject.select_set(True)
    bpy.context.view_layer.objects.active = assetObjects[0]
    bpy.ops.object.transform_apply(location=False, rotation=True, scale=True)
    allPoints = []
    for sceneObject in assetObjects:
        for vertex in sceneObject.data.vertices:
            allPoints.append(sceneObject.matrix_world @ vertex.co)
    minimum = Vector((min(point.x for point in allPoints), min(point.y for point in allPoints), min(point.z for point in allPoints)))
    maximum = Vector((max(point.x for point in allPoints), max(point.y for point in allPoints), max(point.z for point in allPoints)))
    center = (minimum + maximum) * 0.5
    normalization = 1.0 / max(maximum - minimum)
    for sceneObject in assetObjects:
        for vertex in sceneObject.data.vertices:
            vertex.co = (sceneObject.matrix_world @ vertex.co - center) * normalization
        sceneObject.location = (0.0, 0.0, 0.0)
    bpy.ops.object.mode_set(mode='EDIT')
    bpy.ops.mesh.select_all(action='SELECT')
    bpy.ops.uv.smart_project(island_margin=0.035)
    bpy.ops.object.mode_set(mode='OBJECT')
    triangleCount = 0
    for sceneObject in assetObjects:
        sceneObject.data.calc_loop_triangles()
        triangleCount += len(sceneObject.data.loop_triangles)
    bpy.ops.wm.obj_export(filepath=str(kAssetDirectory / 'floating_rock.obj'),
        export_selected_objects=True, export_triangulated_mesh=True, export_materials=True,
        forward_axis='NEGATIVE_Z', up_axis='Y', path_mode='RELATIVE')
    bpy.ops.export_scene.gltf(filepath=str(kAssetDirectory / 'floating_rock.glb'),
        export_format='GLB', use_selection=True, export_yup=True)
    scene = bpy.context.scene
    scene.render.engine = 'CYCLES'
    scene.cycles.samples = 32
    scene.cycles.use_denoising = True
    scene.render.resolution_x = 1100
    scene.render.resolution_y = 1000
    scene.render.resolution_percentage = 100
    scene.render.image_settings.file_format = 'PNG'
    scene.world.color = (0.11, 0.15, 0.19)
    scene.view_settings.view_transform = 'AgX'
    AddAreaLight('WarmKey', (1.2, -1.8, 2.4), 140.0, 2.5, (1.0, 0.86, 0.66))
    AddAreaLight('OceanFill', (-1.7, -0.4, 0.2), 70.0, 2.0, (0.38, 0.74, 0.90))
    AddAreaLight('Rim', (0.8, 1.6, 1.0), 170.0, 1.8, (0.58, 0.90, 1.0))
    bpy.ops.mesh.primitive_plane_add(size=200, location=(0.0, 0.0, -0.75))
    ground = bpy.context.object
    ground.name = 'PreviewGround_NotExported'
    groundMaterial = bpy.data.materials.new('PreviewGround')
    groundMaterial.diffuse_color = (0.018, 0.043, 0.065, 1.0)
    ground.data.materials.append(groundMaterial)
    cameraData = bpy.data.cameras.new('PreviewCamera')
    camera = bpy.data.objects.new('PreviewCamera', cameraData)
    scene.collection.objects.link(camera)
    camera.location = (1.2, -1.9, 0.95)
    cameraData.type = 'ORTHO'
    cameraData.ortho_scale = 1.58
    PointAt(camera, (0.0, 0.0, -0.02))
    scene.camera = camera
    scene.render.filepath = str(kPreviewDirectory / 'floating_rock.png')
    bpy.ops.wm.save_as_mainfile(filepath=str(kAssetDirectory / 'floating_rock.blend'))
    bpy.ops.render.render(write_still=True)
    report = {'triangles': triangleCount, 'meshObjects': len(assetObjects),
        'materials': len(materials) + 2, 'normalizedDimensions': list((maximum - minimum) * normalization),
        'obj': str(kAssetDirectory / 'floating_rock.obj')}
    (kAssetDirectory / 'asset_info.json').write_text(json.dumps(report, indent=2), encoding='utf8')
    print('FLOATING_ROCK_RESULT ' + json.dumps(report))


Main()
