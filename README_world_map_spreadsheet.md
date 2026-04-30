# Spreadsheet-driven overworld setup

This project now uses a **master CSV template** in `master_data/templates/maps/` as the authoritative world map source.

- `master_data/templates/maps/world_map_tiles.csv` — master runtime map file the game reads and the preferred edit target
- `master_data/templates/maps/world_map_tiles.bin` — generated compact runtime map file for faster loading

Each non-comment cell represents **one world-map tile** and can hold all required tile data in one place.

---

## Runtime workflow

1. Edit `master_data/templates/maps/world_map_tiles.csv` with tile codes and map metadata.
2. Run `tools/generate_world_map_sheet.py` to refresh `world_map_tiles.bin`.
3. Build and run the game normally.

> The game now prefers `world_map_tiles.bin` when available for faster startup.
> The CSV file contains the tile text data only; styling is not part of the runtime asset.

---

## Tile cell format

Each tile cell can contain pipe-separated values like this:

```text
BIOME|loc=Name|type=TOWN|index=6|gen=PROCEDURAL|w=1000|h=1000|road=trail|river=major|lake=large
```

### Supported tile tokens

| Token | Meaning |
|---|---|
| `GR`, `FO`, etc. | biome short code for the tile |
| `FA` | farmland feature on a grasslands tile |
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
| `FA` | Grasslands + Farmland feature | `#D4B96E` |
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

Legacy water biome tokens (`RI`, `LA`, `RIVER`, `LAKE`, `r`, `l`) are auto-mapped to water features when loading older CSV files.

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

- `src/world_map.c` reads the unified tile CSV and applies biomes plus road/river/lake features. It now prefers `world_map_tiles.bin` when available for faster runtime loading.
- `src/atlas.c` reads location definitions directly from location-bearing cells.
- The runtime loader now requires `world_map_tiles.csv` and optionally loads `world_map_tiles.bin` for optimized startup.

## Notes

- The overworld is still fixed to `1000 x 1000` tiles.
- Partial CSV grids are allowed; unspecified cells keep their default values.
- For true persistent cell coloring, keep editing in **LibreOffice Calc** and export to CSV when ready to run the game.
