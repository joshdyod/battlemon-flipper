#include "battlemon_scene.h"
#include "../battlemon_app.h"
#include "../assets/sprites.h"
#include "../game/battlemon_creature.h"
#include "../storage/battlemon_save.h"
#include <gui/canvas.h>
#include <gui/view.h>
#include <furi.h>
#include <string.h>

// ---------------------------------------------------------------------------
// The 5 starting species, one per evolution line
// ---------------------------------------------------------------------------
static const BMSpecies LINE_BABIES[5] = {
    BM_ROCK_SLIME,
    BM_LADYBIRD,
    BM_SUNMEOW,
    BM_OCTOPUS,
    BM_ONION_KNIGHT,
};

static const char* LINE_NAMES[5] = {
    "Earth", "Insect", "Beast", "Sea", "Mystic",
};

static const char* LINE_DESCRIPTIONS[5] = {
    "High DEF, volcanic power",
    "Fast attacker, agile",
    "Fierce, balanced fighter",
    "Slippery, speed-focused",
    "Cunning, unpredictable",
};

typedef struct {
    uint8_t selected;
    uint8_t anim_frame;
} LineSelectState;

static LineSelectState s_ls = {0};

// ---------------------------------------------------------------------------
// Draw
// ---------------------------------------------------------------------------
static void lineselect_draw(Canvas* canvas, void* ctx) {
    UNUSED(ctx);
    uint8_t sel = s_ls.selected;

    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 2, AlignCenter, AlignTop, "Choose your line");
    canvas_set_font(canvas, FontSecondary);

    if(sel > 0) canvas_draw_str(canvas, 2,   36, "<");
    if(sel < 4) canvas_draw_str(canvas, 121, 36, ">");

    // Dot indicators
    for(int i = 0; i < 5; i++) {
        int dot_x = 44 + i * 9;
        if(i == sel) canvas_draw_box(canvas, dot_x, 58, 5, 5);
        else         canvas_draw_frame(canvas, dot_x, 58, 5, 5);
    }

    // Sprite with 1px bob
    int sprite_y = 16 + (s_ls.anim_frame & 1);
    canvas_draw_xbm(canvas, 48, sprite_y, 32, 32, SPRITE_FOR(LINE_BABIES[sel]));

    // Baby creature name (top-right corner)
    BMCreature tmp;
    memset(&tmp, 0, sizeof(tmp));
    tmp.species = LINE_BABIES[sel];
    canvas_draw_str_aligned(canvas, 126, 2, AlignRight, AlignTop, creature_name(&tmp));

    // Line name
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 50, AlignCenter, AlignTop, LINE_NAMES[sel]);

    // Description
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 57, AlignCenter, AlignTop, LINE_DESCRIPTIONS[sel]);
}

// ---------------------------------------------------------------------------
// Input
// ---------------------------------------------------------------------------
static bool lineselect_input(InputEvent* event, void* ctx) {
    BMApp* app = (BMApp*)ctx;
    if(event->type != InputTypeShort && event->type != InputTypeRepeat) return false;
    switch(event->key) {
        case InputKeyLeft:
            view_dispatcher_send_custom_event(app->view_dispatcher, 50u);
            break;
        case InputKeyRight:
            view_dispatcher_send_custom_event(app->view_dispatcher, 51u);
            break;
        case InputKeyOk:
            view_dispatcher_send_custom_event(app->view_dispatcher, 52u);
            break;
        default: break;
    }
    return false;

}

// ---------------------------------------------------------------------------
// Scene lifecycle
// ---------------------------------------------------------------------------
void scene_lineselect_on_enter(void* ctx) {
    BMApp* app = (BMApp*)ctx;
    s_ls.selected   = 0;
    s_ls.anim_frame = 0;

    // Guard: only allocate if not already created
    if(!app->view_lineselect) {
        app->view_lineselect = view_alloc();
        view_set_context(app->view_lineselect, app);
        view_set_draw_callback(app->view_lineselect, lineselect_draw);
        view_set_input_callback(app->view_lineselect, lineselect_input);
        view_dispatcher_add_view(app->view_dispatcher, VIEW_LINESELECT, app->view_lineselect);
    }
    view_dispatcher_switch_to_view(app->view_dispatcher, VIEW_LINESELECT);
}

bool scene_lineselect_on_event(void* ctx, SceneManagerEvent event) {
    BMApp* app = (BMApp*)ctx;

    if(event.type == SceneManagerEventTypeCustom) {
        uint32_t ev = event.event;

        if(ev == EV_ANIM_TICK) {
            s_ls.anim_frame ^= 1;
            return true;
        }
        if(ev == EV_MINUTE_TICK) {
            // creature_tick() already ran globally — nothing else to do here
            return true;
        }
        if(ev == 50u) { if(s_ls.selected > 0) s_ls.selected--; return true; }
        if(ev == 51u) { if(s_ls.selected < 4) s_ls.selected++; return true; }
        if(ev == 52u) {
            // Confirm selection — init creature on the chosen line
            creature_init(&app->creature);
            app->creature.species = LINE_BABIES[s_ls.selected];
            // Save immediately so a crash before the first minute tick
            // doesn't lose the line choice
            save_creature(&app->creature);
            scene_manager_next_scene(app->scene_manager, SCENE_HOME);
            return true;
        }
    }

    if(event.type == SceneManagerEventTypeBack) {
        scene_manager_stop(app->scene_manager);
        return true;
    }
    return false;
}

void scene_lineselect_on_exit(void* ctx) {
    BMApp* app = (BMApp*)ctx;
    view_dispatcher_remove_view(app->view_dispatcher, VIEW_LINESELECT);
    if(app->view_lineselect) { view_free(app->view_lineselect); app->view_lineselect = NULL; }
}
