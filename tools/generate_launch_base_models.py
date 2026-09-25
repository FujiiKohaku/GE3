"""Generate the reusable low-poly model kit for the launch-base prototype."""

from __future__ import annotations

import math
import struct
import zlib
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "project" / "resources" / "Models" / "Environment" / "LaunchBase"
TEX = OUT / "textures"

COLORS = {
    "deck": (190, 202, 208),
    "deck_dark": (45, 58, 72),
    "navy": (31, 54, 82),
    "white": (225, 232, 234),
    "orange": (224, 126, 43),
    "cyan": (52, 211, 232),
    "glass": (51, 111, 133),
}


def write_png(path: Path, color: tuple[int, int, int]) -> None:
    size = 8
    raw = b"".join(b"\x00" + bytes((*color, 255)) * size for _ in range(size))

    def chunk(kind: bytes, data: bytes) -> bytes:
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data) & 0xFFFFFFFF)

    data = b"\x89PNG\r\n\x1a\n"
    data += chunk(b"IHDR", struct.pack(">IIBBBBB", size, size, 8, 6, 0, 0, 0))
    data += chunk(b"IDAT", zlib.compress(raw, 9))
    data += chunk(b"IEND", b"")
    path.write_bytes(data)


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
        base = len(self.vertices) + 1
        self.vertices.extend((a, b, c))
        self.normals.append((nx / length, ny / length, nz / length))
        self.faces.append((material, (base, base + 1, base + 2), len(self.normals)))

    def quad(self, material: str, a, b, c, d) -> None:
        self.triangle(material, a, b, c)
        self.triangle(material, a, c, d)

    def box(self, center, size, material: str) -> None:
        cx, cy, cz = center
        hx, hy, hz = (value * 0.5 for value in size)
        p = [
            (cx - hx, cy - hy, cz - hz), (cx + hx, cy - hy, cz - hz),
            (cx + hx, cy + hy, cz - hz), (cx - hx, cy + hy, cz - hz),
            (cx - hx, cy - hy, cz + hz), (cx + hx, cy - hy, cz + hz),
            (cx + hx, cy + hy, cz + hz), (cx - hx, cy + hy, cz + hz),
        ]
        # Counter-clockwise when viewed from outside. The engine reflects X
        # after Assimp flips winding, so source normals must be outward here.
        for a, b, c, d in ((0, 3, 2, 1), (4, 5, 6, 7), (0, 4, 7, 3),
                            (5, 1, 2, 6), (3, 7, 6, 2), (0, 1, 5, 4)):
            self.quad(material, p[a], p[b], p[c], p[d])

    def extrude_xy(self, points, z_min: float, z_max: float, material: str) -> None:
        """Extrude a counter-clockwise XY silhouette along Z."""
        front = [(x, y, z_max) for x, y in points]
        back = [(x, y, z_min) for x, y in points]
        for index in range(1, len(points) - 1):
            self.triangle(material, front[0], front[index], front[index + 1])
            self.triangle(material, back[0], back[index + 1], back[index])
        for index in range(len(points)):
            nxt = (index + 1) % len(points)
            self.quad(material, back[index], back[nxt], front[nxt], front[index])

    def panel(self, top, thickness: float, material: str) -> None:
        """Create a thin four-corner panel; top points face outwards."""
        bottom = [(x, y - thickness, z) for x, y, z in top]
        self.quad(material, top[0], top[1], top[2], top[3])
        self.quad(material, bottom[3], bottom[2], bottom[1], bottom[0])
        for index in range(4):
            nxt = (index + 1) % 4
            self.quad(material, bottom[index], bottom[nxt], top[nxt], top[index])

    def cylinder(self, radius: float, height: float, material: str, sides: int = 16, y: float = 0.0) -> None:
        lower = []
        upper = []
        for index in range(sides):
            angle = math.tau * index / sides
            x, z = math.cos(angle) * radius, math.sin(angle) * radius
            lower.append((x, y - height * 0.5, z))
            upper.append((x, y + height * 0.5, z))
        for index in range(sides):
            nxt = (index + 1) % sides
            self.quad(material, lower[index], upper[index], upper[nxt], lower[nxt])
            self.triangle(material, (0.0, y + height * 0.5, 0.0), upper[nxt], upper[index])
            self.triangle(material, (0.0, y - height * 0.5, 0.0), lower[index], lower[nxt])

    def beam_between(self, start, end, width: float, material: str) -> None:
        sx, sy, sz = start
        ex, ey, ez = end
        dx, dy, dz = ex - sx, ey - sy, ez - sz
        length = math.sqrt(dx * dx + dy * dy + dz * dz)
        direction = (dx / length, dy / length, dz / length)
        helper = (0.0, 1.0, 0.0) if abs(direction[1]) < 0.9 else (1.0, 0.0, 0.0)
        ux = direction[1] * helper[2] - direction[2] * helper[1]
        uy = direction[2] * helper[0] - direction[0] * helper[2]
        uz = direction[0] * helper[1] - direction[1] * helper[0]
        u_length = math.sqrt(ux * ux + uy * uy + uz * uz)
        u = (ux / u_length, uy / u_length, uz / u_length)
        v = (
            u[1] * direction[2] - u[2] * direction[1],
            u[2] * direction[0] - u[0] * direction[2],
            u[0] * direction[1] - u[1] * direction[0],
        )
        half = width * 0.5
        corners = ((-half, -half), (half, -half), (half, half), (-half, half))
        start_ring = [
            (sx + u[0] * a + v[0] * b, sy + u[1] * a + v[1] * b, sz + u[2] * a + v[2] * b)
            for a, b in corners
        ]
        end_ring = [
            (ex + u[0] * a + v[0] * b, ey + u[1] * a + v[1] * b, ez + u[2] * a + v[2] * b)
            for a, b in corners
        ]
        for index in range(4):
            nxt = (index + 1) % 4
            self.quad(material, start_ring[index], end_ring[index], end_ring[nxt], start_ring[nxt])
        self.quad(material, start_ring[0], start_ring[1], start_ring[2], start_ring[3])
        self.quad(material, end_ring[3], end_ring[2], end_ring[1], end_ring[0])

    def write(self, filename: str) -> None:
        lines = ["# Procedurally generated launch-base model", "mtllib launch_base.mtl", f"o {self.name}"]
        lines.extend(f"v {x:.6f} {y:.6f} {z:.6f}" for x, y, z in self.vertices)
        lines.extend(f"vn {x:.6f} {y:.6f} {z:.6f}" for x, y, z in self.normals)
        active = None
        for material, indices, normal_index in self.faces:
            if material != active:
                lines.append(f"usemtl {material}")
                active = material
            a, b, c = indices
            lines.append(f"f {a}//{normal_index} {b}//{normal_index} {c}//{normal_index}")
        path = OUT / filename
        path.write_text("\n".join(lines) + "\n", encoding="utf-8")
        assert max(int(token.split("/")[0]) for line in lines if line.startswith("f ") for token in line.split()[1:]) <= len(self.vertices)
        print(f"generated {filename}: {len(self.faces)} triangles")


