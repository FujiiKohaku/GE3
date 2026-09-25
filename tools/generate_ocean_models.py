"""Generate low-poly ocean scenery for Stage01 as Wavefront OBJ assets."""

from __future__ import annotations

import math
import random
import struct
import zlib
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "project" / "resources" / "Models" / "Environment" / "Ocean"
TEX = OUT / "textures"


COLORS = {
    "rock_dark": (68, 78, 75),
    "rock_light": (112, 119, 105),
    "sand": (218, 190, 126),
    "grass": (72, 137, 78),
    "coral_orange": (236, 112, 69),
    "coral_pink": (225, 105, 145),
    "coral_gold": (232, 177, 71),
    "wood_dark": (91, 58, 39),
    "wood_light": (151, 101, 59),
    "leaf_green": (45, 118, 67),
}


def write_solid_png(path: Path, color: tuple[int, int, int], size: int = 8) -> None:
    raw = b"".join(b"\x00" + bytes((*color, 255)) * size for _ in range(size))

    def chunk(kind: bytes, data: bytes) -> bytes:
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data) & 0xFFFFFFFF)

    png = b"\x89PNG\r\n\x1a\n"
    png += chunk(b"IHDR", struct.pack(">IIBBBBB", size, size, 8, 6, 0, 0, 0))
    png += chunk(b"IDAT", zlib.compress(raw, 9))
    png += chunk(b"IEND", b"")
    path.write_bytes(png)


class Mesh:
    def __init__(self, name: str):
        self.name = name
        self.vertices: list[tuple[float, float, float]] = []
        self.normals: list[tuple[float, float, float]] = []
        self.faces: list[tuple[str, tuple[int, int, int], int]] = []

    def triangle(self, material: str, a, b, c) -> None:
        ux, uy, uz = (b[i] - a[i] for i in range(3))
        vx, vy, vz = (c[i] - a[i] for i in range(3))
        nx, ny, nz = uy * vz - uz * vy, uz * vx - ux * vz, ux * vy - uy * vx
        length = math.sqrt(nx * nx + ny * ny + nz * nz) or 1.0
        normal = (nx / length, ny / length, nz / length)
        base = len(self.vertices) + 1
        self.vertices.extend((a, b, c))
        self.normals.append(normal)
        self.faces.append((material, (base, base + 1, base + 2), len(self.normals)))

    def quad(self, material: str, a, b, c, d) -> None:
        self.triangle(material, a, b, c)
        self.triangle(material, a, c, d)

    def write(self, path: Path) -> None:
        lines = [
            "# Procedurally generated low-poly ocean scenery",
            "mtllib ocean_palette.mtl",
            f"o {self.name}",
        ]
        lines.extend(f"v {x:.6f} {y:.6f} {z:.6f}" for x, y, z in self.vertices)
        lines.extend(f"vn {x:.6f} {y:.6f} {z:.6f}" for x, y, z in self.normals)
        active = None
        for material, indices, normal_index in self.faces:
            if material != active:
                lines.append(f"usemtl {material}")
                active = material
            a, b, c = indices
            lines.append(f"f {a}//{normal_index} {b}//{normal_index} {c}//{normal_index}")
        path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def ring(count: int, rx: float, rz: float, y: float, rng: random.Random, jitter: float = 0.12):
    points = []
    for i in range(count):
        angle = math.tau * i / count
        radius = 1.0 + rng.uniform(-jitter, jitter)
        points.append((math.cos(angle) * rx * radius, y + rng.uniform(-0.22, 0.22), math.sin(angle) * rz * radius))
    return points


def connect_rings(mesh: Mesh, material: str, lower, upper) -> None:
    count = len(lower)
    for i in range(count):
        j = (i + 1) % count
        mesh.quad(material, lower[i], upper[i], upper[j], lower[j])


def cap_ring(mesh: Mesh, material: str, points, center) -> None:
    for i in range(len(points)):
        j = (i + 1) % len(points)
        mesh.triangle(material, points[i], center, points[j])


def add_rock(mesh: Mesh, center, radius, height, material, rng: random.Random, sides: int = 7) -> None:
    cx, cy, cz = center
    base = []
    shoulder = []
    for i in range(sides):
        angle = math.tau * i / sides
        variation = rng.uniform(0.78, 1.18)
        base.append((cx + math.cos(angle) * radius * variation, cy, cz + math.sin(angle) * radius * variation))
        shoulder.append((cx + math.cos(angle) * radius * 0.62 * variation, cy + height * rng.uniform(0.56, 0.72), cz + math.sin(angle) * radius * 0.62 * variation))
    connect_rings(mesh, material, base, shoulder)
    tip = (cx + rng.uniform(-0.18, 0.18) * radius, cy + height, cz + rng.uniform(-0.18, 0.18) * radius)
    cap_ring(mesh, material, shoulder, tip)


