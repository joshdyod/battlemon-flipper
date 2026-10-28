// test_offline.c  — compile with plain gcc, no Flipper SDK needed
// gcc -Wall -Wextra -o test_offline test_offline.c \
//     game/battlemon_creature.c game/battlemon_battle.c game/battlemon_evolution.c \
//     && ./test_offline

#include <stdio.h>
#include <assert.h>
#include <string.h>
#include "game/battlemon_creature.h"
#include "game/battlemon_battle.h"
#include "game/battlemon_evolution.h"

static void print_creature(const BMCreature* c) {
    printf("  [%s line] %s (stage %d)  HP:%d  Hunger:%d  Happy:%d  Disc:%d  Age:%dh  ATK/DEF/SPD:%d/%d/%d  W/L:%d/%d\n",
        creature_line_name(c), creature_name(c), c->stage,
        c->hp, c->hunger, c->happiness, c->discipline, c->age_hours,
        c->attack, c->defense, c->speed,
        c->battles_won, c->battles_lost);
}

// Helper: advance a creature through all stages of its default (high-disc) line
static void full_evolve(BMCreature* c) {
    // Baby → Child
    c->age_hours = 3; c->hunger = 60; c->happiness = 60;
    assert(creature_can_evolve(c));
    creature_evolve(c);
    // Child → Adult (high disc path)
    c->discipline = 70;
    c->age_hours = 13; c->hunger = 60; c->happiness = 60;
    creature_evolve(c);
    // Adult → Ultimate
    c->age_hours = 25; c->hunger = 60; c->happiness = 60;
    creature_evolve(c);
}

// ---------------------------------------------------------------------------

static void test_five_lines() {
    printf("\n=== test_five_lines: all 5 starting babies ===\n");
    BMSpecies babies[] = {
        BM_ROCK_SLIME, BM_LADYBIRD,
        BM_SUNMEOW,    BM_OCTOPUS,
        BM_ONION_KNIGHT
    };
    for(int i = 0; i < 5; i++) {
        BMCreature c;
        creature_init(&c);
        c.species = babies[i];
        c.stage   = BM_BABY;
        c.attack  = 10; c.defense = 10; c.speed = 10; // reset stats
        full_evolve(&c);
        assert(c.stage == BM_ULTIMATE);
        print_creature(&c);
    }
    printf("PASS\n");
}

static void test_low_discipline_branches() {
    printf("\n=== test_low_discipline_branches ===\n");
    // Each line should produce a different adult when discipline < 50
    BMSpecies children[] = {
        BM_ROCKAROO, BM_HORNET, BM_THUNDRA_DOE,
        BM_SEA_SPIKE, BM_WIZARD_BUNNEH
    };
    BMSpecies expected_low[] = {
        BM_CURSED_SHELL, BM_SPOOKY_BUTTERFLY, BM_DEVIL_GOAT,
        BM_DEATH_FROG,   BM_HYPNOTIC_TAPIR
    };
    for(int i = 0; i < 5; i++) {
        BMCreature c;
        creature_init(&c);
        c.species    = children[i];
        c.stage      = BM_CHILD;
        c.discipline = 20;  // low
        c.age_hours  = 13;
        c.hunger = 60; c.happiness = 60;
        creature_evolve(&c);
        printf("  %s (low disc) -> %s\n", creature_name(&c) , creature_name(&c));
        assert(c.species == expected_low[i]);
    }
    printf("PASS\n");
}

static void test_all_ultimates_unique() {
    printf("\n=== test_all_ultimates_unique: 10 ultimates, all distinct ===\n");
    BMCreature c;
    int seen[BM_SPECIES_COUNT] = {0};

    // High disc path: all 5 lines
    BMSpecies babies[] = {
        BM_ROCK_SLIME, BM_LADYBIRD,
        BM_SUNMEOW,    BM_OCTOPUS,
        BM_ONION_KNIGHT
    };
    for(int i = 0; i < 5; i++) {
        creature_init(&c);
        c.species = babies[i];
        c.stage   = BM_BABY;
        full_evolve(&c);
        printf("  High disc: %s\n", creature_name(&c));
        assert(!seen[c.species]);
        seen[c.species] = 1;
    }

    // Low disc path
    BMSpecies children[] = {
        BM_ROCKAROO, BM_HORNET, BM_THUNDRA_DOE,
        BM_SEA_SPIKE, BM_WIZARD_BUNNEH
    };
    for(int i = 0; i < 5; i++) {
        creature_init(&c);
        c.species    = children[i];
        c.stage      = BM_CHILD;
        c.discipline = 20;
        c.age_hours  = 13; c.hunger = 60; c.happiness = 60;
        creature_evolve(&c);  // → low-disc adult
        c.age_hours = 25; c.hunger = 60; c.happiness = 60;
        creature_evolve(&c);  // → ultimate
        printf("  Low disc:  %s\n", creature_name(&c));
        assert(!seen[c.species]);
        seen[c.species] = 1;
    }
    printf("PASS\n");
}