def make_deck() -> Mesh:
    mesh = Mesh("DeckTile")
    mesh.box((0.0, 0.0, 0.0), (20.0, 1.0, 20.0), "deck")
    mesh.box((0.0, 0.55, 0.0), (18.5, 0.15, 18.5), "deck_dark")
    return mesh


def make_runway() -> Mesh:
    mesh = Mesh("RunwaySegment")
    mesh.box((0.0, 0.0, 0.0), (20.0, 0.8, 40.0), "deck_dark")
    mesh.box((0.0, 0.46, 0.0), (0.55, 0.10, 8.0), "white")
    mesh.box((-9.1, 0.47, 0.0), (0.28, 0.12, 39.0), "orange")
    mesh.box((9.1, 0.47, 0.0), (0.28, 0.12, 39.0), "orange")
    return mesh


def make_hangar_frame() -> Mesh:
    mesh = Mesh("HangarFrame")

    # Enclosed service blocks establish the weight of the building while
    # preserving a 38-unit-wide central aircraft opening.
    for side in (-1.0, 1.0):
        mesh.box((side * 26.0, 8.5, 0.0), (14.0, 17.0, 40.0), "white")
        mesh.box((side * 26.0, 2.1, 0.0), (14.6, 4.2, 40.8), "navy")

        # Side wall panel divisions remain large enough to read in game.
        for z in (-14.0, -7.0, 0.0, 7.0, 14.0):
            mesh.box((side * 33.15, 9.2, z), (0.32, 12.5, 0.42), "deck_dark")
        for y in (7.0, 9.1, 11.2):
            mesh.box((side * 33.25, y, -8.5), (0.36, 0.75, 5.0), "deck_dark")

    # The back wall and ceiling structure give the hangar visible depth.
    mesh.box((0.0, 9.0, -20.0), (38.0, 18.0, 1.0), "deck_dark")
    mesh.box((0.0, 3.0, -19.35), (27.0, 5.0, 0.35), "navy")
    for x in (-15.0, -7.5, 0.0, 7.5, 15.0):
        mesh.box((x, 10.5, -19.35), (0.35, 10.0, 0.35), "navy")

    # Front pylons are actual wall masses; the dark braces sit against them.
    entrance_z_min, entrance_z_max = 17.0, 22.0
    left_pylon = [(-33.0, 0.0), (-19.0, 0.0), (-19.0, 17.5), (-25.0, 17.5)]
    right_pylon = [(19.0, 0.0), (33.0, 0.0), (25.0, 17.5), (19.0, 17.5)]
    mesh.extrude_xy(left_pylon, entrance_z_min, entrance_z_max, "white")
    mesh.extrude_xy(right_pylon, entrance_z_min, entrance_z_max, "white")
    mesh.beam_between((-30.2, 1.0, 22.1), (-24.2, 18.0, 22.1), 1.25, "navy")
    mesh.beam_between((30.2, 1.0, 22.1), (24.2, 18.0, 22.1), 1.25, "navy")

    # The entrance header follows the roof line. Its upper edge overlaps the
    # roof underside so no daylight gap can appear at the joint.
    mesh.beam_between((-24.2, 18.0, 22.1), (-7.0, 23.0, 22.1), 1.4, "navy")
    mesh.beam_between((24.2, 18.0, 22.1), (7.0, 23.0, 22.1), 1.4, "navy")
    mesh.box((0.0, 23.0, 22.1), (14.0, 1.4, 1.4), "navy")

    # Three thin, connected roof panels reproduce the layered wing shape.
    # Each panel bottom overlaps the header or side-wall cap by at least 0.5.
    mesh.panel([
        (-7.0, 24.0, 24.0), (-7.0, 24.0, -24.0),
        (-34.0, 20.8, -21.0), (-38.0, 20.3, 21.0),
    ], 1.15, "white")
    mesh.panel([
        (7.0, 24.0, 24.0), (38.0, 20.3, 21.0),
        (34.0, 20.8, -21.0), (7.0, 24.0, -24.0),
    ], 1.15, "white")
    mesh.panel([
        (-8.0, 24.35, 24.0), (8.0, 24.35, 24.0),
        (8.0, 24.35, -24.0), (-8.0, 24.35, -24.0),
    ], 1.15, "white")

    # A thin forward canopy hides the structural joint instead of becoming a
    # second massive roof slab.
    mesh.panel([
        (-37.0, 20.35, 24.5), (37.0, 20.35, 24.5),
        (34.0, 20.65, 20.8), (-34.0, 20.65, 20.8),
    ], 0.55, "navy")

    # Interior roof trusses connect the two side blocks beneath the shell.
    for z in (-14.0, -5.0, 5.0, 14.0):
        mesh.beam_between((-19.0, 17.2, z), (-6.5, 22.4, z), 0.55, "navy")
        mesh.beam_between((19.0, 17.2, z), (6.5, 22.4, z), 0.55, "navy")
        mesh.box((0.0, 22.4, z), (13.0, 0.55, 0.55), "navy")

    # Integrated control rooms touch the service blocks instead of floating.
    for side in (-1.0, 1.0):
        x = side * 37.5
        mesh.box((x, 7.0, 1.0), (9.0, 14.0, 25.0), "white")
        mesh.box((x, 12.2, 6.5), (9.6, 3.8, 11.0), "glass")
        mesh.box((x, 14.35, 6.5), (10.5, 0.7, 12.0), "navy")
        mesh.box((x, 5.8, -6.5), (9.2, 4.0, 5.0), "navy")

    # Central roof pod and restrained navigation/safety accents.
    mesh.extrude_xy(
        [(-5.5, 24.0), (5.5, 24.0), (4.0, 28.3), (-4.0, 28.3)],
        -8.0,
        4.0,
        "navy",
    )
    mesh.box((0.0, 26.0, 4.15), (7.2, 0.45, 0.35), "cyan")
    mesh.box((0.0, 29.4, -2.0), (0.7, 2.2, 0.7), "orange")
    for x in (-25.0, -12.5, 0.0, 12.5, 25.0):
        mesh.box((x, 19.65 if abs(x) > 20.0 else 22.0, 24.85), (2.2, 0.24, 0.28), "cyan")
    mesh.box((-29.0, 1.0, 22.55), (5.5, 0.28, 0.38), "orange")
    mesh.box((29.0, 1.0, 22.55), (5.5, 0.28, 0.38), "orange")
    return mesh


