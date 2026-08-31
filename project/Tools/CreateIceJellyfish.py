"""Run with Blender --background --python project/tools/CreateIceJellyfish.py.

Builds independent Y-up OBJ parts with normals/UVs, an assembled reference,
and a Blender preview. Runtime animation lives in IceJellyfish.cpp.
"""
import json
import math
from pathlib import Path

import bpy
import bmesh
from mathutils import Matrix, Vector


PROJECT = Path(__file__).resolve().parents[1]
OUTPUT = PROJECT / "resources/Models/Boss/IceJellyfish"
PREVIEW = PROJECT.parent / "generated/IceJellyfish"
OUTPUT.mkdir(parents=True, exist_ok=True)
PREVIEW.mkdir(parents=True, exist_ok=True)


def make_mesh(name, vertices, faces):
    mesh = bpy.data.meshes.new(name)
    mesh.from_pydata(vertices, [], faces)
    mesh.update()
    bm = bmesh.new()
    bm.from_mesh(mesh)
    bmesh.ops.recalc_face_normals(bm, faces=list(bm.faces))
    bmesh.ops.triangulate(bm, faces=list(bm.faces))
    bm.to_mesh(mesh)
    bm.free()
    mesh.update()
    obj = bpy.data.objects.new(name, mesh)
    bpy.context.collection.objects.link(obj)
    return obj


def make_bell():
    sides = 64
    # Outer surface runs from the crown to the rim; inner surface returns up.
    profile = [(0.0, 12.0), (4.4, 11.7), (8.8, 10.9),
               (13.2, 9.4), (17.3, 7.2), (20.3, 4.3), (22.0, 0.0),
               (20.9, 0.0), (19.3, 3.6), (16.5, 6.2), (12.7, 8.3),
               (8.5, 9.7), (4.2, 10.5), (0.0, 10.8)]
    vertices = []
    rings = []
    for ring_index, (radius, height) in enumerate(profile):
        if radius == 0.0:
            rings.append([len(vertices)])
            vertices.append((0.0, height, 0.0))
            continue
        ring = []
        for index in range(sides):
            angle = 2.0 * math.pi * index / sides
            y = height
            if ring_index not in (6, 7):
                angle += (ring_index % 2) * math.pi / sides
                y += math.sin(index * 1.7 + ring_index * 2.3) * 0.22
            if ring_index in (6, 7):
                # Sixteen icicle points are part of the hollow shell itself.
                y -= 3.1 * max(0.0, math.cos(angle * 16.0))
            ring.append(len(vertices))
            vertices.append((radius * math.cos(angle), y, radius * math.sin(angle)))
        rings.append(ring)
    faces = []
    for first, second in zip(rings, rings[1:]):
        for index in range(sides):
            following = (index + 1) % sides
            if len(first) == 1:
                faces.append((first[0], second[index], second[following]))
            elif len(second) == 1:
                faces.append((first[index], second[0], first[following]))
            else:
                faces.append((first[index], second[index], second[following], first[following]))
    return make_mesh("IceJellyfishBell", vertices, faces)


def make_link(name, tip):
    profile = [(0.0, 0.28), (-0.09, 0.50), (-0.72, 0.42),
               (-0.94, 0.27), (-1.0, 0.20)]
    if tip:
        profile = [(0.0, 0.28), (-0.10, 0.46), (-0.48, 0.32),
                   (-0.79, 0.14), (-1.0, 0.0)]
    vertices = []
    for height, radius in profile:
        for index in range(8):
            angle = 2.0 * math.pi * index / 8.0
            vertices.append((math.cos(angle) * radius, height, math.sin(angle) * radius))
    faces = [tuple(range(7, -1, -1))]
    for ring in range(len(profile) - 1):
        for index in range(8):
            following = (index + 1) % 8
            faces.append((ring * 8 + index, (ring + 1) * 8 + index,
                          (ring + 1) * 8 + following, ring * 8 + following))
    if not tip:
        faces.append(tuple(range(32, 40)))
    obj = make_mesh(name, vertices, faces)
    # Weld the tip's coincident vertices and remove degenerate triangles.
    bm = bmesh.new()
    bm.from_mesh(obj.data)
    bmesh.ops.remove_doubles(bm, verts=list(bm.verts), dist=0.00001)
    bmesh.ops.recalc_face_normals(bm, faces=list(bm.faces))
    bm.to_mesh(obj.data)
    bm.free()
    obj.data.update()
    return obj