def cylinder_between(mesh: Mesh, start, end, radius: float, material: str, sides: int = 6) -> None:
    sx, sy, sz = start
    ex, ey, ez = end
    dx, dy, dz = ex - sx, ey - sy, ez - sz
    length = math.sqrt(dx * dx + dy * dy + dz * dz)
    direction = (dx / length, dy / length, dz / length)
    helper = (0.0, 1.0, 0.0) if abs(direction[1]) < 0.9 else (1.0, 0.0, 0.0)
    ux = direction[1] * helper[2] - direction[2] * helper[1]
    uy = direction[2] * helper[0] - direction[0] * helper[2]
    uz = direction[0] * helper[1] - direction[1] * helper[0]
    ul = math.sqrt(ux * ux + uy * uy + uz * uz)
    u = (ux / ul, uy / ul, uz / ul)
    v = (u[1] * direction[2] - u[2] * direction[1], u[2] * direction[0] - u[0] * direction[2], u[0] * direction[1] - u[1] * direction[0])
    a_ring, b_ring = [], []
    for i in range(sides):
        angle = math.tau * i / sides
        ox = radius * (math.cos(angle) * u[0] + math.sin(angle) * v[0])
        oy = radius * (math.cos(angle) * u[1] + math.sin(angle) * v[1])
        oz = radius * (math.cos(angle) * u[2] + math.sin(angle) * v[2])
        a_ring.append((sx + ox, sy + oy, sz + oz))
        b_ring.append((ex + ox * 0.78, ey + oy * 0.78, ez + oz * 0.78))
    connect_rings(mesh, material, a_ring, b_ring)
    cap_ring(mesh, material, b_ring, end)


def add_box(mesh: Mesh, center, size, material: str) -> None:
    cx, cy, cz = center
    hx, hy, hz = (value * 0.5 for value in size)
    p = [
        (cx - hx, cy - hy, cz - hz), (cx + hx, cy - hy, cz - hz),
        (cx + hx, cy + hy, cz - hz), (cx - hx, cy + hy, cz - hz),
        (cx - hx, cy - hy, cz + hz), (cx + hx, cy - hy, cz + hz),
        (cx + hx, cy + hy, cz + hz), (cx - hx, cy + hy, cz + hz),
    ]
    for a, b, c, d in ((0, 3, 2, 1), (4, 5, 6, 7), (0, 4, 7, 3),
                        (5, 1, 2, 6), (3, 7, 6, 2), (0, 1, 5, 4)):
        mesh.quad(material, p[a], p[b], p[c], p[d])


def add_leaf(mesh: Mesh, root, tip, width: float, material: str) -> None:
    rx, ry, rz = root
    tx, ty, tz = tip
    dx, dz = tx - rx, tz - rz
    length = math.sqrt(dx * dx + dz * dz) or 1.0
    px, pz = -dz / length * width, dx / length * width
    mid = ((rx + tx) * 0.5, (ry + ty) * 0.5 + 0.35, (rz + tz) * 0.5)
    left = (mid[0] + px, mid[1], mid[2] + pz)
    right = (mid[0] - px, mid[1], mid[2] - pz)
    mesh.triangle(material, root, left, tip)
    mesh.triangle(material, root, tip, right)
    mesh.triangle(material, tip, left, root)
    mesh.triangle(material, right, tip, root)


def make_island() -> Mesh:
    rng = random.Random(2101)
    mesh = Mesh("TropicalIsland")
    bottom = ring(12, 17.0, 13.0, -10.0, rng, 0.16)
    waterline = ring(12, 24.0, 18.0, -1.2, rng, 0.14)
    sand = ring(12, 25.5, 19.5, 0.4, rng, 0.10)
    green = ring(12, 17.5, 12.5, 3.2, rng, 0.13)
    connect_rings(mesh, "rock_dark", bottom, waterline)
    connect_rings(mesh, "sand", waterline, sand)
    connect_rings(mesh, "grass", sand, green)
    cap_ring(mesh, "grass", green, (0.0, 4.8, 0.0))
    add_rock(mesh, (-6.5, 3.0, 1.5), 3.8, 8.0, "rock_light", rng)
    add_rock(mesh, (7.0, 2.2, -3.5), 2.6, 5.7, "rock_dark", rng)
    return mesh


