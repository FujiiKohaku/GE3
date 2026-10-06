import math
from pathlib import Path

import bpy
from mathutils import Vector


MODEL_DIR = Path(__file__).parent


def aim_camera(camera, target):
    direction = Vector(target) - camera.location
    camera.rotation_euler = direction.to_track_quat("-Z", "Y").to_euler()


def add_area_light(name, location, energy, color, size):
    data = bpy.data.lights.new(name=name, type="AREA")
    data.energy = energy
    data.color = color
    data.shape = "DISK"
    data.size = size
    light = bpy.data.objects.new(name, data)
    bpy.context.collection.objects.link(light)
    light.location = location
    aim_camera(light, (0.0, 0.0, 2.7))


def main():
    bpy.ops.object.select_all(action="SELECT")
    bpy.ops.object.delete(use_global=False)
    bpy.ops.wm.obj_import(filepath=str(MODEL_DIR / "ice_boss.obj"))

    world = bpy.context.scene.world
    world.color = (0.008, 0.014, 0.025)

    camera_data = bpy.data.cameras.new("PreviewCamera")
    camera = bpy.data.objects.new("PreviewCamera", camera_data)
    bpy.context.collection.objects.link(camera)
    camera.location = (7.5, -12.0, 5.2)
    camera.data.type = "ORTHO"
    camera.data.ortho_scale = 6.8
    aim_camera(camera, (0.0, 0.0, 2.7))
    bpy.context.scene.camera = camera

    add_area_light("ColdKey", (4.5, -5.0, 8.0), 1500.0, (0.32, 0.72, 1.0), 5.0)
    add_area_light("Rim", (-5.0, 4.0, 6.0), 1300.0, (0.05, 0.38, 1.0), 4.0)
    add_area_light("Fill", (2.0, -4.0, 2.0), 700.0, (0.18, 0.28, 0.45), 3.0)

    bpy.ops.mesh.primitive_plane_add(size=30.0, location=(0.0, 0.0, -0.13))
    ground = bpy.context.active_object
    ground.name = "PreviewGround"
    material = bpy.data.materials.new("PreviewGroundMaterial")
    material.diffuse_color = (0.012, 0.026, 0.045, 1.0)
    material.roughness = 0.35
    ground.data.materials.append(material)

    scene = bpy.context.scene
    scene.render.engine = "BLENDER_EEVEE_NEXT"
    scene.render.resolution_x = 700
    scene.render.resolution_y = 700
    scene.render.resolution_percentage = 100
    scene.render.image_settings.file_format = "PNG"
    scene.render.filepath = str(MODEL_DIR / "ice_boss_preview.png")
    scene.render.film_transparent = False
    scene.render.image_settings.color_mode = "RGBA"
    scene.view_settings.look = "AgX - Medium High Contrast"
    scene.render.resolution_percentage = 100
    bpy.ops.render.render(write_still=True)


if __name__ == "__main__":
    main()
