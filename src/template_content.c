#include <stdio.h>
#include <string.h>

#include "template_content.h"
#include "item_data.h"
#include "furniture.h"
#include "race.h"
#include "bestiary.h"

#define TEMPLATE_PATH_MAX 260

static char g_template_error[256];

static void set_template_error(const char* message, const char* detail)
{
    if(detail && detail[0] != '\0')
        snprintf(g_template_error, sizeof(g_template_error), "%s: %s", message, detail);
    else
        snprintf(g_template_error, sizeof(g_template_error), "%s", message);
}

// Resolves a single file in the template roots (unchanged)
static int resolve_template_path(const char* relative_path, char* out_path, size_t out_size)
{
    static const char* roots[] = {
        "../data/templates",
        "../build/data/templates",
        "../build-win/data/templates",
        "../build-lin/data/templates",
        "data/templates",
        "build/data/templates",
        "build-win/data/templates",
        "build-lin/data/templates",
    };

    for(int i = 0; i < (int)(sizeof(roots) / sizeof(roots[0])); i++)
    {
        FILE* file;
        snprintf(out_path, out_size, "%s/%s", roots[i], relative_path);
        file = fopen(out_path, "r");
        if(file)
        {
            fclose(file);
            return 1;
        }
    }
    out_path[0] = '\0';
    return 0;
}

#include <stdlib.h>
#include <dirent.h>
#include <sys/types.h>

// Collect all .ini files from the first available items root.
// This keeps the source-of-truth order stable and avoids duplicate loads from build mirrors.
static int resolve_item_template_files(char out_paths[][TEMPLATE_PATH_MAX], int max_files)
{
    static const char* roots[] = {
        "../data/templates/items",
        "../build/data/templates/items",
        "../build-win/data/templates/items",
        "../build-lin/data/templates/items",
        "data/templates/items",
        "build/data/templates/items",
        "build-win/data/templates/items",
        "build-lin/data/templates/items",
    };
    int count = 0;

    for(int i = 0; i < (int)(sizeof(roots) / sizeof(roots[0])); i++) {
        DIR* dir = opendir(roots[i]);
        if(!dir)
            continue;

        struct dirent* entry;
        while((entry = readdir(dir)) != NULL) {
            size_t len = strlen(entry->d_name);
            if(len > 4 && strcmp(entry->d_name + len - 4, ".ini") == 0) {
                snprintf(out_paths[count], TEMPLATE_PATH_MAX, "%s/%s", roots[i], entry->d_name);
                count++;
                if(count >= max_files)
                    break;
            }
        }

        closedir(dir);

        if(count > 0 || count >= max_files)
            break;
    }
    return count;
}

int template_content_load_all(void)
{

    char item_paths[16][TEMPLATE_PATH_MAX];
    int item_file_count = 0;
    int loaded_any = 0;
    g_template_error[0] = '\0';

    // Find all item template files
    item_file_count = resolve_item_template_files(item_paths, 16);
    if(item_file_count == 0) {
        set_template_error("Missing item template files", "items/*.ini");
        return 0;
    }

    clear_item_templates();
    for(int i = 0; i < item_file_count; ++i) {
        if(!item_templates_load(item_paths[i])) {
            const char* detail = item_templates_last_error();
            if(detail && detail[0] != '\0')
                set_template_error("Failed to load item templates", detail);
            else
                set_template_error("Failed to load item templates", item_paths[i]);
            // Continue loading other files, but mark as error
        } else {
            loaded_any = 1;
        }
    }
    if(!loaded_any) {
        // All failed
        return 0;
    }

    char path[TEMPLATE_PATH_MAX];

    if(!resolve_template_path("furniture.ini", path, sizeof(path)))
    {
        set_template_error("Missing furniture template file", "furniture.ini");
        return 0;
    }

    clear_furniture_templates();
    if(!furniture_templates_load(path))
    {
        const char* detail = furniture_templates_last_error();
        if(detail && detail[0] != '\0')
            set_template_error("Failed to load furniture templates", detail);
        else
            set_template_error("Failed to load furniture templates", path);
        return 0;
    }

    if(!resolve_template_path("races.ini", path, sizeof(path)))
    {
        set_template_error("Missing race template file", "races.ini");
        return 0;
    }

    clear_race_templates();
    if(!race_templates_load(path))
    {
        const char* detail = race_templates_last_error();
        if(detail && detail[0] != '\0')
            set_template_error("Failed to load race templates", detail);
        else
            set_template_error("Failed to load race templates", path);
        return 0;
    }

    // Creatures (unchanged)
    if(!resolve_template_path("creatures.ini", path, sizeof(path)))
    {
        set_template_error("Missing creature template file", "creatures.ini");
        return 0;
    }
    if(!bestiary_templates_load(path))
    {
        set_template_error("Failed to load creature templates", path);
        return 0;
    }

    return 1;
}

const char* template_content_last_error(void)
{
    return g_template_error;
}