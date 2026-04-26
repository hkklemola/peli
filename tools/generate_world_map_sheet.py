from __future__ import annotations

import argparse
import csv
import math
import xml.etree.ElementTree as ET
import zipfile
from pathlib import Path
from xml.sax.saxutils import escape

WIDTH = 1000
HEIGHT = 1000
CELL_SIZE_CM = 0.45

ROOT = Path(__file__).resolve().parents[1]
OUT_DIR = ROOT / "data" / "templates" / "maps"
CSV_PATH = OUT_DIR / "world_map_tiles.csv"
FODS_PATH = OUT_DIR / "world_map_tiles.fods"
ODS_PATH = OUT_DIR / "world_map_tiles.ods"

ODF_NS = {
    "office": "urn:oasis:names:tc:opendocument:xmlns:office:1.0",
    "table": "urn:oasis:names:tc:opendocument:xmlns:table:1.0",
    "text": "urn:oasis:names:tc:opendocument:xmlns:text:1.0",
}
TABLE_NAME_ATTR = f"{{{ODF_NS['table']}}}name"
COLUMN_REPEAT_ATTR = f"{{{ODF_NS['table']}}}number-columns-repeated"
ROW_REPEAT_ATTR = f"{{{ODF_NS['table']}}}number-rows-repeated"

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
        ["# LibreOffice Calc world map master sheet (.ods recommended; .csv runtime export)"],
        ["# Each tile cell can contain: BIOME|loc=Name|type=TOWN|index=6|gen=PROCEDURAL|w=1000|h=1000|road=trail|river=major|lake=large"],
        ["# Biome codes row 1", "GR", "FO", "FA", "DE", "TU"],
        ["# Biome names row 1", "Grasslands", "Forest", "Farmlands", "Desert", "Tundra"],
        ["# Suggested colors row 1", "#7CB342", "#2E7D32", "#D4B96E", "#E6C97A", "#CFD8DC"],
        ["# Feature tiers", "road=trail|paved|highway", "river=minor|major", "lake=small|large"],
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


def configure_paths(csv_path: Path | None = None,
                    fods_path: Path | None = None,
                    ods_path: Path | None = None) -> None:
    global OUT_DIR, CSV_PATH, FODS_PATH, ODS_PATH

    if csv_path is not None:
        CSV_PATH = csv_path.resolve()
    if fods_path is not None:
        FODS_PATH = fods_path.resolve()
    if ods_path is not None:
        ODS_PATH = ods_path.resolve()

    OUT_DIR = CSV_PATH.parent


def spreadsheet_source_path(prefer_spreadsheet: bool = False) -> Path | None:
    candidates = [path for path in (ODS_PATH, FODS_PATH) if path.exists()]
    if not candidates:
        return None

    newest = max(candidates, key=lambda path: path.stat().st_mtime)
    if prefer_spreadsheet or not CSV_PATH.exists() or newest.stat().st_mtime > CSV_PATH.stat().st_mtime:
        return newest
    return None


def load_rows_from_sheet_xml(xml_text: str) -> list[list[str]]:
    root = ET.fromstring(xml_text)
    tables = root.findall(".//table:table", ODF_NS)
    target_table = None

    for table in tables:
        if table.attrib.get(TABLE_NAME_ATTR) == "World Map":
            target_table = table
            break

    if target_table is None and tables:
        target_table = tables[0]
    if target_table is None:
        return []

    rows: list[list[str]] = []
    for row in target_table.findall("table:table-row", ODF_NS):
        repeated_rows = max(1, int(row.attrib.get(ROW_REPEAT_ATTR, "1") or "1"))
        expanded_row: list[str] = []

        for cell in row:
            tag_name = cell.tag.rsplit("}", 1)[-1]
            repeated_columns = max(1, int(cell.attrib.get(COLUMN_REPEAT_ATTR, "1") or "1"))

            if tag_name == "covered-table-cell":
                text = ""
            else:
                text = "".join(cell.itertext()).strip()

            expanded_row.extend([text] * repeated_columns)

        for _ in range(repeated_rows):
            rows.append(list(expanded_row))

    return rows


