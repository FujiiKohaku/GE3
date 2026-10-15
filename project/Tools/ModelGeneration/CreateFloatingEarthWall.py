import bpy
import math
import random
import json
from pathlib import Path
from mathutils import Vector
import sys

sys.path.insert(0, str(Path(__file__).resolve().parent))
import CreateSolidRuinsWall

kAssetDirectory = Path(__file__).resolve().parents[2] / 'resources/Models/Environment/Ocean/FloatingRuins'
kRandomSeed = 38105
randomEngine = random.Random(kRandomSeed)
assetObjects = []


def CreateMaterial(name, color, roughness=0.9):
    material = bpy.data.materials.new(name)
    material.use_nodes = True
    shader = material.node_tree.nodes.get('Principled BSDF')
    shader.inputs['Roughness'].default_value = roughness
    shader.inputs['Base Color'].default_value = (*color, 1.0)
    material.diffuse_color = (*color, 1.0)
    imageSize = 128
    image = bpy.data.images.new(name + 'Texture', width=imageSize, height=imageSize)
    pixels = []
    for row in range(imageSize):
        for column in range(imageSize):
            variation = randomEngine.uniform(-0.045, 0.045)
            variation += math.sin(row * 0.085 + math.sin(column * 0.045)) * 0.022
            for component in color:
                pixels.append(max(0.0, min(1.0, component + variation)))
            pixels.append(1.0)
    image.pixels = pixels
    image.filepath_raw = str(kAssetDirectory / 'textures' / (name + '.png'))
    image.file_format = 'PNG'
    image.save()
    texture = material.node_tree.nodes.new('ShaderNodeTexImage')
    texture.image = image
    material.node_tree.links.new(texture.outputs['Color'], shader.inputs['Base Color'])
    return material


def CreateMesh(name, vertices, faces, materials, materialIndices=None):
    mesh = bpy.data.meshes.new(name + 'Mesh')
    mesh.from_pydata(vertices, [], faces)
    mesh.update()
    sceneObject = bpy.data.objects.new(name, mesh)
    bpy.context.collection.objects.link(sceneObject)
    for material in materials:
        mesh.materials.append(material)
    if materialIndices:
        for index, face in enumerate(mesh.polygons):
            face.material_index = materialIndices[index]
    assetObjects.append(sceneObject)
    return sceneObject


def CreateBoulder(name, position, dimensions, material):
    bpy.ops.mesh.primitive_ico_sphere_add(subdivisions=1, radius=1.0, location=position)
    boulder = bpy.context.object
    boulder.name = name
    boulder.scale = dimensions
    boulder.rotation_euler = (randomEngine.random(), randomEngine.random(), randomEngine.random() * math.tau)
    for vertex in boulder.data.vertices:
        vertex.co *= randomEngine.uniform(0.8, 1.15)
    boulder.data.materials.append(material)
    assetObjects.append(boulder)
    return boulder


def CreateRoot(name, points, radius, material):
    vertices = []
    faces = []
    kSides = 5
    for pointIndex, point in enumerate(points):
        direction = Vector((0.0, 0.0, -1.0))
        if pointIndex + 1 < len(points):
            direction = Vector(points[pointIndex + 1]) - Vector(point)
        elif pointIndex > 0:
            direction = Vector(point) - Vector(points[pointIndex - 1])
        rotation = direction.normalized().to_track_quat('Z', 'Y')
        ringRadius = radius * (1.0 - pointIndex / len(points) * 0.88)
        for side in range(kSides):
            angle = math.tau * side / kSides
            local = Vector((math.cos(angle) * ringRadius, math.sin(angle) * ringRadius, 0.0))
            vertices.append(Vector(point) + rotation @ local)
        if pointIndex > 0:
            for side in range(kSides):
                nextSide = (side + 1) % kSides
                offset = pointIndex * kSides
                faces.append((offset + side, offset + nextSide, offset - kSides + nextSide, offset - kSides + side))
    faces.append(tuple(reversed(range(kSides))))
    faces.append(tuple(range(len(vertices) - kSides, len(vertices))))
    return CreateMesh(name, vertices, faces, [material])


def PointAt(sceneObject, target):
    sceneObject.rotation_euler = (Vector(target) - sceneObject.location).to_track_quat('-Z', 'Y').to_euler()


