"""Create the anemone turret concept model, animated GLB, OBJ poses and previews.

Run with Blender --background --python <this file>. Does not modify game scenes.
Blender coordinates: Z up, -Y front. Dimensions are model units, not stage scale.
"""
import bpy
import math
import random
import json
from pathlib import Path
from mathutils import Vector

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / 'resources/Models/Enemy/Anemone'
OUT.mkdir(parents=True, exist_ok=True)
(OUT / 'textures').mkdir(exist_ok=True)
(OUT / 'previews').mkdir(exist_ok=True)
random.seed(84)
bpy.ops.wm.read_factory_settings(use_empty=True)
scene = bpy.context.scene
assets = []
tentacles = []


def material(name, color, emission=0.0, roughness=0.6):
    mat = bpy.data.materials.new(name)
    mat.use_nodes = True
    shader = next(node for node in mat.node_tree.nodes if node.type == 'BSDF_PRINCIPLED')
    shader.inputs['Base Color'].default_value = (*color, 1)
    shader.inputs['Roughness'].default_value = roughness
    shader.inputs['Emission Color'].default_value = (*color, 1)
    shader.inputs['Emission Strength'].default_value = emission
    mat.diffuse_color = (*color, 1)
    return mat


stone = material('WeatheredSandstone', (0.35, 0.39, 0.35))
stone_dark = material('StoneRecess', (0.12, 0.21, 0.22))
moss = material('TealPatina', (0.06, 0.27, 0.23))
purple = material('AnemoneViolet', (0.12, 0.018, 0.12), roughness=0.4)
pink = material('SoftMauve', (0.24, 0.045, 0.18), roughness=0.42)
teal = material('TentacleTeal', (0.012, 0.19, 0.18), 0.08, 0.35)
cyan = material('CyanTips', (0.015, 0.55, 0.48), 0.6, 0.3)
orange = material('InnerMouth', (0.95, 0.12, 0.008), 1.0, 0.4)
core_mat = material('CoreGlow', (1.0, 0.28, 0.02), 2.6, 0.3)
materials = [stone, stone_dark, moss, purple, pink, teal, cyan, orange, core_mat]


def finish(obj, name, mat, smooth=True):
    obj.name = name
    obj.data.materials.append(mat)
    if smooth:
        for poly in obj.data.polygons:
            poly.use_smooth = True
    assets.append(obj)
    return obj


def mesh(name, vertices, faces, mat):
    data = bpy.data.meshes.new(name + 'Mesh')
    data.from_pydata(vertices, [], faces)
    data.update()
    return finish(bpy.data.objects.new(name, data), name, mat)


def link(obj):
    scene.collection.objects.link(obj)
    return obj


def lathe(name, rings, sides, mat, flute=0.0):
    vertices = []
    for radius, z in rings:
        for index in range(sides):
            angle = index * math.tau / sides
            radius_here = radius * (1 + flute * math.cos(angle * 12))
            vertices.append((radius_here * math.cos(angle), radius_here * math.sin(angle), z))
    faces = []
    for row in range(len(rings) - 1):
        for index in range(sides):
            a = row * sides + index
            b = row * sides + (index + 1) % sides
            faces.append((a, b, b + sides, a + sides))
    faces.append(tuple(reversed(range(sides))))
    faces.append(tuple((len(rings) - 1) * sides + index for index in range(sides)))
    return link(mesh(name, vertices, faces, mat))


def block(name, location, scale, mat, rotation=0):
    bpy.ops.mesh.primitive_cube_add(size=1, location=location)
    obj = bpy.context.object
    obj.scale = scale
    obj.rotation_euler.z = rotation
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    bevel = obj.modifiers.new('Chipped stone edges', 'BEVEL')
    bevel.width = 0.055
    bevel.segments = 1
    bpy.ops.object.modifier_apply(modifier=bevel.name)
    return finish(obj, name, mat, False)


def ball(name, location, scale, mat, parent=None):
    bpy.ops.mesh.primitive_uv_sphere_add(segments=16, ring_count=8, location=location)
    obj = bpy.context.object
    obj.scale = scale
    finish(obj, name, mat)
    obj.parent = parent
    return obj