def load_rows_from_spreadsheet(path: Path) -> list[list[str]]:
    if path.suffix.lower() == ".ods":
        with zipfile.ZipFile(path, "r") as archive:
            xml_text = archive.read("content.xml").decode("utf-8")
        return load_rows_from_sheet_xml(xml_text)

    return load_rows_from_sheet_xml(path.read_text(encoding="utf-8"))


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


def load_sheet_rows(prefer_spreadsheet: bool = False) -> list[list[str]]:
    rows: list[list[str]]
    spreadsheet_path = spreadsheet_source_path(prefer_spreadsheet)
    if spreadsheet_path is not None:
        spreadsheet_rows = load_rows_from_spreadsheet(spreadsheet_path)
        rows = spreadsheet_rows if spreadsheet_rows else build_default_sheet_rows()
    else:
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


def ods_cell(text: str = "", style: str = "ce_blank", repeat: int = 1) -> str:
    repeat_attr = f' table:number-columns-repeated="{repeat}"' if repeat > 1 else ""
    if text == "":
        return f'<table:table-cell table:style-name="{style}"{repeat_attr}/>'
    return (
        f'<table:table-cell table:style-name="{style}" office:value-type="string"{repeat_attr}>'
        f'<text:p>{escape(text)}</text:p></table:table-cell>'
    )


def row_segments(row: list[str], width: int) -> list[tuple[str, str, int]]:
    padded = row[:width] + [""] * max(0, width - len(row))
    segments: list[tuple[str, str, int]] = []
    current_text = padded[0]
    current_style = cell_style_name(current_text)
    run = 1

    for text in padded[1:]:
        style = cell_style_name(text)
        if text == current_text and style == current_style:
            run += 1
        else:
            segments.append((current_text, current_style, run))
            current_text = text
            current_style = style
            run = 1

    segments.append((current_text, current_style, run))
    return segments


def style_block() -> str:
    parts = [
        f'<style:style style:name="co_map" style:family="table-column"><style:table-column-properties style:column-width="{CELL_SIZE_CM:.2f}cm"/></style:style>',
        f'<style:style style:name="ro_map" style:family="table-row"><style:table-row-properties style:row-height="{CELL_SIZE_CM:.2f}cm" style:use-optimal-row-height="false"/></style:style>',
        '<style:style style:name="ro_legend" style:family="table-row"><style:table-row-properties style:row-height="0.55cm" style:use-optimal-row-height="false"/></style:style>',
        '<style:style style:name="ce_comment" style:family="table-cell"><style:table-cell-properties fo:background-color="#F1F1F1" fo:border="0.002cm solid #CCCCCC" style:vertical-align="middle"/><style:paragraph-properties fo:text-align="left"/><style:text-properties fo:font-size="8pt" fo:font-weight="bold" fo:color="#000000"/></style:style>',
        '<style:style style:name="ce_blank" style:family="table-cell"><style:table-cell-properties fo:background-color="#FFFFFF" fo:border="0.002cm solid #DDDDDD" style:vertical-align="middle"/><style:paragraph-properties fo:text-align="center"/><style:text-properties fo:font-size="6pt" fo:color="#000000"/></style:style>',
    ]

    for code, (_, fill_color, text_color) in BIOME_INFO.items():
        parts.append(
            f'<style:style style:name="ce_{code.lower()}" style:family="table-cell">'
            f'<style:table-cell-properties fo:background-color="{fill_color}" fo:border="0.002cm solid #CCCCCC" style:vertical-align="middle"/>'
            f'<style:paragraph-properties fo:text-align="center"/>'
            f'<style:text-properties fo:font-size="5pt" fo:font-weight="bold" fo:color="{text_color}"/>'
            f'</style:style>'
        )

    return "\n".join(parts)


def table_xml(rows: list[list[str]]) -> str:
    lines = [
        '<table:table table:name="World Map">',
        f'<table:table-column table:style-name="co_map" table:number-columns-repeated="{WIDTH}"/>',
    ]

    data_count = 0
    for row in rows:
        if is_comment_row(row):
            lines.append('<table:table-row table:style-name="ro_legend">')
            for text, style, repeat in row_segments(row, WIDTH):
                lines.append(ods_cell(text, style, repeat))
            lines.append('</table:table-row>')
            continue

        lines.append('<table:table-row table:style-name="ro_map">')
        for text, style, repeat in row_segments(row, WIDTH):
            lines.append(ods_cell(text, style, repeat))
        lines.append('</table:table-row>')

        data_count += 1
        if data_count >= HEIGHT:
            break

    lines.append('</table:table>')
    return "\n".join(lines)


