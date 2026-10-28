#include "battlemon_scene.h"
#include "../assets/sounds.h"
#include "../battlemon_app.h"
#include "../assets/sprites.h"
#include "../game/battlemon_evolution.h"
#include "../storage/battlemon_save.h"
#include <gui/view.h>
#include <gui/canvas.h>
#include <gui/modules/submenu.h>
#include <furi.h>
#include <stdio.h>
#include <string.h>

// ============================================================================
// SCENE: MENU
// ============================================================================

typedef enum {
    MENU_FEED = 0,
    MENU_PLAY,
    MENU_HEAL,
    MENU_DISCIPLINE,
    MENU_SLEEP,
    MENU_STATUS,
    MENU_BATTLE,
    MENU_COUNT
} BMMenuItem;

static void menu_callback(void* ctx, uint32_t index) {
    BMApp* app = (BMApp*)ctx;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void scene_menu_on_enter(void* ctx) {
    BMApp* app = (BMApp*)ctx;
    submenu_reset(app->submenu);

    // Show creature name as header
    submenu_set_header(app->submenu, creature_name(&app->creature));

    submenu_add_item(app->submenu, "Feed",        MENU_FEED,       menu_callback, app);
    submenu_add_item(app->submenu, "Play",        MENU_PLAY,       menu_callback, app);
    submenu_add_item(app->submenu, "Heal",        MENU_HEAL,       menu_callback, app);
    submenu_add_item(app->submenu, "Discipline",  MENU_DISCIPLINE, menu_callback, app);
    submenu_add_item(app->submenu, app->creature.is_sleeping ? "Wake up" : "Sleep",
                                                  MENU_SLEEP,      menu_callback, app);
    submenu_add_item(app->submenu, "Status",      MENU_STATUS,     menu_callback, app);
    submenu_add_item(app->submenu, "Battle!",     MENU_BATTLE,     menu_callback, app);

    view_dispatcher_switch_to_view(app->view_dispatcher, VIEW_SUBMENU);
}

bool scene_menu_on_event(void* ctx, SceneManagerEvent event) {
    BMApp* app = (BMApp*)ctx;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        switch((BMMenuItem)event.event) {
            case MENU_FEED:
                creature_feed(&app->creature);
                sound_action();
                scene_manager_previous_scene(app->scene_manager);
                break;
            case MENU_PLAY:
                creature_play(&app->creature);
                sound_action();
                scene_manager_previous_scene(app->scene_manager);
                break;
            case MENU_HEAL:
                creature_heal(&app->creature);
                sound_action();
                scene_manager_previous_scene(app->scene_manager);
                break;
            case MENU_DISCIPLINE:
                creature_discipline(&app->creature);
                sound_action();
                scene_manager_previous_scene(app->scene_manager);
                break;
            case MENU_SLEEP:
                app->creature.is_sleeping = !app->creature.is_sleeping;
                scene_manager_previous_scene(app->scene_manager);
                break;
            case MENU_STATUS:
                scene_manager_next_scene(app->scene_manager, SCENE_STATUS);
                break;
            case MENU_BATTLE:
                scene_manager_next_scene(app->scene_manager, SCENE_BATTLE);
                break;
            default:
                break;
        }
        consumed = true;
    }

    if(event.type == SceneManagerEventTypeBack) {
        scene_manager_previous_scene(app->scene_manager);
        consumed = true;
    }

    return consumed;
}

void scene_menu_on_exit(void* ctx) { UNUSED(ctx); }


// ============================================================================
// SCENE: STATUS — custom View, two columns
// ============================================================================

