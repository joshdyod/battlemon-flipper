#include "battlemon_creature.h"
#include <string.h>

// ---------------------------------------------------------------------------
// Species data table
// ---------------------------------------------------------------------------
typedef struct {
    const char* name;
    const char* line;
    BMStage   stage;
    uint8_t     base_attack;
    uint8_t     base_defense;
    uint8_t     base_speed;
} SpeciesData;

static const SpeciesData SPECIES_TABLE[BM_SPECIES_COUNT] = {
    // LINE 1: Earth/Rock
    { "Rock Slime",       "Earth", BM_BABY,      9,  12,  8  },
    { "Rockaroo",         "Earth", BM_CHILD,     22, 18,  25 },
    { "Adamantoise",      "Earth", BM_ADULT,     40, 55,  28 },
    { "Cursed Shell",     "Earth", BM_ADULT,     35, 45,  38 },
    { "Magmalin",         "Earth", BM_ULTIMATE,  70, 75,  52 },
    { "Dread Carp",       "Earth", BM_ULTIMATE,  65, 55,  78 },
    // LINE 2: Insect
    { "Ladybird",         "Insect",BM_BABY,      10, 8,   15 },
    { "Hornet",           "Insect",BM_CHILD,     28, 18,  32 },
    { "Bug Chimp",        "Insect",BM_ADULT,     48, 30,  42 },
    { "Spooky Butterfly", "Insect",BM_ADULT,     38, 22,  58 },
    { "Spiker",           "Insect",BM_ULTIMATE,  78, 55,  62 },
    { "Harpy",            "Insect",BM_ULTIMATE,  65, 45,  82 },
    // LINE 3: Beast
    { "SunMeow",          "Beast", BM_BABY,      12, 10,  14 },
    { "Thundra Doe",      "Beast", BM_CHILD,     24, 20,  28 },
    { "Cerberus",         "Beast", BM_ADULT,     52, 38,  35 },
    { "Devil Goat",       "Beast", BM_ADULT,     44, 32,  50 },
    { "Flaming Stallion", "Beast", BM_ULTIMATE,  80, 52,  72 },
    { "Heavens Bull",     "Beast", BM_ULTIMATE,  72, 68,  55 },
    // LINE 4: Sea
    { "Octopus",          "Sea",   BM_BABY,      8,  12,  12 },
    { "Sea Spike",        "Sea",   BM_CHILD,     20, 24,  22 },
    { "Nessie",           "Sea",   BM_ADULT,     42, 42,  38 },
    { "Death Frog",       "Sea",   BM_ADULT,     50, 30,  48 },
    { "Metal Snake",      "Sea",   BM_ULTIMATE,  72, 60,  68 },
    { "Baloonor",         "Sea",   BM_ULTIMATE,  60, 50,  88 },
    // LINE 5: Mystic
    { "Onion Knight",     "Mystic",BM_BABY,      10, 14,  10 },
    { "Wizard Bunneh",    "Mystic",BM_CHILD,     25, 22,  20 },
    { "Specter",          "Mystic",BM_ADULT,     45, 35,  42 },
    { "Hypnotic Tapir",   "Mystic",BM_ADULT,     38, 48,  32 },
    { "Claw King",        "Mystic",BM_ULTIMATE,  75, 62,  65 },
    { "Fungusaurus Rex",  "Mystic",BM_ULTIMATE,  68, 70,  58 },
};

// Evolution path: species -> next species (255 = no further evolution)
// Child → Adult branches on discipline (>=50 = high, <50 = low)
// High-discipline adults are at even offsets within the adult pair.
// Evolution path for babies and children points to the HIGH-discipline adult;
// the branch logic in creature_evolve() handles the low-discipline fork.
static const uint8_t EVOLVE_HIGH[BM_SPECIES_COUNT] = {
    BM_ROCKAROO,          // Rock Slime → Rockaroo
    BM_ADAMANTOISE,       // Rockaroo   → Adamantoise (high disc)
    BM_MAGMALIN,          // Adamantoise→ Magmalin
    BM_DREAD_CARP,        // Cursed Shell→ Dread Carp
    255, 255,                  // Ultimates — no further

    BM_HORNET,            // Ladybird → Hornet
    BM_BUG_CHIMP,         // Hornet   → Bug Chimp (high disc)
    BM_SPIKER,            // Bug Chimp→ Spiker
    BM_HARPY,             // Spooky Butterfly → Harpy
    255, 255,

    BM_THUNDRA_DOE,       // SunMeow → Thundra Doe
    BM_CERBERUS,          // Thundra Doe → Cerberus (high disc)
    BM_FLAMING_STALLION,  // Cerberus → Flaming Stallion
    BM_HEAVENS_BULL,      // Devil Goat → Heavens Bull
    255, 255,

    BM_SEA_SPIKE,         // Octopus → Sea Spike
    BM_NESSIE,            // Sea Spike → Nessie (high disc)
    BM_METAL_SNAKE,       // Nessie → Metal Snake
    BM_BALOONOR,          // Death Frog → Baloonor
    255, 255,

    BM_WIZARD_BUNNEH,     // Onion Knight → Wizard Bunneh
    BM_SPECTER,           // Wizard Bunneh → Specter (high disc)
    BM_CLAW_KING,         // Specter → Claw King
    BM_FUNGUSAURUS_REX,   // Hypnotic Tapir → Fungusaurus Rex
    255, 255,
};