def make_launch_pad() -> Mesh:
    mesh = Mesh("LaunchPad")
    mesh.cylinder(14.0, 1.0, "deck_dark", 24)
    mesh.cylinder(11.8, 1.12, "deck", 24)
    mesh.cylinder(9.8, 1.22, "navy", 24)
    return mesh


def make_control_tower() -> Mesh:
    mesh = Mesh("ControlTower")
    mesh.box((0.0, 8.0, 0.0), (7.0, 16.0, 7.0), "white")
    mesh.box((0.0, 16.5, 0.0), (11.0, 4.0, 10.0), "glass")
    mesh.box((0.0, 19.0, 0.0), (12.0, 1.0, 11.0), "navy")
    mesh.box((0.0, 22.0, 0.0), (0.7, 6.0, 0.7), "orange")
    return mesh


def make_container() -> Mesh:
    mesh = Mesh("CargoContainer")
    mesh.box((0.0, 1.5, 0.0), (4.0, 3.0, 7.0), "navy")
    for x in (-1.65, 1.65):
        mesh.box((x, 1.5, 0.0), (0.18, 2.7, 6.7), "orange")
    return mesh


def make_light() -> Mesh:
    mesh = Mesh("RunwayLight")
    mesh.box((0.0, 0.3, 0.0), (1.0, 0.6, 1.0), "deck_dark")
    mesh.box((0.0, 1.2, 0.0), (0.35, 1.2, 0.35), "white")
    mesh.box((0.0, 1.95, 0.0), (0.75, 0.4, 0.75), "cyan")
    return mesh