def make_reef() -> Mesh:
    rng = random.Random(2102)
    mesh = Mesh("ReefCluster")
    add_rock(mesh, (0.0, -5.0, 0.0), 8.5, 16.0, "rock_dark", rng, 8)
    add_rock(mesh, (-7.0, -4.0, 3.0), 5.0, 10.0, "rock_light", rng)
    add_rock(mesh, (6.0, -4.0, 4.0), 4.4, 8.5, "rock_light", rng)
    add_rock(mesh, (3.0, -4.0, -5.0), 3.8, 7.0, "rock_dark", rng)
    return mesh


def make_coral() -> Mesh:
    rng = random.Random(2103)
    mesh = Mesh("CoralGarden")
    add_rock(mesh, (0.0, -1.8, 0.0), 5.5, 3.0, "rock_light", rng, 8)
    branches = [
        ((-2.2, 0.0, 0.4), (-2.0, 8.0, 0.2), "coral_orange", 0.75),
        ((-2.0, 4.5, 0.2), (-5.0, 8.5, 0.5), "coral_orange", 0.55),
        ((-2.0, 5.2, 0.2), (0.5, 10.0, -0.4), "coral_orange", 0.48),
        ((2.0, 0.0, 0.0), (2.4, 6.5, 0.5), "coral_pink", 0.72),
        ((2.3, 3.8, 0.4), (5.0, 7.4, 1.2), "coral_pink", 0.48),
        ((0.0, 0.0, -2.0), (0.4, 5.5, -2.4), "coral_gold", 0.65),
        ((0.3, 3.0, -2.3), (-2.0, 6.0, -3.4), "coral_gold", 0.42),
    ]
    for start, end, material, radius in branches:
        cylinder_between(mesh, start, end, radius, material)
    return mesh


def make_sea_stack() -> Mesh:
    rng = random.Random(2104)
    mesh = Mesh("SeaStack")
    add_rock(mesh, (0.0, -8.0, 0.0), 9.0, 34.0, "rock_dark", rng, 9)
    add_rock(mesh, (-5.0, -5.0, 3.5), 4.0, 14.0, "rock_light", rng, 7)
    return mesh


def make_palm_islet() -> Mesh:
    rng = random.Random(2105)
    mesh = Mesh("PalmIslet")
    lower = ring(10, 13.5, 10.0, -2.5, rng, 0.12)
    upper = ring(10, 16.0, 11.5, 0.0, rng, 0.10)
    inner = ring(10, 10.5, 7.0, 1.5, rng, 0.10)
    connect_rings(mesh, "rock_light", lower, upper)
    connect_rings(mesh, "sand", upper, inner)
    cap_ring(mesh, "sand", inner, (0.0, 2.0, 0.0))

    palms = [(-3.5, 1.7, 0.5, -0.9), (3.8, 1.5, -1.8, 0.7), (0.8, 1.8, 3.2, 0.15)]
    for px, py, pz, lean in palms:
        mid = (px + lean * 0.7, py + 4.8, pz)
        top = (px + lean * 1.7, py + 9.0, pz + lean * 0.35)
        cylinder_between(mesh, (px, py, pz), mid, 0.55, "wood_light", 7)
        cylinder_between(mesh, mid, top, 0.45, "wood_light", 7)
        for index in range(7):
            angle = math.tau * index / 7 + lean
            length = 5.0 + (index % 2) * 0.8
            tip = (top[0] + math.cos(angle) * length, top[1] - 1.0, top[2] + math.sin(angle) * length)
            add_leaf(mesh, top, tip, 1.25, "leaf_green")
    return mesh


def make_rock_arch() -> Mesh:
    rng = random.Random(2106)
    mesh = Mesh("RockArch")
    add_rock(mesh, (-8.0, -5.0, 0.0), 6.5, 25.0, "rock_dark", rng, 8)
    add_rock(mesh, (8.0, -5.0, 0.0), 6.5, 23.0, "rock_dark", rng, 8)
    cylinder_between(mesh, (-7.0, 15.0, 0.0), (7.0, 14.0, 0.0), 3.6, "rock_light", 8)
    add_rock(mesh, (-13.0, -5.0, 3.0), 3.5, 8.0, "rock_light", rng, 7)
    return mesh


