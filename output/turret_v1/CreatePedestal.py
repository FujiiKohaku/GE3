import bpy
import math
import shutil
from pathlib import Path

kOutputDirectory = Path(__file__).resolve().parent
kModelDirectory = kOutputDirectory.parents[1] / 'project/resources/Models/Environment/Ocean/TurretPedestal'
kModelDirectory.mkdir(parents=True, exist_ok=True)
kStoneDirectory = kModelDirectory.parent / 'FloatingRuins/textures'


def CreateMaterial(name, color):
    material = bpy.data.materials.new(name)
    material.diffuse_color = (*color, 1.0)
    return material


def CreateOctagon(name, radius, bottom, top, material, bevel=0.0):
    bpy.ops.mesh.primitive_cylinder_add(vertices=8, radius=radius, depth=top-bottom,
                                      location=(0.0, 0.0, (top+bottom)*0.5), rotation=(0.0, 0.0, math.pi/8.0))
    obj = bpy.context.object
    obj.name = name
    obj.data.materials.append(material)
    if bevel > 0.0:
        modifier = obj.modifiers.new('StoneEdge', 'BEVEL')
        modifier.width = bevel
        modifier.segments = 1
        bpy.ops.object.modifier_apply(modifier=modifier.name)
    return obj


bpy.ops.object.select_all(action='SELECT')
bpy.ops.object.delete(use_global=False)
stone = CreateMaterial('PedestalStone', (0.57, 0.49, 0.36))
lightStone = CreateMaterial('PedestalLight', (0.67, 0.59, 0.44))
joint = CreateMaterial('PedestalJoint', (0.35, 0.30, 0.23))
parts = [CreateOctagon('StoneShaft', 4.7, -22.0, -1.5, stone),
         CreateOctagon('UpperCollar', 5.35, -1.8, -0.8, stone, 0.12),
         CreateOctagon('TopSlab', 6.4, -0.9, 0.0, lightStone, 0.14),
         CreateOctagon('SubmergedFooting', 5.5, -22.0, -21.2, stone, 0.12)]
for height in [-5.0, -9.0, -13.0, -17.0]:
    parts.append(CreateOctagon('StoneJoint', 4.72, height-0.06, height+0.06, joint))
bpy.ops.object.select_all(action='DESELECT')
for obj in parts:
    obj.select_set(True)
bpy.context.view_layer.objects.active = parts[0]
bpy.ops.object.join()
pedestal = bpy.context.object
pedestal.name = 'TurretPedestal'
bpy.context.scene.cursor.location = (0.0, 0.0, 0.0)
bpy.ops.object.origin_set(type='ORIGIN_CURSOR')
bpy.ops.object.transform_apply(location=False, rotation=True, scale=True)
bpy.ops.wm.obj_export(filepath=str(kModelDirectory / 'TurretPedestal.obj'), export_selected_objects=True,
                      forward_axis='NEGATIVE_Z', up_axis='Y', export_triangulated_mesh=True)
shutil.copyfile(kStoneDirectory / 'Sandstone.png', kModelDirectory / 'Sandstone.png')
shutil.copyfile(kStoneDirectory / 'SandstoneLight.png', kModelDirectory / 'SandstoneLight.png')
shutil.copyfile(kStoneDirectory / 'SandstoneShade.png', kModelDirectory / 'SandstoneShade.png')
materialText = '''newmtl PedestalStone
Kd 1 1 1
map_Kd Sandstone.png

newmtl PedestalLight
Kd 1 1 1
map_Kd SandstoneLight.png

newmtl PedestalJoint
Kd 1 1 1
map_Kd SandstoneShade.png
'''
(kModelDirectory / 'TurretPedestal.mtl').write_text(materialText, encoding='utf-8')
bpy.ops.wm.save_as_mainfile(filepath=str(kOutputDirectory / 'TurretPedestal.blend'))
pedestal.data.calc_loop_triangles()
print('PEDESTAL_COMPLETE triangles=', len(pedestal.data.loop_triangles), 'size=', tuple(pedestal.dimensions))