def make_rail() -> Mesh:
    mesh = Mesh("SafetyRail")
    for x in (-4.5, 0.0, 4.5):
        mesh.box((x, 1.0, 0.0), (0.22, 2.0, 0.22), "orange")
    mesh.box((0.0, 1.7, 0.0), (9.5, 0.22, 0.22), "orange")
    mesh.box((0.0, 0.9, 0.0), (9.5, 0.16, 0.16), "white")
    return mesh


def make_support_pylon() -> Mesh:
    mesh = Mesh("SupportPylon")
    mesh.cylinder(1.75, 15.0, "white", 12, y=-7.5)
    mesh.cylinder(2.30, 1.3, "deck_dark", 12, y=-0.65)
    mesh.cylinder(2.85, 2.2, "deck_dark", 12, y=-8.0)
    mesh.cylinder(2.45, 0.55, "orange", 12, y=-6.9)
    return mesh


def make_underdeck_beam() -> Mesh:
    mesh = Mesh("UnderdeckBeam")
    mesh.box((0.0, -1.15, 0.0), (2.2, 2.0, 38.0), "navy")
    mesh.box((0.0, -2.25, 0.0), (2.8, 0.35, 38.0), "deck_dark")
    return mesh


def make_cross_brace() -> Mesh:
    mesh = Mesh("CrossBrace")
    mesh.beam_between((-7.0, -7.0, 0.0), (7.0, -1.5, 0.0), 0.75, "navy")
    mesh.beam_between((-7.0, -1.5, 0.0), (7.0, -7.0, 0.0), 0.75, "navy")
    mesh.box((0.0, -4.25, 0.0), (1.8, 1.8, 0.6), "deck_dark")
    return mesh


