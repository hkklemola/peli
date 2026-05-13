#include "plant.h"
#include "atlas.h"
#include "spawn.h"
#include "tile.h"
#include "log.h"
#include "player.h"
#include "render_color.h"
#include "map.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

static unsigned int s_plant_turn_tick = 0;
static const PlantTemplate PLANT_TEMPLATE_DEFAULTS[PLANT_TYPE_COUNT] = {
    [PLANT_TYPE_NONE] = {
        .type = PLANT_TYPE_NONE,
        .name = "None",
        .symbol = ' ',
        .stump_symbol = ' ',
        .color = RENDER_COLOR_DEFAULT,
        .stump_color = RENDER_COLOR_DEFAULT,
        .blocks_movement = 0,
        .blocks_sight = 0,
        .blocks_projectile = 0,
        .harvestable = 0,
        .max_health = 0,
        .min_height = 0,
        .max_height = 0,
        .max_growth_stage = 0,
        .growth_turns_per_stage = 0,
    },
    [PLANT_TYPE_TREE] = {
        .type = PLANT_TYPE_TREE,
        .name = "Tree",
        .symbol = 'T',
        .stump_symbol = 't',
        .color = RENDER_COLOR_GREEN,
        .stump_color = RENDER_COLOR_BROWN,
        .blocks_movement = 1,
        .blocks_sight = 1,
        .blocks_projectile = 1,
        .harvestable = 0,
        .max_health = 10,
        .min_height = 1,
        .max_height = 4,
        .max_growth_stage = 1,
        .growth_turns_per_stage = 0,
    },
    [PLANT_TYPE_BUSH] = {
        .type = PLANT_TYPE_BUSH,
        .name = "Bush",
        .symbol = '%',
        .stump_symbol = '%',
        .color = RENDER_COLOR_LIGHT_GREEN,
        .stump_color = RENDER_COLOR_LIGHT_GREEN,
        .blocks_movement = 0,
        .blocks_sight = 0,
        .blocks_projectile = 0,
        .harvestable = 1,
        .max_health = 4,
        .min_height = 1,
        .max_height = 1,
        .max_growth_stage = 1,
        .growth_turns_per_stage = 60,
    },
    [PLANT_TYPE_HERB] = {
        .type = PLANT_TYPE_HERB,
        .name = "Herb Patch",
        .symbol = '"',
        .stump_symbol = '"',
        .color = RENDER_COLOR_LIGHT_GREEN,
        .stump_color = RENDER_COLOR_LIGHT_GREEN,
        .blocks_movement = 0,
        .blocks_sight = 0,
        .blocks_projectile = 0,
        .harvestable = 1,
        .max_health = 3,
        .min_height = 1,
        .max_height = 1,
        .max_growth_stage = 1,
        .growth_turns_per_stage = 60,
    },
    [PLANT_TYPE_FLOWER] = {
        .type = PLANT_TYPE_FLOWER,
        .name = "Flower Patch",
        .symbol = '*',
        .stump_symbol = '*',
        .color = RENDER_COLOR_LIGHT_RED,
        .stump_color = RENDER_COLOR_LIGHT_RED,
        .blocks_movement = 0,
        .blocks_sight = 0,
        .blocks_projectile = 0,
        .harvestable = 1,
        .max_health = 3,
        .min_height = 1,
        .max_height = 1,
        .max_growth_stage = 1,
        .growth_turns_per_stage = 60,
    },
};

static PlantTemplate g_plant_templates[PLANT_TYPE_COUNT];
static int g_plant_template_loaded[PLANT_TYPE_COUNT];
static char g_plant_template_name_buffers[PLANT_TYPE_COUNT][64];
static char g_plant_template_last_error[256];

static void set_plant_template_error(const char* message, const char* detail)
{
    if(!message)
        return;

    if(detail && detail[0] != '\0')
        snprintf(g_plant_template_last_error, sizeof(g_plant_template_last_error), "%s: %s", message, detail);
    else
        snprintf(g_plant_template_last_error, sizeof(g_plant_template_last_error), "%s", message);
}

static void trim_in_place(char* text)
{
    if(!text)
        return;

    char* end = text + strlen(text);
    while(end > text && (end[-1] == '\n' || end[-1] == '\r' || end[-1] == ' ' || end[-1] == '\t'))
        end--;
    *end = '\0';

    char* start = text;
    while(*start == ' ' || *start == '\t')
        start++;
    if(start != text)
        memmove(text, start, strlen(start) + 1);
}

