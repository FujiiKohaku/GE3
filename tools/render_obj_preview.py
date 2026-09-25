"""Render a quick shaded OBJ preview without external 3D dependencies."""

from __future__ import annotations

import argparse
import math
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw


COLORS = {
    "white": (222, 231, 238),
    "navy": (40, 63, 91),
    "deck_dark": (58, 70, 84),
    "glass": (55, 123, 151),
    "cyan": (56, 218, 235),
    "orange": (228, 132, 46),
    "deck": (180, 194, 202),
}


def normalize(value: np.ndarray) -> np.ndarray:
    length = np.linalg.norm(value)
    return value / length if length > 1.0e-6 else value


def load_obj(path: Path):
    vertices: list[tuple[float, float, float]] = []
    faces: list[tuple[str, tuple[int, int, int]]] = []
    material = "white"
    for line in path.read_text(encoding="utf-8").splitlines():
        values = line.split()
        if not values:
            continue
        if values[0] == "v":
            vertices.append(tuple(float(value) for value in values[1:4]))
        elif values[0] == "usemtl":
            material = values[1]
        elif values[0] == "f":
            indices = tuple(int(value.split("/")[0]) - 1 for value in values[1:4])
            faces.append((material, indices))
    return np.asarray(vertices, dtype=np.float64), faces


def render(obj_path: Path, output_path: Path) -> None:
    width, height = 1400, 900
    image = Image.new("RGB", (width, height), (190, 224, 242))
    draw = ImageDraw.Draw(image)
    draw.rectangle((0, height * 0.72, width, height), fill=(62, 151, 188))

    vertices, faces = load_obj(obj_path)
    eye = np.array((0.0, 22.0, 105.0))
    target = np.array((0.0, 12.0, 0.0))
    forward = normalize(target - eye)
    right = normalize(np.cross(forward, np.array((0.0, 1.0, 0.0))))
    up = np.cross(right, forward)
    focal = width / (2.0 * math.tan(math.radians(47.0) * 0.5))

    camera_vertices = []
    screen_vertices = []
    for vertex in vertices:
        relative = vertex - eye
        camera = np.array((np.dot(relative, right), np.dot(relative, up), np.dot(relative, forward)))
        camera_vertices.append(camera)
        depth = max(camera[2], 0.1)
        screen_vertices.append((width * 0.5 + camera[0] * focal / depth,
                                height * 0.55 - camera[1] * focal / depth))

    light = normalize(np.array((-0.4, 0.8, 0.5)))
    render_faces = []
    for material, indices in faces:
        points = [vertices[index] for index in indices]
        normal = normalize(np.cross(points[1] - points[0], points[2] - points[0]))
        brightness = 0.62 + 0.38 * abs(float(np.dot(normal, light)))
        base = COLORS.get(material, (190, 190, 190))
        color = tuple(min(255, int(channel * brightness)) for channel in base)
        depth = sum(camera_vertices[index][2] for index in indices) / 3.0
        polygon = [screen_vertices[index] for index in indices]
        render_faces.append((depth, polygon, color))

    for _, polygon, color in sorted(render_faces, reverse=True):
        draw.polygon(polygon, fill=color, outline=(28, 45, 62))

    output_path.parent.mkdir(parents=True, exist_ok=True)
    image.save(output_path)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("obj", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    render(args.obj, args.output)


if __name__ == "__main__":
    main()
