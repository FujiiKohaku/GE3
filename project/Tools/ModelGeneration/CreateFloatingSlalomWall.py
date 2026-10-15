"""Generate a solid ruined wall and install it at the first left slalom obstacle."""

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
kTranslation = [-12.0, 760.0, -12.0]
kWallScale = [1.55, 2.0, 3.0]
kGroundScale = [1.55, 2.0, 3.0]
kRoofSegments = [(-5.65, -4.40, 6.70), (-4.40, -2.90, 7.50),
    (-2.90, 0.30, 8.00), (0.30, 1.25, 7.35), (1.25, 3.70, 7.75),
    (3.70, 5.65, 6.45)]


def CreateBox(minimumX, maximumX, minimumHeight, maximumHeight, scale, minimumDepth, maximumDepth):
    return {'type': 'BOX',
        'center': [round(-(minimumX + maximumX) * 0.5 * scale[0], 5),
            round(-(minimumDepth + maximumDepth) * 0.5 * scale[1], 5),
            round((minimumHeight + maximumHeight) * 0.5 * scale[2], 5)],
        'size': [round((maximumX - minimumX) * scale[0], 5),
            round((maximumDepth - minimumDepth) * scale[1], 5),
            round((maximumHeight - minimumHeight) * scale[2], 5)]}


def InstallWall(triangleCount):
    boxes = []
    for minimumX, maximumX, roofHeight in kRoofSegments:
        boxes.append(CreateBox(minimumX, maximumX, 0.54, roofHeight, kWallScale, -1.0, 1.0))
    wallEntry = {'name': 'Slalom01_LeftWall', 'type': 'MESH',
        'file_name': 'Environment/Ocean/FloatingRuins/floating_slalom_wall_masonry.obj',
        'transform': {'translation': kTranslation, 'rotation': [0.0, 0.0, 0.0], 'scale': kWallScale},
        'colliders': boxes}
    groundEntry = {'name': 'Slalom01_FloatingRuinsGround', 'type': 'MESH',
        'file_name': 'Environment/Ocean/FloatingRuins/floating_earth_wall_ground.obj',
        'transform': {'translation': kTranslation, 'rotation': [0.0, 0.0, 0.0], 'scale': kGroundScale},
        'collider': CreateBox(-6.35, 6.35, -4.1, 0.90, kGroundScale, -2.65, 2.65)}
    # Preserve the original obstacle's right-hand passage, whose left boundary was X=-2.
    for box in boxes + [groundEntry['collider']]:
        rightEdge = kTranslation[0] + box['center'][0] + box['size'][0] * 0.5
        if rightEdge > -2.0:
            raise RuntimeError('The slalom wall obstructs the original right-hand passage.')
    sceneText = kScenePath.read_text(encoding='utf8')
    sceneData = json.loads(sceneText)
    replacedNames = {'Slalom01_LeftWall', 'Slalom01_FloatingRuinsGround'}
    originalOtherObjects = [entry for entry in sceneData['scene'] if entry['name'] not in replacedNames]
    oldLines = []
    for line in sceneText.splitlines():
        if any('"' + name + '"' in line for name in replacedNames):
            oldLines.append(line)
    if not oldLines:
        raise RuntimeError('The first slalom wall is missing.')
    replacement = '\n'.join('    ' + json.dumps(entry) + ',' for entry in [wallEntry, groundEntry])
    sceneText = sceneText.replace(oldLines[0], replacement, 1)
    for line in oldLines[1:]:
        sceneText = sceneText.replace(line + '\n', '', 1)
    updatedData = json.loads(sceneText)
    updatedOtherObjects = [entry for entry in updatedData['scene'] if entry['name'] not in replacedNames]
    if originalOtherObjects != updatedOtherObjects:
        raise RuntimeError('Unrelated stage objects changed.')
    kScenePath.write_text(sceneText, encoding='utf8')
    report = {'stage': 'stage01', 'progress': 760, 'masonryTriangles': triangleCount,
        'wallColliders': len(boxes), 'sharedGroundModel': groundEntry['file_name'],
        'parts': [wallEntry, groundEntry], 'rightPassageValidated': True}
    (kAssetDirectory / 'slalom_asset_info.json').write_text(json.dumps(report, indent=2), encoding='utf8')
    print('SLALOM_WALL_RESULT ' + json.dumps(report), flush=True)