def build_flat_fods(rows: list[list[str]]) -> str:
    return "\n".join(
        [
            '<?xml version="1.0" encoding="UTF-8"?>',
            '<office:document'
            ' xmlns:office="urn:oasis:names:tc:opendocument:xmlns:office:1.0"'
            ' xmlns:style="urn:oasis:names:tc:opendocument:xmlns:style:1.0"'
            ' xmlns:text="urn:oasis:names:tc:opendocument:xmlns:text:1.0"'
            ' xmlns:table="urn:oasis:names:tc:opendocument:xmlns:table:1.0"'
            ' xmlns:fo="urn:oasis:names:tc:opendocument:xmlns:xsl-fo-compatible:1.0"'
            ' xmlns:meta="urn:oasis:names:tc:opendocument:xmlns:meta:1.0"'
            ' xmlns:config="urn:oasis:names:tc:opendocument:xmlns:config:1.0"'
            ' office:mimetype="application/vnd.oasis.opendocument.spreadsheet"'
            ' office:version="1.3">',
            '<office:meta><meta:generator>GitHub Copilot</meta:generator></office:meta>',
            '<office:settings><config:config-item-set config:name="ooo:view-settings"/></office:settings>',
            '<office:scripts/>',
            '<office:font-face-decls/>',
            '<office:styles/>',
            '<office:automatic-styles>',
            style_block(),
            '</office:automatic-styles>',
            '<office:master-styles/>',
            '<office:body><office:spreadsheet>',
            table_xml(rows),
            '</office:spreadsheet></office:body>',
            '</office:document>',
        ]
    )


def build_content_xml(rows: list[list[str]]) -> str:
    return "\n".join(
        [
            '<?xml version="1.0" encoding="UTF-8"?>',
            '<office:document-content'
            ' xmlns:office="urn:oasis:names:tc:opendocument:xmlns:office:1.0"'
            ' xmlns:style="urn:oasis:names:tc:opendocument:xmlns:style:1.0"'
            ' xmlns:text="urn:oasis:names:tc:opendocument:xmlns:text:1.0"'
            ' xmlns:table="urn:oasis:names:tc:opendocument:xmlns:table:1.0"'
            ' xmlns:fo="urn:oasis:names:tc:opendocument:xmlns:xsl-fo-compatible:1.0"'
            ' office:version="1.3">',
            '<office:scripts/>',
            '<office:automatic-styles>',
            style_block(),
            '</office:automatic-styles>',
            '<office:body><office:spreadsheet>',
            table_xml(rows),
            '</office:spreadsheet></office:body>',
            '</office:document-content>',
        ]
    )


def build_styles_xml() -> str:
    return """<?xml version="1.0" encoding="UTF-8"?>
<office:document-styles xmlns:office="urn:oasis:names:tc:opendocument:xmlns:office:1.0"
    xmlns:style="urn:oasis:names:tc:opendocument:xmlns:style:1.0"
    xmlns:text="urn:oasis:names:tc:opendocument:xmlns:text:1.0"
    xmlns:table="urn:oasis:names:tc:opendocument:xmlns:table:1.0"
    xmlns:fo="urn:oasis:names:tc:opendocument:xmlns:xsl-fo-compatible:1.0"
    office:version="1.3">
  <office:styles>
    <style:default-style style:family="table-cell">
      <style:table-cell-properties fo:padding="0cm" style:vertical-align="middle"/>
      <style:text-properties fo:font-size="6pt"/>
    </style:default-style>
  </office:styles>
  <office:automatic-styles/>
  <office:master-styles/>
</office:document-styles>
"""


