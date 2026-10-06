import math
from pathlib import Path


OUTPUT_PATH = Path(__file__).with_name("ice_boss.obj")


def add(a, b):
    return (a[0] + b[0], a[1] + b[1], a[2] + b[2])


def subtract(a, b):
    return (a[0] - b[0], a[1] - b[1], a[2] - b[2])


def multiply(v, scalar):
    return (v[0] * scalar, v[1] * scalar, v[2] * scalar)


def dot(a, b):
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]


def cross(a, b):
    return (
        a[1] * b[2] - a[2] * b[1],
        a[2] * b[0] - a[0] * b[2],
        a[0] * b[1] - a[1] * b[0],
    )


def normalize(v):
    length = math.sqrt(dot(v, v))
    if length < 0.000001:
        return (0.0, 1.0, 0.0)
    return (v[0] / length, v[1] / length, v[2] / length)


def rotate_xyz(point, rotation):
    x, y, z = point
    rx, ry, rz = rotation

    cos_x = math.cos(rx)
    sin_x = math.sin(rx)
    y, z = y * cos_x - z * sin_x, y * sin_x + z * cos_x

    cos_y = math.cos(ry)
    sin_y = math.sin(ry)
    x, z = x * cos_y + z * sin_y, -x * sin_y + z * cos_y

    cos_z = math.cos(rz)
    sin_z = math.sin(rz)
    x, y = x * cos_z - y * sin_z, x * sin_z + y * cos_z
    return (x, y, z)


class ObjBuilder:
    def __init__(self):
        self.vertices = []
        self.normals = []
        self.faces = []
        self.current_group = "IceBoss"
        self.current_material = "BlackIron"

    def set_surface(self, group, material):
        self.current_group = group
        self.current_material = material

    def triangle(self, a, b, c):
        normal = normalize(cross(subtract(b, a), subtract(c, a)))
        indices = []
        for point in (a, b, c):
            self.vertices.append(point)
            self.normals.append(normal)
            indices.append(len(self.vertices))
        self.faces.append((self.current_group, self.current_material, indices))

    def quad(self, a, b, c, d):
        self.triangle(a, b, c)
        self.triangle(a, c, d)

    def box(self, center, size, rotation=(0.0, 0.0, 0.0)):
        hx = size[0] * 0.5
        hy = size[1] * 0.5
        hz = size[2] * 0.5
        local = [
            (-hx, -hy, -hz), (hx, -hy, -hz), (hx, hy, -hz), (-hx, hy, -hz),
            (-hx, -hy, hz), (hx, -hy, hz), (hx, hy, hz), (-hx, hy, hz),
        ]
        points = []
        for value in local:
            points.append(add(rotate_xyz(value, rotation), center))
        sides = [
            (0, 3, 2, 1), (4, 5, 6, 7), (0, 1, 5, 4),
            (3, 7, 6, 2), (0, 4, 7, 3), (1, 2, 6, 5),
        ]
        for side in sides:
            self.quad(points[side[0]], points[side[1]], points[side[2]], points[side[3]])

    def frustum(self, center, bottom, top, height, depth_bottom, depth_top):
        y0 = center[1] - height * 0.5
        y1 = center[1] + height * 0.5
        x0 = bottom * 0.5
        x1 = top * 0.5
        z0 = depth_bottom * 0.5
        z1 = depth_top * 0.5
        points = [
            add(center, (-x0, y0 - center[1], -z0)), add(center, (x0, y0 - center[1], -z0)),
            add(center, (x0, y0 - center[1], z0)), add(center, (-x0, y0 - center[1], z0)),
            add(center, (-x1, y1 - center[1], -z1)), add(center, (x1, y1 - center[1], -z1)),
            add(center, (x1, y1 - center[1], z1)), add(center, (-x1, y1 - center[1], z1)),
        ]
        self.quad(points[0], points[1], points[2], points[3])
        self.quad(points[4], points[7], points[6], points[5])
        self.quad(points[0], points[4], points[5], points[1])
        self.quad(points[1], points[5], points[6], points[2])
        self.quad(points[2], points[6], points[7], points[3])
        self.quad(points[3], points[7], points[4], points[0])

    def prism_between(self, start, end, radius_start, radius_end, sides=8):
        axis = normalize(subtract(end, start))
        reference = (0.0, 1.0, 0.0)
        if abs(dot(axis, reference)) > 0.92:
            reference = (1.0, 0.0, 0.0)
        side_a = normalize(cross(axis, reference))
        side_b = normalize(cross(axis, side_a))
        ring_start = []
        ring_end = []
        for index in range(sides):
            angle = math.tau * index / sides
            direction = add(multiply(side_a, math.cos(angle)), multiply(side_b, math.sin(angle)))
            ring_start.append(add(start, multiply(direction, radius_start)))
            ring_end.append(add(end, multiply(direction, radius_end)))
        for index in range(sides):
            next_index = (index + 1) % sides
            self.quad(ring_start[index], ring_end[index], ring_end[next_index], ring_start[next_index])
            self.triangle(start, ring_start[next_index], ring_start[index])
            self.triangle(end, ring_end[index], ring_end[next_index])

    def crystal(self, base, tip, radius, sides=6):
        axis = normalize(subtract(tip, base))
        shoulder = add(base, multiply(subtract(tip, base), 0.68))
        reference = (0.0, 1.0, 0.0)
        if abs(dot(axis, reference)) > 0.92:
            reference = (1.0, 0.0, 0.0)
        side_a = normalize(cross(axis, reference))
        side_b = normalize(cross(axis, side_a))
        bottom_ring = []
        upper_ring = []
        for index in range(sides):
            angle = math.tau * index / sides
            direction = add(multiply(side_a, math.cos(angle)), multiply(side_b, math.sin(angle)))
            bottom_ring.append(add(base, multiply(direction, radius)))
            upper_ring.append(add(shoulder, multiply(direction, radius * 0.62)))
        for index in range(sides):
            next_index = (index + 1) % sides
            self.quad(bottom_ring[index], upper_ring[index], upper_ring[next_index], bottom_ring[next_index])
            self.triangle(upper_ring[index], tip, upper_ring[next_index])

    def blade(self, start, end, width, thickness):
        axis = normalize(subtract(end, start))
        side = normalize(cross(axis, (0.0, 0.0, 1.0)))
        if abs(dot(side, side)) < 0.5:
            side = (1.0, 0.0, 0.0)
        front = (0.0, 0.0, thickness * 0.5)
        back = (0.0, 0.0, -thickness * 0.5)
        neck = add(start, multiply(subtract(end, start), 0.82))
        half_side = multiply(side, width * 0.5)
        tip_side = multiply(side, width * 0.18)
        outline = [
            add(start, half_side), subtract(start, half_side), subtract(neck, tip_side),
            end, add(neck, tip_side),
        ]
        front_points = []
        back_points = []
        for point in outline:
            front_points.append(add(point, front))
            back_points.append(add(point, back))
        for index in range(1, len(outline) - 1):
            self.triangle(front_points[0], front_points[index], front_points[index + 1])
            self.triangle(back_points[0], back_points[index + 1], back_points[index])
        for index in range(len(outline)):
            next_index = (index + 1) % len(outline)
            self.quad(front_points[index], back_points[index], back_points[next_index], front_points[next_index])

    def write(self, path):
        lines = ["# Ice Burial Knight Glacia - procedural low-poly concept", "mtllib ice_boss.mtl", "s off", "vt 0.5 0.5"]
        for vertex in self.vertices:
            lines.append("v %.6f %.6f %.6f" % vertex)
        for normal in self.normals:
            lines.append("vn %.6f %.6f %.6f" % normal)
        active_group = ""
        active_material = ""
        for group, material, indices in self.faces:
            if group != active_group:
                lines.append("g " + group)
                active_group = group
            if material != active_material:
                lines.append("usemtl " + material)
                active_material = material
            values = []
            for index in indices:
                values.append("%d/1/%d" % (index, index))
            lines.append("f " + " ".join(values))
        path.write_text("\n".join(lines) + "\n", encoding="ascii")


