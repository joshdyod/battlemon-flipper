#include "battlemon_app.h"
#include "scenes/battlemon_scene.h"
#include "storage/battlemon_save.h"
#include "game/battlemon_creature.h"
#include "game/battlemon_evolution.h"
#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <gui/modules/submenu.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>
#include <string.h>

// ---------------------------------------------------------------------------
// Timer event IDs — defined here, declared extern in battlemon_app.h
// ---------------------------------------------------------------------------
// (EV_ANIM_TICK=0, EV_MINUTE_TICK=2 are macros in the header)

// ---------------------------------------------------------------------------
// Timer callbacks
// ---------------------------------------------------------------------------
static void anim_timer_callback(void* ctx) {
    BMApp* app = (BMApp*)ctx;
    view_dispatcher_send_custom_event(app->view_dispatcher, EV_ANIM_TICK);
}

static void minute_timer_callback(void* ctx) {
    BMApp* app = (BMApp*)ctx;
    view_dispatcher_send_custom_event(app->view_dispatcher, EV_MINUTE_TICK);
}

// ---------------------------------------------------------------------------
// Global custom event pre-filter
// Handles cross-scene concerns before forwarding to the scene manager.
// ---------------------------------------------------------------------------
static bool app_custom_event_callback(void* ctx, uint32_t event) {
    BMApp* app = (BMApp*)ctx;

    if(event == EV_MINUTE_TICK) {
        // Advance creature time regardless of which scene is active
        creature_tick(&app->creature);

        // LED flash when the creature needs attention
        if(app->creature.needs_attention) {
            notification_message(app->notifications, &sequence_blink_red_10);
        }

        // Check for evolution (sets evolution_pending flag; scene handles redirect)
        if(!app->evolution_pending && creature_is_alive(&app->creature)) {
            if(evolution_check(&app->creature)) {
                app->evolution_pending = true;
            }
        }

        // Auto-save every minute
        save_creature(&app->creature);
    }

    // Always forward — each scene handles its own side effects
    return scene_manager_handle_custom_event(app->scene_manager, event);
}

static bool app_back_event_callback(void* ctx) {
    BMApp* app = (BMApp*)ctx;
    return scene_manager_handle_back_event(app->scene_manager);
}

// ---------------------------------------------------------------------------
// Alloc
// ---------------------------------------------------------------------------
static BMApp* battlemon_app_alloc(void) {
    BMApp* app = malloc(sizeof(BMApp));
    furi_check(app);
    memset(app, 0, sizeof(BMApp));

    app->gui           = furi_record_open(RECORD_GUI);
    app->notifications = furi_record_open(RECORD_NOTIFICATION);

    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(app->view_dispatcher, app_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(app->view_dispatcher, app_back_event_callback);
    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    app->scene_manager = scene_manager_alloc(&battlemon_scene_handlers, app);

    app->submenu = submenu_alloc();
    view_dispatcher_add_view(app->view_dispatcher, VIEW_SUBMENU,
                             submenu_get_view(app->submenu));

    app->anim_timer   = furi_timer_alloc(anim_timer_callback,   FuriTimerTypePeriodic, app);
    app->minute_timer = furi_timer_alloc(minute_timer_callback, FuriTimerTypePeriodic, app);

    return app;
}

// ---------------------------------------------------------------------------
// Free
// ---------------------------------------------------------------------------
static void battlemon_app_free(BMApp* app) {
    furi_timer_stop(app->anim_timer);
    furi_timer_stop(app->minute_timer);
    furi_timer_free(app->anim_timer);
    furi_timer_free(app->minute_timer);

    view_dispatcher_remove_view(app->view_dispatcher, VIEW_SUBMENU);

    submenu_free(app->submenu);
    scene_manager_free(app->scene_manager);
    view_dispatcher_free(app->view_dispatcher);

    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_GUI);
    free(app);
}

// ---------------------------------------------------------------------------
// Entry point
// ---------------------------------------------------------------------------
int32_t battlemon_app_main(void* p) {
    UNUSED(p);

    BMApp* app = battlemon_app_alloc();

    if(save_exists()) {
        app->is_first_launch = false;
        if(!load_creature(&app->creature)) {
            app->is_first_launch = true;
            creature_init(&app->creature);
        }
    } else {
        app->is_first_launch = true;
        creature_init(&app->creature);
    }

    furi_timer_start(app->anim_timer, 500);

    // Jitter the minute timer so creature events don't always fire at the same
    // real-world clock boundary. Offset by 0..9 seconds based on current tick.
    // furi_ms_to_ticks(1) == 1 on the Flipper (1kHz tick), but using the
    // conversion function is correct regardless of firmware tick rate.
    uint32_t jitter_ticks = furi_get_tick() % furi_ms_to_ticks(10000);
    furi_timer_start(app->minute_timer, furi_ms_to_ticks(60000) + jitter_ticks);

    if(app->is_first_launch) {
        scene_manager_next_scene(app->scene_manager, SCENE_LINESELECT);
    } else {
        scene_manager_next_scene(app->scene_manager, SCENE_HOME);
    }

    view_dispatcher_run(app->view_dispatcher);

    save_creature(&app->creature);

    battlemon_app_free(app);
    return 0;
}
