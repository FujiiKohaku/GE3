import bpy
from pathlib import Path

kOutputDirectory = Path(__file__).resolve().parents[2] / 'project/resources/Models/Environment/SavePoint'
kOutputDirectory.mkdir(parents=True, exist_ok=True)
kMaterialColors = {
    'PearlArmor': (0.56, 0.65, 0.7),
    'GraphiteFrame': (0.027, 0.046, 0.065),
    'LanternCore': (0.015, 0.22, 0.27),
    'CyanEmission': (0.025, 0.8, 1.0),
}


def ExportModel(fileName, shouldExportGlow):
    materialFaces = {}
    vertexLines = []
    normalLines = []
    vertexCount = 0
    dependencyGraph = bpy.context.evaluated_depsgraph_get()
    for meshObject in bpy.data.collections['CheckpointAsset'].objects:
        if meshObject.name == 'LightBoundary':
            continue
        materialName = meshObject.data.materials[0].name
        isGlow = materialName == 'CyanEmission'
        if isGlow != shouldExportGlow:
            continue
        evaluatedObject = meshObject.evaluated_get(dependencyGraph)
        mesh = evaluatedObject.to_mesh()
        mesh.calc_loop_triangles()
        normalMatrix = meshObject.matrix_world.to_3x3().inverted().transposed()
        for vertex in mesh.vertices:
            point = meshObject.matrix_world @ vertex.co
            normal = (normalMatrix @ vertex.normal).normalized()
            vertexLines.append(f'v {point.x:.7f} {point.z:.7f} {-point.y:.7f}')
            normalLines.append(f'vn {normal.x:.7f} {normal.z:.7f} {-normal.y:.7f}')
        if materialName not in materialFaces:
            materialFaces[materialName] = []
        for triangle in mesh.loop_triangles:
            faceIndices = []
            for index in triangle.vertices:
                faceIndex = vertexCount + index + 1
                faceIndices.append(f'{faceIndex}/1/{faceIndex}')
            materialFaces[materialName].append('f ' + ' '.join(faceIndices))
        vertexCount += len(mesh.vertices)
        evaluatedObject.to_mesh_clear()
    lines = ['mtllib SavePoint.mtl', 'o ' + fileName]
    lines.extend(vertexLines)
    lines.append('vt 0.5 0.5')
    lines.extend(normalLines)
    for materialName, faceLines in materialFaces.items():
        lines.append('usemtl ' + materialName)
        lines.extend(faceLines)
    (kOutputDirectory / (fileName + '.obj')).write_text('\n'.join(lines) + '\n', encoding='utf-8')


materialLines = []
for materialName, color in kMaterialColors.items():
    image = bpy.data.images.new(materialName + 'Texture', width=4, height=4)
    image.generated_color = (*color, 1.0)
    image.filepath_raw = str(kOutputDirectory / (materialName + '.png'))
    image.file_format = 'PNG'
    image.save()
    materialLines.extend(['newmtl ' + materialName, 'Kd 1 1 1', 'map_Kd ' + materialName + '.png', ''])
(kOutputDirectory / 'SavePoint.mtl').write_text('\n'.join(materialLines), encoding='utf-8')
ExportModel('SavePointBody', False)
ExportModel('SavePointGlow', True)
print('GAME_MODEL_EXPORTED')