def AddLight(name, location, energy, size, color):
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
    (kAssetDirectory / 'textures').mkdir(parents=True, exist_ok=True)
    (kAssetDirectory / 'previews').mkdir(exist_ok=True)
    bpy.ops.object.select_all(action='SELECT')
    bpy.ops.object.delete(use_global=False)
    soil = CreateMaterial('Earth', (0.26, 0.12, 0.047))
    soilLight = CreateMaterial('EarthStrata', (0.38, 0.20, 0.085))
    grass = CreateMaterial('Grass', (0.22, 0.36, 0.045))
    moss = CreateMaterial('Moss', (0.095, 0.20, 0.036))
    stoneMaterials = [CreateMaterial('Sandstone', (0.64, 0.48, 0.29)), CreateMaterial('SandstoneLight', (0.78, 0.65, 0.43)), CreateMaterial('SandstoneShade', (0.47, 0.36, 0.22))]
    rootMaterial = CreateMaterial('Roots', (0.16, 0.087, 0.035))
    crystalMaterial = CreateMaterial('SeaCrystal', (0.08, 0.56, 0.46), 0.32)
    kRingCount = 24
    perimeter = []
    for index in range(kRingCount):
        angle = index * math.tau / kRingCount
        directionX = math.cos(angle)
        directionY = math.sin(angle)
        pointX = math.copysign(abs(directionX) ** 0.55, directionX) * 6.2
        pointY = math.copysign(abs(directionY) ** 0.65, directionY) * 2.45
        perimeter.append((pointX * randomEngine.uniform(0.94, 1.05), pointY * randomEngine.uniform(0.94, 1.05)))
    ringSettings = [(1.0, 0.54), (1.015, 0.28), (0.97, -0.36), (0.94, -0.63), (0.88, -1.40), (0.73, -1.75), (0.66, -2.56), (0.38, -3.40), (0.10, -3.90)]
    vertices = []
    for ringIndex, (radiusScale, height) in enumerate(ringSettings):
        for pointX, pointY in perimeter:
            vertices.append((pointX * radiusScale, pointY * radiusScale, height + randomEngine.uniform(-0.13, 0.13)))
    faces = []
    materialIndices = []
    for ringIndex in range(len(ringSettings) - 1):
        for index in range(kRingCount):
            nextIndex = (index + 1) % kRingCount
            faces.append((ringIndex * kRingCount + index, (ringIndex + 1) * kRingCount + index, (ringIndex + 1) * kRingCount + nextIndex, ringIndex * kRingCount + nextIndex))
            materialIndex = 0
            if ringIndex % 3 == 1:
                materialIndex = 1
            if ringIndex == 0:
                materialIndex = 3
            materialIndices.append(materialIndex)
    vertices.append((0.0, 0.0, 0.60))
    centerIndex = len(vertices) - 1
    for index in range(kRingCount):
        faces.append((index, (index + 1) % kRingCount, centerIndex))
        materialIndices.append(2)
    faces.append(tuple(reversed(range((len(ringSettings) - 1) * kRingCount, len(ringSettings) * kRingCount))))
    materialIndices.append(0)
    CreateMesh('FloatingEarth', vertices, faces, [soil, soilLight, grass, moss], materialIndices)
    masonryMaterial = CreateMaterial('RuinsMasonry', (0.64, 0.48, 0.29))
    CreateSolidRuinsWall.CreateSolidMasonry(sys.modules[__name__], masonryMaterial)
    # Embedded stones interrupt the exposed soil strata.
    for index in range(15):
        angle = randomEngine.uniform(0.0, math.tau)
        layerScale = randomEngine.uniform(0.68, 0.95)
        CreateBoulder('EmbeddedRock', (math.cos(angle) * 5.6 * layerScale, math.sin(angle) * 2.20 * layerScale, randomEngine.uniform(-2.2, -0.2)),
            (randomEngine.uniform(0.45, 0.85), 0.45, randomEngine.uniform(0.40, 0.70)), stoneMaterials[1])
    for index in range(10):
        CreateBoulder('LooseStone', (randomEngine.uniform(-5.3, 5.3), randomEngine.uniform(-1.7, 1.6), 0.69), (0.34, 0.26, 0.22), stoneMaterials[index % 3])
    for index in range(22):
        pointX, pointY = perimeter[index]
        length = randomEngine.uniform(1.2, 3.8)
        points = []
        for step in range(7):
            rate = step / 6.0
            points.append((pointX * (1.0 - rate * 0.24) + math.sin(rate * 6 + index) * 0.12,
                pointY * (1.0 - rate * 0.15) + math.sin(rate * 4 + index) * 0.10,
                0.48 - length * rate))
        CreateRoot('HangingRoot', points, randomEngine.uniform(0.045, 0.085), rootMaterial)
    for index in range(20):
        pointX = randomEngine.uniform(-5.4, 5.4)
        if abs(pointX) < 1.45:
            continue
        startHeight = randomEngine.uniform(1.6, 3.4)
        points = [(pointX + math.sin(step * 0.7) * 0.09, -0.10, startHeight - step * 0.30) for step in range(6)]
        CreateRoot('WallVine', points, 0.032, moss)
        for step in [1, 3, 5]:
            CreateBoulder('VineLeaves', points[step], (0.14, 0.08, 0.16), moss)
    # Opaque triangular blades avoid alpha texture sorting in the game.
    grassVertices = []
    grassFaces = []
    for index in range(100):
        angle = index * math.tau / 100
        pointX = math.copysign(abs(math.cos(angle)) ** 0.55, math.cos(angle)) * 6.05
        pointY = math.copysign(abs(math.sin(angle)) ** 0.65, math.sin(angle)) * 2.36
        for blade in range(3):
            baseX = pointX + randomEngine.uniform(-0.14, 0.14)
            baseY = pointY + randomEngine.uniform(-0.10, 0.10)
            bladeHeight = randomEngine.uniform(0.15, 0.34)
            offset = len(grassVertices)
            grassVertices.extend([(baseX - 0.06, baseY, 0.55), (baseX + 0.06, baseY, 0.55), (baseX + 0.11, baseY + 0.05, 0.55 + bladeHeight)])
            grassFaces.append((offset, offset + 1, offset + 2))
            grassFaces.append((offset + 2, offset + 1, offset))
    CreateMesh('GrassFringe', grassVertices, grassFaces, [grass])
    for position in [(-3.9, -1.5, -1.95), (3.8, -1.2, -2.10), (1.1, 0.8, -3.0)]:
        CreateBoulder('LevitationCrystal', position, (0.22, 0.25, 0.56), crystalMaterial)
    bpy.ops.object.select_all(action='DESELECT')
    for sceneObject in assetObjects:
        sceneObject.select_set(True)
    bpy.context.view_layer.objects.active = assetObjects[0]
    bpy.ops.object.join()
    model = bpy.context.object
    model.name = 'FloatingEarthWall'
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    model.data.calc_loop_triangles()
    triangleCount = len(model.data.loop_triangles)
    bpy.ops.object.mode_set(mode='EDIT')
    bpy.ops.mesh.select_all(action='SELECT')
    bpy.ops.uv.smart_project(island_margin=0.015)
    bpy.ops.object.mode_set(mode='OBJECT')
    CreateSolidRuinsWall.SetMasonryUvs(model, masonryMaterial)
    CreateSolidRuinsWall.ExportParts(model, kAssetDirectory, masonryMaterial)
    bpy.ops.wm.obj_export(filepath=str(kAssetDirectory / 'floating_earth_wall.obj'),
        export_selected_objects=True, export_materials=True, export_triangulated_mesh=True,
        forward_axis='NEGATIVE_Z', up_axis='Y', path_mode='RELATIVE')
    bpy.ops.export_scene.gltf(filepath=str(kAssetDirectory / 'floating_earth_wall.glb'), export_format='GLB', use_selection=True, export_yup=True)
    CreateSolidRuinsWall.SetPreviewMaterial(masonryMaterial)
    scene = bpy.context.scene
    scene.render.engine = 'CYCLES'
    scene.cycles.samples = 40
    scene.cycles.use_denoising = True
    scene.render.resolution_x = 1400
    scene.render.resolution_y = 1000
    scene.render.resolution_percentage = 100
    scene.render.image_settings.file_format = 'PNG'
    scene.view_settings.view_transform = 'AgX'
    scene.world.color = (0.32, 0.43, 0.54)
    AddLight('WarmSun', (-3, -8, 15), 2400, 10, (1.0, 0.86, 0.65))
    AddLight('SkyFill', (5, 2, 10), 1900, 9, (0.62, 0.82, 1.0))
    AddLight('FrontFill', (1, -10, 1), 500, 8, (0.65, 0.90, 1.0))
    bpy.ops.mesh.primitive_plane_add(size=2000, location=(0, 0, -7.8))
    plane = bpy.context.object
    plane.name = 'PreviewOcean_NotExported'
    oceanMaterial = bpy.data.materials.new('PreviewOcean')
    oceanMaterial.diffuse_color = (0.035, 0.23, 0.32, 1.0)
    plane.data.materials.append(oceanMaterial)
    cameraData = bpy.data.cameras.new('PreviewCamera')
    camera = bpy.data.objects.new('PreviewCamera', cameraData)
    scene.collection.objects.link(camera)
    cameraData.type = 'ORTHO'
    cameraData.ortho_scale = 17.4
    camera.location = (11, -19, 10)
    PointAt(camera, (0, 0, -0.15))
    scene.camera = camera
    scene.render.filepath = str(kAssetDirectory / 'previews/floating_earth_wall.png')
    bpy.ops.wm.save_as_mainfile(filepath=str(kAssetDirectory / 'floating_earth_wall.blend'))
    bpy.ops.render.render(write_still=True)
    camera.location = (-11, 17, 7)
    PointAt(camera, (0, 0, -0.15))
    scene.render.resolution_x = 1120
    scene.render.resolution_y = 800
    scene.cycles.samples = 24
    scene.render.filepath = str(kAssetDirectory / 'previews/floating_earth_wall_back.png')
    bpy.ops.render.render(write_still=True)
    report = {'triangles': triangleCount, 'materials': len(model.data.materials), 'dimensionsBlender': list(model.dimensions), 'placedInStage': False}
    (kAssetDirectory / 'asset_info.json').write_text(json.dumps(report, indent=2), encoding='utf8')
    print('FLOATING_EARTH_WALL_RESULT ' + json.dumps(report))


Main()
