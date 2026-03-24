#include <stdio.h>
#include <string.h>

#include "template_content.h"
#include "item_data.h"
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

static int resolve_template_path(const char* relative_path, char* out_path, size_t out_size)
{
    static const char* roots[] = {
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

int template_content_load_all(void)
{
    char path[TEMPLATE_PATH_MAX];

    g_template_error[0] = '\0';

    if(!resolve_template_path("items.ini", path, sizeof(path)))
    {
        set_template_error("Missing item template file", "items.ini");
        return 0;
    }
    if(!item_templates_load(path))
    {
        const char* detail = item_templates_last_error();
        if(detail && detail[0] != '\0')
            set_template_error("Failed to load item templates", detail);
        else
            set_template_error("Failed to load item templates", path);
        return 0;
    }

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