def build_meta_xml() -> str:
    return """<?xml version="1.0" encoding="UTF-8"?>
<office:document-meta xmlns:office="urn:oasis:names:tc:opendocument:xmlns:office:1.0"
    xmlns:meta="urn:oasis:names:tc:opendocument:xmlns:meta:1.0"
    office:version="1.3">
  <office:meta>
    <meta:generator>GitHub Copilot</meta:generator>
  </office:meta>
</office:document-meta>
"""


def build_settings_xml() -> str:
    return """<?xml version="1.0" encoding="UTF-8"?>
<office:document-settings xmlns:office="urn:oasis:names:tc:opendocument:xmlns:office:1.0"
    xmlns:config="urn:oasis:names:tc:opendocument:xmlns:config:1.0"
    office:version="1.3">
  <office:settings>
    <config:config-item-set config:name="ooo:view-settings"/>
  </office:settings>
</office:document-settings>
"""


def build_manifest_xml() -> str:
    return """<?xml version="1.0" encoding="UTF-8"?>
<manifest:manifest xmlns:manifest="urn:oasis:names:tc:opendocument:xmlns:manifest:1.0" manifest:version="1.3">
  <manifest:file-entry manifest:full-path="/" manifest:media-type="application/vnd.oasis.opendocument.spreadsheet"/>
  <manifest:file-entry manifest:full-path="content.xml" manifest:media-type="text/xml"/>
  <manifest:file-entry manifest:full-path="styles.xml" manifest:media-type="text/xml"/>
  <manifest:file-entry manifest:full-path="meta.xml" manifest:media-type="text/xml"/>
  <manifest:file-entry manifest:full-path="settings.xml" manifest:media-type="text/xml"/>
</manifest:manifest>
"""


def write_fods(rows: list[list[str]]) -> None:
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    FODS_PATH.write_text(build_flat_fods(rows), encoding="utf-8", newline="")


def write_ods(rows: list[list[str]]) -> Path:
    OUT_DIR.mkdir(parents=True, exist_ok=True)

    def write_archive(target_path: Path) -> None:
        with zipfile.ZipFile(target_path, "w") as archive:
            archive.writestr(
                zipfile.ZipInfo("mimetype"),
                "application/vnd.oasis.opendocument.spreadsheet",
                compress_type=zipfile.ZIP_STORED,
            )
            archive.writestr("content.xml", build_content_xml(rows), compress_type=zipfile.ZIP_DEFLATED)
            archive.writestr("styles.xml", build_styles_xml(), compress_type=zipfile.ZIP_DEFLATED)
            archive.writestr("meta.xml", build_meta_xml(), compress_type=zipfile.ZIP_DEFLATED)
            archive.writestr("settings.xml", build_settings_xml(), compress_type=zipfile.ZIP_DEFLATED)
            archive.writestr("META-INF/manifest.xml", build_manifest_xml(), compress_type=zipfile.ZIP_DEFLATED)

    try:
        write_archive(ODS_PATH)
        return ODS_PATH
    except PermissionError:
        fallback_path = ODS_PATH.with_name(f"{ODS_PATH.stem}_updated{ODS_PATH.suffix}")
        write_archive(fallback_path)
        print(f"Could not overwrite {ODS_PATH} because it is open; wrote {fallback_path} instead.")
        return fallback_path


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Generate or sync the spreadsheet-driven world map files.")
    parser.add_argument("--csv", type=Path, default=CSV_PATH, help="Path to the runtime CSV map file.")
    parser.add_argument("--fods", type=Path, default=FODS_PATH, help="Path to the flat XML spreadsheet file.")
    parser.add_argument("--ods", type=Path, default=ODS_PATH, help="Path to the Calc-native workbook file.")
    parser.add_argument(
        "--prefer-spreadsheet",
        action="store_true",
        help="Import tile data from the spreadsheet when it exists, even if the CSV is already present.",
    )
    return parser.parse_args()


if __name__ == "__main__":
    args = parse_args()
    configure_paths(args.csv, args.fods, args.ods)
    build_roads()
    rows = load_sheet_rows(prefer_spreadsheet=args.prefer_spreadsheet)
    write_csv(rows)
    write_fods(rows)
    ods_output_path = write_ods(rows)
    print(f"Updated {CSV_PATH}")
    print(f"Generated {FODS_PATH}")
    print(f"Generated {ods_output_path}")
