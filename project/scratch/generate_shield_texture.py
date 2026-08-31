import math
from PIL import Image, ImageDraw

def create_hex_shield_texture(filepath, width=512, height=512):
    # RGBA Transparent Image
    img = Image.new('RGBA', (width, height), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)

    # Hexagon parameters
    radius = 32
    h = radius * math.sqrt(3)
    w = radius * 2

    cols = int(width / (w * 0.75)) + 4
    rows = int(height / h) + 4

    center_x = width / 2.0
    center_y = height / 2.0
    max_dist = width * 0.45

    for c in range(-2, cols):
        for r in range(-2, rows):
            cx = c * (w * 0.75)
            cy = r * h + (h / 2.0 if (c % 2 == 1) else 0.0)

            # Distance from center for radial mask
            dist = math.sqrt((cx - center_x)**2 + (cy - center_y)**2)
            if dist > max_dist:
                continue

            alpha_factor = max(0.0, 1.0 - (dist / max_dist)**2)

            # Calculate 6 vertices
            points = []
            for i in range(6):
                angle = math.pi / 3.0 * i
                px = cx + radius * math.cos(angle)
                py = cy + radius * math.sin(angle)
                points.append((px, py))

            # Cyan Glow Colors
            line_color = (40, 220, 255, int(255 * alpha_factor))
            fill_color = (10, 80, 180, int(60 * alpha_factor))

            # Draw hexagon fill & outline
            draw.polygon(points, fill=fill_color, outline=line_color, width=3)

    img.save(filepath, 'PNG')
    print(f"Created shield texture: {filepath}")

if __name__ == "__main__":
    create_hex_shield_texture("c:/Projects/KohakuEngine/project/resources/Textures/shield_hex.png")
