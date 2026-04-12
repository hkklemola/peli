# Spreadsheet-driven overworld setup

This project now supports a **LibreOffice Calc-native master workbook** plus export formats:

- `data/templates/maps/world_map_tiles.ods` — **recommended** editable Calc workbook with real biome background colors and square map cells
- `data/templates/maps/world_map_tiles.fods` — flat-XML variant for compatibility/debugging
- `data/templates/maps/world_map_tiles.csv` — runtime/export version the game reads

Each non-comment cell represents **one world-map tile** and can hold all required tile data in one place.

---

## LibreOffice Calc workflow

1. Open `world_map_tiles.ods` in **LibreOffice Calc**.
2. Edit the square map cells using the short biome codes and the built-in background colors.
3. When needed, export/save the sheet as `world_map_tiles.csv` for runtime use.
4. Build and run the game normally.

> When the game starts, it now checks whether `world_map_tiles.ods` / `.fods` is newer than `world_map_tiles.csv` and refreshes the CSV automatically before loading the world map.
>
> CSV stores the **cell text**, not the actual formatting. The color legend is included so you can apply it in Calc or through conditional formatting.

---

## Tile cell format

Each tile cell can contain pipe-separated values like this:

```text
BIOME|loc=Name|type=TOWN|index=6|gen=PROCEDURAL|w=1000|h=1000|road=trail|river=major|lake=large
```

### Supported tile tokens

| Token | Meaning |
|---|---|
| `GR`, `FO`, `FA`, etc. | biome short code for the tile |
| `loc=Name` | named location on this tile |
| `type=STARTER/TOWN/DUNGEON/CRYPT/CAVERN` | atlas location type |
| `index=0-7` | fixed atlas slot |
| `gen=PROCEDURAL` or `PREDEFINED` | generation mode |
| `w=1000`, `h=1000` | local area size |
| `road=trail/paved/highway` | road feature on this tile |
| `river=minor/major` | river feature on this tile |
| `lake=small/large` | lake feature on this tile |
| `map=...` | optional predefined map path |

---

## Biome short-code and color legend

The legend rows at the top of the spreadsheet now show:
- the **short code**
- the **full biome name**
- the **suggested color hex value**

| Code | Biome | Suggested Calc cell color |
|---|---|---|
| `GR` | Grasslands | `#7CB342` |
| `FO` | Forest | `#2E7D32` |
| `FA` | Farmlands | `#D4B96E` |
| `DE` | Desert | `#E6C97A` |
| `TU` | Tundra | `#CFD8DC` |
| `SE` | Sea | `#1565C0` |
| `SA` | Savannah | `#C0CA33` |
| `MO` | Mountains | `#8D8D8D` |
| `HI` / `FH` | Foothills | `#A1887F` |
| `SW` | Swamp | `#6D8B74` |
| `JU` | Jungle | `#1B5E20` |

Water features now use token tiers:
- `river=minor|major` (suggested color `#4FC3F7`)
- `lake=small|large` (suggested color `#29B6F6`)

Legacy water biome tokens (`RI`, `LA`, `RIVER`, `LAKE`, `r`, `l`) are auto-mapped to water features when loading old sheets.

---

## Example cells

```text
GR
GR|road=trail
FA|loc=Village|type=TOWN|index=6|gen=PROCEDURAL|road=trail|river=minor
MO|loc=Old Mine|type=CAVERN|index=4|gen=PROCEDURAL
```

---

## Runtime behavior

- `src/world_map.c` reads the unified tile CSV and applies biomes plus road/river/lake features.
- `src/atlas.c` reads location definitions directly from location-bearing cells.
- If the single-file sheet is missing, the older fallback files still work.

## Notes

- The overworld is still fixed to `1000 x 1000` tiles.
- Partial CSV grids are allowed; unspecified cells keep their default values.
- For true persistent cell coloring, keep editing in **LibreOffice Calc** and export to CSV when ready to run the game.
