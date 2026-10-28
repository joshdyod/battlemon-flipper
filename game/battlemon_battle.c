#include "battlemon_battle.h"

// ---------------------------------------------------------------------------
// Deterministic PRNG (LCG — fast, tiny, no stdlib needed)
// Both devices seed this identically so they produce the same sequence.
// ---------------------------------------------------------------------------
static uint32_t lcg_next(uint32_t* state) {
    *state = (*state * 1103515245u + 12345u) & 0x7fffffffu;
    return *state;
}

static uint32_t battle_seed(const BMPacket* a, const BMPacket* b) {
    // Seed is commutative so it doesn't matter who is "mine" vs "theirs"
    return (uint32_t)(a->species + b->species) * 31u
         + (uint32_t)(a->attack  + b->attack)  * 17u
         + (uint32_t)(a->speed   + b->speed)   * 7u
         + (uint32_t)(a->battles_won + b->battles_won);
}

// ---------------------------------------------------------------------------
// Power formula
// Higher discipline = tighter variance (less luck, more skill)
// ---------------------------------------------------------------------------
static uint32_t compute_power(const BMPacket* p, uint32_t* rng) {
    uint32_t base = (uint32_t)p->attack * 3u
                  + (uint32_t)p->defense
                  + (uint32_t)p->speed * 2u
                  + (uint32_t)p->battles_won * 4u;

    // Random component: 0..32, scaled down by discipline (more discipline = less luck)
    uint32_t luck_range = 32u - ((uint32_t)p->discipline * 28u / 100u);
    uint32_t luck       = lcg_next(rng) % (luck_range + 1u);

    return base + luck;
}

// ---------------------------------------------------------------------------
// Round-by-round simulation (max 5 rounds)
// ---------------------------------------------------------------------------
BattleReport battle_resolve(const BMPacket* mine, const BMPacket* theirs) {
    BattleReport report = {0};

    uint32_t rng   = battle_seed(mine, theirs);
    uint32_t my_hp = 100;
    uint32_t th_hp = 100;
    uint8_t  rounds = 0;

    for(int i = 0; i < 5 && my_hp > 0 && th_hp > 0; i++) {
        rounds++;

        uint32_t my_pow   = compute_power(mine,   &rng);
        uint32_t their_pow = compute_power(theirs, &rng);

        // Damage = attacker power reduced by defender's defense stat
        uint32_t my_dmg   = (my_pow   > (uint32_t)theirs->defense) ?
                             my_pow   - (uint32_t)theirs->defense / 2u : 1u;
        uint32_t their_dmg = (their_pow > (uint32_t)mine->defense) ?
                             their_pow - (uint32_t)mine->defense  / 2u : 1u;

        // Speed determines who strikes first each round
        if(mine->speed >= theirs->speed) {
            // I attack first
            if(their_dmg > th_hp) { th_hp = 0; break; }
            th_hp -= their_dmg;
            if(my_dmg > my_hp)   { my_hp = 0; break; }
            my_hp -= my_dmg;
        } else {
            // They attack first
            if(my_dmg > my_hp)   { my_hp = 0; break; }
            my_hp -= my_dmg;
            if(their_dmg > th_hp) { th_hp = 0; break; }
            th_hp -= their_dmg;
        }

        report.my_damage   += (uint8_t)(my_dmg   < 255 ? my_dmg   : 255);
        report.their_damage += (uint8_t)(their_dmg < 255 ? their_dmg : 255);
    }

    report.rounds = rounds;

    if(my_hp == 0 && th_hp == 0)     report.result = BATTLE_RESULT_DRAW;
    else if(th_hp == 0 || my_hp > th_hp) report.result = BATTLE_RESULT_WIN;
    else                               report.result = BATTLE_RESULT_LOSE;

    // XP: always gain something; more for a win, less for a loss
    switch(report.result) {
        case BATTLE_RESULT_WIN:  report.xp_gained = 50 + theirs->stage * 25; break;
        case BATTLE_RESULT_DRAW: report.xp_gained = 20;                      break;
        case BATTLE_RESULT_LOSE: report.xp_gained = 10;                      break;
    }

    return report;
}

void battle_apply_result(BMCreature* c, const BattleReport* report, bool won) {
    c->total_xp += report->xp_gained;

    if(won) {
        c->battles_won++;
        c->happiness = (c->happiness + 20 > 100) ? 100 : c->happiness + 20;
    } else {
        c->battles_lost++;
        c->happiness = (c->happiness > 10) ? c->happiness - 10 : 0;
    }

    // Take some HP damage from fighting
    uint8_t fatigue = report->my_damage / 4;
    c->hp = (c->hp > fatigue) ? c->hp - fatigue : 1;
}