def make_coral_fan() -> Mesh:
    rng = random.Random(2107)
    mesh = Mesh("CoralFan")
    add_rock(mesh, (0.0, -1.5, 0.0), 4.8, 2.8, "rock_light", rng, 8)
    root = (0.0, 0.0, 0.0)
    tips = [(-6.0, 8.0, 0.3), (-3.8, 10.5, 0.0), (-1.2, 12.0, 0.2),
            (1.5, 11.5, -0.2), (4.2, 10.0, 0.1), (6.2, 7.5, -0.1)]
    for index, tip in enumerate(tips):
        material = "coral_pink" if index % 2 == 0 else "coral_orange"
        elbow = (tip[0] * 0.46, tip[1] * 0.48, tip[2])
        cylinder_between(mesh, root, elbow, 0.42, material, 5)
        cylinder_between(mesh, elbow, tip, 0.28, material, 5)
        if index < len(tips) - 1:
            neighbor = tips[index + 1]
            bridge_start = (tip[0] * 0.62, tip[1] * 0.62, tip[2])
            bridge_end = (neighbor[0] * 0.62, neighbor[1] * 0.62, neighbor[2])
            cylinder_between(mesh, bridge_start, bridge_end, 0.18, "coral_gold", 5)
    return mesh


def make_shipwreck() -> Mesh:
    mesh = Mesh("Shipwreck")
    bow_top_l = (-10.0, 2.0, -1.0)
    bow_top_r = (-10.0, 2.0, 1.0)
    bow_bottom = (-8.0, -2.0, 0.0)
    stern_top_l = (8.0, 4.0, -4.0)
    stern_top_r = (8.0, 4.0, 4.0)
    stern_bottom_l = (7.0, -2.0, -2.0)
    stern_bottom_r = (7.0, -2.0, 2.0)
    mesh.quad("wood_dark", bow_top_l, stern_top_l, stern_bottom_l, bow_bottom)
    mesh.quad("wood_dark", bow_bottom, stern_bottom_r, stern_top_r, bow_top_r)
    mesh.quad("wood_light", bow_top_l, bow_top_r, stern_top_r, stern_top_l)
    mesh.quad("wood_dark", stern_bottom_l, stern_top_l, stern_top_r, stern_bottom_r)
    add_box(mesh, (0.0, 3.5, 0.0), (13.0, 0.8, 6.2), "wood_light")
    cylinder_between(mesh, (1.0, 3.8, 0.0), (1.8, 14.0, 0.0), 0.48, "wood_dark", 7)
    cylinder_between(mesh, (1.8, 11.0, 0.0), (6.0, 8.5, 0.0), 0.32, "wood_dark", 6)
    add_box(mesh, (-4.0, 4.8, -3.5), (5.0, 0.55, 0.5), "wood_light")
    add_box(mesh, (4.5, 5.0, 3.4), (4.0, 0.55, 0.5), "wood_light")
    return mesh


def main() -> None:
    TEX.mkdir(parents=True, exist_ok=True)
    for name, color in COLORS.items():
        write_solid_png(TEX / f"{name}.png", color)

    mtl = ["# Shared material palette for Stage01 ocean scenery"]
    for name in COLORS:
        mtl.extend([
            f"newmtl {name}",
            "Ka 1.000 1.000 1.000",
            "Kd 1.000 1.000 1.000",
            "Ks 0.080 0.080 0.080",
            "Ns 12.0",
            "d 1.0",
            "illum 2",
            f"map_Kd textures/{name}.png",
            "",
        ])
    (OUT / "ocean_palette.mtl").write_text("\n".join(mtl), encoding="utf-8")

    models = {
        "tropical_island.obj": make_island(),
        "reef_cluster.obj": make_reef(),
        "coral_garden.obj": make_coral(),
        "sea_stack.obj": make_sea_stack(),
        "palm_islet.obj": make_palm_islet(),
        "rock_arch.obj": make_rock_arch(),
        "coral_fan.obj": make_coral_fan(),
        "shipwreck.obj": make_shipwreck(),
    }
    for filename, model in models.items():
        output_path = OUT / filename
        model.write(output_path)
        lines = output_path.read_text(encoding="utf-8").splitlines()
        vertex_count = sum(line.startswith("v ") for line in lines)
        face_indices = [
            int(token.split("/")[0])
            for line in lines if line.startswith("f ")
            for token in line.split()[1:]
        ]
        assert face_indices and max(face_indices) <= vertex_count
        assert "nan" not in output_path.read_text(encoding="utf-8").lower()
        print(f"generated {filename}: {len(model.faces)} triangles (validated)")


if __name__ == "__main__":
    main()
