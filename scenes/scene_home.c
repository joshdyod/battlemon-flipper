#include "battlemon_scene.h"
#include "../battlemon_app.h"
#include "../assets/sprites.h"
#include "../game/battlemon_evolution.h"
#include "../storage/battlemon_save.h"
#include <gui/view.h>
#include <gui/canvas.h>
#include <furi.h>

// ---------------------------------------------------------------------------
// Draw — void, no return value
// ---------------------------------------------------------------------------
static void home_draw(Canvas* canvas, void* ctx) {
    BMApp* app = (BMApp*)ctx;
    canvas_clear(canvas);

    const int sprite_x = 48;
    const int sprite_y = 14;

    if(!creature_is_alive(&app->creature)) {
        canvas_draw_xbm(canvas, sprite_x + 12, sprite_y + 12, 8, 8, ICON_SKULL);
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 56, AlignCenter, AlignBottom, "GAME OVER");
        return;
    }

    canvas_draw_xbm(canvas, sprite_x, sprite_y + (app->anim_frame & 1), 32, 32,
                    SPRITE_FOR(app->creature.species));

    if(app->creature.is_sleeping) {
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, sprite_x + 24, sprite_y,     "z");
        canvas_draw_str(canvas, sprite_x + 28, sprite_y - 5, "z");
    }

    canvas_set_font(canvas, FontSecondary);
    char top_buf[32];
    snprintf(top_buf, sizeof(top_buf), "[%s] %s %dh",
             creature_line_name(&app->creature),
             creature_name(&app->creature),
             app->creature.age_hours);
    canvas_draw_str(canvas, 2, 7, top_buf);

    if(creature_can_evolve(&app->creature))
        canvas_draw_xbm(canvas, 120, 0, 8, 8, ICON_STAR);
    else if(app->creature.needs_attention)
        canvas_draw_str(canvas, 122, 7, "!");

    canvas_draw_xbm(canvas, 2, 55, 8, 8, ICON_FOOD);
    canvas_draw_rbox(canvas, 12, 59, 28, 4, 1);
    canvas_set_color(canvas, ColorWhite);
    canvas_draw_box(canvas, 13, 60, (uint8_t)(26u * app->creature.hunger / 100u), 2);
    canvas_set_color(canvas, ColorBlack);

    canvas_draw_xbm(canvas, 45, 55, 8, 8, ICON_HEART);
    canvas_draw_rbox(canvas, 55, 59, 28, 4, 1);
    canvas_set_color(canvas, ColorWhite);
    canvas_draw_box(canvas, 56, 60, (uint8_t)(26u * app->creature.happiness / 100u), 2);
    canvas_set_color(canvas, ColorBlack);

    char hp_buf[8];
    snprintf(hp_buf, sizeof(hp_buf), "HP:%d", app->creature.hp);
    canvas_draw_str(canvas, 92, 63, hp_buf);

    if(app->creature.is_sick)
        canvas_draw_str(canvas, 2, 63, "SICK");

    for(int s = 0; s < 4; s++) {
        int px = 54 + s * 6;
        if(s <= (int)app->creature.stage)
            canvas_draw_box(canvas, px, 61, 4, 3);
        else
            canvas_draw_frame(canvas, px, 61, 4, 3);
    }
}

// ---------------------------------------------------------------------------
// Input — bool, must return bool
// ---------------------------------------------------------------------------
static bool home_input(InputEvent* event, void* ctx) {
    BMApp* app = (BMApp*)ctx;
    if(event->type != InputTypeShort) {
        return false;
    }
    switch(event->key) {
        case InputKeyOk:
            view_dispatcher_send_custom_event(app->view_dispatcher, 3u);
            return true;
        case InputKeyUp:
            app->creature.is_sleeping = !app->creature.is_sleeping;
            return true;
        default:
            break;
    }
    return false;
}

// ---------------------------------------------------------------------------
// Scene lifecycle — all void
// ---------------------------------------------------------------------------
void scene_home_on_enter(void* ctx) {
    BMApp* app = (BMApp*)ctx;

    if(app->evolution_pending) {
        app->evolution_pending = false;
        scene_manager_next_scene(app->scene_manager, SCENE_EVOLVE);
        return;
    }

    app->view_home = view_alloc();
    view_set_context(app->view_home, app);
    view_set_draw_callback(app->view_home, home_draw);
    view_set_input_callback(app->view_home, home_input);
    view_dispatcher_add_view(app->view_dispatcher, VIEW_HOME, app->view_home);
    view_dispatcher_switch_to_view(app->view_dispatcher, VIEW_HOME);
}

bool scene_home_on_event(void* ctx, SceneManagerEvent event) {
    BMApp* app = (BMApp*)ctx;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        uint32_t ev = event.event;
        if(ev == EV_ANIM_TICK) {
            app->anim_frame ^= 1;
            consumed = true;
        } else if(ev == EV_MINUTE_TICK) {
            if(app->evolution_pending) {
                app->evolution_pending = false;
                scene_manager_next_scene(app->scene_manager, SCENE_EVOLVE);
            } else if(!creature_is_alive(&app->creature)) {
                scene_manager_next_scene(app->scene_manager, SCENE_GAMEOVER);
            }
            consumed = true;
        } else if(ev == 3u) {
            scene_manager_next_scene(app->scene_manager, SCENE_MENU);
            consumed = true;
        }
    }

    if(event.type == SceneManagerEventTypeBack) {
        scene_manager_stop(app->scene_manager);
        consumed = true;
    }

    return consumed;
}

void scene_home_on_exit(void* ctx) {
    BMApp* app = (BMApp*)ctx;
    view_dispatcher_remove_view(app->view_dispatcher, VIEW_HOME);
    if(app->view_home) {
        view_free(app->view_home);
        app->view_home = NULL;
    }
}
