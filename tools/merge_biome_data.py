import csv

# File paths
main_csv = 'build-win/data/templates/maps/world_map_tiles.csv'
new_biome_csv = 'build-win/data/templates/maps/world_map_tiles_from_bmp.csv'
out_csv = 'build-win/data/templates/maps/world_map_tiles_merged.csv'

def split_biome_and_meta(cell):
    if '|' in cell:
        biome, meta = cell.split('|', 1)
        return biome, '|' + meta
    else:
        return cell, ''

def main():
    with open(main_csv, newline='', encoding='utf-8') as f_main, \
         open(new_biome_csv, newline='', encoding='utf-8') as f_new, \
         open(out_csv, 'w', newline='', encoding='utf-8') as f_out:
        reader_main = csv.reader(f_main)
        reader_new = csv.reader(f_new)
        writer = csv.writer(f_out)

        for row_main, row_new in zip(reader_main, reader_new):
            merged_row = []
            for cell_main, cell_new in zip(row_main, row_new):
                _, meta = split_biome_and_meta(cell_main)
                merged_row.append(cell_new + meta)
            writer.writerow(merged_row)

    print(f'Merged CSV written to: {out_csv}')

if __name__ == '__main__':
    main()