static int equals_ignore_case(const char* left, const char* right)
{
    if(!left || !right)
        return 0;

    while(*left && *right)
    {
        char a = *left++;
        char b = *right++;
        if(a >= 'A' && a <= 'Z') a += 'a' - 'A';
        if(b >= 'A' && b <= 'Z') b += 'a' - 'A';
        if(a != b)
            return 0;
    }

    return *left == *right;
}

static int starts_with_ignore_case(const char* text, const char* prefix)
{
    if(!text || !prefix)
        return 0;

    while(*prefix)
    {
        char a = *text++;
        char b = *prefix++;
        if(a >= 'A' && a <= 'Z') a += 'a' - 'A';
        if(b >= 'A' && b <= 'Z') b += 'a' - 'A';
        if(a != b)
            return 0;
    }
    return 1;
}

static void copy_text(char* out, size_t out_size, const char* text)
{
    if(!out || out_size == 0)
        return;
    snprintf(out, out_size, "%s", text ? text : "");
}

static int parse_boolean(const char* value, int* out)
{
    if(!value || !out)
        return 0;

    if(equals_ignore_case(value, "true") || equals_ignore_case(value, "yes") || equals_ignore_case(value, "on") || equals_ignore_case(value, "1"))
    {
        *out = 1;
        return 1;
    }
    if(equals_ignore_case(value, "false") || equals_ignore_case(value, "no") || equals_ignore_case(value, "off") || equals_ignore_case(value, "0"))
    {
        *out = 0;
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

    if(!value || !out)
        return 0;

    const char* normalized = value;
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

    char* endptr = NULL;
    long numeric = strtol(value, &endptr, 10);
    if(endptr && *endptr == '\0')
    {
        *out = (int)numeric;
        return 1;
    }

    return 0;
}

static int parse_plant_type_value(const char* value, PlantType* out)
{
    if(!value || !out)
        return 0;

    if(equals_ignore_case(value, "tree"))
    {
        *out = PLANT_TYPE_TREE;
        return 1;
    }
    if(equals_ignore_case(value, "bush"))
    {
        *out = PLANT_TYPE_BUSH;
        return 1;
    }
    if(equals_ignore_case(value, "herb"))
    {
        *out = PLANT_TYPE_HERB;
        return 1;
    }
    if(equals_ignore_case(value, "flower"))
    {
        *out = PLANT_TYPE_FLOWER;
        return 1;
    }
    if(equals_ignore_case(value, "none"))
    {
        *out = PLANT_TYPE_NONE;
        return 1;
    }

    return 0;
}

void clear_plant_templates(void)
{
    memset(g_plant_template_loaded, 0, sizeof(g_plant_template_loaded));
    g_plant_template_last_error[0] = '\0';
    for(int i = 0; i < PLANT_TYPE_COUNT; ++i)
    {
        g_plant_templates[i] = PLANT_TEMPLATE_DEFAULTS[i];
        g_plant_template_name_buffers[i][0] = '\0';
    }
}

const char* plant_templates_last_error(void)
{
    return g_plant_template_last_error;
}

static int finalize_plant_template(PlantType type, PlantTemplate* tmpl, const char* name_buffer, char* name_storage, size_t name_storage_size, const char* path, int line_number)
{
    char detail[128];

    if(!tmpl)
        return 0;
    if(type <= PLANT_TYPE_NONE || type >= PLANT_TYPE_COUNT)
    {
        snprintf(detail, sizeof(detail), "%s (line %d)", path, line_number);
        set_plant_template_error("Unknown plant type", detail);
        return 0;
    }
    if(!name_buffer || name_buffer[0] == '\0')
    {
        snprintf(detail, sizeof(detail), "%s (line %d)", path, line_number);
        set_plant_template_error("Plant name is required", detail);
        return 0;
    }
    if(tmpl->symbol == '\0')
    {
        snprintf(detail, sizeof(detail), "%s (line %d)", path, line_number);
        set_plant_template_error("Plant symbol is required", detail);
        return 0;
    }
    if(tmpl->color == RENDER_COLOR_DEFAULT && !equals_ignore_case(name_buffer, "None"))
    {
        // default color is valid, but we still allow it if explicitly provided by name.
    }

    if(tmpl->stump_symbol == '\0')
        tmpl->stump_symbol = tmpl->symbol;
    if(tmpl->stump_color == RENDER_COLOR_DEFAULT)
        tmpl->stump_color = tmpl->color;

    if(tmpl->max_growth_stage <= 0)
        tmpl->max_growth_stage = 1;
    if(tmpl->growth_turns_per_stage < 0)
        tmpl->growth_turns_per_stage = 0;

    tmpl->type = type;
    copy_text(name_storage, name_storage_size, name_buffer);
    tmpl->name = name_storage;

    g_plant_templates[type] = *tmpl;
    g_plant_template_loaded[type] = 1;
    return 1;
}

int plant_templates_load(const char* path)
{
    FILE* file;
    char line[256];
    int line_number = 0;
    PlantTemplate current = {0};
    char current_name[64] = "";
    PlantType current_type = PLANT_TYPE_NONE;
    int in_section = 0;
    int loaded_any = 0;

    if(!path || path[0] == '\0')
    {
        set_plant_template_error("Missing plant template path", NULL);
        return 0;
    }

    file = fopen(path, "r");
    if(!file)
    {
        set_plant_template_error("Could not open plant template file", path);
        return 0;
    }

    clear_plant_templates();
    current = (PlantTemplate){0};
    current_name[0] = '\0';
    current_type = PLANT_TYPE_NONE;

    while(fgets(line, sizeof(line), file))
    {
        char* equals;
        char* key;
        char* value;

        line_number++;
        trim_in_place(line);

        if(line[0] == '\0' || line[0] == '#' || line[0] == ';')
            continue;

        if(line[0] == '[')
        {
            if(in_section)
            {
                if(!finalize_plant_template(current_type, &current, current_name, g_plant_template_name_buffers[current_type], sizeof(g_plant_template_name_buffers[current_type]), path, line_number - 1))
                {
                    fclose(file);
                    return 0;
                }
                loaded_any = 1;
            }

            if(equals_ignore_case(line, "[plant]"))
            {
                current = (PlantTemplate){0};
                current_name[0] = '\0';
                current_type = PLANT_TYPE_NONE;
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
            char detail[128];
            snprintf(detail, sizeof(detail), "%s (line %d)", path, line_number);
            set_plant_template_error("Malformed plant template line", detail);
            fclose(file);
            return 0;
        }

        *equals = '\0';
        key = line;
        value = equals + 1;
        trim_in_place(key);
        trim_in_place(value);

        if(equals_ignore_case(key, "type") || equals_ignore_case(key, "id"))
        {
            if(!parse_plant_type_value(value, &current_type))
            {
                char detail[128];
                snprintf(detail, sizeof(detail), "%s (line %d)", value, line_number);
                set_plant_template_error("Unknown plant type", detail);
                fclose(file);
                return 0;
            }
        }
        else if(equals_ignore_case(key, "name"))
        {
            copy_text(current_name, sizeof(current_name), value);
        }
        else if(equals_ignore_case(key, "symbol"))
        {
            if(value[0] == '\0')
            {
                char detail[128];
                snprintf(detail, sizeof(detail), "%s (line %d)", path, line_number);
                set_plant_template_error("Plant symbol cannot be empty", detail);
                fclose(file);
                return 0;
            }
            current.symbol = value[0];
        }
        else if(equals_ignore_case(key, "stump_symbol"))
        {
            if(value[0] == '\0')
            {
                char detail[128];
                snprintf(detail, sizeof(detail), "%s (line %d)", path, line_number);
                set_plant_template_error("Plant stump symbol cannot be empty", detail);
                fclose(file);
                return 0;
            }
            current.stump_symbol = value[0];
        }
        else if(equals_ignore_case(key, "color"))
        {
            if(!parse_render_color(value, &current.color))
            {
                char detail[128];
                snprintf(detail, sizeof(detail), "%s (line %d)", value, line_number);
                set_plant_template_error("Unknown plant color", detail);
                fclose(file);
                return 0;
            }
        }
        else if(equals_ignore_case(key, "stump_color"))
        {
            if(!parse_render_color(value, &current.stump_color))
            {
                char detail[128];
                snprintf(detail, sizeof(detail), "%s (line %d)", value, line_number);
                set_plant_template_error("Unknown plant stump color", detail);
                fclose(file);
                return 0;
            }
        }
        else if(equals_ignore_case(key, "blocks_movement"))
        {
            if(!parse_boolean(value, &current.blocks_movement))
            {
                char detail[128];
                snprintf(detail, sizeof(detail), "%s (line %d)", value, line_number);
                set_plant_template_error("Invalid blocks_movement value", detail);
                fclose(file);
                return 0;
            }
        }
        else if(equals_ignore_case(key, "blocks_sight"))
        {
            if(!parse_boolean(value, &current.blocks_sight))
            {
                char detail[128];
                snprintf(detail, sizeof(detail), "%s (line %d)", value, line_number);
                set_plant_template_error("Invalid blocks_sight value", detail);
                fclose(file);
                return 0;
            }
        }
        else if(equals_ignore_case(key, "blocks_projectile"))
        {
            if(!parse_boolean(value, &current.blocks_projectile))
            {
                char detail[128];
                snprintf(detail, sizeof(detail), "%s (line %d)", value, line_number);
                set_plant_template_error("Invalid blocks_projectile value", detail);
                fclose(file);
                return 0;
            }
        }
        else if(equals_ignore_case(key, "harvestable"))
        {
            if(!parse_boolean(value, &current.harvestable))
            {
                char detail[128];
                snprintf(detail, sizeof(detail), "%s (line %d)", value, line_number);
                set_plant_template_error("Invalid harvestable value", detail);
                fclose(file);
                return 0;
            }
        }
        else if(equals_ignore_case(key, "max_health"))
        {
            current.max_health = atoi(value);
        }
        else if(equals_ignore_case(key, "min_height"))
        {
            current.min_height = atoi(value);
        }
        else if(equals_ignore_case(key, "max_height"))
        {
            current.max_height = atoi(value);
        }
        else if(equals_ignore_case(key, "max_growth_stage"))
        {
            current.max_growth_stage = atoi(value);
        }
        else if(equals_ignore_case(key, "growth_turns_per_stage"))
        {
            current.growth_turns_per_stage = atoi(value);
        }
    }

    if(in_section)
    {
        if(!finalize_plant_template(current_type, &current, current_name, g_plant_template_name_buffers[current_type], sizeof(g_plant_template_name_buffers[current_type]), path, line_number))
        {
            fclose(file);
            return 0;
        }
        loaded_any = 1;
    }

    fclose(file);
    if(!loaded_any)
    {
        set_plant_template_error("No plant templates found", path);
        return 0;
    }

    return 1;
}

Plant* plant_at_3d(Area* area, int x, int y, int z)
{
    if(!area)
        return NULL;

    for(int i = 0; i < MAX_AREA_PLANTS; ++i)
    {
        Plant* plant = &area->plants[i];
        if(!plant->active)
            continue;
        if(plant->entity.x == x && plant->entity.y == y && plant->entity.z == z)
            return plant;
    }

    return NULL;
}

Plant* plant_find_free_slot(Area* area, PlantType type)
{
    if(!area)
        return NULL;

    int start_index = 0;
    int limit = 0;

    switch(type)
    {
        case PLANT_TYPE_TREE:
            start_index = 0;
            limit = MAX_AREA_TREES;
            break;
        case PLANT_TYPE_BUSH:
            start_index = MAX_AREA_TREES;
            limit = MAX_AREA_BUSHES;
            break;
        case PLANT_TYPE_HERB:
            start_index = MAX_AREA_TREES + MAX_AREA_BUSHES;
            limit = MAX_AREA_HERBS;
            break;
        case PLANT_TYPE_FLOWER:
            start_index = MAX_AREA_TREES + MAX_AREA_BUSHES + MAX_AREA_HERBS;
            limit = MAX_AREA_FLOWERS;
            break;
        default:
            return NULL;
    }

    for(int i = 0; i < limit; ++i)
    {
        Plant* plant = &area->plants[start_index + i];
        if(!plant->active)
            return plant;
    }

    return NULL;
}

const PlantTemplate* plant_template_for_type(PlantType type)
{
    if(type <= PLANT_TYPE_NONE || type >= PLANT_TYPE_COUNT)
        return NULL;
    return &g_plant_templates[type];
}

const PlantTemplate* plant_template_for_species(TreeSpecies species)
{
    if(species <= TREE_SPECIES_NONE || species >= TREE_SPECIES_COUNT)
        return plant_template_for_type(PLANT_TYPE_TREE);
    return plant_template_for_type(PLANT_TYPE_TREE);
}

Plant* plant_init_at_3d(Area* area, Plant* plant, PlantType type, TreeSpecies species, int x, int y, int z)
{
    const PlantTemplate* template_data;
    const TreeSpeciesInfo* species_info;

    if(!area || !plant || type <= PLANT_TYPE_NONE || type >= PLANT_TYPE_COUNT)
        return NULL;

    if(type == PLANT_TYPE_TREE && (species <= TREE_SPECIES_NONE || species >= TREE_SPECIES_COUNT))
        species = TREE_SPECIES_OAK;

    template_data = plant_template_for_type(type);
    if(!template_data)
        return NULL;

    plant->active = 1;
    plant->entity.x = x;
    plant->entity.y = y;
    plant->entity.z = z;
    plant->entity.id = spawn_next_entity_id();
    plant->entity.symbol = template_data->symbol;
    plant->entity.color = template_data->color;
    plant->entity.blocks = template_data->blocks_movement;
    plant->entity.layer = TILE_LAYER_EFFECT;
    plant->entity.hide_below = 1;
    plant->type = type;
    plant->template_data = template_data;
    plant->species = species;
    plant->state = PLANT_STATE_MATURE;
    plant->growth_stage = template_data->max_growth_stage;
    plant->growth_progress = 0;
    plant->harvest_cooldown = 0;

    if(type == PLANT_TYPE_TREE)
    {
        species_info = tree_species_info(species);
        plant->max_health = species_info->max_structure_points;
        plant->health = plant->max_health;
        plant->height = species_info->height > 1 ? 1 + (rand() % species_info->height) : 1;
    }
    else
    {
        plant->max_health = template_data->max_health;
        plant->health = plant->max_health;
        plant->height = template_data->min_height;
    }

    area->plant_count++;
    switch(type)
    {
        case PLANT_TYPE_TREE:
            area->tree_plant_count++;
            break;
        case PLANT_TYPE_BUSH:
            area->bush_plant_count++;
            break;
        case PLANT_TYPE_HERB:
            area->herb_plant_count++;
            break;
        case PLANT_TYPE_FLOWER:
            area->flower_plant_count++;
            break;
        default:
            break;
    }
    return plant;
}

Plant* plant_spawn(Area* area, PlantType type, TreeSpecies species, int x, int y, int z)
{
    Plant* plant = plant_find_free_slot(area, type);
    if(!plant)
    {
        log_add("No free plant slot available to spawn plant at %d,%d.", x, y);
        return NULL;
    }

    return plant_init_at_3d(area, plant, type, species, x, y, z);
}

void plant_clear_area(Area* area)
{
    if(!area)
        return;

    for(int i = 0; i < MAX_AREA_PLANTS; ++i)
        memset(&area->plants[i], 0, sizeof(area->plants[i]));

    area->plant_count = 0;
    area->tree_plant_count = 0;
    area->bush_plant_count = 0;
    area->herb_plant_count = 0;
    area->flower_plant_count = 0;
}

int plant_damage(Plant* plant, int damage)
{
    if(!plant || !plant->active || damage <= 0)
        return 0;

    plant->health -= damage;
    if(plant->health < 0)
        plant->health = 0;

    return damage;
}

void plant_transition_to_stump(Plant* plant)
{
    if(!plant || !plant->active)
        return;

    plant->state = PLANT_STATE_STUMP;
    plant->entity.symbol = plant->template_data ? plant->template_data->stump_symbol : 't';
    plant->entity.color = plant->template_data ? plant->template_data->stump_color : RENDER_COLOR_BROWN;
    plant->entity.blocks = 0;
    plant->entity.hide_below = 0;
    plant->height = 1;
}

void plants_take_turns(Player* p)
{
    if(!current_area)
        return;

    s_plant_turn_tick++;
    if((s_plant_turn_tick % 720) != 0)
        return;

    for(int i = 0; i < MAX_AREA_PLANTS; ++i)
    {
        Plant* plant = &current_area->plants[i];
        if(!plant->active)
            continue;
        if(plant->state == PLANT_STATE_STUMP || plant->state == PLANT_STATE_DEAD)
            continue;
        if(!plant->template_data)
            continue;

        if(plant->growth_stage < plant->template_data->max_growth_stage)
        {
            plant->growth_progress++;
            if(plant->growth_progress >= plant->template_data->growth_turns_per_stage)
            {
                plant->growth_progress = 0;
                plant->growth_stage++;
                if(plant->growth_stage > plant->template_data->max_growth_stage)
                    plant->growth_stage = plant->template_data->max_growth_stage;
            }
        }
    }
}
