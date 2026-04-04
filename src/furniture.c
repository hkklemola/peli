#include "furniture.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "atlas.h"
#include "character.h"
#include "log.h"
#include "world_items.h"

static FurnitureTemplate g_furniture_templates[FURNITURE_TYPE_COUNT];
static int g_furniture_template_loaded[FURNITURE_TYPE_COUNT];
static char g_furniture_template_last_error[256];

static void set_furniture_template_error(const char* message, const char* detail)
{
    if(detail && detail[0] != '\0')
        snprintf(g_furniture_template_last_error, sizeof(g_furniture_template_last_error), "%s: %s", message, detail);
    else
        snprintf(g_furniture_template_last_error, sizeof(g_furniture_template_last_error), "%s", message);
}

static void trim_in_place(char* text)
{
    char* start;
    char* end;

    if(!text)
        return;

    start = text;
    while(*start && isspace((unsigned char)*start))
        start++;

    if(start != text)
        memmove(text, start, strlen(start) + 1);

    end = text + strlen(text);
    while(end > text && isspace((unsigned char)end[-1]))
        end--;
    *end = '\0';
}

static int equals_ignore_case(const char* left, const char* right)
{
    if(!left || !right)
        return 0;

    while(*left && *right)
    {
        unsigned char lc = (unsigned char)tolower((unsigned char)*left);
        unsigned char rc = (unsigned char)tolower((unsigned char)*right);
        if(lc != rc)
            return 0;
        left++;
        right++;
    }

    return *left == '\0' && *right == '\0';
}

static int starts_with_ignore_case(const char* text, const char* prefix)
{
    if(!text || !prefix)
        return 0;

    while(*prefix)
    {
        if(*text == '\0')
            return 0;
        if(tolower((unsigned char)*text) != tolower((unsigned char)*prefix))
            return 0;
        text++;
        prefix++;
    }

    return 1;
}

static void copy_text(char* out, size_t out_size, const char* text)
{
    if(!out || out_size == 0)
        return;

    snprintf(out, out_size, "%s", text ? text : "");
}

static const char* furniture_default_id(FurnitureType type)
{
    switch(type)
    {
        case FURNITURE_CHEST: return "chest";
        case FURNITURE_BARREL: return "barrel";
        case FURNITURE_CHAIR: return "chair";
        case FURNITURE_TABLE: return "table";
        case FURNITURE_DOOR: return "door";
        case FURNITURE_SIGNPOST: return "signpost";
        case FURNITURE_BED: return "bed";
        case FURNITURE_WARDROBE: return "wardrobe";
        case FURNITURE_WEAPON_RACK: return "weapon_rack";
        case FURNITURE_NONE:
        case FURNITURE_TYPE_COUNT:
        default:
            return "none";
    }
}

static const char* furniture_default_name(FurnitureType type)
{
    switch(type)
    {
        case FURNITURE_CHEST: return "Chest";
        case FURNITURE_BARREL: return "Barrel";
        case FURNITURE_CHAIR: return "Chair";
        case FURNITURE_TABLE: return "Table";
        case FURNITURE_DOOR: return "Door";
        case FURNITURE_SIGNPOST: return "Signpost";
        case FURNITURE_BED: return "Bed";
        case FURNITURE_WARDROBE: return "Wardrobe";
        case FURNITURE_WEAPON_RACK: return "Weapon Rack";
        case FURNITURE_NONE:
        case FURNITURE_TYPE_COUNT:
        default:
            return "Furniture";
    }
}

static void furniture_template_set_defaults(FurnitureTemplate* tmpl)
{
    if(!tmpl)
        return;

    memset(tmpl, 0, sizeof(*tmpl));
    tmpl->type = FURNITURE_NONE;
    tmpl->symbol = '?';
    tmpl->symbol_open = '\0';
    tmpl->color = RENDER_COLOR_DEFAULT;
    tmpl->open_blocks_movement = -1;
    tmpl->open_blocks_sight = -1;
    tmpl->open_blocks_projectile = -1;
    tmpl->interaction_type = FURNITURE_INTERACTION_NONE;
    copy_text(tmpl->name, sizeof(tmpl->name), "Furniture");
}