# Layered, fluted stone plinth with radial masonry and patinated recesses.
lathe('StoneFoot', [(1.7, 0.03), (1.78, 0.14), (1.72, 0.32), (1.52, 0.44)], 12, stone)
lathe('PedestalRecess', [(1.40, 0.40), (1.35, 0.65), (0.88, 1.45), (0.93, 1.95), (1.28, 2.17)], 48, stone_dark, 0.07)
for i in range(12):
    angle = math.tau * i / 12
    # Individual flutes curve inward toward the organic crown.
    rings = [(1.46, 0.45), (1.30, 0.75), (1.02, 1.25), (0.94, 1.65), (1.16, 2.12)]
    verts = []
    for radius, z in rings:
        for offset in (-0.16, 0.16):
            a = angle + offset
            verts.append((radius * math.cos(a), radius * math.sin(a), z))
    faces = [(j * 2, j * 2 + 1, j * 2 + 3, j * 2 + 2) for j in range(len(rings) - 1)]
    flute_obj = link(mesh('StoneFlute_%02d' % i, verts, faces, stone if i % 4 else moss))
    solid = flute_obj.modifiers.new('Stone thickness', 'SOLIDIFY')
    solid.thickness = 0.10
    bpy.context.view_layer.objects.active = flute_obj
    bpy.ops.object.modifier_apply(modifier=solid.name)
    angle_mid = angle + math.pi / 12
    block('FootMasonry_%02d' % i, (1.43 * math.cos(angle_mid), 1.43 * math.sin(angle_mid), 0.38),
          (0.65, 0.42, 0.40), stone if i % 3 else stone_dark, angle_mid + math.pi / 2)
lathe('UpperStoneLip', [(1.27, 2.00), (1.36, 2.12), (1.32, 2.29), (1.10, 2.34)], 24, stone)

# Ribbed living mantle emerges from the stone.
lathe('OrganicMantle', [(0.79, 1.70), (1.00, 2.0), (1.16, 2.35), (1.22, 2.7),
      (1.16, 3.03), (1.0, 3.25), (0.85, 3.38)], 72, purple, 0.055)
crown = bpy.data.objects.new('CrownRoot', None)
scene.collection.objects.link(crown)
crown.location = (0, -0.10, 3.65)
crown.rotation_euler.x = math.radians(38)
assets.append(crown)

funnel = lathe('MouthFunnel', [(0.18, -0.42), (0.43, -0.27), (0.72, -0.12), (0.94, 0.10)], 48, orange, 0.025)
funnel.parent = crown
ball('ExposedCore', (0, 0, -0.18), (0.52, 0.52, 0.26), core_mat, crown)
for i in range(12):
    angle = math.tau * i / 12
    lip = ball('MouthLobe_%02d' % i, (0.91 * math.cos(angle), 0.91 * math.sin(angle), 0.08),
               (0.31, 0.25, 0.22), pink, crown)
    lip.rotation_euler.z = angle


def bezier(points, t):
    return sum((Vector(p) * weight for p, weight in zip(points,
               ((1-t)**3, 3*(1-t)**2*t, 3*(1-t)*t*t, t**3))), Vector())


def tentacle_vertices(index, closed):
    angle = math.tau * index / 12
    variation = 1 + 0.07 * math.sin(index * 2.1)
    if closed:
        points = [(1.07, 0, 0.0), (1.60, 0, 0.65), (1.12, 0, 2.32), (0.08, 0, 2.43)]
    else:
        points = [(1.07, 0, 0.0), (1.95, 0, -0.12), (3.75 * variation, 0, 0.05), (3.20 * variation, 0, 1.47)]
    verts = []
    for row in range(25):
        t = row / 24
        p = bezier(points, t)
        tangent = (bezier(points, min(1, t+0.001)) - bezier(points, max(0, t-0.001))).normalized()
        side = Vector((0, 1, 0))
        normal = tangent.cross(side).normalized()
        # Fleshy ridges are actual geometry, not a render-only texture.
        radius = (0.41 * (1-t) + 0.23 * t) * (1 + 0.045 * math.sin(t * math.tau * 10))
        if t > 0.88:
            radius *= max(0.05, math.sqrt(max(0, 1-((t-0.88)/0.125)**2)))
        for corner in range(12):
            phase = math.tau * corner / 12
            q = p + radius * (normal * math.cos(phase) + side * math.sin(phase))
            verts.append((q.x * math.cos(angle) - q.y * math.sin(angle),
                          q.x * math.sin(angle) + q.y * math.cos(angle), q.z))
    return verts


