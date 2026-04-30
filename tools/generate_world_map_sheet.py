from __future__ import annotations

import argparse
import csv
import math
from pathlib import Path

WIDTH = 1000
HEIGHT = 1000
CELL_SIZE_CM = 0.45

ROOT = Path(__file__).resolve().parents[1]
OUT_DIR = ROOT / "master_data" / "templates" / "maps"
CSV_PATH = OUT_DIR / "world_map_tiles.csv"

BIOME_INFO = {
    "GR": ("Grasslands", "#7CB342", "#000000"),
    "FO": ("Forest", "#2E7D32", "#FFFFFF"),
    "FA": ("Farmlands", "#D4B96E", "#000000"),
    "DE": ("Desert", "#E6C97A", "#000000"),
    "TU": ("Tundra", "#CFD8DC", "#000000"),
    "SE": ("Sea", "#1565C0", "#FFFFFF"),
    "SA": ("Savannah", "#C0CA33", "#000000"),
    "MO": ("Mountains", "#8D8D8D", "#000000"),
    "HI": ("Foothills", "#A1887F", "#000000"),
    "SW": ("Swamp", "#6D8B74", "#FFFFFF"),
    "JU": ("Jungle", "#1B5E20", "#FFFFFF"),
    "TA": ("Taiga", "#87AF87", "#000000"),
    "SH": ("Shrubland", "#AFAF87", "#000000"),
    "ST": ("Steppe", "#D7AF87", "#000000"),
    "GL": ("Glacier", "#D7D7FF", "#000000"),
}

LOCATIONS = [
    {"x": 50, "y": 50, "name": "The Glade of Beginnings", "type": "STARTER", "index": 0},
    {"x": 54, "y": 47, "name": "Goblin Warrens", "type": "DUNGEON", "index": 1},
    {"x": 46, "y": 45, "name": "Ancient Crypt", "type": "CRYPT", "index": 2},
    {"x": 52, "y": 53, "name": "Market Town", "type": "TOWN", "index": 3},
    {"x": 50, "y": 42, "name": "Old Mine", "type": "CAVERN", "index": 4},
    {"x": 58, "y": 50, "name": "Castle Ruins", "type": "DUNGEON", "index": 5},
    {"x": 50, "y": 58, "name": "Village", "type": "TOWN", "index": 6},
    {"x": 42, "y": 50, "name": "Forest Lake", "type": "CAVERN", "index": 7},
]

LOCATION_BY_COORD = {(entry["x"], entry["y"]): entry for entry in LOCATIONS}
ROAD_BY_COORD: dict[tuple[int, int], str] = {}


def stamp_road(x0: int, y0: int, x1: int, y1: int, tier: str) -> None:
    dx = abs(x1 - x0)
    sx = 1 if x0 < x1 else -1
    dy = -abs(y1 - y0)
    sy = 1 if y0 < y1 else -1
    err = dx + dy
    x, y = x0, y0

    while True:
        ROAD_BY_COORD[(x, y)] = tier
        if x == x1 and y == y1:
            break
        e2 = 2 * err
        if e2 >= dy:
            err += dy
            x += sx
        if e2 <= dx:
            err += dx
            y += sy


build_road_network = None


def build_roads() -> None:
    stamp_road(50, 50, 50, 58, "trail")
    stamp_road(50, 50, 52, 53, "trail")
    stamp_road(52, 53, 50, 58, "paved")
    stamp_road(50, 50, 54, 47, "trail")
    stamp_road(50, 50, 46, 45, "trail")
    stamp_road(50, 42, 50, 50, "trail")
    stamp_road(58, 50, 52, 53, "trail")


def local_start_region_biome(x: int, y: int, current: str) -> str:
    if not (30 <= x <= 70 and 30 <= y <= 70):
        return current

    code = "GR"
    if x <= 46 and y <= 55:
        code = "FO"
    if (x - 42) * (x - 42) + (y - 50) * (y - 50) <= 18:
        code = "SW"
    if x >= 55 and y <= 50:
        code = "HI"
    if x >= 58 and y <= 47:
        code = "MO"
    if y >= 53 and 46 <= x <= 60:
        code = "FA"
    return code


