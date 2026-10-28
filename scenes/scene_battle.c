#include "battlemon_scene.h"
#include "../assets/sounds.h"
#include "../battlemon_app.h"
#include "../comms/battlemon_ir.h"
#include "../game/battlemon_battle.h"
#include "../assets/sprites.h"
#include <gui/view.h>
#include <gui/canvas.h>
#include <furi.h>
#include <furi_hal.h>
#include <string.h>

// ---------------------------------------------------------------------------
// Battle scene internal state — stored separately from BMApp so the
// draw callback has access without the "simplified cast" hack.
// ---------------------------------------------------------------------------
typedef enum {
    BSTATE_SEARCHING,   // broadcasting IR challenge
    BSTATE_CONNECTED,   // handshake done, exchanging packets
    BSTATE_FIGHTING,    // animating fight rounds
    BSTATE_RESULT,      // showing win/loss screen
} BattlePhase;

typedef struct {
    BattlePhase phase;
    uint8_t     anim_tick;
    uint8_t     fight_frames_left;
    uint8_t     my_hp_display;     // HP shown during fight (counts down for drama)
    uint8_t     opp_hp_display;
} BattleModel;

static BattleModel s_bm;

// ---------------------------------------------------------------------------
// Draw
// ---------------------------------------------------------------------------
static void battle_draw(Canvas* canvas, void* ctx) {
    BMApp* app = (BMApp*)ctx;
    canvas_clear(canvas);
    canvas_set_font(canvas, FontSecondary);

    switch(s_bm.phase) {

        // ---- Searching ---------------------------------------------------
        case BSTATE_SEARCHING: {
            canvas_set_font(canvas, FontPrimary);
            canvas_draw_str_aligned(canvas, 64, 4, AlignCenter, AlignTop, "Battle!");
            canvas_set_font(canvas, FontSecondary);
            canvas_draw_str_aligned(canvas, 64, 18, AlignCenter, AlignTop,
                                    "Point at opponent");
            canvas_draw_str_aligned(canvas, 64, 30, AlignCenter, AlignTop,
                                    ir_state_label(app->ir_ctx.state));
            // Animated search dots
            char dots[5] = "    ";
            int  n = (s_bm.anim_tick / 4) % 5;
            for(int i = 0; i < n; i++) dots[i] = '.';
            canvas_draw_str_aligned(canvas, 64, 44, AlignCenter, AlignTop, dots);
            canvas_draw_str_aligned(canvas, 64, 58, AlignCenter, AlignBottom,
                                    "[Back] cancel");
            break;
        }

        // ---- Connected ---------------------------------------------------
        case BSTATE_CONNECTED:
            canvas_set_font(canvas, FontPrimary);
            canvas_draw_str_aligned(canvas, 64, 16, AlignCenter, AlignTop, "Opponent found!");
            canvas_set_font(canvas, FontSecondary);
            canvas_draw_str_aligned(canvas, 64, 34, AlignCenter, AlignTop,
                                    "Exchanging data...");
            break;

        // ---- Fighting ----------------------------------------------------
        case BSTATE_FIGHTING: {
            // My sprite left, opponent sprite right
            // Shake effect: alternate ±1px x-offset on fast ticks
            int my_shake  = (s_bm.anim_tick & 2) ? 1 : 0;
            int opp_shake = (s_bm.anim_tick & 2) ? 0 : 1;

            canvas_draw_xbm(canvas, 4 + my_shake,  16, 32, 32,
                            SPRITE_FOR(app->creature.species));
            canvas_draw_xbm(canvas, 92 - opp_shake, 16, 32, 32,
                            SPRITE_FOR((BMSpecies)app->remote_packet.species));

            // VS flash (only while fight_frames_left > 10)
            if(s_bm.fight_frames_left > 10 || (s_bm.anim_tick & 2)) {
                canvas_set_font(canvas, FontPrimary);
                canvas_draw_str_aligned(canvas, 64, 28, AlignCenter, AlignTop, "VS");
            }

            // My HP bar (bottom left)
            canvas_set_font(canvas, FontSecondary);
            char my_hp[10];
            snprintf(my_hp, sizeof(my_hp), "HP:%d", s_bm.my_hp_display);
            canvas_draw_str(canvas, 4, 56, my_hp);

            // Opponent HP bar (bottom right)
            char opp_hp[10];
            snprintf(opp_hp, sizeof(opp_hp), "HP:%d", s_bm.opp_hp_display);
            canvas_draw_str_aligned(canvas, 124, 56, AlignRight, AlignBottom, opp_hp);

            // Opponent name
            BMCreature tmp;
            memset(&tmp, 0, sizeof(tmp));
            tmp.species = (BMSpecies)app->remote_packet.species;
            canvas_draw_str_aligned(canvas, 64, 58, AlignCenter, AlignBottom,
                                    creature_name(&tmp));
            break;
        }

        // ---- Result ------------------------------------------------------
        case BSTATE_RESULT: {
            const char* headline = "DRAW!";
            if(app->battle_report.result == BATTLE_RESULT_WIN)       headline = "YOU WIN!";
            else if(app->battle_report.result == BATTLE_RESULT_LOSE) headline = "You lose...";

            canvas_set_font(canvas, FontPrimary);
            canvas_draw_str_aligned(canvas, 64, 6, AlignCenter, AlignTop, headline);
            canvas_set_font(canvas, FontSecondary);

            char xp_buf[20];
            snprintf(xp_buf, sizeof(xp_buf), "+%lu XP",
                     (unsigned long)app->battle_report.xp_gained);
            canvas_draw_str_aligned(canvas, 64, 22, AlignCenter, AlignTop, xp_buf);

            char rounds_buf[20];
            snprintf(rounds_buf, sizeof(rounds_buf), "%d rounds",
                     app->battle_report.rounds);
            canvas_draw_str_aligned(canvas, 64, 34, AlignCenter, AlignTop, rounds_buf);

            // W/L record
            char record_buf[20];
            snprintf(record_buf, sizeof(record_buf), "Record: %dW %dL",
                     app->creature.battles_won, app->creature.battles_lost);
            canvas_draw_str_aligned(canvas, 64, 46, AlignCenter, AlignTop, record_buf);

            canvas_draw_str_aligned(canvas, 64, 62, AlignCenter, AlignBottom,
                                    "[OK/Back] continue");
            break;
        }
    }
}