for index in range(12):
    verts = tentacle_vertices(index, False)
    faces = []
    for row in range(24):
        for corner in range(12):
            a = row * 12 + corner
            b = row * 12 + (corner + 1) % 12
            faces.append((a, b, b + 12, a + 12))
    faces.extend([tuple(reversed(range(12))), tuple(24 * 12 + j for j in range(12))])
    obj = link(mesh('Tentacle_%02d' % (index + 1), verts, faces, purple))
    obj.parent = crown
    for mat in (pink, teal, cyan):
        obj.data.materials.append(mat)
    for polygon in obj.data.polygons:
        row = min(23, polygon.index // 12)
        polygon.material_index = 3 if row >= 20 else 2 if row >= 17 else 1 if row >= 11 else 0
    obj.shape_key_add(name='Open')
    key = obj.shape_key_add(name='Closed')
    for vertex, coordinate in zip(key.data, tentacle_vertices(index, True)):
        vertex.co = coordinate
    for frame, value in ((1, 0), (30, 0), (60, 1), (90, 1), (120, 0), (150, 0)):
        key.value = value
        key.keyframe_insert(data_path='value', frame=frame)
    tentacles.append(obj)

# Tiny geometric patina patches and foot chips keep the stone from feeling pristine.
for i in range(14):
    angle = random.uniform(0, math.tau)
    r = random.uniform(1.3, 1.65)
    bpy.ops.mesh.primitive_ico_sphere_add(subdivisions=1, radius=1,
                                        location=(r*math.cos(angle), r*math.sin(angle), random.uniform(0.22, 0.50)))
    obj = bpy.context.object
    obj.scale = (random.uniform(0.10, 0.22), random.uniform(0.10, 0.22), 0.065)
    finish(obj, 'Patina_%02d' % i, moss)

scene.frame_start = 1
scene.frame_end = 150
scene.render.fps = 30
scene.frame_set(1)
bpy.context.view_layer.update()

# A shared color atlas makes the static OBJ robust to single-material importers.
atlas = bpy.data.images.new('AnemonePalette', width=128, height=128)
pixels = []
for y in range(128):
    for x in range(128):
        tile_x, tile_y = x // 32, y // 32
        mat = materials[min(tile_y * 4 + tile_x, len(materials)-1)]
        color = list(mat.diffuse_color)
        noise = 1 + random.uniform(-0.045, 0.045)
        pixels.extend([min(1, c*noise) for c in color[:3]] + [1])
atlas.pixels = pixels
atlas.filepath_raw = str(OUT / 'textures/AnemonePalette.png')
atlas.save()
atlas.pack()
atlas_mat = bpy.data.materials.new('AnemonePalette')
atlas_mat.use_nodes = True
shader = next(n for n in atlas_mat.node_tree.nodes if n.type == 'BSDF_PRINCIPLED')
texture = atlas_mat.node_tree.nodes.new('ShaderNodeTexImage')
texture.image = atlas
atlas_mat.node_tree.links.new(texture.outputs['Color'], shader.inputs['Base Color'])
shader.inputs['Roughness'].default_value = 0.55

def export_obj(path):
    clones = []
    bpy.ops.object.select_all(action='DESELECT')
    depsgraph = bpy.context.evaluated_depsgraph_get()
    for original in assets:
        if original.type != 'MESH':
            continue
        data = bpy.data.meshes.new_from_object(original.evaluated_get(depsgraph), depsgraph=depsgraph)
        copy = bpy.data.objects.new(original.name + '_Export', data)
        scene.collection.objects.link(copy)
        copy.matrix_world = original.matrix_world.copy()
        slots = list(data.materials)
        uv = data.uv_layers.new(name='Palette')
        for polygon in data.polygons:
            mat = slots[polygon.material_index]
            material_index = materials.index(mat)
            u = (material_index % 4 + 0.5) / 4
            v = (material_index // 4 + 0.5) / 4
            for loop in polygon.loop_indices:
                uv.data[loop].uv = (u, v)
            polygon.material_index = 0
        data.materials.clear()
        data.materials.append(atlas_mat)
        copy.select_set(True)
        clones.append(copy)
    bpy.ops.wm.obj_export(filepath=str(path), export_selected_objects=True,
                          forward_axis='NEGATIVE_Z', up_axis='Y', export_triangulated_mesh=True,
                          path_mode='RELATIVE')
    # Packed Blender images can lose their external base path in OBJ export.
    mtl_path = path.with_suffix('.mtl')
    mtl_lines = mtl_path.read_text(encoding='utf-8').splitlines()
    mtl_path.write_text('\n'.join('map_Kd textures/AnemonePalette.png' if line.startswith('map_Kd ') else line
                                  for line in mtl_lines) + '\n', encoding='utf-8')
    for copy in clones:
        data = copy.data
        bpy.data.objects.remove(copy, do_unlink=True)
        bpy.data.meshes.remove(data)


export_obj(OUT / 'AnemoneOpen.obj')
scene.frame_set(75)
bpy.context.view_layer.update()
export_obj(OUT / 'AnemoneClosed.obj')
scene.frame_set(1)
bpy.ops.object.select_all(action='DESELECT')
for obj in assets:
    obj.select_set(True)
bpy.ops.export_scene.gltf(filepath=str(OUT / 'AnemoneEnemy.glb'), use_selection=True,
                          export_animations=True, export_frame_range=True)

# Dedicated preview setup, excluded from the model exports.
scene.render.engine = 'CYCLES'
scene.cycles.samples = 32
scene.cycles.use_denoising = True
scene.render.resolution_x = 1100
scene.render.resolution_y = 1000
scene.render.resolution_percentage = 100
scene.render.image_settings.file_format = 'PNG'
scene.render.film_transparent = False
scene.view_settings.view_transform = 'AgX'
world = bpy.data.worlds.new('UnderwaterStudio')
world.use_nodes = True
background = next(n for n in world.node_tree.nodes if n.type == 'BACKGROUND')
background.inputs['Color'].default_value = (0.025, 0.065, 0.09, 1)
background.inputs['Strength'].default_value = 0.40
scene.world = world
floor_mat = material('PreviewFloor', (0.025, 0.065, 0.075), roughness=0.85)
bpy.ops.mesh.primitive_plane_add(size=200, location=(0, 0, -0.03))
floor = bpy.context.object
floor.name = 'PREVIEW_Floor'
floor.data.materials.append(floor_mat)

def aim(obj, point):
    obj.rotation_euler = (Vector(point) - obj.location).to_track_quat('-Z', 'Y').to_euler()


for name, location, power, color, size in (
    ('Key', (2, -6, 10), 1000, (0.63, 0.85, 1.0), 6),
    ('Fill', (-6, -3, 5), 500, (0.48, 0.65, 1.0), 5),
    ('Rim', (3, 5, 8), 1300, (0.25, 1.0, 0.85), 4)):
    data = bpy.data.lights.new(name, 'AREA')
    data.energy, data.color, data.shape, data.size = power, color, 'DISK', size
    obj = bpy.data.objects.new('PREVIEW_' + name, data)
    scene.collection.objects.link(obj)
    obj.location = location
    aim(obj, (0, 0, 2.7))
bpy.ops.object.camera_add(location=(8.2, -13.8, 9.0))
camera = bpy.context.object
camera.name = 'PREVIEW_Camera'
camera.data.type = 'ORTHO'
camera.data.ortho_scale = 9.8
aim(camera, (0, 0, 2.9))
scene.camera = camera

for frame, name in ((1, 'AnemoneOpen'), (75, 'AnemoneClosed')):
    scene.frame_set(frame)
    scene.render.filepath = str(OUT / 'previews' / (name + '.png'))
    bpy.ops.render.render(write_still=True)

# Save a clean, immediately useful editable scene in the open pose.
scene.frame_set(1)
bpy.ops.object.select_all(action='DESELECT')
for obj in assets:
    obj.select_set(True)
bpy.context.view_layer.objects.active = tentacles[0]
for screen in bpy.data.screens:
    for area in screen.areas:
        if area.type == 'VIEW_3D':
            area.spaces.active.region_3d.view_perspective = 'CAMERA'
            area.spaces.active.shading.type = 'MATERIAL'
bpy.ops.wm.save_as_mainfile(filepath=str(OUT / 'AnemoneEnemy.blend'))
triangles = 0
for obj in assets:
    if obj.type == 'MESH':
        obj.data.calc_loop_triangles()
        triangles += len(obj.data.loop_triangles)
report = {'meshObjects': sum(o.type == 'MESH' for o in assets), 'triangles': triangles,
          'tentacles': 12, 'openFrame': 1, 'closedFrame': 75, 'animationFrames': 150,
          'fps': 30, 'frontBlender': '-Y', 'upBlender': '+Z',
          'integration': 'Stage01 lower route: 3 stationary open-pose enemies fire five-shot fans; no open/close behavior yet.'}
(OUT / 'asset_info.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
print('ANEMONE_COMPLETE', json.dumps(report), flush=True)