def base_biome(x: int, y: int) -> str:
    code = "GR"

    if y < 10 or y > HEIGHT - 10 or x < 8 or x > WIDTH - 8:
        code = "SE"
    elif y < 55:
        code = "TU"

    if 90 <= x <= 250 and 100 <= y <= 470:
        code = "FO"
    if 720 <= x <= 980 and 90 <= y <= 420:
        code = "DE"
    if 700 <= x <= 980 and 420 <= y <= 650:
        code = "SA"
    if 640 <= x <= 960 and 640 <= y <= 960:
        code = "JU" if y > 800 else "SW"
    if 180 <= x <= 330 and 680 <= y <= 930:
        code = "FO"
    if 330 <= x <= 520 and 730 <= y <= 930:
        code = "SW"

    ridge_x = 520 + int(120.0 * math.sin(y / 65.0))
    if 120 <= y <= 870:
        dist = abs(x - ridge_x)
        if dist <= 14:
            code = "MO"
        elif dist <= 32 and code != "SE":
            code = "HI"

    return local_start_region_biome(x, y, code)


def water_features(x: int, y: int) -> tuple[str | None, str | None]:
    river: str | None = None
    lake: str | None = None

    river_x = 180 + (y // 2) + int(30.0 * math.sin(y / 35.0))
    if 70 <= y <= 920 and abs(x - river_x) <= 1:
        river = "major"

    if ((x - 770) ** 2) / 900 + ((y - 220) ** 2) / 625 <= 1:
        lake = "large"
    if ((x - 250) ** 2) / 1600 + ((y - 780) ** 2) / 900 <= 1:
        lake = "large"

    return river, lake


def default_tile_text(x: int, y: int) -> str:
    biome = base_biome(x, y)
    tokens = [biome]
    river_tier, lake_tier = water_features(x, y)

    if (x, y) in LOCATION_BY_COORD:
        entry = LOCATION_BY_COORD[(x, y)]
        tokens.extend(
            [
                f"loc={entry['name']}",
                f"type={entry['type']}",
                f"index={entry['index']}",
                "gen=PROCEDURAL",
                "w=1000",
                "h=1000",
            ]
        )

    road = ROAD_BY_COORD.get((x, y))
    if road:
        tokens.append(f"road={road}")
    if river_tier:
        tokens.append(f"river={river_tier}")
    if lake_tier:
        tokens.append(f"lake={lake_tier}")

    return "|".join(tokens)


def build_comment_rows() -> list[list[str]]:
    return [
        ["# World map master sheet (.csv runtime export)"],
        ["# Each tile cell can contain: BIOME|loc=Name|type=TOWN|index=6|gen=PROCEDURAL|w=1000|h=1000|road=trail|river=major|lake=large"],
        ["# Biome codes row 1", "GR", "FO", "FA", "DE", "TU"],
        ["# Biome names row 1", "Grasslands", "Forest", "Farmlands", "Desert", "Tundra"],
        ["# Suggested colors row 1", "#7CB342", "#2E7D32", "#D4B96E", "#E6C97A", "#CFD8DC"],
        ["# Feature tiers", "road=trail|paved|highway", "river=minor|major|tiny|small|medium|large|massive|gigantic", "lake=small|large"],
        ["# Feature colors", "road:#8D8D8D", "river:#4FC3F7", "lake:#29B6F6"],
        ["# Biome codes row 2", "SE", "SA", "MO", "HI", "SW", "JU"],
        ["# Biome names row 2", "Sea", "Savannah", "Mountains", "Foothills", "Swamp", "Jungle"],
        ["# Suggested colors row 2", "#1565C0", "#C0CA33", "#8D8D8D", "#A1887F", "#6D8B74", "#1B5E20"],
    ]


def build_default_sheet_rows() -> list[list[str]]:
    rows = build_comment_rows()
    for y in range(HEIGHT):
        rows.append([default_tile_text(x, y) for x in range(WIDTH)])
    return rows


def configure_paths(csv_path: Path | None = None) -> None:
    global OUT_DIR, CSV_PATH

    if csv_path is not None:
        CSV_PATH = csv_path.resolve()
    OUT_DIR = CSV_PATH.parent


def ensure_csv_exists() -> None:
    if CSV_PATH.exists():
        return

    OUT_DIR.mkdir(parents=True, exist_ok=True)
    with CSV_PATH.open("w", encoding="utf-8", newline="") as handle:
        writer = csv.writer(handle)
        for row in build_default_sheet_rows():
            writer.writerow(row)


def is_comment_row(row: list[str]) -> bool:
    if not row:
        return False
    first = (row[0] or "").lstrip("\ufeff").strip()
    return first.startswith("#") or first.startswith(";")


def load_sheet_rows() -> list[list[str]]:
    ensure_csv_exists()
    with CSV_PATH.open("r", encoding="utf-8", newline="") as handle:
        rows = list(csv.reader(handle))

    if not rows:
        rows = build_default_sheet_rows()

    normalized: list[list[str]] = build_comment_rows()
    data_count = 0

    for row in rows:
        if not row or is_comment_row(row):
            continue

        fixed = row[:WIDTH] + [""] * max(0, WIDTH - len(row))
        normalized.append(fixed)
        data_count += 1
        if data_count >= HEIGHT:
            break

    while data_count < HEIGHT:
        normalized.append([default_tile_text(x, data_count) for x in range(WIDTH)])
        data_count += 1

    return normalized


def write_csv(rows: list[list[str]]) -> None:
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    with CSV_PATH.open("w", encoding="utf-8", newline="") as handle:
        writer = csv.writer(handle)
        for row in rows:
            writer.writerow(row)


def parse_tile_cell_for_binary(cell_text: str) -> tuple[int, int, int, int]:
    biome = 0
    road_tier = 0
    river_tier = 0
    lake_tier = 0

    if not cell_text:
        return biome, road_tier, river_tier, lake_tier

    cell_text = cell_text.strip()
    if not cell_text:
        return biome, road_tier, river_tier, lake_tier

    tokens = [segment.strip() for segment in cell_text.split("|") if segment.strip()]
    if not tokens:
        return biome, road_tier, river_tier, lake_tier

    token = tokens[0].upper()
    biome_map = {
        "GR": 1,
        "FO": 2,
        "DE": 3,
        "TU": 4,
        "SE": 5,
        "SA": 6,
        "MO": 7,
        "HI": 8,
        "SW": 9,
        "JU": 10,
        "TA": 11,
        "SH": 12,
        "ST": 13,
        "GL": 14,
        "FA": 1,
        ".": 1,
        "\"": 2,
        "~": 3,
        "'": 4,
        "S": 5,
        "N": 8,
        "M": 9,
        "J": 10,
    }
    if token in biome_map:
        biome = biome_map[token]

    for segment in tokens[1:]:
        if '=' not in segment:
            continue
        key, value = [part.strip().lower() for part in segment.split('=', 1)]
        if key == "road":
            if value == "trail":
                road_tier = 1
            elif value == "paved":
                road_tier = 2
            elif value == "highway":
                road_tier = 3
        elif key == "river":
            if value in ("minor",):
                river_tier = 1
            elif value in ("major",):
                river_tier = 2
            elif value == "tiny":
                river_tier = 3
            elif value == "small":
                river_tier = 4
            elif value == "medium":
                river_tier = 5
            elif value == "large":
                river_tier = 6
            elif value == "massive":
                river_tier = 7
            elif value == "gigantic":
                river_tier = 8
            elif value in ("r", "ri", "river"):
                river_tier = 2
        elif key == "lake":
            if value == "small":
                lake_tier = 1
            elif value == "large":
                lake_tier = 2
            elif value in ("l", "la", "lake"):
                lake_tier = 2

    return biome, road_tier, river_tier, lake_tier


def pack_tile_value(biome: int, road_tier: int, river_tier: int, lake_tier: int) -> int:
    return (
        (biome & 0xF)
        | ((road_tier & 0x3) << 4)
        | ((river_tier & 0xF) << 6)
        | ((lake_tier & 0x3) << 10)
    )


def write_bin(rows: list[list[str]]) -> None:
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    bin_path = CSV_PATH.with_suffix(".bin")

    with bin_path.open("wb") as handle:
        handle.write(b"WMP1")
        handle.write(WIDTH.to_bytes(2, "little"))
        handle.write(HEIGHT.to_bytes(2, "little"))

        for row in rows:
            if is_comment_row(row):
                continue
            for x in range(WIDTH):
                cell = row[x] if x < len(row) else ""
                biome, road_tier, river_tier, lake_tier = parse_tile_cell_for_binary(cell)
                handle.write(pack_tile_value(biome, road_tier, river_tier, lake_tier).to_bytes(2, "little"))



def biome_code_from_text(text: str) -> str | None:
    if not text:
        return None
    token = text.split("|", 1)[0].strip().strip('"').upper()
    return token if token in BIOME_INFO else None


def cell_style_name(text: str) -> str:
    if not text:
        return "ce_blank"
    if text.strip().startswith("#"):
        return "ce_comment"
    biome_code = biome_code_from_text(text)
    if biome_code:
        return f"ce_{biome_code.lower()}"
    return "ce_blank"




def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Generate the runtime world map files from master_data templates.")
    parser.add_argument("--csv", type=Path, default=CSV_PATH, help="Path to the runtime CSV map file.")
    return parser.parse_args()


if __name__ == "__main__":
    args = parse_args()
    configure_paths(args.csv)
    build_roads()
    rows = load_sheet_rows()
    write_csv(rows)
    write_bin(rows)
    print(f"Updated {CSV_PATH}")
    print(f"Generated {CSV_PATH.with_suffix('.bin')}")
