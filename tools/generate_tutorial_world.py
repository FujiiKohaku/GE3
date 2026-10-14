"""Build the tutorial's data-space scenery from solid, untextured geometry."""

import json
import math
import random
from pathlib import Path

from generate_ocean_models import Mesh, add_box

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "project/resources/Models/Environment/Tutorial"
LAYOUT = ROOT / "project/resources/Stages/Tutorial/layout.json"


def hex_ring(mesh, center, radius, thickness=0.65, depth=1.6):
    cx, cy, cz = center
    for i in range(6):
        a, b = i * math.tau / 6, (i + 1) * math.tau / 6
        points = []
        for z in (cz - depth / 2, cz + depth / 2):
            for r, angle in ((radius, a), (radius, b),
                             (radius - thickness, b), (radius - thickness, a)):
                points.append((cx + r * math.cos(angle), cy + r * math.sin(angle), z))
        for face in ((0, 3, 2, 1), (4, 5, 6, 7), (0, 1, 5, 4),
                     (3, 7, 6, 2), (0, 4, 7, 3), (1, 2, 6, 5)):
            mesh.quad("data", *(points[j] for j in face))


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    (OUT / "data_palette.mtl").write_text(
        "newmtl data\nKd 1 1 1\nKa 1 1 1\nd 1\n"
        "map_Kd ../../../Textures/white.png\n", encoding="utf-8")
    grid, dark, edges = Mesh("DataGrid"), Mesh("DataArchitecture"), Mesh("DataEdges")
    rings, gate = Mesh("TrainingRings"), Mesh("LaunchGate")
    add_box(dark, (0, -38.5, 1750), (760, 0.8, 4100), "data")
    for x in range(-360, 361, 30):
        add_box(grid, (x, -38, 1750), (0.14, 0.12, 4100), "data")
    for z in range(-300, 3801, 50):
        add_box(grid, (0, -38, z), (720, 0.12, 0.18), "data")
        for x in (-52, 52):
            add_box(edges, (x, -37.8, z), (0.4, 0.2, 14), "data")
    for i, z in enumerate(range(150, 3551, 200)):
        for side in (-1, 1):
            x = side * (135 + (i % 3) * 36)
            height = 55 + (i * 37 % 105)
            width, depth = 12 + (i % 3) * 5, 18
            y = -38 + height / 2
            add_box(dark, (x, y, z), (width, height, depth), "data")
            for dx in (-width / 2, width / 2):
                for dz in (-depth / 2, depth / 2):
                    add_box(edges, (x + dx, y, z + dz), (0.25, height, 0.25), "data")
            add_box(edges, (x, -38 + height, z), (width + 1, 0.25, depth + 1), "data")
    for z in range(240, 3121, 360):
        hex_ring(rings, (0, 10, z), 55)
    hex_ring(gate, (0, 28, 3400), 78, 1.3, 3)
    hex_ring(gate, (0, 28, 3407), 85, 0.5, 1.5)
    rng = random.Random(13)
    for _ in range(100):
        x = rng.choice((-1, 1)) * rng.uniform(110, 370)
        add_box(grid, (x, rng.uniform(45, 190), rng.uniform(0, 3600)),
                (0.6, 0.6, 0.6), "data")
    objects = [{"name": "TutorialPlayerStart", "type": "PlayerSpawn",
                "transform": {"translation": [0, 0, 0], "rotation": [0, 0, 0]}}]
    for filename, mesh in (("data_grid", grid), ("data_architecture", dark),
                           ("data_edges", edges), ("training_rings", rings),
                           ("launch_gate", gate)):
        path = OUT / (filename + ".obj")
        mesh.write(path)
        path.write_text(path.read_text(encoding="utf-8").replace(
            "ocean_palette.mtl", "data_palette.mtl").replace(
            "low-poly ocean scenery", "tutorial data-space scenery"), encoding="utf-8")
        objects.append({"name": filename, "type": "MESH",
                        "file_name": "Environment/Tutorial/" + filename + ".obj",
                        "transform": {"translation": [0, 0, 0], "rotation": [0, 0, 0],
                                      "scale": [1, 1, 1]}})
        print(f"Created {filename}: {len(mesh.faces)} triangles")
    LAYOUT.write_text(json.dumps({"scene": objects}, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