def write_obj(path, objects):
    lines = ["# KohakuEngine ice jellyfish. Y up; tentacle pivot at root, length along -Y.",
             "mtllib IceJellyfish.mtl"]
    offset = 0
    stats = {}
    for obj in objects:
        matrix = obj.matrix_world
        normals = matrix.to_3x3().inverted().transposed()
        mesh = obj.data
        lines.extend(["o " + obj.name, "usemtl " + obj.data.materials[0].name, "s off"])
        count = 0
        for face in mesh.polygons:
            assert len(face.vertices) == 3
            normal = (normals @ face.normal).normalized()
            indices = []
            for vertex_index in face.vertices:
                local = mesh.vertices[vertex_index].co
                position = matrix @ local
                u = local.x / 44.0 + 0.5
                v = local.z / 44.0 + 0.5
                if "Segment" in mesh.name or "Tip" in mesh.name:
                    u = math.atan2(local.z, local.x) / (2.0 * math.pi) + 0.5
                    v = -local.y
                lines.append("v %.7f %.7f %.7f" % tuple(position))
                lines.append("vt %.7f %.7f" % (u, v))
                lines.append("vn %.7f %.7f %.7f" % tuple(normal))
                offset += 1
                indices.append(str(offset) + "/" + str(offset) + "/" + str(offset))
            lines.append("f " + " ".join(indices))
            count += 1
        stats[obj.name] = count
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")
    return stats


def make_material(name, color, metallic, roughness):
    material = bpy.data.materials.new(name)
    material.diffuse_color = (*color, 1.0)
    material.use_nodes = True
    shader = material.node_tree.nodes.get("Principled BSDF")
    shader.inputs["Base Color"].default_value = (*color, 1.0)
    shader.inputs["Metallic"].default_value = metallic
    shader.inputs["Roughness"].default_value = roughness
    return material


def add_uv(obj):
    uv = obj.data.uv_layers.new(name="UVMap")
    for face in obj.data.polygons:
        for loop_index in face.loop_indices:
            local = obj.data.vertices[obj.data.loops[loop_index].vertex_index].co
            u = local.x / 44.0 + 0.5
            v = local.z / 44.0 + 0.5
            if "Segment" in obj.name or "Tip" in obj.name:
                u = math.atan2(local.z, local.x) / (2.0 * math.pi) + 0.5
                v = -local.y
            uv.data[loop_index].uv = (u, v)


def tentacle_matrices(tentacle, time):
    azimuth = 2.0 * math.pi * tentacle / 6.0 + 0.25
    radial_x = math.cos(azimuth)
    radial_z = math.sin(azimuth)
    joint = Vector((radial_x * 10.5, -1.2, radial_z * 10.5))
    bend = 0.30
    result = []
    for segment in range(7):
        phase = time * 1.05 - segment * 0.65 + azimuth * 1.3
        bend += math.sin(phase) * (0.065 + segment * 0.012)
        sway = math.sin(phase * 0.73) * 0.12
        down = Vector((radial_x * math.sin(bend) - radial_z * sway,
                       -math.cos(bend), radial_z * math.sin(bend) + radial_x * sway)).normalized()
        up = -down
        tangent = Vector((-radial_z, 0.0, radial_x))
        right = up.cross(tangent).normalized()
        forward = right.cross(up)
        length = (5.4 - segment * 0.32) * 1.5
        # Match the broad-root / slender-tip profile in IceJellyfish.cpp.
        width = 6.6 - segment * 1.0
        matrix = Matrix.Identity(4)
        matrix.col[0] = (*tuple(right * width), 0.0)
        matrix.col[1] = (*tuple(up * length), 0.0)
        matrix.col[2] = (*tuple(forward * width), 0.0)
        matrix.translation = joint
        result.append(matrix)
        joint = joint + down * (length + 0.18)
    return result


def area_light(name, location, power, color, size):
    data = bpy.data.lights.new(name, "AREA")
    data.energy = power
    data.color = color
    data.shape = "DISK"
    data.size = size
    obj = bpy.data.objects.new(name, data)
    bpy.context.collection.objects.link(obj)
    obj.location = location
    obj.rotation_euler = (Vector((0, -6, 0)) - obj.location).to_track_quat("-Z", "Y").to_euler()


