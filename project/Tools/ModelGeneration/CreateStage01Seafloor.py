import bpy
import math
import random
import json
from pathlib import Path
from mathutils import Vector

kAssetDirectory = Path(r'C:/Projects/KohakuEngine/project/resources/Models/Environment/Ocean/Seafloor')
kSegmentLength = 400.0
kSegmentCount = 3
kWidth = 240.0
kGridStep = 5.0
kRandomSeed = 4271


def RailHeight(distance):
    progress = distance / (kSegmentLength * kSegmentCount)
    transition = 1.0
    if progress < 0.3:
        transition = progress / 0.3
    elif progress > 0.7:
        transition = (1.0 - progress) / 0.3
    transition = max(0.0, min(1.0, transition))
    weight = transition ** 3 * (transition * (transition * 6.0 - 15.0) + 10.0)
    return -20.0 - 65.0 * weight


def FloorHeight(horizontal, distance):
    phase = math.tau * distance / kSegmentLength
    bank = 26.0 * (abs(horizontal) / 95.0) ** 1.8
    ripples = math.sin(horizontal * 0.067 + math.sin(phase) * 1.4) * 1.2
    ripples += math.cos(phase * 3.0 + horizontal * 0.07) * 0.9
    ripples += math.sin(phase * 7.0 + horizontal * 0.09) * 0.45
    ridge = 5.0 * math.exp(-((abs(horizontal) - 65.0) / 12.0) ** 2)
    ridge *= 0.5 + 0.5 * math.sin(phase * 2.0)
    return RailHeight(distance) - 30.0 + bank + ripples + ridge


def MakeMaterial(name, color, texture=None):
    material = bpy.data.materials.new(name)
    material.diffuse_color = (*color, 1.0)
    material.use_nodes = True
    shader = material.node_tree.nodes.get('Principled BSDF')
    shader.inputs['Base Color'].default_value = (*color, 1.0)
    shader.inputs['Roughness'].default_value = 0.88
    if texture is not None:
        imageNode = material.node_tree.nodes.new('ShaderNodeTexImage')
        imageNode.image = texture
        imageNode.extension = 'REPEAT'
        multiply = material.node_tree.nodes.new('ShaderNodeMixRGB')
        multiply.blend_type = 'MULTIPLY'
        multiply.inputs[0].default_value = 1.0
        multiply.inputs[1].default_value = (*color, 1.0)
        material.node_tree.links.new(imageNode.outputs['Color'], multiply.inputs[2])
        material.node_tree.links.new(multiply.outputs['Color'], shader.inputs['Base Color'])
        bump = material.node_tree.nodes.new('ShaderNodeBump')
        bump.inputs['Strength'].default_value = 0.28
        bump.inputs['Distance'].default_value = 0.07
        material.node_tree.links.new(imageNode.outputs['Color'], bump.inputs['Height'])
        material.node_tree.links.new(bump.outputs['Normal'], shader.inputs['Normal'])
        material['diffuseTexture'] = 'textures/sand_ripple.png'
    return material


def MakeMesh(name, vertices, faces, collection, materials):
    mesh = bpy.data.meshes.new(name + '_Mesh')
    mesh.from_pydata(vertices, [], faces)
    mesh.update()
    for material in materials:
        mesh.materials.append(material)
    obj = bpy.data.objects.new(name, mesh)
    collection.objects.link(obj)
    return obj


def AddRod(vertices, faces, start, end, radius, tipRadius):
    start = Vector(start)
    end = Vector(end)
    direction = (end - start).normalized()
    side = direction.cross(Vector((0.0, 1.0, 0.0))).normalized()
    around = direction.cross(side).normalized()
    firstIndex = len(vertices)
    for center, ringRadius in ((start, radius), (end, tipRadius)):
        for cornerIndex in range(6):
            angle = math.tau * cornerIndex / 6.0
            position = center + side * (math.cos(angle) * ringRadius) + around * (math.sin(angle) * ringRadius)
            vertices.append(tuple(position))
    for cornerIndex in range(6):
        nextCorner = (cornerIndex + 1) % 6
        faces.append((firstIndex + cornerIndex, firstIndex + nextCorner,
                      firstIndex + 6 + nextCorner, firstIndex + 6 + cornerIndex))
    faces.append(tuple(firstIndex + 6 + cornerIndex for cornerIndex in range(6)))