def make_catwalk() -> Mesh:
    mesh = Mesh("MaintenanceCatwalk")
    mesh.box((0.0, 0.0, 0.0), (2.6, 0.3, 18.0), "orange")
    for z in (-8.5, -4.25, 0.0, 4.25, 8.5):
        mesh.box((-1.15, 0.85, z), (0.16, 1.7, 0.16), "orange")
        mesh.box((1.15, 0.85, z), (0.16, 1.7, 0.16), "orange")
    mesh.box((-1.15, 1.55, 0.0), (0.16, 0.16, 18.0), "orange")
    mesh.box((1.15, 1.55, 0.0), (0.16, 0.16, 18.0), "orange")
    return mesh


def make_ladder() -> Mesh:
    mesh = Mesh("ServiceLadder")
    mesh.box((-0.48, -3.5, 0.0), (0.15, 7.0, 0.18), "orange")
    mesh.box((0.48, -3.5, 0.0), (0.15, 7.0, 0.18), "orange")
    for index in range(9):
        mesh.box((0.0, -0.45 - index * 0.75, 0.0), (1.1, 0.12, 0.18), "orange")
    return mesh


def make_pipe_module() -> Mesh:
    mesh = Mesh("PipeModule")
    mesh.beam_between((0.0, 0.0, -9.0), (0.0, 0.0, 9.0), 0.45, "white")
    mesh.beam_between((0.8, 0.0, -9.0), (0.8, 0.0, 9.0), 0.32, "cyan")
    for z in (-8.0, 0.0, 8.0):
        mesh.box((0.4, 0.0, z), (1.8, 1.0, 0.22), "deck_dark")
    return mesh


def make_fuel_station() -> Mesh:
    mesh = Mesh("FuelStation")
    mesh.cylinder(2.6, 6.2, "navy", 16, y=3.1)
    mesh.cylinder(2.72, 0.45, "orange", 16, y=2.1)
    mesh.cylinder(2.72, 0.45, "orange", 16, y=4.6)
    mesh.box((4.2, 2.0, 0.0), (2.6, 4.0, 2.3), "white")
    mesh.box((4.2, 2.7, 1.2), (1.5, 0.65, 0.22), "cyan")
    mesh.beam_between((2.2, 3.4, 0.0), (3.2, 3.0, 0.0), 0.32, "deck_dark")
    mesh.beam_between((5.1, 1.4, 0.0), (6.4, 0.8, 1.6), 0.28, "deck_dark")
    mesh.beam_between((6.4, 0.8, 1.6), (7.4, 0.5, 2.8), 0.28, "deck_dark")
    mesh.box((0.0, 6.5, 0.0), (1.2, 0.7, 1.2), "white")
    return mesh


