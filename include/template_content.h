#ifndef TEMPLATE_CONTENT_H
#define TEMPLATE_CONTENT_H

/*
 * Purpose:
 *   Declares centralized runtime loading for external content templates.
 */

// Load all core template registries needed to start the game.
int template_content_load_all(void);

// Return the last template loading error, or an empty string when none.
const char* template_content_last_error(void);

#endif