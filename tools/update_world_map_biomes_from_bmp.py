import csv
from pathlib import Path
from PIL import Image

# --- CONFIG ---
MASTER_DATA_DIR = Path("master_data/templates/maps")
BMP_PATH = MASTER_DATA_DIR / "world_map_tiles_1000.bmp"
MAIN_CSV_PATH = MASTER_DATA_DIR / "world_map_tiles.csv"
TMP_BIOME_CSV_PATH = MASTER_DATA_DIR / "world_map_tiles_from_bmp.csv"
MERGED_CSV_PATH = MASTER_DATA_DIR / "world_map_tiles_merged.csv"

# --- BIOME COLOR MAPPING ---
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

def closest_biome_code(rgb):
    min_dist = float('inf')
    best_code = None
    for color, code in BIOME_COLOR_TO_CODE.items():
        dist = sum((a - b) ** 2 for a, b in zip(rgb, color))
        if dist < min_dist:
            min_dist = dist
            best_code = code
    return best_code

def split_biome_and_meta(cell):
    if '|' in cell:
        biome, meta = cell.split('|', 1)
        return biome, '|' + meta
    else:
        return cell, ''

def bmp_to_biome_csv():
    img = Image.open(BMP_PATH)
    if img.mode != "RGB":
        img = img.convert("RGB")
    width, height = img.size
    with TMP_BIOME_CSV_PATH.open("w", newline="", encoding="utf-8") as f:
        writer = csv.writer(f)
        for y in range(height):
            row = []
            for x in range(width):
                rgb = img.getpixel((x, y))
                code = closest_biome_code(rgb)
                row.append(code if code else "")
            writer.writerow(row)
    print(f"Wrote updated biome CSV: {TMP_BIOME_CSV_PATH}")

def merge_biome_data():
    with MAIN_CSV_PATH.open(newline='', encoding='utf-8') as f_main, \
         TMP_BIOME_CSV_PATH.open(newline='', encoding='utf-8') as f_new, \
         MERGED_CSV_PATH.open('w', newline='', encoding='utf-8') as f_out:
        reader_main = csv.reader(f_main)
        reader_new = csv.reader(f_new)
        writer = csv.writer(f_out)
        for row_main, row_new in zip(reader_main, reader_new):
            merged_row = []
            for cell_main, cell_new in zip(row_main, row_new):
                _, meta = split_biome_and_meta(cell_main)
                merged_row.append(cell_new + meta)
            writer.writerow(merged_row)
    print(f"Merged CSV written to: {MERGED_CSV_PATH}")

def replace_original():

    MAIN_CSV_PATH.unlink()
    MERGED_CSV_PATH.rename(MAIN_CSV_PATH)
    print(f"Replaced {MAIN_CSV_PATH} with merged biome data.")

def copy_to_master():
    print(f"Master data updated in place: {MAIN_CSV_PATH}")
    print(f"Temporary BMP-derived biome CSV written to: {TMP_BIOME_CSV_PATH}")

def main():
    bmp_to_biome_csv()
    merge_biome_data()
    replace_original()
    copy_to_master()

if __name__ == "__main__":
    main()
