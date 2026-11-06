"""Generate FearWorm head poses and a comparison preview using Blender."""
import bpy, math
from pathlib import Path
from mathutils import Vector

root = Path(__file__).resolve().parents[2]
out = root / 'resources/Models/Boss/FearWorm/Head'
(out / 'textures').mkdir(parents=True, exist_ok=True)
bpy.ops.wm.read_factory_settings(use_empty=True)
colors = [(0.90,0.85,0.73,1), (0.76,0.52,0.18,1), (0.035,0.09,0.11,1), (0.04,0.80,0.87,1), (0.95,0.07,0.025,1)]
image = bpy.data.images.new('HeadPalette', width=80, height=16)
image.pixels = [c for y in range(16) for x in range(80) for c in colors[x//16]]
image.filepath_raw=str(out/'textures/HeadPalette.png'); image.file_format='PNG'; image.save()
mat=bpy.data.materials.new('HeadPalette'); mat.use_nodes=True
node=next(n for n in mat.node_tree.nodes if n.type=='BSDF_PRINCIPLED')
tex=mat.node_tree.nodes.new('ShaderNodeTexImage'); tex.image=image
mat.node_tree.links.new(tex.outputs['Color'],node.inputs['Base Color'])

def finish(obj, name, tile):
    obj.name=name; mesh=obj.data; uv=mesh.uv_layers.active or mesh.uv_layers.new(name='Palette')
    for loop in uv.data: loop.uv=((tile+0.5)/5,0.5)
    mesh.materials.clear(); mesh.materials.append(mat)
    return obj
def sphere(name, loc, scale, tile):
    bpy.ops.mesh.primitive_uv_sphere_add(segments=24, ring_count=12, location=loc)
    o=bpy.context.object; o.scale=scale
    for p in o.data.polygons: p.use_smooth=True
    return finish(o,name,tile)
def torus(name, loc, major, minor, tile):
    bpy.ops.mesh.primitive_torus_add(major_segments=32,minor_segments=8,major_radius=major,minor_radius=minor,location=loc,rotation=(math.pi/2,0,0))
    return finish(bpy.context.object,name,tile)
def box(name, loc, size, rotation, tile):
    bpy.ops.mesh.primitive_cube_add(size=1,location=loc); o=bpy.context.object; o.dimensions=size; o.rotation_euler=rotation
    bpy.ops.object.transform_apply(location=False,rotation=False,scale=True)
    mod=o.modifiers.new('Rounded edges','BEVEL'); mod.width=0.045; mod.segments=2
    return finish(o,name,tile)

for step in range(5):
    bpy.ops.object.select_all(action='SELECT'); bpy.ops.object.delete(use_global=False)
    amount=step/4
    parts=[]
    # Front faces Blender -Y, exported as game -Z.
    parts.append(sphere('Armoured head',(0,0,0),(1.0,.88,1.0),0))
    parts.append(torus('Rear connector',(0,.73,0),.69,.10,1))
    parts.append(torus('Rear energy',(0,.84,0),.69,.05,3))
    parts.append(sphere('Dark eye socket',(0,-.80,0),(.62,.22,.62),2))
    parts.append(torus('Eye gold frame',(0,-.94,0),.52,.09,1))
    eyes=[sphere('Red eye',(0,-1.00,0),(.40,.18,.40),4),
          sphere('Eye highlight',(0,-1.17,0),(.14,.045,.14),0)]
    for i in range(4):
        angle=i*math.pi/2
        radius=.59 + amount*.31
        x,z=math.sin(angle)*radius, math.cos(angle)*radius
        # Rounded panels slide out from the central eye as it charges.
        parts.append(box('Gold petal %d'%i,(x,-1.09,z),(.46,.19,.40),(0,angle,0),1))
        parts.append(box('Ivory petal %d'%i,(x,-1.20,z),(.32,.12,.27),(0,angle,0),0))
    bpy.ops.object.select_all(action='DESELECT')
    for o in parts: o.select_set(True)
    bpy.context.view_layer.objects.active=parts[0]
    if step==0:
        bpy.ops.wm.save_as_mainfile(filepath=str(out/'FearWormHead.blend'))
        bpy.ops.object.select_all(action='DESELECT')
        for o in eyes: o.select_set(True)
        bpy.context.view_layer.objects.active=eyes[0]
        bpy.ops.object.convert(target='MESH')
        bpy.ops.object.join()
        eye_path=out/'FearWormEye.obj'
        bpy.ops.wm.obj_export(filepath=str(eye_path),export_selected_objects=True,forward_axis='NEGATIVE_Z',up_axis='Y',export_triangulated_mesh=True,path_mode='RELATIVE')
        eye_mtl=eye_path.with_suffix('.mtl'); lines=eye_mtl.read_text(encoding='utf-8').splitlines()
        eye_mtl.write_text('\n'.join('map_Kd textures/HeadPalette.png' if s.startswith('map_Kd ') else s for s in lines)+'\n',encoding='utf-8')
    else:
        for o in eyes: bpy.data.objects.remove(o, do_unlink=True)
    bpy.ops.object.select_all(action='DESELECT')
    for o in parts: o.select_set(True)
    bpy.context.view_layer.objects.active=parts[0]
    bpy.ops.object.convert(target='MESH')
    bpy.ops.object.join()
    path=out/('FearWormHead_%d.obj'%step)
    bpy.ops.wm.obj_export(filepath=str(path),export_selected_objects=True,forward_axis='NEGATIVE_Z',up_axis='Y',export_triangulated_mesh=True,path_mode='RELATIVE')
    mtl=path.with_suffix('.mtl'); lines=mtl.read_text(encoding='utf-8').splitlines()
    mtl.write_text('\n'.join('map_Kd textures/HeadPalette.png' if s.startswith('map_Kd ') else s for s in lines)+'\n',encoding='utf-8')
    print('EXPORTED',path)
