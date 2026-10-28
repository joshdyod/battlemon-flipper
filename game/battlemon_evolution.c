#include "battlemon_evolution.h"
#include <stdio.h>

bool evolution_check(BMCreature* c) {
    if(!creature_can_evolve(c)) return false;

    // Extra quality checks — poor care gives a weaker evolution path
    // (Currently all paths are fixed, but this is where you'd branch
    // to a "dark" evolution line if happiness/discipline are very low)

    creature_evolve(c);
    return true;
}

bool evolution_next_hint(const BMCreature* c, char* buf, int len) {
    if(c->stage == BM_ULTIMATE) {
        snprintf(buf, len, "Final form!");
        return false;
    }

    // Work out what's still missing
    int age_needed = 0;
    switch(c->stage) {
        case BM_BABY:    age_needed = 2;  break;
        case BM_CHILD:   age_needed = 12; break;
        case BM_ADULT:   age_needed = 24; break;
        default: break;
    }

    int age_left = age_needed - (int)c->age_hours;
    if(age_left < 0) age_left = 0;

    if(age_left > 0) {
        snprintf(buf, len, "Evolves in %dh", age_left);
    } else if(c->hunger < 20) {
        snprintf(buf, len, "Feed me!");
    } else if(c->happiness < 20) {
        snprintf(buf, len, "Play with me!");
    } else {
        snprintf(buf, len, "Ready!");
    }

    return true;
}
