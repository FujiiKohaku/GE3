"""Export the approved FearWorm body segment as a single textured game OBJ.

Run Blender in background with --python and pass the source .blend after --.
The model is centered at the origin and its length runs along Blender's Y axis,
which the OBJ exporter maps to the game's Z axis.
"""
import bpy
import math
import sys
from pathlib import Path
from mathutils import Matrix

root = Path(__file__).resolve().parents[2]
out = root / 'resources/Models/Boss/FearWorm/Body'
(out / 'textures').mkdir(parents=True, exist_ok=True)
source = Path(sys.argv[sys.argv.index('--') + 1])
bpy.ops.wm.open_mainfile(filepath=str(source))
collection = bpy.data.collections['FearWorm_BodySegment']
objects = [o for o in collection.objects if o.type == 'MESH']
colors = {
    'Ivory ceramic': (0.87, 0.81, 0.68, 1),
    'Warm gold': (0.74, 0.49, 0.16, 1),
    'Joint graphite': (0.055, 0.13, 0.14, 1),
    'Cyan core': (0.06, 0.91, 0.96, 1),
    'Plate inset': (0.97, 0.91, 0.79, 1),
}
names = list(colors)
image = bpy.data.images.new('FearWormBodyPalette', width=80, height=16)
image.pixels = [channel for y in range(16) for x in range(80) for channel in colors[names[x // 16]]]
image.filepath_raw = str(out / 'textures/FearWormBodyPalette.png')
image.file_format = 'PNG'
image.save()
material = bpy.data.materials.new('FearWormBodyPalette')
material.use_nodes = True
shader = next(node for node in material.node_tree.nodes if node.type == 'BSDF_PRINCIPLED')
texture = material.node_tree.nodes.new('ShaderNodeTexImage')
texture.image = image
material.node_tree.links.new(texture.outputs['Color'], shader.inputs['Base Color'])

bpy.ops.object.select_all(action='DESELECT')
clones = []
for source_obj in objects:
    depsgraph = bpy.context.evaluated_depsgraph_get()
    evaluated = source_obj.evaluated_get(depsgraph)
    mesh = bpy.data.meshes.new_from_object(evaluated)
    mesh.transform(Matrix.Rotation(math.pi / 2, 4, 'Z') @ source_obj.matrix_world)
    original_materials = list(mesh.materials)
    uv = mesh.uv_layers.active or mesh.uv_layers.new(name='Palette')
    for polygon in mesh.polygons:
        name = original_materials[polygon.material_index].name
        u = (names.index(name) + 0.5) / len(names)
        for loop in polygon.loop_indices:
            uv.data[loop].uv = (u, 0.5)
        polygon.material_index = 0
    mesh.materials.clear()
    mesh.materials.append(material)
    clone = bpy.data.objects.new(source_obj.name + '_Game', mesh)
    bpy.context.scene.collection.objects.link(clone)
    clone.select_set(True)
    clones.append(clone)
bpy.context.view_layer.objects.active = clones[0]
bpy.ops.object.join()
bpy.context.object.name = 'FearWormBody'
path = out / 'FearWormBody.obj'
bpy.ops.wm.obj_export(filepath=str(path), export_selected_objects=True,
                      forward_axis='NEGATIVE_Z', up_axis='Y', export_triangulated_mesh=True,
                      path_mode='RELATIVE')
mtl = path.with_suffix('.mtl')
lines = mtl.read_text(encoding='utf-8').splitlines()
mtl.write_text('\n'.join('map_Kd textures/FearWormBodyPalette.png' if line.startswith('map_Kd ') else line for line in lines) + '\n', encoding='utf-8')
print('EXPORTED', path, 'vertices', len(bpy.context.object.data.vertices), 'faces', len(bpy.context.object.data.polygons))