def make_tool_crate() -> Mesh:
    mesh = Mesh("ToolCrate")
    mesh.box((0.0, 1.15, 0.0), (3.4, 2.3, 2.6), "deck_dark")
    mesh.box((0.0, 2.4, 0.0), (3.6, 0.25, 2.8), "navy")
    for x in (-1.45, 1.45):
        mesh.box((x, 1.2, 1.34), (0.18, 1.8, 0.18), "orange")
    mesh.box((0.0, 1.4, 1.35), (1.0, 0.32, 0.2), "cyan")
    return mesh


def make_guidance_sign() -> Mesh:
    mesh = Mesh("GuidanceSign")
    mesh.box((0.0, 0.25, 0.0), (3.4, 0.5, 1.7), "deck_dark")
    mesh.box((-1.1, 1.9, 0.0), (0.25, 3.3, 0.25), "white")
    mesh.box((1.1, 1.9, 0.0), (0.25, 3.3, 0.25), "white")
    mesh.box((0.0, 3.35, 0.0), (4.2, 1.7, 0.45), "navy")
    mesh.box((0.0, 3.55, 0.25), (2.9, 0.28, 0.16), "cyan")
    mesh.box((0.0, 3.05, 0.25), (1.7, 0.22, 0.16), "orange")
    return mesh


def make_fire_station() -> Mesh:
    mesh = Mesh("FireStation")
    mesh.box((0.0, 1.7, 0.0), (2.4, 3.4, 1.4), "orange")
    mesh.box((0.0, 1.8, 0.76), (1.75, 2.5, 0.16), "white")
    mesh.cylinder(0.55, 1.9, "deck_dark", 12, y=1.45)
    mesh.box((0.0, 3.65, 0.0), (0.75, 0.55, 0.75), "cyan")
    return mesh


def make_windsock() -> Mesh:
    mesh = Mesh("Windsock")
    mesh.box((0.0, 5.0, 0.0), (0.35, 10.0, 0.35), "white")
    mesh.box((1.8, 9.2, 0.0), (3.8, 0.25, 0.25), "navy")
    # Tapered fabric silhouette, kept chunky enough for the game camera.
    mesh.extrude_xy([(3.5, 8.75), (7.8, 8.55), (7.8, 9.15), (3.5, 9.65)], -0.35, 0.35, "orange")
    mesh.box((0.0, 10.25, 0.0), (0.7, 0.5, 0.7), "cyan")
    return mesh


def main() -> None:
    TEX.mkdir(parents=True, exist_ok=True)
    for name, color in COLORS.items():
        write_png(TEX / f"{name}.png", color)
    material_lines = ["# Launch-base palette"]
    for name in COLORS:
        material_lines += [f"newmtl {name}", "Ka 1 1 1", "Kd 1 1 1", "Ks 0.08 0.08 0.08", "Ns 12", "d 1", "illum 2", f"map_Kd textures/{name}.png", ""]
    (OUT / "launch_base.mtl").write_text("\n".join(material_lines), encoding="utf-8")

    for filename, model in {
        "deck_tile.obj": make_deck(),
        "runway_segment.obj": make_runway(),
        "hangar_frame.obj": make_hangar_frame(),
        "launch_pad.obj": make_launch_pad(),
        "control_tower.obj": make_control_tower(),
        "cargo_container.obj": make_container(),
        "runway_light.obj": make_light(),
        "safety_rail.obj": make_rail(),
        "support_pylon.obj": make_support_pylon(),
        "underdeck_beam.obj": make_underdeck_beam(),
        "cross_brace.obj": make_cross_brace(),
        "maintenance_catwalk.obj": make_catwalk(),
        "service_ladder.obj": make_ladder(),
        "pipe_module.obj": make_pipe_module(),
        "fuel_station.obj": make_fuel_station(),
        "tool_crate.obj": make_tool_crate(),
        "guidance_sign.obj": make_guidance_sign(),
        "fire_station.obj": make_fire_station(),
        "windsock.obj": make_windsock(),
    }.items():
        model.write(filename)


if __name__ == "__main__":
    main()