static int parse_boolean(const char* value, int* out)
{
    char* endptr = NULL;
    long numeric;

    if(!value || !out)
        return 0;

    if(equals_ignore_case(value, "true") || equals_ignore_case(value, "yes") || equals_ignore_case(value, "on"))
    {
        *out = 1;
        return 1;
    }
    if(equals_ignore_case(value, "false") || equals_ignore_case(value, "no") || equals_ignore_case(value, "off"))
    {
        *out = 0;
        return 1;
    }

    numeric = strtol(value, &endptr, 10);
    if(endptr && *endptr == '\0')
    {
        *out = (numeric != 0) ? 1 : 0;
        return 1;
    }

    return 0;
}

static int parse_render_color(const char* value, int* out)
{
    static const struct {
        const char* name;
        int color;
    } mappings[] = {
        { "BLACK", RENDER_COLOR_BLACK },
        { "RED", RENDER_COLOR_RED },
        { "GREEN", RENDER_COLOR_GREEN },
        { "BROWN", RENDER_COLOR_BROWN },
        { "BLUE", RENDER_COLOR_BLUE },
        { "MAGENTA", RENDER_COLOR_MAGENTA },
        { "CYAN", RENDER_COLOR_CYAN },
        { "LIGHT_GRAY", RENDER_COLOR_LIGHT_GRAY },
        { "DEFAULT", RENDER_COLOR_DEFAULT },
        { "DARK_GRAY", RENDER_COLOR_DARK_GRAY },
        { "LIGHT_RED", RENDER_COLOR_LIGHT_RED },
        { "LIGHT_GREEN", RENDER_COLOR_LIGHT_GREEN },
        { "LIGHT_YELLOW", RENDER_COLOR_LIGHT_YELLOW },
        { "LIGHT_BLUE", RENDER_COLOR_LIGHT_BLUE },
        { "LIGHT_MAGENTA", RENDER_COLOR_LIGHT_MAGENTA },
        { "LIGHT_CYAN", RENDER_COLOR_LIGHT_CYAN },
        { "WHITE", RENDER_COLOR_WHITE }
    };
    const char* normalized = value;
    char* endptr = NULL;
    long numeric;

    if(!value || !out)
        return 0;

    if(starts_with_ignore_case(normalized, "RENDER_COLOR_"))
        normalized += strlen("RENDER_COLOR_");

    for(int i = 0; i < (int)(sizeof(mappings) / sizeof(mappings[0])); i++)
    {
        if(equals_ignore_case(normalized, mappings[i].name))
        {
            *out = mappings[i].color;
            return 1;
        }
    }

    numeric = strtol(value, &endptr, 10);
    if(endptr && *endptr == '\0')
    {
        *out = (int)numeric;
        return 1;
    }

    return 0;
}

static int parse_furniture_type_value(const char* value, FurnitureType* out)
{
    static const struct {
        const char* name;
        FurnitureType type;
    } mappings[] = {
        { "NONE", FURNITURE_NONE },
        { "CHEST", FURNITURE_CHEST },
        { "BARREL", FURNITURE_BARREL },
        { "CHAIR", FURNITURE_CHAIR },
        { "TABLE", FURNITURE_TABLE },
        { "DOOR", FURNITURE_DOOR },
        { "SIGNPOST", FURNITURE_SIGNPOST },
        { "BED", FURNITURE_BED },
        { "WARDROBE", FURNITURE_WARDROBE },
        { "WEAPON_RACK", FURNITURE_WEAPON_RACK },
        { "WEAPON RACK", FURNITURE_WEAPON_RACK }
    };
    const char* normalized = value;
    char* endptr = NULL;
    long numeric;

    if(!value || !out)
        return 0;

    if(starts_with_ignore_case(normalized, "FURNITURE_"))
        normalized += strlen("FURNITURE_");

    for(int i = 0; i < (int)(sizeof(mappings) / sizeof(mappings[0])); i++)
    {
        if(equals_ignore_case(normalized, mappings[i].name))
        {
            *out = mappings[i].type;
            return 1;
        }
    }

    numeric = strtol(value, &endptr, 10);
    if(endptr && *endptr == '\0' && numeric >= FURNITURE_NONE && numeric < FURNITURE_TYPE_COUNT)
    {
        *out = (FurnitureType)numeric;
        return 1;
    }

    return 0;
}