static void status_draw(Canvas* canvas, void* ctx) {
    BMApp* app = (BMApp*)ctx;
    const BMCreature* c = &app->creature;

    canvas_clear(canvas);
    canvas_set_font(canvas, FontSecondary);

    // Header: sprite (small area) + name
    canvas_draw_xbm(canvas, 0, 0, 32, 32, SPRITE_FOR(c->species));

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 36, 10, creature_name(c));
    canvas_set_font(canvas, FontSecondary);

    char line_stage[24];
    const char* stage_names[] = {"Baby","Child","Adult","Ultimate"};
    snprintf(line_stage, sizeof(line_stage), "%s  %s",
             creature_line_name(c), stage_names[c->stage]);
    canvas_draw_str(canvas, 36, 20, line_stage);

    char age_buf[16];
    snprintf(age_buf, sizeof(age_buf), "Age: %dh", c->age_hours);
    canvas_draw_str(canvas, 36, 30, age_buf);

    // Divider
    canvas_draw_line(canvas, 0, 34, 128, 34);

    // Two-column stat grid
    // Left column  x=2   Right column x=66
    char buf[24];

    snprintf(buf, sizeof(buf), "HP:   %d", c->hp);
    canvas_draw_str(canvas, 2, 44, buf);

    snprintf(buf, sizeof(buf), "ATK:  %d", c->attack);
    canvas_draw_str(canvas, 66, 44, buf);

    snprintf(buf, sizeof(buf), "Hunger:  %d", c->hunger);
    canvas_draw_str(canvas, 2, 53, buf);

    snprintf(buf, sizeof(buf), "DEF:  %d", c->defense);
    canvas_draw_str(canvas, 66, 53, buf);

    snprintf(buf, sizeof(buf), "Happy:   %d", c->happiness);
    canvas_draw_str(canvas, 2, 62, buf);

    snprintf(buf, sizeof(buf), "SPD:  %d", c->speed);
    canvas_draw_str(canvas, 66, 62, buf);

    // W/L record + evolution hint at very bottom
    // (second page via second draw — but Flipper has no scroll,
    //  so show W/L on the sprite row)
    char wl[20];
    snprintf(wl, sizeof(wl), "W:%d L:%d XP:%lu",
             c->battles_won, c->battles_lost, (unsigned long)c->total_xp);
    canvas_draw_str(canvas, 36, 34 - 3, wl);

    // Discipline bar, right of sprite
    char disc[16];
    snprintf(disc, sizeof(disc), "Disc:%d", c->discipline);
    // (fits in the age row gap — skip if tight)
    (void)disc; // already have age there; omit

    // Sick / sleeping status flags
    if(c->is_sick)      canvas_draw_str(canvas, 100, 10, "SICK");
    if(c->is_sleeping)  canvas_draw_str(canvas, 100, 20, "zzz");

    canvas_draw_str_aligned(canvas, 64, 64, AlignCenter, AlignBottom,
                            "[Back]");
}

static bool status_input(InputEvent* event, void* ctx) {
    BMApp* app = (BMApp*)ctx;
    if(event->type == InputTypeShort &&
       (event->key == InputKeyBack || event->key == InputKeyOk)) {
        view_dispatcher_send_custom_event(app->view_dispatcher, 20);
        return true;
    }
    return false;
}

void scene_status_on_enter(void* ctx) {
    BMApp* app = (BMApp*)ctx;

    app->view_status = view_alloc();
    view_set_context(app->view_status, app);
    view_set_draw_callback(app->view_status, status_draw);
    view_set_input_callback(app->view_status, status_input);
    view_dispatcher_add_view(app->view_dispatcher, VIEW_STATUS, app->view_status);
    view_dispatcher_switch_to_view(app->view_dispatcher, VIEW_STATUS);
}

bool scene_status_on_event(void* ctx, SceneManagerEvent event) {
    BMApp* app = (BMApp*)ctx;
    if(event.type == SceneManagerEventTypeCustom && event.event == EV_MINUTE_TICK) return true;
    if(event.type == SceneManagerEventTypeCustom && event.event == EV_ANIM_TICK) return true;
    if(event.type == SceneManagerEventTypeCustom && event.event == 20) {
        scene_manager_previous_scene(app->scene_manager);
        return true;
    }
    if(event.type == SceneManagerEventTypeBack) {
        scene_manager_previous_scene(app->scene_manager);
        return true;
    }
    return false;
}

void scene_status_on_exit(void* ctx) {
    BMApp* app = (BMApp*)ctx;
    view_dispatcher_remove_view(app->view_dispatcher, VIEW_STATUS);
    if(app->view_status) { view_free(app->view_status); app->view_status = NULL; }
}


// ============================================================================
// SCENE: EVOLVE — creature reveal screen
// ============================================================================

static void evolve_draw(Canvas* canvas, void* ctx) {
    BMApp* app = (BMApp*)ctx;

    canvas_clear(canvas);

    // Big sprite centered
    canvas_draw_xbm(canvas, 48, 10, 32, 32, SPRITE_FOR(app->creature.species));

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 2, AlignCenter, AlignTop, "EVOLVED!");

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 46, AlignCenter, AlignTop,
                            creature_name(&app->creature));

    char stage_buf[20];
    const char* stage_names[] = {"Baby","Child","Adult","Ultimate"};
    snprintf(stage_buf, sizeof(stage_buf), "%s  [%s line]",
             stage_names[app->creature.stage],
             creature_line_name(&app->creature));
    canvas_draw_str_aligned(canvas, 64, 55, AlignCenter, AlignTop, stage_buf);

    canvas_draw_str_aligned(canvas, 64, 64, AlignCenter, AlignBottom,
                            "[OK] continue");
}

static bool evolve_input(InputEvent* event, void* ctx) {
    BMApp* app = (BMApp*)ctx;
    if(event->type == InputTypeShort &&
       (event->key == InputKeyOk || event->key == InputKeyBack)) {
        view_dispatcher_send_custom_event(app->view_dispatcher, 30);
        return true;
    }
    return false;
}