def main():
    # Only run in the dedicated background process, never the user's live scene.
    bpy.ops.object.select_all(action="SELECT")
    bpy.ops.object.delete(use_global=False)
    ice = make_material("JellyfishIce", (0.34, 0.66, 0.88), 0.28, 0.24)
    texture = ice.node_tree.nodes.new("ShaderNodeTexImage")
    texture.image = bpy.data.images.load(str(PROJECT / "resources/Textures/ice_texture.jpg"))
    shader = ice.node_tree.nodes.get("Principled BSDF")
    tint = ice.node_tree.nodes.new("ShaderNodeMixRGB")
    tint.blend_type = "MULTIPLY"
    tint.inputs[0].default_value = 1.0
    tint.inputs[2].default_value = (0.65, 0.86, 1.0, 1.0)
    ice.node_tree.links.new(texture.outputs["Color"], tint.inputs[1])
    ice.node_tree.links.new(tint.outputs["Color"], shader.inputs["Base Color"])
    core_material = make_material("JellyfishCore", (1.0, 0.40, 0.06), 0.1, 0.26)
    shader = core_material.node_tree.nodes.get("Principled BSDF")
    shader.inputs["Emission Color"].default_value = (1.0, 0.26, 0.025, 1.0)
    shader.inputs["Emission Strength"].default_value = 2.0
    # OBJ material files carry the existing ice texture; runtime uses reflection.
    white = bpy.data.images.new("JellyfishWhite", width=2, height=2)
    white.pixels = [1.0] * 16
    white.filepath_raw = str(OUTPUT / "CoreWhite.png")
    white.file_format = "PNG"
    white.save()
    (OUTPUT / "IceJellyfish.mtl").write_text(
        "newmtl JellyfishIce\nKa 0.3 0.5 0.6\nKd 0.65 0.86 1.0\n"
        "Ks 0.9 0.95 1.0\nNs 96\nd 1.0\nillum 2\n"
        "map_Kd ../../../Textures/ice_texture.jpg\n\n"
        "newmtl JellyfishCore\nKd 1.0 0.45 0.08\nKe 1.0 0.35 0.04\n"
        "d 1.0\nillum 2\nmap_Kd CoreWhite.png\n", encoding="utf-8")
    bell = make_bell()
    segment = make_link("IceJellyfishSegment", False)
    tip = make_link("IceJellyfishTip", True)
    bpy.ops.mesh.primitive_ico_sphere_add(subdivisions=2, radius=1.0)
    core = bpy.context.object
    core.name = "IceJellyfishCore"
    for vertex in core.data.vertices:
        vertex.co.x *= 3.6
        vertex.co.y *= 4.2
        vertex.co.z *= 3.6
    core.data.update()
    parts = [bell, segment, tip, core]
    stats = {}
    for obj in parts:
        material = ice
        if obj == core:
            material = core_material
        obj.data.materials.append(material)
        add_uv(obj)
        stats.update(write_obj(OUTPUT / (obj.name + ".obj"), [obj]))
    segment.hide_render = True
    tip.hide_render = True
    segment.hide_set(True)
    tip.hide_set(True)
    core.location.y = -2.0
    assembled = [bell, core]
    for tentacle in range(6):
        for index, matrix in enumerate(tentacle_matrices(tentacle, 0.0)):
            template = segment
            if index == 6:
                template = tip
            obj = bpy.data.objects.new("Tentacle_%02d_Segment_%02d" % (tentacle, index), template.data)
            bpy.context.collection.objects.link(obj)
            obj.matrix_world = matrix
            assembled.append(obj)
    bpy.context.view_layer.update()
    write_obj(OUTPUT / "IceJellyfishAssembled.obj", assembled)
    (PREVIEW / "mesh_stats.json").write_text(json.dumps(stats, indent=2), encoding="utf-8")

    scene = bpy.context.scene
    scene.render.engine = "CYCLES"
    scene.cycles.samples = 32
    scene.cycles.use_denoising = True
    scene.world.color = (0.035, 0.045, 0.07)
    scene.world.use_nodes = True
    scene.world.node_tree.nodes.get("Background").inputs["Color"].default_value = (0.015, 0.028, 0.055, 1.0)
    area_light("Key", (15, 45, -45), 65000, (0.65, 0.82, 1.0), 35)
    area_light("Rim", (-40, 20, 15), 85000, (0.25, 0.66, 1.0), 25)
    area_light("Fill", (35, -12, -25), 28000, (0.55, 0.8, 1.0), 25)
    area_light("Lower fill", (0, -20, -15), 3500, (1.0, 0.5, 0.15), 8)
    camera_data = bpy.data.cameras.new("PreviewCamera")
    camera = bpy.data.objects.new("PreviewCamera", camera_data)
    bpy.context.collection.objects.link(camera)
    camera.location = (42, 1, -100)
    direction = (Vector((0, -16, 0)) - camera.location).normalized()
    right = direction.cross(Vector((0, 1, 0))).normalized()
    up = right.cross(direction)
    camera.rotation_euler = Matrix((right, up, -direction)).transposed().to_euler()
    camera_data.type = "ORTHO"
    camera_data.ortho_scale = 85
    scene.camera = camera
    scene.render.resolution_x = 1200
    scene.render.resolution_y = 1200
    scene.render.resolution_percentage = 100
    scene.render.image_settings.file_format = "PNG"
    scene.render.filepath = str(PREVIEW / "IceJellyfishPreview.png")
    bpy.ops.wm.save_as_mainfile(filepath=str(PREVIEW / "IceJellyfish.blend"))
    bpy.ops.render.render(write_still=True)
    print("ICE_JELLYFISH_PARTS=" + json.dumps(stats))


if __name__ == "__main__":
    main()
