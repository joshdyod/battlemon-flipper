#pragma once
#include "battlemon_creature.h"

// Check every minute tick whether evolution should fire.
// Returns true if the creature evolved this tick.
bool evolution_check(BMCreature* c);

// Human-readable description of what the creature needs to evolve.
// Written into buf (max len bytes).  Returns false if at final form.
bool evolution_next_hint(const BMCreature* c, char* buf, int len);
