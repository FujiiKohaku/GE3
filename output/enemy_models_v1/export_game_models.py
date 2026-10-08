import bpy
import os
from mathutils import Vector

outputDirectory = 'C:/Projects/KohakuEngine/project/resources/Models/Enemy/Frozen'
os.makedirs(outputDirectory, exist_ok=True)
modelNames = ['NormalEnemy', 'MoveEnemy', 'ArmoredEnemy', 'PaintShooterEnemy', 'SwarmEnemy']
displayLocations = [(-4.8, 1.8, 1.8), (0, 1.8, 1.8), (4.8, 1.8, 1.8), (-2.6, -3.0, 1.8), (3.0, -3.0, 1.8)]
for modelName, displayLocation in zip(modelNames, displayLocations):
    bpy.ops.object.select_all(action='DESELECT')
    modelObject = bpy.data.objects[modelName]
    modelObject.location -= Vector(displayLocation)
    modelObject.select_set(True)
    bpy.context.view_layer.objects.active = modelObject
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    # Match the existing collision volume rather than the concept-sheet display scale.
    radius = max(vertex.co.length for vertex in modelObject.data.vertices)
    targetRadius = 1.35
    if modelName == 'SwarmEnemy':
        targetRadius = 1.8
    modelObject.scale = (targetRadius / radius,) * 3
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    for material in modelObject.data.materials:
        imagePath = os.path.join(outputDirectory, material.name + '.png')
        if not os.path.isfile(imagePath):
            image = bpy.data.images.new(material.name + 'Color', width=8, height=8)
            image.generated_color = material.diffuse_color
            image.filepath_raw = imagePath
            image.file_format = 'PNG'
            image.save()
    modelPath = os.path.join(outputDirectory, modelName + '.obj')
    bpy.ops.wm.obj_export(filepath=modelPath, export_selected_objects=True,
        forward_axis='NEGATIVE_Z', up_axis='Y', export_triangulated_mesh=True)
    materialPath = os.path.join(outputDirectory, modelName + '.mtl')
    with open(materialPath, 'r', encoding='utf-8') as materialFile:
        lines = materialFile.readlines()
    with open(materialPath, 'w', encoding='utf-8') as materialFile:
        for line in lines:
            materialFile.write(line)
            if line.startswith('newmtl '):
                materialName = line.strip().split(' ', 1)[1]
                materialFile.write('map_Kd ' + materialName + '.png\n')
__result__ = {'directory': outputDirectory}
