#!/usr/bin/env python3
"""Generate a BMP sample image showing each biome color with labels."""
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

# Biome colors matching the current 256-color palette mapping
BIOME_SAMPLES = [
    ("GR", "Grasslands", "#87d700"),
    ("FO", "Forest", "#5faf5f"),
    ("FA", "Farmlands", "#ffd787"),
    ("DE", "Desert", "#ffd787"),
    ("TU", "Tundra", "#d7d7d7"),
    ("SE", "Sea", "#005f87"),
    ("SA", "Savannah", "#afaf5f"),
    ("MO", "Mountains", "#808080"),
    ("HI", "Foothills", "#af875f"),
    ("SW", "Swamp", "#5f875f"),
    ("JU", "Jungle", "#008700"),
    ("TA", "Taiga", "#87af87"),
    ("SH", "Shrubland", "#afaf87"),
    ("ST", "Steppe", "#d7af87"),
    ("GL", "Glacier", "#d7d7ff"),
]

OUTPUT_PATH = Path("build-win/data/templates/maps/biome_palette_samples.bmp")
BOX_SIZE = 50
PADDING = 20
LABEL_HEIGHT = 40
COLUMNS = 5

font = ImageFont.load_default()

rows = (len(BIOME_SAMPLES) + COLUMNS - 1) // COLUMNS
width = COLUMNS * (BOX_SIZE + PADDING) + PADDING
height = rows * (BOX_SIZE + LABEL_HEIGHT + PADDING) + PADDING

image = Image.new("RGB", (width, height), "black")
draw = ImageDraw.Draw(image)

for idx, (code, name, color_hex) in enumerate(BIOME_SAMPLES):
    col = idx % COLUMNS
    row = idx // COLUMNS
    x = PADDING + col * (BOX_SIZE + PADDING)
    y = PADDING + row * (BOX_SIZE + LABEL_HEIGHT + PADDING)

    # Draw label text above the color box
    label = f"{code} - {name}"
    bbox = draw.textbbox((0, 0), label, font=font)
    text_width = bbox[2] - bbox[0]
    text_height = bbox[3] - bbox[1]
    text_x = x + (BOX_SIZE - text_width) // 2
    text_y = y
    draw.text((text_x, text_y), label, fill="white", font=font)

    box_y = y + LABEL_HEIGHT
    draw.rectangle([x, box_y, x + BOX_SIZE, box_y + BOX_SIZE], fill=color_hex, outline="white")

OUTPUT_PATH.parent.mkdir(parents=True, exist_ok=True)
image.save(OUTPUT_PATH, format="BMP")
print(f"Wrote biome palette sample BMP: {OUTPUT_PATH}")