static int parse_interaction_type(const char* value, FurnitureInteractionType* out)
{
    static const struct {
        const char* name;
        FurnitureInteractionType type;
    } mappings[] = {
        { "NONE", FURNITURE_INTERACTION_NONE },
        { "OPEN_CONTAINER", FURNITURE_INTERACTION_OPEN_CONTAINER },
        { "TOGGLE_DOOR", FURNITURE_INTERACTION_TOGGLE_DOOR },
        { "READ_SIGN", FURNITURE_INTERACTION_READ_SIGN },
        { "REST", FURNITURE_INTERACTION_REST },
        { "INSPECT", FURNITURE_INTERACTION_INSPECT },
        { "SIT", FURNITURE_INTERACTION_SIT }
    };

    if(!value || !out)
        return 0;

    for(int i = 0; i < (int)(sizeof(mappings) / sizeof(mappings[0])); i++)
    {
        if(equals_ignore_case(value, mappings[i].name))
        {
            *out = mappings[i].type;
            return 1;
        }
    }

    return 0;
}

static int finalize_furniture_template(FurnitureTemplate* tmpl)
{
    if(!tmpl)
        return 0;

    if(tmpl->type <= FURNITURE_NONE || tmpl->type >= FURNITURE_TYPE_COUNT)
    {
        set_furniture_template_error("Invalid or missing furniture id", "Use an `id=` value like chest, door, or signpost");
        return 0;
    }

    if(tmpl->id[0] == '\0')
        copy_text(tmpl->id, sizeof(tmpl->id), furniture_default_id(tmpl->type));
    if(tmpl->name[0] == '\0')
        copy_text(tmpl->name, sizeof(tmpl->name), furniture_default_name(tmpl->type));
    if(tmpl->open_name[0] == '\0')
        copy_text(tmpl->open_name, sizeof(tmpl->open_name), tmpl->name);
    if(tmpl->symbol_open == '\0')
        tmpl->symbol_open = tmpl->symbol;
    if(tmpl->open_blocks_movement < 0)
        tmpl->open_blocks_movement = tmpl->blocks_movement;
    if(tmpl->open_blocks_sight < 0)
        tmpl->open_blocks_sight = tmpl->blocks_sight;
    if(tmpl->open_blocks_projectile < 0)
        tmpl->open_blocks_projectile = tmpl->blocks_projectile;
    if(tmpl->interaction_label_open[0] == '\0')
        copy_text(tmpl->interaction_label_open, sizeof(tmpl->interaction_label_open), tmpl->interaction_label);
    if(tmpl->uses_container && tmpl->container_label[0] == '\0')
        copy_text(tmpl->container_label, sizeof(tmpl->container_label), tmpl->name);

    return 1;
}

static void furniture_apply_state(Furniture* f)
{
    const FurnitureTemplate* tmpl;

    if(!f)
        return;

    tmpl = f->template_data ? f->template_data : furniture_template_by_type(f->type);
    if(!tmpl)
    {
        f->base.base.symbol = '?';
        f->base.base.color = RENDER_COLOR_DEFAULT;
        f->interactable = 0;
        f->blocks_movement = 0;
        f->blocks_sight = 0;
        f->blocks_projectile = 0;
        f->base.base.blocks = 0;
        return;
    }

    f->template_data = tmpl;
    f->base.base.color = tmpl->color;
    f->interactable = tmpl->interactable;

    if(f->is_open)
    {
        f->base.base.symbol = tmpl->symbol_open;
        f->blocks_movement = tmpl->open_blocks_movement;
        f->blocks_sight = tmpl->open_blocks_sight;
        f->blocks_projectile = tmpl->open_blocks_projectile;
    }
    else
    {
        f->base.base.symbol = tmpl->symbol;
        f->blocks_movement = tmpl->blocks_movement;
        f->blocks_sight = tmpl->blocks_sight;
        f->blocks_projectile = tmpl->blocks_projectile;
    }

    f->base.base.blocks = f->blocks_movement;
}

const FurnitureTemplate* furniture_template_by_type(FurnitureType type)
{
    if(type <= FURNITURE_NONE || type >= FURNITURE_TYPE_COUNT)
        return NULL;
    if(!g_furniture_template_loaded[type])
        return NULL;
    return &g_furniture_templates[type];
}

const char* furniture_templates_last_error(void)
{
    return g_furniture_template_last_error;
}

void clear_furniture_templates(void)
{
    memset(g_furniture_templates, 0, sizeof(g_furniture_templates));
    memset(g_furniture_template_loaded, 0, sizeof(g_furniture_template_loaded));
    g_furniture_template_last_error[0] = '\0';
}

