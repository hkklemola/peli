#!/usr/bin/env python3
"""Replace lake feature cells with surrounding biome codes in world map CSV."""

from __future__ import annotations

import argparse
import csv
from collections import Counter
from pathlib import Path

BIOMES = {"GR", "FO", "FA", "DE", "TU", "SE", "SA", "MO", "HI", "SW", "JU"}


def read_csv_rows(path: Path) -> list[list[str]]:
    with path.open("r", encoding="utf-8-sig", newline="") as f:
        return list(csv.reader(f))


def write_csv_rows(path: Path, rows: list[list[str]]) -> None:
    with path.open("w", encoding="utf-8", newline="") as f:
        writer = csv.writer(f, lineterminator="\n")
        writer.writerows(rows)


def is_lake_cell(cell: str) -> bool:
    base = cell.split("|", 1)[0].strip().lower()
    return base.startswith("lake=") or base == "la" or base == "lake"


def cell_biome(cell: str) -> str | None:
    base = cell.split("|", 1)[0].strip().upper()
    if base in BIOMES:
        return base
    return None


def pick_biome(grid: list[list[str]], x: int, y: int) -> str:
    height = len(grid)
    width = len(grid[0])

    max_radius = max(width, height)
    for radius in range(1, max_radius + 1):
        counts: Counter[str] = Counter()
        y0 = max(0, y - radius)
        y1 = min(height - 1, y + radius)
        x0 = max(0, x - radius)
        x1 = min(width - 1, x + radius)

        for ny in range(y0, y1 + 1):
            for nx in range(x0, x1 + 1):
                if nx == x and ny == y:
                    continue
                if max(abs(nx - x), abs(ny - y)) != radius:
                    continue
                biome = cell_biome(grid[ny][nx])
                if biome:
                    counts[biome] += 1

        if counts:
            return counts.most_common(1)[0][0]

    return "GR"


def replace_lakes(rows: list[list[str]]) -> tuple[list[list[str]], int]:
    data_indexes = []
    widths = Counter()

    for i, row in enumerate(rows):
        if not row:
            continue
        first = row[0].strip()
        if first.startswith("#"):
            continue
        if all(not c.strip() for c in row):
            continue
        widths[len(row)] += 1
        data_indexes.append(i)

    if not widths:
        return rows, 0

    target_width = widths.most_common(1)[0][0]
    map_row_indexes = [i for i in data_indexes if len(rows[i]) == target_width]
    if not map_row_indexes:
        return rows, 0

    grid = [rows[i][:] for i in map_row_indexes]

    replacements = []
    for y, row in enumerate(grid):
        for x, cell in enumerate(row):
            if is_lake_cell(cell):
                replacements.append((x, y, pick_biome(grid, x, y)))

    for x, y, biome in replacements:
        rest = ""
        parts = grid[y][x].split("|", 1)
        if len(parts) == 2 and parts[1].strip():
            rest = "|" + parts[1]
        grid[y][x] = biome + rest

    for idx, src_i in enumerate(map_row_indexes):
        rows[src_i] = grid[idx]

    return rows, len(replacements)


def main() -> None:
    parser = argparse.ArgumentParser(description="Replace lakes with surrounding biomes")
    parser.add_argument("--input", default="data/templates/maps/world_map_tiles.csv")
    parser.add_argument("--output", default="")
    args = parser.parse_args()

    in_path = Path(args.input)
    out_path = Path(args.output) if args.output else in_path

    rows = read_csv_rows(in_path)
    updated_rows, replaced = replace_lakes(rows)
    write_csv_rows(out_path, updated_rows)

    print(f"Replaced {replaced} lake cells in {out_path}")


if __name__ == "__main__":
    main()
