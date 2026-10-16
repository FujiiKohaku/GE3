"""Build each gate as the boundary of a rectangle union, without internal faces."""

import bpy
import bmesh
import json
import sys
from pathlib import Path
from mathutils import Vector

sys.path.insert(0, str(Path(__file__).resolve().parent))
import CreateSolidRuinsWall

kProjectDirectory = Path(__file__).resolve().parents[2]
kAssetDirectory = kProjectDirectory / 'resources/Models/Environment/Ocean/FloatingRuins'
kScenePath = kProjectDirectory / 'resources/Scenes/stage01.json'
kGateParts = {'Gate02': ['Gate02_Left', 'Gate02_Right', 'Gate02_LowCeiling'],
    'OffsetGate': ['OffsetGate_Left', 'OffsetGate_Right', 'OffsetGate_Top'],
    'FinalGate': ['FinalGate_Left', 'FinalGate_Right', 'FinalGate_Bottom'],
    'SecondActGate': ['SecondActGate_Left', 'SecondActGate_Right', 'SecondActGate_Bottom']}


def GetRectangles(parts, originX):
    rectangles = []
    for part in parts:
        position = part['transform']['translation']
        size = part['collider']['size']
        center = part['collider']['center']
        rectangles.append((position[0] + center[0] - originX - size[0] * 0.5,
            position[0] + center[0] - originX + size[0] * 0.5,
            position[2] + center[2] - size[2] * 0.5,
            position[2] + center[2] + size[2] * 0.5))
    return rectangles


def CreateGate(gateName, parts, material):
    originX = (parts[0]['transform']['translation'][0] + parts[1]['transform']['translation'][0]) * 0.5
    progress = parts[0]['transform']['translation'][1]
    depth = parts[0]['collider']['size'][1]
    rectangles = GetRectangles(parts, originX)
    xValues = sorted(set(value for rectangle in rectangles for value in rectangle[:2]))
    heights = sorted(set(value for rectangle in rectangles for value in rectangle[2:]))
    cells = set()
    for column in range(len(xValues) - 1):
        for row in range(len(heights) - 1):
            pointX = (xValues[column] + xValues[column + 1]) * 0.5
            height = (heights[row] + heights[row + 1]) * 0.5
            for left, right, bottom, top in rectangles:
                if left < pointX < right and bottom < height < top:
                    cells.add((column, row))
                    break
    vertices = []
    vertexIndices = {}
    for side, forward in enumerate([-depth * 0.5, depth * 0.5]):
        for column, positionX in enumerate(xValues):
            for row, height in enumerate(heights):
                vertexIndices[(side, column, row)] = len(vertices)
                # Engine importer mirrors OBJ X; OBJ export maps Blender Y to -Z.
                vertices.append((-positionX, -forward, height))
    faces = []
    for column, row in sorted(cells):
        front = [vertexIndices[(0, column, row)], vertexIndices[(0, column + 1, row)],
            vertexIndices[(0, column + 1, row + 1)], vertexIndices[(0, column, row + 1)]]
        back = [vertexIndices[(1, column, row)], vertexIndices[(1, column + 1, row)],
            vertexIndices[(1, column + 1, row + 1)], vertexIndices[(1, column, row + 1)]]
        faces.extend([front, list(reversed(back))])
        for edgeIndex, neighbor in enumerate([(column, row - 1), (column + 1, row),
                (column, row + 1), (column - 1, row)]):
            if neighbor not in cells:
                nextIndex = (edgeIndex + 1) % 4
                faces.append([front[edgeIndex], back[edgeIndex], back[nextIndex], front[nextIndex]])
    mesh = bpy.data.meshes.new(gateName + 'Mesh')
    mesh.from_pydata(vertices, [], faces)
    mesh.update()
    meshData = bmesh.new()
    meshData.from_mesh(mesh)
    # Remove unused grid points before checking the closed boundary.
    unusedVertices = [vertex for vertex in meshData.verts if not vertex.link_faces]
    bmesh.ops.delete(meshData, geom=unusedVertices, context='VERTS')
    bmesh.ops.recalc_face_normals(meshData, faces=list(meshData.faces))
    bmesh.ops.triangulate(meshData, faces=list(meshData.faces))
    if any(not edge.is_manifold for edge in meshData.edges):
        raise RuntimeError('Gate boundary is not closed: ' + gateName)
    faceKeys = set()
    for face in meshData.faces:
        faceKey = tuple(sorted(tuple(round(value, 6) for value in vertex.co) for vertex in face.verts))
        if faceKey in faceKeys:
            raise RuntimeError('Gate has duplicate faces: ' + gateName)
        faceKeys.add(faceKey)
    meshData.to_mesh(mesh)
    meshData.free()
    model = bpy.data.objects.new(gateName + '_StoneGate', mesh)
    bpy.context.collection.objects.link(model)
    mesh.materials.append(material)
    mesh.uv_layers.new(name='UVMap')
    CreateSolidRuinsWall.SetMasonryUvs(model, material, 2.50, 1.25)
    fileName = 'floating_gate_' + gateName.lower() + '.obj'
    bpy.ops.object.select_all(action='DESELECT')
    model.select_set(True)
    bpy.context.view_layer.objects.active = model
    bpy.ops.wm.obj_export(filepath=str(kAssetDirectory / fileName), export_selected_objects=True,
        export_materials=True, export_triangulated_mesh=True, forward_axis='NEGATIVE_Z', up_axis='Y', path_mode='RELATIVE')
    boxes = []
    for part in parts:
        position = part['transform']['translation']
        center = part['collider']['center']
        boxes.append({'type': 'BOX', 'center': [position[0] + center[0] - originX,
            position[1] + center[1] - progress, position[2] + center[2]], 'size': part['collider']['size']})
    entry = {'name': gateName + '_StoneGate', 'type': 'MESH',
        'file_name': 'Environment/Ocean/FloatingRuins/' + fileName,
        'transform': {'translation': [originX, progress, 0.0], 'rotation': [0.0, 0.0, 0.0], 'scale': [1.0, 1.0, 1.0]},
        'colliders': boxes}
    return model, entry, len(mesh.polygons)