int furniture_templates_load(const char* path)
{
    FILE* file;
    char line[256];
    int line_number = 0;
    FurnitureTemplate current;
    int in_section = 0;

    if(!path || path[0] == '\0')
    {
        set_furniture_template_error("Missing furniture template path", NULL);
        return 0;
    }

    file = fopen(path, "r");
    if(!file)
    {
        set_furniture_template_error("Could not open furniture template file", path);
        return 0;
    }

    clear_furniture_templates();
    furniture_template_set_defaults(&current);

    while(fgets(line, sizeof(line), file))
    {
        char* equals;
        char* key;
        char* value;
        char detail[128];

        line_number++;
        trim_in_place(line);

        if(line[0] == '\0' || line[0] == '#' || line[0] == ';')
            continue;

        if(line[0] == '[')
        {
            if(in_section)
            {
                if(!finalize_furniture_template(&current))
                {
                    fclose(file);
                    return 0;
                }
                g_furniture_templates[current.type] = current;
                g_furniture_template_loaded[current.type] = 1;
            }

            if(equals_ignore_case(line, "[furniture]"))
            {
                furniture_template_set_defaults(&current);
                in_section = 1;
            }
            else
            {
                in_section = 0;
            }
            continue;
        }

        if(!in_section)
            continue;

        equals = strchr(line, '=');
        if(!equals)
        {
            snprintf(detail, sizeof(detail), "%s (line %d)", path, line_number);
            set_furniture_template_error("Malformed furniture template line", detail);
            fclose(file);
            return 0;
        }

        *equals = '\0';
        key = line;
        value = equals + 1;
        trim_in_place(key);
        trim_in_place(value);

        if(equals_ignore_case(key, "id") || equals_ignore_case(key, "type"))
        {
            if(!parse_furniture_type_value(value, &current.type))
            {
                snprintf(detail, sizeof(detail), "%s (line %d)", value, line_number);
                set_furniture_template_error("Unknown furniture id", detail);
                fclose(file);
                return 0;
            }
            copy_text(current.id, sizeof(current.id), furniture_default_id(current.type));
        }
        else if(equals_ignore_case(key, "name"))
        {
            copy_text(current.name, sizeof(current.name), value);
        }
        else if(equals_ignore_case(key, "open_name"))
        {
            copy_text(current.open_name, sizeof(current.open_name), value);
        }
        else if(equals_ignore_case(key, "symbol") || equals_ignore_case(key, "symbol_closed"))
        {
            if(value[0] == '\0')
            {
                snprintf(detail, sizeof(detail), "%s (line %d)", path, line_number);
                set_furniture_template_error("Furniture symbol cannot be empty", detail);
                fclose(file);
                return 0;
            }
            current.symbol = value[0];
        }
        else if(equals_ignore_case(key, "symbol_open"))
        {
            if(value[0] == '\0')
            {
                snprintf(detail, sizeof(detail), "%s (line %d)", path, line_number);
                set_furniture_template_error("Furniture open symbol cannot be empty", detail);
                fclose(file);
                return 0;
            }
            current.symbol_open = value[0];
        }
        else if(equals_ignore_case(key, "color"))
        {
            if(!parse_render_color(value, &current.color))
            {
                snprintf(detail, sizeof(detail), "%s (line %d)", value, line_number);
                set_furniture_template_error("Unknown furniture color", detail);
                fclose(file);
                return 0;
            }
        }
        else if(equals_ignore_case(key, "interactable"))
        {
            if(!parse_boolean(value, &current.interactable))
            {
                snprintf(detail, sizeof(detail), "%s (line %d)", value, line_number);
                set_furniture_template_error("Invalid interactable value", detail);
                fclose(file);
                return 0;
            }
        }
        else if(equals_ignore_case(key, "blocks_movement"))
        {
            if(!parse_boolean(value, &current.blocks_movement))
            {
                snprintf(detail, sizeof(detail), "%s (line %d)", value, line_number);
                set_furniture_template_error("Invalid blocks_movement value", detail);
                fclose(file);
                return 0;
            }
        }
        else if(equals_ignore_case(key, "blocks_sight"))
        {
            if(!parse_boolean(value, &current.blocks_sight))
            {
                snprintf(detail, sizeof(detail), "%s (line %d)", value, line_number);
                set_furniture_template_error("Invalid blocks_sight value", detail);
                fclose(file);
                return 0;
            }
        }
        else if(equals_ignore_case(key, "blocks_projectile"))
        {
            if(!parse_boolean(value, &current.blocks_projectile))
            {
                snprintf(detail, sizeof(detail), "%s (line %d)", value, line_number);
                set_furniture_template_error("Invalid blocks_projectile value", detail);
                fclose(file);
                return 0;
            }
        }
        else if(equals_ignore_case(key, "open_blocks_movement"))
        {
            if(!parse_boolean(value, &current.open_blocks_movement))
            {
                snprintf(detail, sizeof(detail), "%s (line %d)", value, line_number);
                set_furniture_template_error("Invalid open_blocks_movement value", detail);
                fclose(file);
                return 0;
            }
        }
        else if(equals_ignore_case(key, "open_blocks_sight"))
        {
            if(!parse_boolean(value, &current.open_blocks_sight))
            {
                snprintf(detail, sizeof(detail), "%s (line %d)", value, line_number);
                set_furniture_template_error("Invalid open_blocks_sight value", detail);
                fclose(file);
                return 0;
            }
        }
        else if(equals_ignore_case(key, "open_blocks_projectile"))
        {
            if(!parse_boolean(value, &current.open_blocks_projectile))
            {
                snprintf(detail, sizeof(detail), "%s (line %d)", value, line_number);
                set_furniture_template_error("Invalid open_blocks_projectile value", detail);
                fclose(file);
                return 0;
            }
        }
        else if(equals_ignore_case(key, "uses_container"))
        {
            if(!parse_boolean(value, &current.uses_container))
            {
                snprintf(detail, sizeof(detail), "%s (line %d)", value, line_number);
                set_furniture_template_error("Invalid uses_container value", detail);
                fclose(file);
                return 0;
            }
        }
        else if(equals_ignore_case(key, "container_label"))
        {
            copy_text(current.container_label, sizeof(current.container_label), value);
        }
        else if(equals_ignore_case(key, "interaction_type"))
        {
            if(!parse_interaction_type(value, &current.interaction_type))
            {
                snprintf(detail, sizeof(detail), "%s (line %d)", value, line_number);
                set_furniture_template_error("Unknown furniture interaction type", detail);
                fclose(file);
                return 0;
            }
        }
        else if(equals_ignore_case(key, "interaction_label"))
        {
            copy_text(current.interaction_label, sizeof(current.interaction_label), value);
        }
        else if(equals_ignore_case(key, "interaction_label_open"))
        {
            copy_text(current.interaction_label_open, sizeof(current.interaction_label_open), value);
        }
    }

    if(in_section)
    {
        if(!finalize_furniture_template(&current))
        {
            fclose(file);
            return 0;
        }
        g_furniture_templates[current.type] = current;
        g_furniture_template_loaded[current.type] = 1;
    }

    fclose(file);

    for(int type = FURNITURE_CHEST; type < FURNITURE_TYPE_COUNT; type++)
    {
        if(!g_furniture_template_loaded[type])
        {
            set_furniture_template_error("Missing furniture template", furniture_default_id((FurnitureType)type));
            return 0;
        }
    }

    return 1;
}