void scene_evolve_on_enter(void* ctx) {
    BMApp* app = (BMApp*)ctx;

    View* view = view_alloc();
    view_set_context(view, app);
    view_set_draw_callback(view, evolve_draw);
    view_set_input_callback(view, evolve_input);
    view_dispatcher_add_view(app->view_dispatcher, VIEW_EVOLVE, view);
    sound_evolve();
    view_dispatcher_switch_to_view(app->view_dispatcher, VIEW_EVOLVE);
}

bool scene_evolve_on_event(void* ctx, SceneManagerEvent event) {
    BMApp* app = (BMApp*)ctx;
    if(event.type == SceneManagerEventTypeCustom &&
       (event.event == EV_MINUTE_TICK || event.event == EV_ANIM_TICK)) return true;
    if((event.type == SceneManagerEventTypeCustom && event.event == 30) ||
        event.type == SceneManagerEventTypeBack) {
        scene_manager_search_and_switch_to_previous_scene(
            app->scene_manager, SCENE_HOME);
        return true;
    }
    return false;
}

void scene_evolve_on_exit(void* ctx) {
    BMApp* app = (BMApp*)ctx;
    view_dispatcher_remove_view(app->view_dispatcher, VIEW_EVOLVE);
    if(app->view_evolve) { view_free(app->view_evolve); app->view_evolve = NULL; }
}


// ============================================================================
// SCENE: GAME OVER
// ============================================================================

static void gameover_draw(Canvas* canvas, void* ctx) {
    BMApp* app = (BMApp*)ctx;
    const BMCreature* c = &app->creature;

    canvas_clear(canvas);

    // Skull
    canvas_draw_xbm(canvas, 60, 4, 8, 8, ICON_SKULL);

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 16, AlignCenter, AlignTop, "GAME OVER");

    canvas_set_font(canvas, FontSecondary);

    char name_buf[24];
    snprintf(name_buf, sizeof(name_buf), "%s is gone", creature_name(c));
    canvas_draw_str_aligned(canvas, 64, 28, AlignCenter, AlignTop, name_buf);

    char wl_buf[24];
    snprintf(wl_buf, sizeof(wl_buf), "W:%d  L:%d  XP:%lu",
             c->battles_won, c->battles_lost, (unsigned long)c->total_xp);
    canvas_draw_str_aligned(canvas, 64, 39, AlignCenter, AlignTop, wl_buf);

    char age_buf[20];
    snprintf(age_buf, sizeof(age_buf), "Lived %dh as %s",
             c->age_hours + (c->stage * 24),
             creature_line_name(c));
    canvas_draw_str_aligned(canvas, 64, 50, AlignCenter, AlignTop, age_buf);

    canvas_draw_str_aligned(canvas, 64, 64, AlignCenter, AlignBottom,
                            "[OK] try again");
}

static bool gameover_input(InputEvent* event, void* ctx) {
    BMApp* app = (BMApp*)ctx;
    if(event->type == InputTypeShort && event->key == InputKeyOk) {
        view_dispatcher_send_custom_event(app->view_dispatcher, 40);
        return true;
    }
    return false;
}

void scene_gameover_on_enter(void* ctx) {
    BMApp* app = (BMApp*)ctx;

    View* view = view_alloc();
    view_set_context(view, app);
    view_set_draw_callback(view, gameover_draw);
    view_set_input_callback(view, gameover_input);
    view_dispatcher_add_view(app->view_dispatcher, VIEW_GAMEOVER, view);
    sound_death();
    view_dispatcher_switch_to_view(app->view_dispatcher, VIEW_GAMEOVER);
}

bool scene_gameover_on_event(void* ctx, SceneManagerEvent event) {
    BMApp* app = (BMApp*)ctx;
    if(event.type == SceneManagerEventTypeCustom && event.event == EV_MINUTE_TICK) return true;
    if(event.type == SceneManagerEventTypeCustom && event.event == EV_ANIM_TICK) return true;
    if(event.type == SceneManagerEventTypeCustom && event.event == 40) {
        save_delete();
        creature_init(&app->creature);
        scene_manager_search_and_switch_to_previous_scene(
            app->scene_manager, SCENE_LINESELECT);
        return true;
    }
    if(event.type == SceneManagerEventTypeBack) {
        // Back without restarting — just exit the app
        scene_manager_stop(app->scene_manager);
        return true;
    }
    return false;
}

void scene_gameover_on_exit(void* ctx) {
    BMApp* app = (BMApp*)ctx;
    view_dispatcher_remove_view(app->view_dispatcher, VIEW_GAMEOVER);
    if(app->view_gameover) { view_free(app->view_gameover); app->view_gameover = NULL; }
}
