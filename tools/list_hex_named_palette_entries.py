from pathlib import Path
import re

text = Path('src/color_palette.c').read_text(encoding='utf-8')
for match in re.finditer(r'\{\s*(\d+),\s*"(#(?:[0-9A-Fa-f]{6}))"', text):
    print(match.group(1))
