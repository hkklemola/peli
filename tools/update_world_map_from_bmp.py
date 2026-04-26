#!/usr/bin/env python3
"""
Update world_map_tiles.csv based on a BMP image (world_map_tiles_1000.bmp).
Each pixel in the BMP is mapped to a biome code using the 256-color palette mapping.
"""
import csv
from pathlib import Path
from PIL import Image
import sys

# 256-color palette-based biome color mapping (must match game logic)
BIOME_COLOR_TO_CODE = {
    (0x87, 0xd7, 0x00): "GR",   # PISTACHIO (index 112)
    (0x5f, 0xaf, 0x5f): "FO",   # FOREST_GREEN (index 71)
    (0xff, 0xd7, 0x87): "FA",   # KHAKI (index 222, for farmland)
    (0xff, 0xd7, 0x87): "DE",   # KHAKI (index 222)
    (0xd7, 0xd7, 0xd7): "TU",   # LIGHT_SILVER (index 188)
    (0x87, 0xaf, 0x87): "TA",   # BAY_LEAF (index 108)
    (0xaf, 0xaf, 0x87): "SH",   # SAGE (index 144)
    (0xd7, 0xaf, 0x87): "ST",   # TAN (index 180)
    (0xd7, 0xd7, 0xff): "GL",   # VERY_PALE_BLUE (index 189)
    (0x00, 0x5f, 0x87): "SE",   # SEA_BLUE (index 24)
    (0xaf, 0xaf, 0x5f): "SA",   # OLIVE_GREEN (index 143)
    (0x80, 0x80, 0x80): "MO",   # GRAY (index 8)
    (0xaf, 0x87, 0x5f): "HI",   # BRONZE (index 137)
    (0x5f, 0x87, 0x5f): "SW",   # GLADE_GREEN (index 65)
    (0x00, 0x87, 0x00): "JU",   # AO (index 28)
}

# Fallback order for ambiguous colors
BIOME_PRIORITY = ["FA", "DE", "GR", "FO", "TA", "SH", "ST", "GL", "TU", "SE", "SA", "MO", "HI", "SW", "JU"]

# Reverse mapping for quick lookup
CODE_TO_COLOR = {v: k for k, v in BIOME_COLOR_TO_CODE.items()}


def closest_biome_code(rgb):
    # Find the closest color in BIOME_COLOR_TO_CODE
    min_dist = float('inf')
    best_code = None
    for color, code in BIOME_COLOR_TO_CODE.items():
        dist = sum((a - b) ** 2 for a, b in zip(rgb, color))
        if dist < min_dist:
            min_dist = dist
            best_code = code
    return best_code


def main():
    bmp_path = Path("build-win/data/templates/maps/world_map_tiles_1000.bmp")
    out_csv_path = Path("build-win/data/templates/maps/world_map_tiles_from_bmp.csv")

    img = Image.open(bmp_path)
    if img.mode != "RGB":
        img = img.convert("RGB")
    width, height = img.size

    # Write CSV
    with out_csv_path.open("w", newline="", encoding="utf-8") as f:
        writer = csv.writer(f)
        for y in range(height):
            row = []
            for x in range(width):
                rgb = img.getpixel((x, y))
                code = closest_biome_code(rgb)
                row.append(code if code else "")
            writer.writerow(row)
    print(f"Wrote updated biome CSV: {out_csv_path}")

if __name__ == "__main__":
    main()
