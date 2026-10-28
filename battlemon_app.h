#pragma once
#include <gui/gui.h>
#include <gui/view.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <gui/modules/submenu.h>
#include <notification/notification.h>
#include <furi.h>

#include "game/battlemon_creature.h"
#include "game/battlemon_battle.h"
#include "comms/battlemon_ir.h"

// ---------------------------------------------------------------------------
// Timer event IDs (also used by scene handlers — keep in sync with app.c)
// ---------------------------------------------------------------------------
#define EV_ANIM_TICK    0u
#define EV_MINUTE_TICK  2u

// ---------------------------------------------------------------------------
// View IDs
// All views except VIEW_SUBMENU are allocated/freed by their own scene.
// ---------------------------------------------------------------------------
typedef enum {
    VIEW_LINESELECT = 0,
    VIEW_HOME,
    VIEW_SUBMENU,       // persistent — wraps SDK Submenu
    VIEW_BATTLE,
    VIEW_STATUS,
    VIEW_EVOLVE,
    VIEW_GAMEOVER,
    VIEW_COUNT
} BMViewId;

// ---------------------------------------------------------------------------
// Central app state
// ---------------------------------------------------------------------------
typedef struct {
    // Flipper SDK handles
    Gui*                  gui;
    NotificationApp*      notifications;
    ViewDispatcher*       view_dispatcher;
    SceneManager*         scene_manager;
    Submenu*              submenu;

    // Game state
    BMCreature creature;

    // Battle
    BMPacket   remote_packet;
    BattleReport battle_report;

    // IR
    IrContext ir_ctx;

    // Dynamically allocated views — stored here so on_exit can free them
    // without needing view_dispatcher_get_view (not in SDK 1.5.x)
    View* view_home;
    View* view_lineselect;
    View* view_battle;
    View* view_status;
    View* view_evolve;
    View* view_gameover;

    // Animation frame counter (flipped every 500ms by anim_timer)
    uint8_t anim_frame;

    // Timers
    FuriTimer* minute_timer;
    FuriTimer* anim_timer;

    // Flags
    bool evolution_pending;
    bool is_first_launch;
} BMApp;