// ---------------------------------------------------------------------------
// Input callback
// ---------------------------------------------------------------------------
static bool battle_input(InputEvent* event, void* ctx) {
    BMApp* app = (BMApp*)ctx;
    if(event->type != InputTypeShort) return false;

    if(event->key == InputKeyOk || event->key == InputKeyBack) {
        view_dispatcher_send_custom_event(app->view_dispatcher, 10 /* BATTLE_DONE */);
    }
    return false;

}

// ---------------------------------------------------------------------------
// Scene lifecycle
// ---------------------------------------------------------------------------

void scene_battle_on_enter(void* ctx) {
    BMApp* app = (BMApp*)ctx;

    memset(&s_bm, 0, sizeof(s_bm));
    s_bm.phase           = BSTATE_SEARCHING;
    s_bm.fight_frames_left = 40;  // ~2s of fight animation at 20fps
    s_bm.my_hp_display   = app->creature.hp;
    s_bm.opp_hp_display  = 100;

    ir_init(&app->ir_ctx);
    ir_start_challenge(&app->ir_ctx);

    app->view_battle = view_alloc();
    view_set_context(app->view_battle, app);
    view_set_draw_callback(app->view_battle, battle_draw);
    view_set_input_callback(app->view_battle, battle_input);
    view_dispatcher_add_view(app->view_dispatcher, VIEW_BATTLE, app->view_battle);
    view_dispatcher_switch_to_view(app->view_dispatcher, VIEW_BATTLE);
}

bool scene_battle_on_event(void* ctx, SceneManagerEvent event) {
    BMApp* app = (BMApp*)ctx;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        uint32_t ev = event.event;

        // Global minute tick — creature already updated in app_custom_event_callback.
        // Check if creature just died while we were in the battle scene.
        if(ev == EV_MINUTE_TICK) {
            if(!creature_is_alive(&app->creature)) {
                scene_manager_next_scene(app->scene_manager, SCENE_GAMEOVER);
            }
            return true;
        }

        // BATTLE_DONE — OK/Back pressed on result screen
        if(ev == 10) {
            if(s_bm.phase == BSTATE_RESULT || s_bm.phase == BSTATE_SEARCHING) {
                scene_manager_previous_scene(app->scene_manager);
            }
            return true;
        }

        // Anim tick (EV_ANIM_TICK = 0)
        if(ev != EV_ANIM_TICK) return false;
        s_bm.anim_tick++;

        IrState ir = ir_tick(&app->ir_ctx);

        switch(s_bm.phase) {

            case BSTATE_SEARCHING:
                if(ir == IR_STATE_SENDING_PACKET) {
                    s_bm.phase = BSTATE_CONNECTED;
                    BMPacket my_pkt;
                    creature_to_packet(&app->creature, &my_pkt);
                    ir_send_packet(&app->ir_ctx, &my_pkt);
                } else if(ir == IR_STATE_ERROR_TIMEOUT) {
                    // Timed out — go back silently
                    scene_manager_previous_scene(app->scene_manager);
                }
                break;

            case BSTATE_CONNECTED:
                if(app->ir_ctx.packet_ready) {
                    app->remote_packet = app->ir_ctx.received_packet;
                    s_bm.phase = BSTATE_FIGHTING;
                    s_bm.opp_hp_display = 100;
                }
                break;

            case BSTATE_FIGHTING:
                if(s_bm.fight_frames_left > 0) {
                    s_bm.fight_frames_left--;
                    // Interpolate displayed HP downward as fight animates
                    if(s_bm.fight_frames_left % 4 == 0) {
                        if(s_bm.my_hp_display  > 80) s_bm.my_hp_display--;
                        if(s_bm.opp_hp_display > 80) s_bm.opp_hp_display--;
                    }
                } else {
                    // Resolve the battle
                    BMPacket my_pkt;
                    creature_to_packet(&app->creature, &my_pkt);
                    app->battle_report = battle_resolve(&my_pkt, &app->remote_packet);

                    bool won = (app->battle_report.result == BATTLE_RESULT_WIN);
                    battle_apply_result(&app->creature, &app->battle_report, won);

                    if(app->battle_report.result == BATTLE_RESULT_WIN)       sound_battle_win();
                    else if(app->battle_report.result == BATTLE_RESULT_LOSE) sound_battle_lose();

                    s_bm.my_hp_display  = app->creature.hp;
                    s_bm.opp_hp_display = 0;
                    s_bm.phase          = BSTATE_RESULT;
                }
                break;

            case BSTATE_RESULT:
                // Waiting for OK/Back — handled above
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

void scene_battle_on_exit(void* ctx) {
    BMApp* app = (BMApp*)ctx;
    ir_deinit(&app->ir_ctx);

    view_dispatcher_remove_view(app->view_dispatcher, VIEW_BATTLE);
    if(app->view_battle) { view_free(app->view_battle); app->view_battle = NULL; }
}