def MakeArch(collection, materials):
    vertices = []
    faces = []
    baseHeight = -113.0
    for ringIndex in range(19):
        angle = math.pi * ringIndex / 18.0
        center = Vector((52.0 * math.cos(angle), 620.0, baseHeight + 60.0 * math.sin(angle)))
        tangent = Vector((-52.0 * math.sin(angle), 0.0, 60.0 * math.cos(angle))).normalized()
        side = Vector((0.0, 1.0, 0.0))
        around = tangent.cross(side).normalized()
        radius = 5.2 + math.sin(ringIndex * 1.71) * 0.7
        for cornerIndex in range(8):
            crossAngle = math.tau * cornerIndex / 8.0
            position = center + side * (math.cos(crossAngle) * radius) + around * (math.sin(crossAngle) * radius)
            vertices.append(tuple(position))
    for ringIndex in range(18):
        for cornerIndex in range(8):
            nextCorner = (cornerIndex + 1) % 8
            first = ringIndex * 8
            second = first + 8
            faces.append((first + cornerIndex, first + nextCorner, second + nextCorner, second + cornerIndex))
    arch = MakeMesh('Landmark_StoneArch', vertices, faces, collection, materials)
    for polygon in arch.data.polygons:
        polygon.material_index = polygon.index % len(materials)
    return arch


def MakeGuide(name, positions, collection, color, radius):
    curve = bpy.data.curves.new(name, 'CURVE')
    curve.dimensions = '3D'
    curve.bevel_depth = radius
    curve.bevel_resolution = 1
    spline = curve.splines.new('POLY')
    spline.points.add(len(positions) - 1)
    for point, position in zip(spline.points, positions):
        point.co = (*position, 1.0)
    obj = bpy.data.objects.new(name, curve)
    collection.objects.link(obj)
    material = MakeMaterial(name + '_Material', color)
    shader = material.node_tree.nodes.get('Principled BSDF')
    shader.inputs['Emission Color'].default_value = (*color, 1.0)
    shader.inputs['Emission Strength'].default_value = 2.0
    curve.materials.append(material)
    obj.hide_render = True
    return obj


def PointAt(obj, position):
    obj.rotation_euler = (Vector(position) - obj.location).to_track_quat('-Z', 'Y').to_euler()


def MakeCamera(scene, name, position, target, isOrthographic=False):
    data = bpy.data.cameras.new(name)
    obj = bpy.data.objects.new(name, data)
    scene.collection.objects.link(obj)
    obj.location = position
    data.clip_start = 0.1
    data.clip_end = 5000.0
    data.lens = 24.0
    if isOrthographic:
        data.type = 'ORTHO'
        data.ortho_scale = 1420.0
    PointAt(obj, target)
    return obj


def ExportObj(obj, outputPath, forwardOrigin):
    mesh = obj.data
    mesh.calc_loop_triangles()
    normalMatrix = obj.matrix_world.to_3x3().inverted().transposed()
    uvLayer = mesh.uv_layers.active
    lines = ['# Stage01 seafloor. Engine axes: X right, Y up, Z forward.', 'mtllib seafloor.mtl', 'o ' + obj.name]
    for vertex in mesh.vertices:
        position = obj.matrix_world @ vertex.co
        lines.append('v %.6f %.6f %.6f' % (position.x, position.z, position.y - forwardOrigin))
    cornerIndex = 1
    currentMaterial = -1
    for triangle in mesh.loop_triangles:
        if triangle.material_index != currentMaterial:
            currentMaterial = triangle.material_index
            lines.append('usemtl ' + mesh.materials[currentMaterial].name)
        references = []
        for loopIndex in reversed(triangle.loops):
            loop = mesh.loops[loopIndex]
            uv = (0.0, 0.0)
            if uvLayer is not None:
                uv = uvLayer.data[loopIndex].uv
            normal = (normalMatrix @ mesh.corner_normals[loopIndex].vector).normalized()
            lines.append('vt %.6f %.6f' % (uv[0], uv[1]))
            lines.append('vn %.6f %.6f %.6f' % (normal.x, normal.z, normal.y))
            references.append('%d/%d/%d' % (loop.vertex_index + 1, cornerIndex, cornerIndex))
            cornerIndex += 1
        lines.append('f ' + ' '.join(references))
    outputPath.write_text('\n'.join(lines) + '\n', encoding='utf-8')
    return len(mesh.loop_triangles)


