"""Generate the static sand floor for Stage01's lower branch (standard library only)."""

import json
import math
from pathlib import Path

kProjectDirectory = Path(__file__).resolve().parents[2]
kAssetDirectory = kProjectDirectory / "resources/Models/Environment/Ocean/Seafloor"
kSegmentCount = 3
kWidth = 240.0
kGridStep = 10.0
kRailClearance = 30.0


def FloorHeight(horizontal, distance, routeLength, railHeight, branchDepth):
    progress = max(0.0, min(1.0, distance / routeLength))
    transition = 1.0
    if progress < 0.3:
        transition = progress / 0.3
    elif progress > 0.7:
        transition = (1.0 - progress) / 0.3
    weight = transition ** 3 * (transition * (transition * 6.0 - 15.0) + 10.0)
    bank = 12.0 * (horizontal / (kWidth * 0.5)) ** 2
    ripples = math.sin(horizontal * 0.067 + distance * 0.018) * 1.2
    ripples += math.cos(distance * 0.043 + horizontal * 0.07) * 0.5
    return railHeight + branchDepth * weight - kRailClearance + bank + ripples


def Main():
    settingsPath = kProjectDirectory / "resources/Stages/Stage01/settings.json"
    settings = json.loads(settingsPath.read_text(encoding="utf-8"))
    branch = settings["rail"]["lower_branch"]
    routeLength = branch["end_distance"] - branch["start_distance"]
    railHeight = settings["rail"]["control_points"][0][1]
    branchDepth = branch["offset"][1]
    segmentLength = routeLength / kSegmentCount
    columnCount = round(kWidth / kGridStep) + 1
    rowCount = round(segmentLength / kGridStep) + 1
    kAssetDirectory.mkdir(parents=True, exist_ok=True)
    material = "newmtl Sand\nKd 1 1 1\nKa 1 1 1\nKs 0 0 0\nNs 1\nd 1\nillum 2\nmap_Kd ../textures/sand.png\n"
    (kAssetDirectory / "sand_floor.mtl").write_text(material, encoding="utf-8")

    for segmentIndex in range(kSegmentCount):
        lines = ["# Engine axes: X right, Y up, Z forward.",
                 "mtllib sand_floor.mtl", "o SandFloor", "usemtl Sand"]
        for rowIndex in range(rowCount):
            localForward = rowIndex * segmentLength / (rowCount - 1)
            distance = segmentIndex * segmentLength + localForward
            for columnIndex in range(columnCount):
                horizontal = -kWidth * 0.5 + columnIndex * kGridStep
                height = FloorHeight(horizontal, distance, routeLength, railHeight, branchDepth)
                slopeX = (FloorHeight(horizontal + 0.1, distance, routeLength, railHeight, branchDepth) -
                          FloorHeight(horizontal - 0.1, distance, routeLength, railHeight, branchDepth)) / 0.2
                slopeZ = (FloorHeight(horizontal, distance + 0.1, routeLength, railHeight, branchDepth) -
                          FloorHeight(horizontal, distance - 0.1, routeLength, railHeight, branchDepth)) / 0.2
                normalLength = math.sqrt(slopeX * slopeX + 1.0 + slopeZ * slopeZ)
                lines.append("v %.6f %.6f %.6f" % (horizontal, height, localForward))
                lines.append("vt %.6f %.6f" % (horizontal / 18.0, distance / 18.0))
                lines.append("vn %.6f %.6f %.6f" % (-slopeX / normalLength, 1.0 / normalLength, -slopeZ / normalLength))
        for rowIndex in range(rowCount - 1):
            for columnIndex in range(columnCount - 1):
                first = rowIndex * columnCount + columnIndex + 1
                # Counterclockwise from above; the importer handles the engine's handedness.
                for corners in ((first, first + columnCount, first + 1),
                                (first + 1, first + columnCount, first + columnCount + 1)):
                    references = []
                    for corner in corners:
                        references.append("%d/%d/%d" % (corner, corner, corner))
                    lines.append("f " + " ".join(references))
        outputPath = kAssetDirectory / ("sand_floor_%02d.obj" % (segmentIndex + 1))
        outputPath.write_text("\n".join(lines) + "\n", encoding="utf-8")
        print("Generated", outputPath.name, "at engine Z", branch["start_distance"] + segmentIndex * segmentLength)


if __name__ == "__main__":
    Main()