// Low-discipline adult for each child species (only used at BM_CHILD)
// Maps child species -> low-discipline adult. 255 = not a child or N/A.
static const uint8_t EVOLVE_LOW_ADULT[BM_SPECIES_COUNT] = {
    255, BM_CURSED_SHELL,   255, 255, 255, 255,  // Line 1
    255, BM_SPOOKY_BUTTERFLY,255,255, 255, 255,  // Line 2
    255, BM_DEVIL_GOAT,     255, 255, 255, 255,  // Line 3
    255, BM_DEATH_FROG,     255, 255, 255, 255,  // Line 4
    255, BM_HYPNOTIC_TAPIR, 255, 255, 255, 255,  // Line 5
};

// Age in hours required to be eligible for each stage transition
static const uint8_t EVOLVE_AGE[BM_STAGE_COUNT] = {
    2,   // Baby    → Child    (2h)
    12,  // Child   → Adult    (12h)
    24,  // Adult   → Ultimate (24h)
    255, // Ultimate — never
};

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
static void apply_species_stats(BMCreature* c) {
    c->attack  = SPECIES_TABLE[c->species].base_attack;
    c->defense = SPECIES_TABLE[c->species].base_defense;
    c->speed   = SPECIES_TABLE[c->species].base_speed;
}

static uint8_t clamp8(int v) {
    if(v < 0)   return 0;
    if(v > 100) return 100;
    return (uint8_t)v;
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------
void creature_init(BMCreature* c) {
    memset(c, 0, sizeof(BMCreature));
    // Player picks a line at start — default to Earth line (Rock Slime)
    c->species    = BM_ROCK_SLIME;
    c->stage      = BM_BABY;
    c->hp         = 100;
    c->hunger     = 80;
    c->happiness  = 80;
    c->discipline = 50;
    apply_species_stats(c);
}

void creature_tick(BMCreature* c) {
    if(c->is_sleeping) {
        c->hp = clamp8((int)c->hp + 1);
        return;
    }

    if(c->hunger > 0) c->hunger--;

    static uint8_t minute_counter = 0;
    minute_counter++;

    if(minute_counter % 2 == 0 && c->happiness > 0) c->happiness--;

    if(c->hunger == 0) {
        c->hp = clamp8((int)c->hp - 2);
        c->is_sick = true;
        c->needs_attention = true;
    }

    if(c->happiness == 0) {
        c->hp = clamp8((int)c->hp - 1);
        c->needs_attention = true;
    }

    if(minute_counter >= 60) {
        minute_counter = 0;
        if(c->age_hours < 255) c->age_hours++;
    }
}

void creature_feed(BMCreature* c) {
    if(!creature_is_alive(c)) return;
    if(c->hunger > 90) {
        c->happiness = clamp8((int)c->happiness - 5);
    } else {
        c->hunger = clamp8((int)c->hunger + 30);
    }
    c->needs_attention = false;
}

void creature_play(BMCreature* c) {
    if(!creature_is_alive(c) || c->is_sleeping) return;
    c->happiness = clamp8((int)c->happiness + 20);
    c->hunger    = clamp8((int)c->hunger    - 10);
    if(c->is_sick) c->happiness = clamp8((int)c->happiness - 10);
}

void creature_heal(BMCreature* c) {
    if(!creature_is_alive(c)) return;
    c->hp      = clamp8((int)c->hp + 40);
    c->is_sick = false;
    c->needs_attention = false;
}

void creature_discipline(BMCreature* c) {
    if(!creature_is_alive(c)) return;
    c->discipline = clamp8((int)c->discipline + 10);
    c->happiness  = clamp8((int)c->happiness  - 5);
}

bool creature_is_alive(const BMCreature* c) {
    return c->hp > 0;
}

bool creature_can_evolve(const BMCreature* c) {
    if(!creature_is_alive(c))              return false;
    if(c->stage == BM_ULTIMATE)         return false;
    if(EVOLVE_HIGH[c->species] == 255)     return false;
    if(c->age_hours < EVOLVE_AGE[c->stage]) return false;
    if(c->hunger < 20 || c->happiness < 20) return false;
    return true;
}

void creature_evolve(BMCreature* c) {
    if(!creature_can_evolve(c)) return;

    // At child stage: branch on discipline
    if(c->stage == BM_CHILD) {
        uint8_t low = EVOLVE_LOW_ADULT[c->species];
        if(c->discipline < 50 && low != 255) {
            c->species = (BMSpecies)low;
        } else {
            c->species = (BMSpecies)EVOLVE_HIGH[c->species];
        }
    } else {
        c->species = (BMSpecies)EVOLVE_HIGH[c->species];
    }

    c->stage     = SPECIES_TABLE[c->species].stage;
    c->age_hours = 0;
    c->hp        = 100;
    apply_species_stats(c);
}

const char* creature_name(const BMCreature* c) {
    return SPECIES_TABLE[c->species].name;
}

const char* creature_line_name(const BMCreature* c) {
    return SPECIES_TABLE[c->species].line;
}

void creature_to_packet(const BMCreature* c, BMPacket* pkt) {
    pkt->species     = (uint8_t)c->species;
    pkt->stage       = (uint8_t)c->stage;
    pkt->attack      = c->attack;
    pkt->defense     = c->defense;
    pkt->speed       = c->speed;
    pkt->battles_won = c->battles_won;
    pkt->discipline  = c->discipline;
    pkt->checksum    = pkt->species ^ pkt->stage ^ pkt->attack ^
                       pkt->defense ^ pkt->speed ^ pkt->battles_won ^
                       pkt->discipline;
}

bool packet_is_valid(const BMPacket* pkt) {
    uint8_t check = pkt->species ^ pkt->stage ^ pkt->attack ^
                    pkt->defense ^ pkt->speed ^ pkt->battles_won ^
                    pkt->discipline;
    return check == pkt->checksum && pkt->species < BM_SPECIES_COUNT;
}