int furniture_uses_container_type(FurnitureType type)
{
    const FurnitureTemplate* tmpl = furniture_template_by_type(type);
    return (tmpl && tmpl->uses_container) ? 1 : 0;
}

FurnitureInteractionType furniture_interaction_type(const Furniture* furniture)
{
    const FurnitureTemplate* tmpl;

    if(!furniture)
        return FURNITURE_INTERACTION_NONE;

    tmpl = furniture->template_data ? furniture->template_data : furniture_template_by_type(furniture->type);
    return tmpl ? tmpl->interaction_type : FURNITURE_INTERACTION_NONE;
}

const char* furniture_display_name(const Furniture* furniture)
{
    const FurnitureTemplate* tmpl;

    if(!furniture)
        return "Furniture";

    tmpl = furniture->template_data ? furniture->template_data : furniture_template_by_type(furniture->type);
    if(!tmpl)
        return furniture_default_name(furniture->type);

    if(furniture->is_open && tmpl->open_name[0] != '\0')
        return tmpl->open_name;
    if(tmpl->name[0] != '\0')
        return tmpl->name;
    return furniture_default_name(furniture->type);
}

const char* furniture_container_label_for_type(FurnitureType type)
{
    const FurnitureTemplate* tmpl = furniture_template_by_type(type);

    if(tmpl && tmpl->container_label[0] != '\0')
        return tmpl->container_label;
    if(tmpl && tmpl->name[0] != '\0')
        return tmpl->name;
    return "Container";
}

