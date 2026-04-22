from pathlib import Path
import re

# Deterministic single-name replacements from the provided palette list.
mapping = {
    1: 'MAROON',
    2: 'OFFICE_GREEN',
    3: 'YELLOW_003',
    4: 'BLUE_004',
    5: 'PATRIARCH',
    6: 'CYAN_006',
    7: 'ARGENT',
    20: 'MEDIUM_BLUE',
    28: 'AO',
    30: 'TEAL',
    31: 'DEEP_CERULEAN',
    38: 'CERULEAN',
    40: 'STRONG_LIME_GREEN',
    41: 'MALACHITE',
    42: 'CARIBBEAN_GREEN_042',
    45: 'VIVID_SKY_BLUE',
    47: 'SPRING_GREEN_047',
    48: 'GUPPIE_GREEN',
    63: 'CORNFLOWER_BLUE',
    64: 'AVOCADO',
    75: 'BLUE_JEANS',
    77: 'MODERATE_LIME_GREEN',
    78: 'CARIBBEAN_GREEN_PEARL',
    81: 'MAYA_BLUE',
    82: 'BRIGHT_GREEN',
    83: 'LIGHT_LIME_GREEN',
    84: 'VERY_LIGHT_MALACHITE_GREEN',
    85: 'MEDIUM_AQUAMARINE',
    86: 'AQUAMARINE_086',
    87: 'AQUAMARINE_087',
    88: 'DEEP_RED',
    89: 'FRENCH_PLUM',
    99: 'BLUEBERRY_099',
    102: 'TAUPE_GRAY',
    105: 'VIOLETS_ARE_BLUE',
    112: 'PISTACHIO',
    113: 'MANTIS',
    120: 'VERY_LIGHT_LIME_GREEN',
    121: 'MINT_GREEN_121',
    127: 'HELIOTROPE_MAGENTA',
    128: 'VIVID_MULBERRY',
    129: 'ELECTRIC_PURPLE',
    134: 'RICH_LILAC',
    141: 'BRIGHT_LAVENDER',
    145: 'SILVER_FOIL',
    163: 'HOLLYWOOD_CERISE_163',
    165: 'PHLOX',
    220: 'GOLD',
    232: 'VAMPIRE_BLACK',
    233: 'CHINESE_BLACK',
    234: 'EERIE_BLACK',
    235: 'RAISIN_BLACK',
    236: 'DARK_CHARCOAL',
    238: 'OUTER_SPACE',
    240: 'DAVYS_GREY',
    245: 'PHILIPPINE_GRAY',
    246: 'DUSTY_GRAY',
    247: 'SPANISH_GRAY',
    250: 'SILVER',
    251: 'SILVER_SAND',
}

source_path = Path('src/color_palette.c')
text = source_path.read_text(encoding='utf-8')
pattern = re.compile(r'(\{\s*(\d+),\s*")([^"]+)("\s*,\s*COLOR_PALETTE_GROUP_[A-Z_]+,\s*")([^"]+)("\s*\})')

changed = []
remaining_hex_indices = []


def replacement(match):
    idx = int(match.group(2))
    name = match.group(3)
    hex_code = match.group(5)
    if name.startswith('#') and idx in mapping:
        changed.append(idx)
        return f"{{ {idx}, \"{mapping[idx]}\", {match.group(4)}{hex_code}{match.group(6)}"
    if name.startswith('#'):
        remaining_hex_indices.append(idx)
    return match.group(0)

new_text = pattern.sub(replacement, text)
source_path.write_text(new_text, encoding='utf-8')
print(f"Replaced hex names for indices: {sorted(changed)}")
print(f"Remaining hex-named indices: {sorted(set(remaining_hex_indices))}")