def InstallGates(entries, triangleCounts):
    sceneText = kScenePath.read_text(encoding='utf8')
    originalData = json.loads(sceneText)
    touchedNames = {'FinalGate_Bottom_FloatingRuinsGround', 'SecondActGate_Bottom_FloatingRuinsGround'}
    for gateName, partNames in kGateParts.items():
        touchedNames.update(partNames)
        touchedNames.add(gateName + '_StoneGate')
    lines = sceneText.splitlines()
    outputLines = []
    installed = set()
    for line in lines:
        isTouched = False
        for name in touchedNames:
            if '"' + name + '"' in line:
                isTouched = True
                for gateName, partNames in kGateParts.items():
                    if name == partNames[0] or name == gateName + '_StoneGate':
                        if gateName not in installed:
                            outputLines.append('    ' + json.dumps(entries[gateName]) + ',')
                            installed.add(gateName)
                break
        if not isTouched:
            outputLines.append(line)
    if len(installed) != 4:
        raise RuntimeError('Expected four gates in Stage01.')
    sceneText = '\n'.join(outputLines) + '\n'
    updatedData = json.loads(sceneText)
    originalOtherObjects = [entry for entry in originalData['scene'] if entry['name'] not in touchedNames]
    updatedOtherObjects = [entry for entry in updatedData['scene'] if entry['name'] not in touchedNames]
    if originalOtherObjects != updatedOtherObjects:
        raise RuntimeError('Unrelated stage objects changed.')
    kScenePath.write_text(sceneText, encoding='utf8')
    report = {'gates': entries, 'triangles': triangleCounts, 'closedManifold': True,
        'duplicateFaces': 0, 'tileSizeWorld': [2.50, 1.25], 'collidersPerGate': 3}
    (kAssetDirectory / 'gate_asset_info.json').write_text(json.dumps(report, indent=2), encoding='utf8')
    print('GATE_RESULT ' + json.dumps({'triangles': triangleCounts, 'gates': len(entries),
        'duplicateFaces': 0, 'collidersPerGate': 3}), flush=True)


def Main():
    bpy.ops.wm.open_mainfile(filepath=str(kAssetDirectory / 'floating_earth_wall.blend'))
    ground = bpy.data.objects.get('FloatingEarthWall')
    material = bpy.data.materials.get('RuinsMasonry')
    meshData = bmesh.new()
    meshData.from_mesh(ground.data)
    removeFaces = [face for face in meshData.faces if ground.data.materials[face.material_index] == material]
    bmesh.ops.delete(meshData, geom=removeFaces, context='FACES')
    meshData.to_mesh(ground.data)
    meshData.free()
    ground.hide_render = True
    shader = material.node_tree.nodes.get('Principled BSDF')
    colorLink = shader.inputs['Base Color'].links[0]
    previewColorOutput = colorLink.from_socket
    material.node_tree.links.remove(colorLink)
    for node in material.node_tree.nodes:
        if node.type == 'TEX_IMAGE':
            material.node_tree.links.new(node.outputs['Color'], shader.inputs['Base Color'])
            break
    source = json.loads((kAssetDirectory / 'stage01_wall_replacements.json').read_text(encoding='utf8'))
    originalByName = {entry['name']: entry for entry in source['originalPanels']}
    entries = {}
    triangleCounts = {}
    models = {}
    for gateName, partNames in kGateParts.items():
        parts = [originalByName[name] for name in partNames]
        model, entry, triangleCount = CreateGate(gateName, parts, material)
        models[gateName] = model
        entries[gateName] = entry
        triangleCounts[gateName] = triangleCount
        model.hide_render = gateName != 'Gate02'
    material.node_tree.links.new(previewColorOutput, shader.inputs['Base Color'])
    stage = json.loads(kScenePath.read_text(encoding='utf8'))
    for side in ['Left', 'Right']:
        groundEntry = next(entry for entry in stage['scene'] if entry['name'] == 'Gate02_' + side + '_FloatingRuinsGround')
        previewGround = ground.copy()
        previewGround.data = ground.data.copy()
        bpy.context.collection.objects.link(previewGround)
        previewGround.hide_render = False
        previewGround.name = 'PreviewGround_' + side
        position = groundEntry['transform']['translation']
        scale = groundEntry['transform']['scale']
        previewGround.location = (-position[0], 0, position[2])
        previewGround.scale = (scale[0], scale[1], scale[2])
    scene = bpy.context.scene
    scene.render.resolution_x = 1400
    scene.render.resolution_y = 1000
    scene.cycles.samples = 24
    scene.render.filepath = str(kAssetDirectory / 'previews/floating_gate.png')
    scene.camera.location = (45, -75, 40)
    scene.camera.rotation_euler = (Vector((0, 0, -5)) - scene.camera.location).to_track_quat('-Z', 'Y').to_euler()
    scene.camera.data.ortho_scale = 80
    bpy.data.objects['PreviewOcean_NotExported'].location.z = -30
    for sceneObject in bpy.data.objects:
        if sceneObject.type == 'LIGHT':
            sceneObject.location *= 4
            sceneObject.data.energy *= 16
            sceneObject.data.size *= 4
    bpy.ops.wm.save_as_mainfile(filepath=str(kAssetDirectory / 'floating_gates.blend'))
    bpy.ops.render.render(write_still=True)
    InstallGates(entries, triangleCounts)


Main()
