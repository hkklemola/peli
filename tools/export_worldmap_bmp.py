#!/usr/bin/env python3
"""Export world_map_tiles.csv to a 24-bit BMP with fixed output size."""

from __future__ import annotations

import argparse
import csv
import struct
from pathlib import Path


DEFAULT_COLORS = {
    "GR": "#7CB342",
    "FO": "#2E7D32",
    "FA": "#D4B96E",
    "DE": "#E6C97A",
    "TU": "#CFD8DC",
    "SE": "#EED39A",
    "SA": "#D9C27A",
    "MO": "#8D6E63",
    "HI": "#BCAAA4",
    "SW": "#7CB342",
    "JU": "#4CAF50",
}

FEATURE_COLORS = {
    "lake": "#29B6F6",
    "river": "#4FC3F7",
}


def hex_to_bgr(hex_color: str) -> tuple[int, int, int]:
    hex_color = hex_color.strip().lstrip("#")
    if len(hex_color) != 6:
        raise ValueError(f"Invalid hex color: {hex_color}")
    r = int(hex_color[0:2], 16)
    g = int(hex_color[2:4], 16)
    b = int(hex_color[4:6], 16)
    return (b, g, r)


def parse_map_rows(csv_path: Path) -> list[list[str]]:
    rows: list[list[str]] = []
    with csv_path.open("r", encoding="utf-8-sig", newline="") as f:
        reader = csv.reader(f)
        for row in reader:
            if not row:
                continue
            first = row[0].strip()
            if first.startswith("#"):
                continue
            if all(not cell.strip() for cell in row):
                continue
            rows.append([cell.strip() for cell in row])

    if not rows:
        raise ValueError(f"No non-comment rows found in {csv_path}")

    max_cols = max(len(r) for r in rows)
    grid = [r for r in rows if len(r) == max_cols]
    if not grid:
        raise ValueError("No map grid rows detected")

    return grid


def cell_to_bgr(cell: str, palette_bgr: dict[str, tuple[int, int, int]]) -> tuple[int, int, int]:
    if not cell:
        return (0, 0, 0)

    base = cell.split("|", 1)[0].strip()

    if "=" in base:
        key = base.split("=", 1)[0].strip().lower()
        feature = FEATURE_COLORS.get(key)
        if feature:
            return hex_to_bgr(feature)

    if base in palette_bgr:
        return palette_bgr[base]

    return (0, 0, 0)


def write_bmp_nearest_neighbor(
    grid: list[list[str]],
    out_path: Path,
    width: int,
    height: int,
    palette_bgr: dict[str, tuple[int, int, int]],
) -> None:
    src_h = len(grid)
    src_w = len(grid[0])

    row_bytes = width * 3
    padding = (4 - (row_bytes % 4)) % 4
    image_size = (row_bytes + padding) * height
    file_size = 14 + 40 + image_size

    with out_path.open("wb") as f:
        f.write(b"BM")
        f.write(struct.pack("<I", file_size))
        f.write(struct.pack("<HH", 0, 0))
        f.write(struct.pack("<I", 54))

        f.write(struct.pack("<I", 40))
        f.write(struct.pack("<i", width))
        f.write(struct.pack("<i", height))
        f.write(struct.pack("<H", 1))
        f.write(struct.pack("<H", 24))
        f.write(struct.pack("<I", 0))
        f.write(struct.pack("<I", image_size))
        f.write(struct.pack("<i", 2835))
        f.write(struct.pack("<i", 2835))
        f.write(struct.pack("<I", 0))
        f.write(struct.pack("<I", 0))

        pad = b"\x00" * padding

        for y in range(height - 1, -1, -1):
            sy = int((y * src_h) / height)
            src_row = grid[sy]
            line = bytearray(row_bytes)
            idx = 0
            for x in range(width):
                sx = int((x * src_w) / width)
                b, g, r = cell_to_bgr(src_row[sx], palette_bgr)
                line[idx] = b
                line[idx + 1] = g
                line[idx + 2] = r
                idx += 3
            f.write(line)
            if padding:
                f.write(pad)


def main() -> None:
    parser = argparse.ArgumentParser(description="Export world map CSV to BMP")
    parser.add_argument(
        "--input",
        default="data/templates/maps/world_map_tiles.csv",
        help="Input world map CSV path",
    )
    parser.add_argument(
        "--output",
        default="build-win/data/templates/maps/world_map_tiles_10000.bmp",
        help="Output BMP path",
    )
    parser.add_argument("--width", type=int, default=10000)
    parser.add_argument("--height", type=int, default=10000)
    args = parser.parse_args()

    in_path = Path(args.input)
    out_path = Path(args.output)

    if args.width <= 0 or args.height <= 0:
        raise ValueError("Width and height must be positive")

    grid = parse_map_rows(in_path)
    palette_bgr = {k: hex_to_bgr(v) for k, v in DEFAULT_COLORS.items()}

    out_path.parent.mkdir(parents=True, exist_ok=True)
    write_bmp_nearest_neighbor(grid, out_path, args.width, args.height, palette_bgr)

    print(
        f"Wrote {out_path} ({args.width}x{args.height}) from {in_path} "
        f"using source grid {len(grid[0])}x{len(grid)}"
    )


if __name__ == "__main__":
    main()
