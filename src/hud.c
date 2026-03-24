#include "hud.h"
#include "combat.h"
#include "log.h"
#include "layout.h"
#include "character.h"
#include <stdio.h>
#include <string.h>

#define HUD_BASE_LINE_COUNT 11

/*
 * Purpose:
 *   Builds printable HUD lines from player and combat state.
 *
 * Functions:
 *   - hud_clamped_* helpers: keep text widths inside buffer bounds.
 *   - hud_make_border / hud_make_row: build boxed HUD row strings.
 *   - hud_init: initializes HUD-adjacent log state.
 *   - hud_get_lines: produces complete HUD text rows for renderer.
 */

// Clamp border dash count to safe string length.
static int hud_clamped_dash_width(int dash_width)
{
    if(dash_width < 1) return 1;
    if(dash_width > HUD_LINE_LENGTH - 3) return HUD_LINE_LENGTH - 3;
    return dash_width;
}

// Clamp printable text width to safe string length.
static int hud_clamped_text_width(int text_width)
{
    if(text_width < 1) return 1;
    if(text_width > HUD_LINE_LENGTH - 5) return HUD_LINE_LENGTH - 5;
    return text_width;
}

// Build one horizontal border row.
static void hud_make_border(char out[HUD_LINE_LENGTH], int dash_width)
{
    dash_width = hud_clamped_dash_width(dash_width);
    out[0] = '+';
    for(int i = 0; i < dash_width; i++)
        out[i + 1] = '-';
    out[dash_width + 1] = '+';
    out[dash_width + 2] = '\0';
}

// Build one boxed text row.
static void hud_make_row(char out[HUD_LINE_LENGTH], int text_width, const char* text)
{
    text_width = hud_clamped_text_width(text_width);
    snprintf(out, HUD_LINE_LENGTH, "| %-*.*s |", text_width, text_width, text ? text : "");
}

// Build one line listing all available attack modes and mark the active mode.
static void hud_make_attack_modes_text(char out[HUD_LINE_LENGTH], int attack_mode_mask, AttackMode active_mode)
{
    static const struct {
        int flag;
        AttackMode mode;
    } ordered_modes[] = {
        { ATTACK_MODE_FLAG_PUNCH, ATTACK_MODE_PUNCH },
        { ATTACK_MODE_FLAG_KICK, ATTACK_MODE_KICK },
        { ATTACK_MODE_FLAG_STAB, ATTACK_MODE_STAB },
        { ATTACK_MODE_FLAG_CUT, ATTACK_MODE_CUT },
        { ATTACK_MODE_FLAG_SMASH, ATTACK_MODE_SMASH },
    };
    int used = 0;
    int wrote;

    out[0] = '\0';

    if(attack_mode_mask == ATTACK_MODE_FLAG_NONE)
    {
        snprintf(out, HUD_LINE_LENGTH, "None");
        return;
    }

    for(int i = 0; i < (int)(sizeof(ordered_modes) / sizeof(ordered_modes[0])); i++)
    {
        if(used >= HUD_LINE_LENGTH - 1)
            break;

        if(!(attack_mode_mask & ordered_modes[i].flag))
            continue;

        if(used > 0)
        {
            wrote = snprintf(out + used, HUD_LINE_LENGTH - used, " | ");
            if(wrote < 0)
                break;
            used += wrote;
            if(used >= HUD_LINE_LENGTH - 1)
                break;
        }

        if(ordered_modes[i].mode == active_mode)
            wrote = snprintf(out + used, HUD_LINE_LENGTH - used, "[%s]", attack_mode_name(ordered_modes[i].mode));
        else
            wrote = snprintf(out + used, HUD_LINE_LENGTH - used, "%s", attack_mode_name(ordered_modes[i].mode));

        if(wrote < 0)
            break;

        used += wrote;

        if(used >= HUD_LINE_LENGTH - 1)
            break;
    }
}

// Initialize HUD-related systems.
void hud_init(void)
{
    log_init();
}

// Return preferred static HUD row count for the current HUD sections.
int hud_preferred_height(void)
{
    return HUD_BASE_LINE_COUNT;
}

// Produce formatted HUD rows based on current player/combat state.
int hud_get_lines(Player* p, char out_lines[][HUD_LINE_LENGTH], int max_lines)
{
    LayoutState layout;
    int dash_width;
    int text_width;
    if(!p || !out_lines || max_lines <= 0) return 0;
    char text[HUD_LINE_LENGTH];
    char mode_text[HUD_LINE_LENGTH];
    char border[HUD_LINE_LENGTH];
    Character* c = &p->character;
    CombatProfile attack_profile = combat_profile_for_character_attack(c, p->selected_attack_mode);
    CombatSummary combat_summary = combat_summary_for_character(c, p->selected_attack_mode);

    layout_get_default(&layout);
    dash_width = hud_clamped_dash_width(layout.hud.inner_width);
    text_width = hud_clamped_text_width(layout_box_text_width(&layout.hud));
    hud_make_border(border, dash_width);

    int line = 0;
    if(line < max_lines) snprintf(out_lines[line++], HUD_LINE_LENGTH, "%s", border);
    if(line < max_lines) hud_make_row(out_lines[line++], text_width, "Player Stats");
    if(line < max_lines) snprintf(out_lines[line++], HUD_LINE_LENGTH, "%s", border);

    snprintf(text, sizeof(text), "Name: %s", c->name);
    if(line < max_lines) hud_make_row(out_lines[line++], text_width, text);

    snprintf(text, sizeof(text), "Health: %d/%d  Stamina: %d/%d", c->actor.health, c->actor.max_health, c->actor.stamina, c->actor.max_stamina);
    if(line < max_lines) hud_make_row(out_lines[line++], text_width, text);

    snprintf(text, sizeof(text), "Willpower: %d/%d  Mana: %d/%d", c->actor.willpower, c->actor.max_willpower, c->actor.mana, c->actor.max_mana);
    if(line < max_lines) hud_make_row(out_lines[line++], text_width, text);

    snprintf(text, sizeof(text), "Weapon: %s  %s %d  Mode: %s  Type: %s",
             combat_summary.weapon_name,
             weapon_skill_short_name(combat_summary.skill_type),
             combat_summary.skill_level,
             attack_mode_name(combat_summary.attack_mode),
             damage_type_name(combat_summary.active_damage_type));
    if(line < max_lines) hud_make_row(out_lines[line++], text_width, text);

    if(line < max_lines) hud_make_row(out_lines[line++], text_width, "Attack Modes:");
    hud_make_attack_modes_text(mode_text, attack_profile.attack_mode_mask, combat_summary.attack_mode);
    if(line < max_lines) hud_make_row(out_lines[line++], text_width, mode_text);

    snprintf(text, sizeof(text), "Hit: %d%%  Crit: %d%%  Damage: %d  Armor: %d  Block: %d%%  Parry: %d%%",
             combat_summary.hit_chance,
             combat_summary.crit_chance,
             combat_summary.damage,
             c->actor.armor_rating,
             c->actor.block,
             combat_summary.parry_chance);
    if(line < max_lines) hud_make_row(out_lines[line++], text_width, text);

    if(line < max_lines) snprintf(out_lines[line++], HUD_LINE_LENGTH, "%s", border);
    return line;
}