void furniture_get_interaction_label(const Furniture* furniture, char* out, size_t out_size)
{
    const FurnitureTemplate* tmpl;
    const char* fallback = "Use furniture";

    if(!out || out_size == 0)
        return;

    out[0] = '\0';

    if(!furniture)
    {
        copy_text(out, out_size, fallback);
        return;
    }

    tmpl = furniture->template_data ? furniture->template_data : furniture_template_by_type(furniture->type);
    if(!tmpl)
    {
        copy_text(out, out_size, fallback);
        return;
    }

    if(furniture->is_open && tmpl->interaction_label_open[0] != '\0')
        copy_text(out, out_size, tmpl->interaction_label_open);
    else if(tmpl->interaction_label[0] != '\0')
        copy_text(out, out_size, tmpl->interaction_label);
    else
        copy_text(out, out_size, fallback);
}

void furniture_init(Furniture* f, FurnitureType type, int x, int y)
{
    furniture_init_at_z(f, type, x, y, AREA_GROUND_Z);
}

void furniture_init_at_z(Furniture* f, FurnitureType type, int x, int y, int z)
{
    if(!f)
        return;

    object_init(&f->base);
    f->base.base.x = x;
    f->base.base.y = y;
    f->base.base.z = z;
    f->base.base.hide_below = 1;
    f->type = type;
    f->template_data = furniture_template_by_type(type);
    f->is_open = 0;
    f->world_container_index = -1;

    furniture_apply_state(f);
}

Furniture* furniture_at_3d(const Area* area, int x, int y, int z)
{
    if(!area || x < 0 || y < 0 || x >= area->width || y >= area->height)
        return NULL;

    for(int i = 0; i < area->furniture_count; ++i)
    {
        Furniture* f = (Furniture*)&area->furniture[i];
        if(f->type != FURNITURE_NONE &&
           f->base.base.x == x &&
           f->base.base.y == y &&
           f->base.base.z == z)
            return f;
    }

    return NULL;
}

Furniture* furniture_at(const Area* area, int x, int y)
{
    int active_z = character_z();

    if(active_z < AREA_GROUND_Z || active_z > AREA_MAX_Z)
        active_z = AREA_GROUND_Z;

    return furniture_at_3d(area, x, y, active_z);
}

int furniture_spawn(Area* area, FurnitureType type, int x, int y)
{
    return furniture_spawn_at_z(area, type, x, y, AREA_GROUND_Z);
}

int furniture_spawn_at_z(Area* area, FurnitureType type, int x, int y, int z)
{
    if(!area || x < 0 || y < 0 || x >= area->width || y >= area->height)
        return -1;

    if(area->furniture_count >= MAX_AREA_FURNITURE)
        return -1;

    if(furniture_at_3d(area, x, y, z))
        return -1;

    Furniture* f = &area->furniture[area->furniture_count];
    furniture_init_at_z(f, type, x, y, z);

    if(furniture_uses_container_type(type))
    {
        const char* label = furniture_container_label_for_type(type);
        int idx = world_container_spawn_3d(area->name, x, y, f->base.base.z, label);
        if(idx >= 0)
        {
            f->world_container_index = idx;
        }
        else
        {
            f->interactable = 0;
            log_add("[ERROR] Could not create container for %s at %d,%d (z=%d) in %s (container capacity reached).",
                    label,
                    x,
                    y,
                    z,
                    area->name);
        }
    }

    area->furniture_count++;
    return area->furniture_count - 1;
}

void furniture_clear(Area* area)
{
    if(!area)
        return;

    area->furniture_count = 0;
}

int furniture_toggle_door(Area* area, int x, int y)
{
    Furniture* f;

    if(!area)
        return 0;

    f = furniture_at(area, x, y);
    if(!f || furniture_interaction_type(f) != FURNITURE_INTERACTION_TOGGLE_DOOR)
        return 0;

    f->is_open = !f->is_open;
    furniture_apply_state(f);
    return 1;
}