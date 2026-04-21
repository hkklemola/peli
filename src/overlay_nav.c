#include "overlay_nav.h"

#include "draw.h"
#include "inventory.h"
#include "journal.h"
#include "keybind_helpers.h"
#include "log.h"
#include "player.h"
#include "atlas_overlay.h"
#include "codex.h"
#include "ui_overlay.h"

/**
 * @file overlay_nav.h
 * @brief Implementation of overlay screen navigation and request queueing.
 *
 * Manages hotkey-to-overlay routing, overlay request queueing for chained navigation,
 * and the main overlay execution loop that handle transitions between different UI screens.
 */

static OverlayType g_next_overlay = OVERLAY_TYPE_NONE;

int overlay_type_from_key(int key, OverlayType* out_type)
{
    OverlayType type = OVERLAY_TYPE_NONE;

    if(KEYBIND_MATCH_ALPHA(key, 'i', 'I'))
        type = OVERLAY_TYPE_INVENTORY;
    else if(KEYBIND_MATCH_ALPHA(key, 'c', 'C'))
        type = OVERLAY_TYPE_CHARACTER;
    else if(KEYBIND_LOG_OVERLAY(key))
        type = OVERLAY_TYPE_LOG;
    else if(KEYBIND_MATCH_ALPHA(key, 'j', 'J'))
        type = OVERLAY_TYPE_JOURNAL;
    else if(KEYBIND_MATCH_ALPHA(key, 'o', 'O'))
        type = OVERLAY_TYPE_CODEX;

    if(type == OVERLAY_TYPE_NONE)
        return 0;

    if(out_type)
        *out_type = type;
    return 1;
}

void overlay_request(OverlayType next_overlay)
{
    g_next_overlay = next_overlay;
}

OverlayType overlay_take_request(void)
{
    OverlayType next = g_next_overlay;
    g_next_overlay = OVERLAY_TYPE_NONE;
    return next;
}

void overlay_open(OverlayType initial_overlay, Player* player)
{
    OverlayType current = initial_overlay;

    if(!player)
        return;

    while(current != OVERLAY_TYPE_NONE)
    {
        overlay_request(OVERLAY_TYPE_NONE);

        switch(current)
        {
            case OVERLAY_TYPE_INVENTORY:
                inventory_menu(&player->character);
                break;
            case OVERLAY_TYPE_CHARACTER:
                player_show_character_sheet(player);
                break;
            case OVERLAY_TYPE_LOG:
                log_show_overlay();
                break;
            case OVERLAY_TYPE_JOURNAL:
                journal_show_overlay(player);
                break;
            case OVERLAY_TYPE_CODEX:
                codex_show_overlay(player);
                break;
            default:
                break;
        }

        current = overlay_take_request();
    }

    draw_invalidate_viewport_contents();
    ui_overlay_invalidate_cache();
}
