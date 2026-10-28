#pragma once
#include <furi.h>
#include <furi_hal_speaker.h>

// ---------------------------------------------------------------------------
// Simple blocking sound effects using the Flipper's built-in speaker.
// All functions block for the duration of the sound (<500ms each).
// Call only from the GUI thread (inside scene callbacks), never from ISR.
// ---------------------------------------------------------------------------

// Note frequencies (Hz)
#define NOTE_C4   262
#define NOTE_D4   294
#define NOTE_E4   330
#define NOTE_F4   349
#define NOTE_G4   392
#define NOTE_A4   440
#define NOTE_B4   494
#define NOTE_C5   523
#define NOTE_D5   587
#define NOTE_E5   659
#define NOTE_G5   784
#define NOTE_REST 0

static inline void _play_note(uint32_t freq, uint32_t ms) {
    if(freq == NOTE_REST) {
        furi_delay_ms(ms);
        return;
    }
    if(furi_hal_speaker_acquire(500)) {
        furi_hal_speaker_start((float)freq, 0.3f);
        furi_delay_ms(ms);
        furi_hal_speaker_stop();
        furi_hal_speaker_release();
    }
}

// Rising 4-note fanfare — played when creature evolves
static inline void sound_evolve(void) {
    _play_note(NOTE_C4, 80);
    _play_note(NOTE_E4, 80);
    _play_note(NOTE_G4, 80);
    _play_note(NOTE_C5, 200);
    _play_note(NOTE_REST, 60);
    _play_note(NOTE_C5, 120);
}

// Two quick high beeps — played on battle victory
static inline void sound_battle_win(void) {
    _play_note(NOTE_G4, 80);
    _play_note(NOTE_REST, 40);
    _play_note(NOTE_C5, 80);
    _play_note(NOTE_REST, 40);
    _play_note(NOTE_E5, 160);
}

// Descending two notes — played on battle loss
static inline void sound_battle_lose(void) {
    _play_note(NOTE_E4, 120);
    _play_note(NOTE_REST, 40);
    _play_note(NOTE_C4, 200);
}

// Low descending tone — played when creature dies
static inline void sound_death(void) {
    _play_note(NOTE_G4, 100);
    _play_note(NOTE_F4, 100);
    _play_note(NOTE_E4, 100);
    _play_note(NOTE_D4, 100);
    _play_note(NOTE_C4, 300);
}

// Single short blip — played on feed / play / heal
static inline void sound_action(void) {
    _play_note(NOTE_A4, 60);
}