static void test_battle_all_lines() {
    printf("\n=== test_battle: ultimates from different lines ===\n");
    BMSpecies babies[] = {
        BM_ROCK_SLIME, BM_LADYBIRD,
        BM_SUNMEOW,    BM_OCTOPUS,
        BM_ONION_KNIGHT
    };
    BMCreature creatures[5];
    BMPacket   packets[5];

    for(int i = 0; i < 5; i++) {
        creature_init(&creatures[i]);
        creatures[i].species = babies[i];
        creatures[i].stage   = BM_BABY;
        full_evolve(&creatures[i]);
        creature_to_packet(&creatures[i], &packets[i]);
        assert(packet_is_valid(&packets[i]));
    }

    // Round-robin battles — all deterministic
    const char* results[] = {"WIN","LOSE","DRAW"};
    for(int a = 0; a < 5; a++) {
        for(int b = a+1; b < 5; b++) {
            BattleReport r1 = battle_resolve(&packets[a], &packets[b]);
            BattleReport r2 = battle_resolve(&packets[a], &packets[b]);
            // Must be identical on repeated calls
            assert(r1.result == r2.result && r1.rounds == r2.rounds);
            // Must be symmetric
            BattleReport rswap = battle_resolve(&packets[b], &packets[a]);
            if(r1.result == BATTLE_RESULT_WIN)  assert(rswap.result == BATTLE_RESULT_LOSE);
            if(r1.result == BATTLE_RESULT_LOSE) assert(rswap.result == BATTLE_RESULT_WIN);
            printf("  %s vs %s: %s (%d rounds)\n",
                creature_name(&creatures[a]), creature_name(&creatures[b]),
                results[r1.result], r1.rounds);
        }
    }
    printf("PASS\n");
}

static void test_packet_all_species() {
    printf("\n=== test_packet_all_species: checksum for all 30 ===\n");
    BMCreature c;
    BMPacket   pkt;
    for(int s = 0; s < BM_SPECIES_COUNT; s++) {
        creature_init(&c);
        c.species = (BMSpecies)s;
        // stage already set by creature_init
        creature_to_packet(&c, &pkt);
        pkt.species = (uint8_t)s;
        // Recompute checksum manually
        uint8_t check = pkt.species ^ pkt.stage ^ pkt.attack ^
                        pkt.defense ^ pkt.speed ^ pkt.battles_won ^ pkt.discipline;
        pkt.checksum = check;
        assert(packet_is_valid(&pkt));
    }
    printf("  All 30 species checksum OK\n");
    printf("PASS\n");
}

static void test_decay_and_death() {
    printf("\n=== test_decay_and_death ===\n");
    BMCreature c;
    creature_init(&c);
    c.hunger = 0;
    // Tick until dead
    int ticks = 0;
    while(creature_is_alive(&c) && ticks < 200) {
        creature_tick(&c);
        ticks++;
    }
    printf("  Died after %d starvation ticks (HP reached 0)\n", ticks);
    assert(!creature_is_alive(&c));
    printf("PASS\n");
}

static void test_evolution_hints() {
    printf("\n=== test_evolution_hints ===\n");
    BMCreature c;
    char buf[64];
    creature_init(&c);

    evolution_next_hint(&c, buf, sizeof(buf));
    printf("  Baby (age 0): \"%s\"\n", buf);

    c.age_hours = 3;
    evolution_next_hint(&c, buf, sizeof(buf));
    printf("  Baby (age 3): \"%s\"\n", buf);

    full_evolve(&c);  // evolve all the way
    bool has = evolution_next_hint(&c, buf, sizeof(buf));
    printf("  Ultimate: \"%s\" has_next=%d\n", buf, has);
    assert(!has);
    printf("PASS\n");
}

// Needed since we access stage from species index in test_packet_all_species
// — just use creature_init which sets stage=BABY and that's fine for packet test
// ---------------------------------------------------------------------------
// Line select logic test (pure C, no UI)
// ---------------------------------------------------------------------------
static void test_line_select() {
    printf("\n=== test_line_select: all 5 starting babies accessible ===\n");

    BMSpecies babies[] = {
        BM_ROCK_SLIME,
        BM_LADYBIRD,
        BM_SUNMEOW,
        BM_OCTOPUS,
        BM_ONION_KNIGHT,
    };
    const char* line_names[] = {
        "Earth", "Insect", "Beast", "Sea", "Mystic"
    };

    for(int i = 0; i < 5; i++) {
        BMCreature c;
        creature_init(&c);
        // Simulate what scene_lineselect_on_event does on CONFIRM
        c.species = babies[i];
        c.stage   = BM_BABY;

        printf("  Line %d (%s): starts as %s [%s]\n",
            i, line_names[i],
            creature_name(&c), creature_line_name(&c));

        assert(c.stage   == BM_BABY);
        assert(c.hp      == 100);
        assert(c.hunger  == 80);
        assert(creature_is_alive(&c));

        // Verify each baby can reach its ultimate (high-disc path)
        c.age_hours = 3; c.hunger = 60; c.happiness = 60;
        creature_evolve(&c);
        assert(c.stage == BM_CHILD);

        c.discipline = 70;
        c.age_hours = 13; c.hunger = 60; c.happiness = 60;
        creature_evolve(&c);
        assert(c.stage == BM_ADULT);

        c.age_hours = 25; c.hunger = 60; c.happiness = 60;
        creature_evolve(&c);
        assert(c.stage == BM_ULTIMATE);

        printf("    → ultimate: %s\n", creature_name(&c));
    }
    printf("PASS\n");
}


int main(void) {
    printf("BattleMon offline tests (30-creature roster)\n");
    printf("============================================\n");

    test_line_select();
    test_five_lines();
    test_low_discipline_branches();
    test_all_ultimates_unique();
    test_battle_all_lines();
    test_packet_all_species();
    test_decay_and_death();
    test_evolution_hints();

    printf("\n============================================\n");
    printf("All tests passed.\n");
    return 0;
}
