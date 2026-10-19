"""Keep shipped particle prewarm reservations within the GPU resource pool budget."""
import json
import re
from pathlib import Path

project = Path(__file__).resolve().parents[1]
header = (project / "Engine/Effect/EffectManager.h").read_text(encoding="utf-8")
match = re.search(r"kMaxPooledResources\s*=\s*(\d+)", header)
assert match, "Effect pool capacity definition was not found"
capacity = int(match.group(1))
reservations = []
for config in (project / "resources/Effects").glob("*/Effect.json"):
    data = json.loads(config.read_text(encoding="utf-8-sig"))
    reserve = data.get("ResourcePoolReserve", 0)
    assert isinstance(reserve, int) and 0 <= reserve <= 64, config
    reservations.append((config.parent.name, reserve))
total = sum(reserve for _, reserve in reservations)
assert total <= capacity, f"Effect warmup reserves {total}, pool capacity {capacity}: {reservations}"
print(f"PASS: effect warmup reservations {total}/{capacity}")
