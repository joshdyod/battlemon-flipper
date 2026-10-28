#pragma once
#include <gui/scene_manager.h>

// ---------------------------------------------------------------------------
// Scene IDs
// ---------------------------------------------------------------------------
typedef enum {
    SCENE_LINESELECT = 0,  // first-launch line picker
    SCENE_HOME,
    SCENE_MENU,
    SCENE_STATUS,
    SCENE_BATTLE,
    SCENE_EVOLVE,
    SCENE_GAMEOVER,
    SCENE_COUNT
} BMScene;

// ---------------------------------------------------------------------------
// Scene callbacks
// ---------------------------------------------------------------------------

// Line select (first launch)
void scene_lineselect_on_enter(void* ctx);
bool scene_lineselect_on_event(void* ctx, SceneManagerEvent event);
void scene_lineselect_on_exit(void* ctx);

// Home screen
void scene_home_on_enter(void* ctx);
bool scene_home_on_event(void* ctx, SceneManagerEvent event);
void scene_home_on_exit(void* ctx);

// Action menu
void scene_menu_on_enter(void* ctx);
bool scene_menu_on_event(void* ctx, SceneManagerEvent event);
void scene_menu_on_exit(void* ctx);

// Status screen
void scene_status_on_enter(void* ctx);
bool scene_status_on_event(void* ctx, SceneManagerEvent event);
void scene_status_on_exit(void* ctx);

// IR battle
void scene_battle_on_enter(void* ctx);
bool scene_battle_on_event(void* ctx, SceneManagerEvent event);
void scene_battle_on_exit(void* ctx);

// Evolution cutscene
void scene_evolve_on_enter(void* ctx);
bool scene_evolve_on_event(void* ctx, SceneManagerEvent event);
void scene_evolve_on_exit(void* ctx);

// Game over
void scene_gameover_on_enter(void* ctx);
bool scene_gameover_on_event(void* ctx, SceneManagerEvent event);
void scene_gameover_on_exit(void* ctx);

// ---------------------------------------------------------------------------
// Handler table passed to scene_manager_alloc()
// ---------------------------------------------------------------------------
extern const SceneManagerHandlers battlemon_scene_handlers;
