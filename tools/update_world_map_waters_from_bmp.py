#!/usr/bin/env python3
"""
Update world_map_tiles.csv by reading world_map_tiles_waters_1000.bmp.

Pixels matching #FF00FF are marked with river metadata.
Pixels matching #0087FF are marked with lake metadata.
Existing river/lake metadata is replaced by the BMP-derived value.
"""
import csv
from pathlib import Path
from PIL import Image

MASTER_DATA_DIR = Path("master_data/templates/maps")
BMP_PATH = MASTER_DATA_DIR / "world_map_tiles_waters_1000.bmp"
MAIN_CSV_PATH = MASTER_DATA_DIR / "world_map_tiles.csv"
OUT_CSV_PATH = MASTER_DATA_DIR / "world_map_tiles_waters_from_bmp.csv"

RIVER_COLOR = (0xFF, 0x00, 0xFF)
LAKE_COLOR = (0x00, 0x87, 0xFF)
WATER_META_BY_COLOR = {
    RIVER_COLOR: ("river", "major"),
    LAKE_COLOR: ("lake", "large"),
}


def normalize_rgb(rgb):
    if isinstance(rgb, int):
        return (rgb, rgb, rgb)
    if len(rgb) >= 3:
        return tuple(rgb[:3])
    return tuple(rgb)


def update_water_meta(cell, water_meta):
    if cell is None:
        return ""

    parts = cell.split("|") if cell != "" else [""]
    biome = parts[0]
    meta_parts = [p for p in parts[1:] if not p.startswith("river=") and not p.startswith("lake=")]

    if water_meta:
        meta_parts.append(f"{water_meta[0]}={water_meta[1]}")

    if biome == "" and not meta_parts:
        return ""
    if meta_parts:
        return biome + "|" + "|".join(meta_parts)
    return biome


def main():
    repo_root = Path(__file__).resolve().parent.parent
    bmp_path = repo_root / BMP_PATH
    main_csv_path = repo_root / MAIN_CSV_PATH
    out_csv_path = repo_root / OUT_CSV_PATH

    if not bmp_path.exists():
        raise FileNotFoundError(f"BMP file not found: {bmp_path}")
    if not main_csv_path.exists():
        raise FileNotFoundError(f"CSV file not found: {main_csv_path}")

    img = Image.open(bmp_path)
    if img.mode != "RGB":
        img = img.convert("RGB")
    width, height = img.size

    with main_csv_path.open("r", newline="", encoding="utf-8") as f:
        lines = f.readlines()

    data_lines = [line for line in lines if line.strip() != "" and not line.lstrip().startswith("#")]
    rows = list(csv.reader(data_lines))

    full_width_indexes = [i for i, row in enumerate(rows) if len(row) == width]
    if not full_width_indexes:
        raise ValueError(f"No rows with width {width} found in CSV.")

    # Find the first consecutive block of full-width rows of height size.
    grid_start = None
    for i in range(len(full_width_indexes) - height + 1):
        if full_width_indexes[i + height - 1] - full_width_indexes[i] == height - 1:
            grid_start = full_width_indexes[i]
            break

    if grid_start is None:
        grid_start = full_width_indexes[0]
        available_rows = len(full_width_indexes)
        print(
            f"Warning: only {available_rows} full-width rows found;"
            f" updating rows {grid_start}..{grid_start + available_rows - 1}."
        )
        height = available_rows
    else:
        print(f"Detected full grid block starting at CSV data row {grid_start}. Updating {height} rows.")

    with out_csv_path.open("w", newline="", encoding="utf-8") as f:
        writer = csv.writer(f)
        data_index = 0
        for line in lines:
            stripped = line.strip()
            if stripped == "" or stripped.startswith("#"):
                f.write(line)
                continue

            row = rows[data_index]
            if grid_start <= data_index < grid_start + height and len(row) == width:
                out_row = []
                bmp_row = data_index - grid_start
                for x, cell in enumerate(row):
                    rgb = normalize_rgb(img.getpixel((x, bmp_row)))
                    water_meta = WATER_META_BY_COLOR.get(rgb)
                    out_row.append(update_water_meta(cell, water_meta))
                writer.writerow(out_row)
            else:
                writer.writerow(row)
            data_index += 1

    print(f"Wrote updated water metadata CSV: {out_csv_path}")


if __name__ == "__main__":
    main()
