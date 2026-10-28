#pragma once
#include "../game/battlemon_creature.h"

// Save file lives at /ext/apps_data/battlemon_flipper/save.bmon
// Format: plain FlipperFormat (key:value text) for easy debugging

#define SAVE_DIR  "/ext/apps_data/battlemon_flipper"
#define SAVE_PATH "/ext/apps_data/battlemon_flipper/save.bmon"

// Returns true on success
bool save_creature(const BMCreature* c);
bool load_creature(BMCreature* c);  // fills c; returns false if no save exists
bool save_exists(void);
void save_delete(void);
