#pragma once
#include <stdint.h>
#include <stdbool.h>

// ---------------------------------------------------------------------------
// Species — 30 creatures across 5 evolution lines, 6 per line
// Each line: Baby → Child → Adult A / Adult B → Ultimate A / Ultimate B
// Adult branch is determined by discipline score at Child stage.
// ---------------------------------------------------------------------------
typedef enum {
    // LINE 1: Earth/Rock
    BM_ROCK_SLIME = 0,    // Baby
    BM_ROCKAROO,          // Child
    BM_ADAMANTOISE,       // Adult  (high discipline)
    BM_CURSED_SHELL,      // Adult  (low discipline)
    BM_MAGMALIN,          // Ultimate (from Adamantoise)
    BM_DREAD_CARP,        // Ultimate (from Cursed Shell)

    // LINE 2: Insect
    BM_LADYBIRD,          // Baby
    BM_HORNET,            // Child
    BM_BUG_CHIMP,         // Adult  (high discipline)
    BM_SPOOKY_BUTTERFLY,  // Adult  (low discipline)
    BM_SPIKER,            // Ultimate (from Bug Chimp)
    BM_HARPY,             // Ultimate (from Spooky Butterfly)

    // LINE 3: Beast
    BM_SUNMEOW,           // Baby
    BM_THUNDRA_DOE,       // Child
    BM_CERBERUS,          // Adult  (high discipline)
    BM_DEVIL_GOAT,        // Adult  (low discipline)
    BM_FLAMING_STALLION,  // Ultimate (from Cerberus)
    BM_HEAVENS_BULL,      // Ultimate (from Devil Goat)

    // LINE 4: Sea
    BM_OCTOPUS,           // Baby
    BM_SEA_SPIKE,         // Child
    BM_NESSIE,            // Adult  (high discipline)
    BM_DEATH_FROG,        // Adult  (low discipline)
    BM_METAL_SNAKE,       // Ultimate (from Nessie)
    BM_BALOONOR,          // Ultimate (from Death Frog)

    // LINE 5: Mystic
    BM_ONION_KNIGHT,      // Baby
    BM_WIZARD_BUNNEH,     // Child
    BM_SPECTER,           // Adult  (high discipline)
    BM_HYPNOTIC_TAPIR,    // Adult  (low discipline)
    BM_CLAW_KING,         // Ultimate (from Specter)
    BM_FUNGUSAURUS_REX,   // Ultimate (from Hypnotic Tapir)

    BM_SPECIES_COUNT              // = 30
} BMSpecies;

typedef enum {
    BM_BABY = 0,
    BM_CHILD,
    BM_ADULT,
    BM_ULTIMATE,
    BM_STAGE_COUNT
} BMStage;

typedef struct {
    BMSpecies species;
    BMStage   stage;
    uint8_t hp;
    uint8_t hunger;
    uint8_t happiness;
    uint8_t discipline;
    uint8_t  age_hours;
    uint8_t  battles_won;
    uint8_t  battles_lost;
    uint32_t total_xp;
    uint8_t attack;
    uint8_t defense;
    uint8_t speed;
    bool is_sick;
    bool is_sleeping;
    bool needs_attention;
} BMCreature;

typedef struct {
    uint8_t species;
    uint8_t stage;
    uint8_t attack;
    uint8_t defense;
    uint8_t speed;
    uint8_t battles_won;
    uint8_t discipline;
    uint8_t checksum;
} BMPacket;

void        creature_init(BMCreature* c);
void        creature_tick(BMCreature* c);
void        creature_feed(BMCreature* c);
void        creature_play(BMCreature* c);
void        creature_heal(BMCreature* c);
void        creature_discipline(BMCreature* c);
bool        creature_is_alive(const BMCreature* c);
bool        creature_can_evolve(const BMCreature* c);
void        creature_evolve(BMCreature* c);
const char* creature_name(const BMCreature* c);
const char* creature_line_name(const BMCreature* c);
void        creature_to_packet(const BMCreature* c, BMPacket* pkt);
bool        packet_is_valid(const BMPacket* pkt);