def Main():
    kAssetDirectory.mkdir(parents=True, exist_ok=False)
    (kAssetDirectory / 'textures').mkdir()
    (kAssetDirectory / 'previews').mkdir()
    random.seed(kRandomSeed)
    scene = bpy.data.scenes.new('Stage01_Seafloor')
    bpy.context.window.scene = scene
    scene.unit_settings.system = 'METRIC'
    scene.render.engine = 'BLENDER_EEVEE_NEXT'
    scene.render.resolution_x = 1600
    scene.render.resolution_y = 1000
    scene.render.resolution_percentage = 100
    scene.view_settings.view_transform = 'AgX'
    assetCollection = bpy.data.collections.new('Seafloor_Assets')
    guideCollection = bpy.data.collections.new('Reference_Rail_NotExported')
    scene.collection.children.link(assetCollection)
    scene.collection.children.link(guideCollection)

    texture = bpy.data.images.new('Sand_Ripple', width=256, height=256)
    pixels = []
    for rowIndex in range(256):
        for columnIndex in range(256):
            horizontal = columnIndex / 256.0 * math.tau
            vertical = rowIndex / 256.0 * math.tau
            ridge = math.sin(vertical * 12.0 + math.sin(horizontal * 2.0) * 0.8)
            value = 0.78 + ridge * 0.065 + random.uniform(-0.018, 0.018)
            pixels.extend((value, value, value, 1.0))
    texture.pixels.foreach_set(pixels)
    texture.filepath_raw = str(kAssetDirectory / 'textures/sand_ripple.png')
    texture.file_format = 'PNG'
    texture.save()
    sandMaterials = [MakeMaterial('Sand_Pearl', (0.62, 0.68, 0.49), texture),
                     MakeMaterial('Sand_Seafoam', (0.39, 0.59, 0.49), texture)]
    rockMaterials = [MakeMaterial('Rock_Slate', (0.10, 0.23, 0.25)),
                     MakeMaterial('Rock_Limestone', (0.28, 0.42, 0.36)),
                     MakeMaterial('Rock_Highlight', (0.40, 0.54, 0.43))]
    coralMaterials = [MakeMaterial('Coral_Amber', (0.68, 0.29, 0.12)),
                      MakeMaterial('Coral_Lilac', (0.43, 0.31, 0.57)),
                      MakeMaterial('Coral_Turquoise', (0.06, 0.43, 0.39))]
    grassMaterial = MakeMaterial('Grass_SeaGreen', (0.07, 0.28, 0.20))
    segmentObjects = []
    segmentReports = []
    columnCount = int(kWidth / kGridStep) + 1
    rowCount = int(kSegmentLength / kGridStep) + 1

    for segmentIndex in range(kSegmentCount):
        startDistance = segmentIndex * kSegmentLength
        vertices = []
        faces = []
        for rowIndex in range(rowCount):
            localForward = rowIndex * kGridStep
            for columnIndex in range(columnCount):
                horizontal = -kWidth * 0.5 + columnIndex * kGridStep
                vertices.append((horizontal, startDistance + localForward, FloorHeight(horizontal, startDistance + localForward)))
        for rowIndex in range(rowCount - 1):
            for columnIndex in range(columnCount - 1):
                first = rowIndex * columnCount + columnIndex
                faces.append((first, first + 1, first + columnCount + 1, first + columnCount))
        floor = MakeMesh('Seafloor_Segment_%02d' % (segmentIndex + 1), vertices, faces, assetCollection, sandMaterials)
        uvLayer = floor.data.uv_layers.new(name='UVMap')
        for polygon in floor.data.polygons:
            polygon.material_index = 0
            if polygon.center.x < -35.0:
                polygon.material_index = 1
            for loopIndex in polygon.loop_indices:
                position = floor.data.vertices[floor.data.loops[loopIndex].vertex_index].co
                uvLayer.data[loopIndex].uv = (position.x / 18.0, position.y / 18.0)
        pieces = [floor]

        for rockIndex in range(24):
            horizontal = random.uniform(46.0, 112.0)
            if rockIndex % 2 == 0:
                horizontal = -horizontal
            forward = startDistance + random.uniform(26.0, 374.0)
            size = random.uniform(3.5, 9.0)
            height = FloorHeight(horizontal, forward)
            bpy.ops.mesh.primitive_ico_sphere_add(subdivisions=2, radius=1.0, location=(horizontal, forward, height + size * 0.25))
            rock = bpy.context.object
            rock.name = 'ReefRock_%02d_%02d' % (segmentIndex, rockIndex)
            for collection in list(rock.users_collection):
                collection.objects.unlink(rock)
            assetCollection.objects.link(rock)
            rock.scale = (size * random.uniform(0.7, 1.4), size * random.uniform(0.8, 1.8), size * random.uniform(0.8, 1.7))
            rock.rotation_euler = (random.uniform(-0.3, 0.3), random.uniform(-0.3, 0.3), random.uniform(0.0, math.tau))
            for material in rockMaterials:
                rock.data.materials.append(material)
            for polygon in rock.data.polygons:
                polygon.material_index = random.randrange(len(rockMaterials))
            pieces.append(rock)

        for coralIndex in range(12):
            horizontal = random.uniform(30.0, 102.0)
            if coralIndex % 2 == 0:
                horizontal = -horizontal
            forward = startDistance + random.uniform(24.0, 376.0)
            baseHeight = FloorHeight(horizontal, forward)
            height = random.uniform(2.0, 5.5)
            coralVertices = []
            coralFaces = []
            base = (horizontal, forward, baseHeight)
            fork = (horizontal, forward, baseHeight + height * 0.5)
            AddRod(coralVertices, coralFaces, base, (horizontal, forward, baseHeight + height), 0.55, 0.20)
            AddRod(coralVertices, coralFaces, fork, (horizontal - height * 0.5, forward + 0.5, baseHeight + height * 0.9), 0.30, 0.12)
            AddRod(coralVertices, coralFaces, fork, (horizontal + height * 0.5, forward - 0.5, baseHeight + height * 0.95), 0.30, 0.12)
            coral = MakeMesh('Coral', coralVertices, coralFaces, assetCollection, [coralMaterials[coralIndex % 3]])
            pieces.append(coral)

        grassVertices = []
        grassFaces = []
        for patchIndex in range(28):
            horizontal = random.uniform(-108.0, 108.0)
            forward = startDistance + random.uniform(15.0, 385.0)
            for bladeIndex in range(6):
                baseX = horizontal + random.uniform(-2.0, 2.0)
                baseY = forward + random.uniform(-2.0, 2.0)
                baseZ = FloorHeight(baseX, baseY)
                height = random.uniform(1.0, 3.8)
                first = len(grassVertices)
                grassVertices.extend([(baseX - 0.3, baseY, baseZ), (baseX + 0.3, baseY, baseZ),
                                      (baseX, baseY + 0.3, baseZ), (baseX + 0.4, baseY + 0.7, baseZ + height)])
                grassFaces.extend([(first, first + 1, first + 3), (first + 1, first + 2, first + 3), (first + 2, first, first + 3)])
        pieces.append(MakeMesh('Seagrass', grassVertices, grassFaces, assetCollection, [grassMaterial]))
        if segmentIndex == 1:
            pieces.append(MakeArch(assetCollection, rockMaterials))
        bpy.ops.object.select_all(action='DESELECT')
        for piece in pieces:
            piece.select_set(True)
        bpy.context.view_layer.objects.active = floor
        bpy.ops.object.join()
        floor['segmentLength'] = kSegmentLength
        floor['segmentRole'] = ('Entry', 'Repeat', 'Exit')[segmentIndex]
        segmentObjects.append(floor)
        bpy.context.view_layer.update()
        triangleCount = ExportObj(floor, kAssetDirectory / ('seafloor_%02d.obj' % (segmentIndex + 1)), startDistance)
        segmentReports.append({'file': 'seafloor_%02d.obj' % (segmentIndex + 1), 'triangles': triangleCount,
                               'placementEngine': [0.0, 0.0, 900.0 + startDistance], 'length': kSegmentLength})

    materials = sandMaterials + rockMaterials + coralMaterials + [grassMaterial]
    materialLines = []
    for material in materials:
        color = material.diffuse_color
        materialLines.extend(['newmtl ' + material.name, 'Kd %.5f %.5f %.5f' % (color[0], color[1], color[2]), 'Ka 0.1 0.1 0.1', 'Ks 0 0 0', 'Ns 8', 'd 1', 'illum 2'])
        if 'diffuseTexture' in material:
            materialLines.append('map_Kd ' + material['diffuseTexture'])
        materialLines.append('')
    (kAssetDirectory / 'seafloor.mtl').write_text('\n'.join(materialLines), encoding='utf-8')
    guide = MakeGuide('Guide_CurrentUnderwaterRail', [(0.0, float(distance), RailHeight(float(distance))) for distance in range(0, 1201, 5)], guideCollection, (0.06, 0.95, 1.0), 0.35)
    for distance in range(0, 1201, 100):
        height = RailHeight(float(distance))
        MakeGuide('Guide_Clearance_%04d' % distance, [(-20.0, distance, height - 15.0), (20.0, distance, height - 15.0), (20.0, distance, height + 15.0), (-20.0, distance, height + 15.0), (-20.0, distance, height - 15.0)], guideCollection, (0.08, 0.45, 0.55), 0.09)

    world = bpy.data.worlds.new('Seafloor_BlueWorld')
    world.use_nodes = True
    world.node_tree.nodes.get('Background').inputs['Color'].default_value = (0.035, 0.10, 0.15, 1.0)
    world.node_tree.nodes.get('Background').inputs['Strength'].default_value = 0.5
    volume = world.node_tree.nodes.new('ShaderNodeVolumeScatter')
    volume.name = 'UnderwaterPreviewVolume'
    volume.inputs['Color'].default_value = (0.20, 0.55, 0.65, 1.0)
    volume.inputs['Density'].default_value = 0.0
    volume.inputs['Anisotropy'].default_value = 0.2
    world.node_tree.links.new(volume.outputs['Volume'], world.node_tree.nodes.get('World Output').inputs['Volume'])
    scene.world = world
    sunData = bpy.data.lights.new('Seafloor_Sun', 'SUN')
    sunData.energy = 2.2
    sunData.angle = 0.15
    sunData.color = (0.61, 0.86, 1.0)
    sun = bpy.data.objects.new('Seafloor_Sun', sunData)
    scene.collection.objects.link(sun)
    sun.rotation_euler = (0.35, -0.45, -0.2)
    for lightIndex in range(3):
        lightData = bpy.data.lights.new('WaterShaft_%02d' % lightIndex, 'AREA')
        lightData.energy = 150000.0
        lightData.color = (0.30, 0.72, 0.88)
        lightData.shape = 'DISK'
        lightData.size = 130.0
        light = bpy.data.objects.new(lightData.name, lightData)
        scene.collection.objects.link(light)
        light.location = (-45.0, 180.0 + lightIndex * 400.0, 45.0)
        PointAt(light, (15.0, 240.0 + lightIndex * 400.0, -110.0))
    overview = MakeCamera(scene, 'Camera_Overview', (610.0, -120.0, 650.0), (0.0, 600.0, -80.0), True)
    beauty = MakeCamera(scene, 'Camera_Underwater', (8.0, 445.0, -72.0), (0.0, 650.0, -82.0))
    scene.camera = overview
    scene.render.filepath = str(kAssetDirectory / 'previews/seafloor_overview.png')
    for area in bpy.context.screen.areas:
        if area.type == 'VIEW_3D':
            area.spaces.active.shading.type = 'SOLID'
            area.spaces.active.shading.color_type = 'MATERIAL'
            area.spaces.active.region_3d.view_location = (0.0, 600.0, -80.0)
            area.spaces.active.region_3d.view_distance = 950.0
    bpy.ops.object.select_all(action='DESELECT')
    segmentObjects[1].select_set(True)
    bpy.context.view_layer.objects.active = segmentObjects[1]
    texture.pack()
    report = {'segmentLength': kSegmentLength, 'currentRouteLength': 1200.0, 'width': kWidth,
              'triangles': sum(item['triangles'] for item in segmentReports), 'materials': len(materials),
              'segments': segmentReports, 'gamePlacementChanged': False,
              'fourMinuteDistanceAtNormalSpeed': 11520.0, 'repeatSegmentsForAboutFourMinutes': 27,
              'originalScenePreserved': 'Scene' in bpy.data.scenes}
    (kAssetDirectory / 'asset_info.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
    bpy.ops.wm.save_as_mainfile(filepath=str(kAssetDirectory / 'stage01_seafloor.blend'))
    return report


__result__ = Main()