def build_knight():
    mesh = ObjBuilder()

    mesh.set_surface("InnerFrame", "BlackIron")
    mesh.box((-0.43, 0.28, 0.08), (0.68, 0.52, 1.02), (0.0, 0.0, -0.08))
    mesh.box((0.43, 0.28, 0.08), (0.68, 0.52, 1.02), (0.0, 0.0, 0.08))
    mesh.prism_between((-0.42, 0.48, 0.0), (-0.37, 1.62, 0.0), 0.25, 0.31, 7)
    mesh.prism_between((0.42, 0.48, 0.0), (0.37, 1.62, 0.0), 0.25, 0.31, 7)
    mesh.prism_between((-0.37, 1.58, 0.0), (-0.49, 2.62, 0.0), 0.30, 0.35, 7)
    mesh.prism_between((0.37, 1.58, 0.0), (0.49, 2.62, 0.0), 0.30, 0.35, 7)
    mesh.box((0.0, 2.62, 0.0), (1.2, 0.55, 0.65))
    mesh.frustum((0.0, 3.5, 0.0), 1.05, 1.72, 1.5, 0.62, 0.78)
    mesh.prism_between((-0.78, 4.0, 0.0), (-1.22, 3.18, 0.02), 0.28, 0.23, 7)
    mesh.prism_between((0.78, 4.0, 0.0), (1.30, 3.24, 0.02), 0.28, 0.23, 7)
    mesh.prism_between((-1.22, 3.18, 0.02), (-1.36, 2.35, 0.12), 0.23, 0.18, 7)
    mesh.prism_between((1.30, 3.24, 0.02), (1.42, 2.42, 0.10), 0.23, 0.18, 7)
    mesh.box((-1.38, 2.22, 0.14), (0.34, 0.38, 0.34), (0.0, 0.0, -0.12))
    mesh.box((1.44, 2.28, 0.12), (0.34, 0.38, 0.34), (0.0, 0.0, 0.12))

    mesh.set_surface("Helmet", "BlackIron")
    mesh.prism_between((0.0, 4.17, 0.0), (0.0, 4.92, 0.0), 0.40, 0.30, 8)
    mesh.box((0.0, 4.56, 0.34), (0.62, 0.12, 0.07), (0.0, 0.0, 0.0))
    mesh.set_surface("VisorGlow", "CyanGlow")
    mesh.box((0.0, 4.57, 0.395), (0.43, 0.055, 0.025))

    mesh.set_surface("Crown", "DeepIce")
    crown_bases = [(-0.30, 4.76, 0.0), (-0.14, 4.86, 0.0), (0.14, 4.86, 0.0), (0.30, 4.76, 0.0)]
    crown_tips = [(-0.56, 5.48, -0.02), (-0.20, 5.66, 0.0), (0.20, 5.66, 0.0), (0.56, 5.48, -0.02)]
    for index in range(len(crown_bases)):
        mesh.crystal(crown_bases[index], crown_tips[index], 0.11, 5)

    mesh.set_surface("GlacierArmor", "GlacierIce")
    mesh.frustum((0.0, 3.55, 0.27), 1.18, 1.98, 1.18, 0.56, 0.72)
    mesh.crystal((-0.72, 4.04, 0.0), (-1.95, 4.47, -0.05), 0.43, 6)
    mesh.crystal((0.72, 4.04, 0.0), (2.10, 4.62, -0.10), 0.48, 6)
    mesh.crystal((0.94, 4.08, 0.05), (1.52, 5.02, -0.06), 0.30, 6)
    mesh.crystal((1.10, 3.72, 0.04), (1.96, 3.82, 0.02), 0.25, 6)
    mesh.box((-0.39, 1.62, 0.06), (0.72, 0.76, 0.73), (0.02, 0.0, -0.08))
    mesh.box((0.39, 1.62, 0.06), (0.72, 0.76, 0.73), (-0.02, 0.0, 0.08))
    mesh.crystal((-0.40, 1.12, 0.02), (-0.55, 1.72, 0.55), 0.25, 5)
    mesh.crystal((0.40, 1.12, 0.02), (0.58, 1.66, 0.55), 0.25, 5)
    mesh.crystal((-1.28, 3.12, 0.02), (-1.65, 2.48, 0.26), 0.24, 5)
    mesh.crystal((1.34, 3.18, 0.02), (1.76, 2.57, 0.22), 0.24, 5)

    mesh.set_surface("ArmorCracks", "CyanGlow")
    mesh.box((-0.22, 3.58, 0.655), (0.055, 0.82, 0.025), (0.0, 0.0, -0.30))
    mesh.box((0.28, 3.38, 0.655), (0.05, 0.62, 0.025), (0.0, 0.0, 0.40))
    mesh.box((-0.42, 1.62, 0.44), (0.045, 0.50, 0.02), (0.0, 0.0, -0.20))

    mesh.set_surface("TatteredCoat", "FrostCloth")
    mesh.blade((-0.48, 2.63, -0.18), (-0.72, 0.65, -0.28), 0.82, 0.035)
    mesh.blade((0.12, 2.65, -0.20), (0.02, 0.82, -0.34), 0.72, 0.035)
    mesh.blade((0.57, 2.58, -0.16), (0.88, 1.05, -0.27), 0.58, 0.035)

    mesh.set_surface("GreatswordGrip", "BlackIron")
    grip_top = (-1.37, 2.45, 0.16)
    grip_bottom = (-1.72, 1.82, 0.17)
    mesh.prism_between(grip_top, grip_bottom, 0.09, 0.09, 8)
    mesh.prism_between((-1.29, 2.28, 0.16), (-1.78, 2.00, 0.17), 0.10, 0.10, 6)

    mesh.set_surface("Greatsword", "DeepIce")
    mesh.blade(grip_bottom, (-2.78, -0.12, 0.18), 0.72, 0.19)
    mesh.set_surface("SwordCore", "CyanGlow")
    mesh.prism_between((-1.82, 1.64, 0.285), (-2.64, 0.12, 0.295), 0.035, 0.02, 5)

    return mesh


def main():
    knight = build_knight()
    knight.write(OUTPUT_PATH)
    print("Wrote", OUTPUT_PATH)
    print("Vertices", len(knight.vertices))
    print("Triangles", len(knight.faces))


if __name__ == "__main__":
    main()