def Main():
    bpy.ops.wm.open_mainfile(filepath=str(kAssetDirectory / 'floating_earth_wall.blend'))
    ground = bpy.data.objects.get('FloatingEarthWall')
    if ground is None:
        raise RuntimeError('Generate the first floating ruins model before this variant.')
    material = bpy.data.materials.get('RuinsMasonry')
    meshData = bmesh.new()
    meshData.from_mesh(ground.data)
    removeFaces = []
    for face in meshData.faces:
        if ground.data.materials[face.material_index] == material:
            removeFaces.append(face)
    bmesh.ops.delete(meshData, geom=removeFaces, context='FACES')
    meshData.to_mesh(ground.data)
    meshData.free()
    ground.name = 'SlalomPreviewGround'
    outline = [(-5.65, 0.54), (5.65, 0.54)]
    for minimumX, maximumX, roofHeight in reversed(kRoofSegments):
        outline.extend([(maximumX, roofHeight), (minimumX, roofHeight)])
    wall = CreateSolidRuinsWall.CreateExtrudedMasonry(material, outline, [-1.0, 1.0])
    wall.name = 'FloatingSlalomWall'
    wall.data.uv_layers.new(name='UVMap')
    CreateSolidRuinsWall.SetMasonryUvs(wall, material, 1.60, 0.45)
    bpy.ops.object.select_all(action='DESELECT')
    wall.select_set(True)
    bpy.context.view_layer.objects.active = wall
    # Export the original image material; the game provides the procedural PS.
    shader = material.node_tree.nodes.get('Principled BSDF')
    colorLink = shader.inputs['Base Color'].links[0]
    previewColorOutput = colorLink.from_socket
    material.node_tree.links.remove(colorLink)
    for node in material.node_tree.nodes:
        if node.type == 'TEX_IMAGE':
            material.node_tree.links.new(node.outputs['Color'], shader.inputs['Base Color'])
            break
    bpy.ops.wm.obj_export(filepath=str(kAssetDirectory / 'floating_slalom_wall_masonry.obj'),
        export_selected_objects=True, export_materials=True, export_triangulated_mesh=True,
        forward_axis='NEGATIVE_Z', up_axis='Y', path_mode='RELATIVE')
    material.node_tree.links.new(previewColorOutput, shader.inputs['Base Color'])
    wall.data.calc_loop_triangles()
    triangleCount = len(wall.data.loop_triangles)
    wall.scale = (1.0, kWallScale[1] / kWallScale[0], kWallScale[2] / kWallScale[0])
    ground.scale = (1.0, kGroundScale[1] / kGroundScale[0], kGroundScale[2] / kGroundScale[0])
    ocean = bpy.data.objects.get('PreviewOcean_NotExported')
    ocean.location.z = -11.7
    camera = bpy.context.scene.camera
    camera.location = (15, -25, 18)
    camera.rotation_euler = (Vector((0, 0, 3.8)) - camera.location).to_track_quat('-Z', 'Y').to_euler()
    camera.data.ortho_scale = 30.0
    scene = bpy.context.scene
    scene.render.resolution_x = 1200
    scene.render.resolution_y = 1000
    scene.cycles.samples = 24
    scene.render.filepath = str(kAssetDirectory / 'previews/floating_slalom_wall.png')
    bpy.ops.wm.save_as_mainfile(filepath=str(kAssetDirectory / 'floating_slalom_wall.blend'))
    bpy.ops.render.render(write_still=True)
    InstallWall(triangleCount)


Main()
