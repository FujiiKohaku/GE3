"""Build a continuous arch silhouette; all masonry detail is shaded in the PS."""

import bpy
import bmesh
import math


def CreateSolidMasonry(generator, material):
    outline = [(-5.65, 0.54), (-1.4, 0.54)]
    for index in range(25):
        angle = math.pi - index * math.pi / 24
        outline.append((1.4 * math.cos(angle), 2.01 + 1.4 * math.sin(angle)))
    outline.extend([(1.4, 0.54), (5.65, 0.54), (5.65, 2.30),
        (4.60, 2.30), (4.60, 2.82), (3.60, 2.82), (3.60, 3.98),
        (-4.80, 3.98), (-4.80, 3.38), (-5.65, 3.38)])
    wall = CreateExtrudedMasonry(material, outline, [-0.06, 0.78])
    generator.assetObjects.append(wall)
    return wall


def CreateExtrudedMasonry(material, outline, depths):
    vertices = []
    for depth in depths:
        for positionX, height in outline:
            vertices.append((positionX, depth, height))
    count = len(outline)
    faces = [tuple(reversed(range(count))), tuple(range(count, count * 2))]
    for index in range(count):
        nextIndex = (index + 1) % count
        faces.append((index, nextIndex, nextIndex + count, index + count))
    mesh = bpy.data.meshes.new('MasonrySurfaceMesh')
    mesh.from_pydata(vertices, [], faces)
    mesh.update()
    wall = bpy.data.objects.new('MasonrySurface', mesh)
    bpy.context.collection.objects.link(wall)
    mesh.materials.append(material)
    meshData = bmesh.new()
    meshData.from_mesh(wall.data)
    bmesh.ops.recalc_face_normals(meshData, faces=list(meshData.faces))
    if any(not edge.is_manifold for edge in meshData.edges):
        raise RuntimeError('Solid masonry is not closed.')
    meshData.to_mesh(wall.data)
    meshData.free()
    return wall


def SetMasonryUvs(model, material, tileWidth=1.05, tileHeight=0.63):
    uvLayer = model.data.uv_layers.active
    for polygon in model.data.polygons:
        if model.data.materials[polygon.material_index] != material:
            continue
        for loopIndex in polygon.loop_indices:
            vertex = model.data.vertices[model.data.loops[loopIndex].vertex_index].co
            horizontal = vertex.x
            vertical = vertex.z
            if abs(polygon.normal.x) > 0.5:
                horizontal = vertex.y
            elif abs(polygon.normal.z) > 0.5:
                vertical = vertex.y
            uvLayer.data[loopIndex].uv = (horizontal / tileWidth, vertical / tileHeight)


def SetPreviewMaterial(material):
    nodes = material.node_tree.nodes
    links = material.node_tree.links
    shader = nodes.get('Principled BSDF')
    coordinates = nodes.new('ShaderNodeTexCoord')
    brick = nodes.new('ShaderNodeTexBrick')
    links.new(coordinates.outputs['UV'], brick.inputs['Vector'])
    brick.inputs['Scale'].default_value = 1.0
    brick.inputs['Brick Width'].default_value = 1.0
    brick.inputs['Row Height'].default_value = 1.0
    brick.inputs['Mortar Size'].default_value = 0.018
    brick.inputs['Mortar Smooth'].default_value = 0.06
    brick.inputs['Color1'].default_value = (0.64, 0.48, 0.29, 1)
    brick.inputs['Color2'].default_value = (0.74, 0.57, 0.36, 1)
    brick.inputs['Mortar'].default_value = (0.24, 0.17, 0.10, 1)
    links.new(brick.outputs['Color'], shader.inputs['Base Color'])
    bump = nodes.new('ShaderNodeBump')
    bump.invert = True
    bump.inputs['Strength'].default_value = 0.65
    bump.inputs['Distance'].default_value = 0.035
    links.new(brick.outputs['Fac'], bump.inputs['Height'])
    links.new(bump.outputs['Normal'], shader.inputs['Normal'])


def ExportParts(model, assetDirectory, material):
    # Copies share the original UVs, and each triangle belongs to exactly one part.
    for partName in ['ground', 'masonry']:
        part = model.copy()
        part.data = model.data.copy()
        bpy.context.collection.objects.link(part)
        meshData = bmesh.new()
        meshData.from_mesh(part.data)
        removeFaces = []
        for face in meshData.faces:
            isMasonry = part.data.materials[face.material_index] == material
            if partName == 'ground' and isMasonry:
                removeFaces.append(face)
            elif partName == 'masonry' and not isMasonry:
                removeFaces.append(face)
        bmesh.ops.delete(meshData, geom=removeFaces, context='FACES')
        meshData.to_mesh(part.data)
        meshData.free()
        bpy.ops.object.select_all(action='DESELECT')
        part.select_set(True)
        bpy.context.view_layer.objects.active = part
        bpy.ops.object.material_slot_remove_unused()
        part.name = 'FloatingEarthWall_' + partName
        bpy.ops.wm.obj_export(filepath=str(assetDirectory / ('floating_earth_wall_' + partName + '.obj')),
            export_selected_objects=True, export_materials=True, export_triangulated_mesh=True,
            forward_axis='NEGATIVE_Z', up_axis='Y', path_mode='RELATIVE')
        bpy.data.objects.remove(part, do_unlink=True)
    bpy.ops.object.select_all(action='DESELECT')
    model.select_set(True)
    bpy.context.view_layer.objects.active = model
