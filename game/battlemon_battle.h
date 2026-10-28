#pragma once
#include "battlemon_creature.h"

// ---------------------------------------------------------------------------
// Battle result
// ---------------------------------------------------------------------------
typedef enum {
    BATTLE_RESULT_WIN,
    BATTLE_RESULT_LOSE,
    BATTLE_RESULT_DRAW
} BattleResult;

typedef struct {
    BattleResult result;
    uint8_t      my_damage;
    uint8_t      their_damage;
    uint8_t      rounds;        // how many rounds the fight lasted
    uint32_t     xp_gained;
} BattleReport;

// ---------------------------------------------------------------------------
// API
// ---------------------------------------------------------------------------

// Core deterministic resolver — both Flippers call this with the same inputs
// and get the same result without any further communication.
BattleReport battle_resolve(const BMPacket* mine, const BMPacket* theirs);

// Apply battle outcome back to the living creature
void battle_apply_result(BMCreature* c, const BattleReport* report, bool won);
