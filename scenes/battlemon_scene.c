#include "battlemon_scene.h"

// SDK 1.5.x SceneManagerHandlers uses three separate callback arrays,
// not a single SceneManagerHandler struct array.

static const AppSceneOnEnterCallback on_enter_handlers[SCENE_COUNT] = {
    [SCENE_LINESELECT] = scene_lineselect_on_enter,
    [SCENE_HOME]       = scene_home_on_enter,
    [SCENE_MENU]       = scene_menu_on_enter,
    [SCENE_STATUS]     = scene_status_on_enter,
    [SCENE_BATTLE]     = scene_battle_on_enter,
    [SCENE_EVOLVE]     = scene_evolve_on_enter,
    [SCENE_GAMEOVER]   = scene_gameover_on_enter,
};

static const AppSceneOnEventCallback on_event_handlers[SCENE_COUNT] = {
    [SCENE_LINESELECT] = scene_lineselect_on_event,
    [SCENE_HOME]       = scene_home_on_event,
    [SCENE_MENU]       = scene_menu_on_event,
    [SCENE_STATUS]     = scene_status_on_event,
    [SCENE_BATTLE]     = scene_battle_on_event,
    [SCENE_EVOLVE]     = scene_evolve_on_event,
    [SCENE_GAMEOVER]   = scene_gameover_on_event,
};

static const AppSceneOnExitCallback on_exit_handlers[SCENE_COUNT] = {
    [SCENE_LINESELECT] = scene_lineselect_on_exit,
    [SCENE_HOME]       = scene_home_on_exit,
    [SCENE_MENU]       = scene_menu_on_exit,
    [SCENE_STATUS]     = scene_status_on_exit,
    [SCENE_BATTLE]     = scene_battle_on_exit,
    [SCENE_EVOLVE]     = scene_evolve_on_exit,
    [SCENE_GAMEOVER]   = scene_gameover_on_exit,
};

const SceneManagerHandlers battlemon_scene_handlers = {
    .on_enter_handlers = on_enter_handlers,
    .on_event_handlers = on_event_handlers,
    .on_exit_handlers  = on_exit_handlers,
    .scene_num         = SCENE_COUNT,
};
