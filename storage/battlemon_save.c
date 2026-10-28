#include "battlemon_save.h"
#include <storage/storage.h>
#include <flipper_format/flipper_format.h>
#include <furi.h>

// FlipperFormat is a simple key:value text format native to the Flipper SDK.
// It's human-readable on the SD card, which is handy for debugging.

static Storage* storage_open(void) {
    return furi_record_open(RECORD_STORAGE);
}

static void storage_close(Storage* s) {
    furi_record_close(RECORD_STORAGE);
    UNUSED(s);
}

bool save_creature(const BMCreature* c) {
    Storage* storage = storage_open();
    storage_common_mkdir(storage, SAVE_DIR);

    FlipperFormat* ff = flipper_format_file_alloc(storage);
    bool ok = false;

    do {
        if(!flipper_format_file_open_always(ff, SAVE_PATH)) break;
        if(!flipper_format_write_header_cstr(ff, "BattleMon Save", 1)) break;

        uint32_t v;

        v = c->species;    if(!flipper_format_write_uint32(ff, "species",    &v, 1)) break;
        v = c->stage;      if(!flipper_format_write_uint32(ff, "stage",      &v, 1)) break;
        v = c->hp;         if(!flipper_format_write_uint32(ff, "hp",         &v, 1)) break;
        v = c->hunger;     if(!flipper_format_write_uint32(ff, "hunger",     &v, 1)) break;
        v = c->happiness;  if(!flipper_format_write_uint32(ff, "happiness",  &v, 1)) break;
        v = c->discipline; if(!flipper_format_write_uint32(ff, "discipline", &v, 1)) break;
        v = c->age_hours;  if(!flipper_format_write_uint32(ff, "age_hours",  &v, 1)) break;
        v = c->battles_won;if(!flipper_format_write_uint32(ff, "battles_won",&v, 1)) break;
        v = c->battles_lost;if(!flipper_format_write_uint32(ff,"battles_lost",&v,1)) break;
        v = c->total_xp;   if(!flipper_format_write_uint32(ff, "total_xp",  &v, 1)) break;
        v = c->attack;     if(!flipper_format_write_uint32(ff, "attack",     &v, 1)) break;
        v = c->defense;    if(!flipper_format_write_uint32(ff, "defense",    &v, 1)) break;
        v = c->speed;      if(!flipper_format_write_uint32(ff, "speed",      &v, 1)) break;
        v = c->is_sick;    if(!flipper_format_write_uint32(ff, "is_sick",    &v, 1)) break;

        ok = true;
    } while(false);

    flipper_format_free(ff);
    storage_close(storage);
    return ok;
}

bool load_creature(BMCreature* c) {
    Storage* storage = storage_open();
    FlipperFormat* ff = flipper_format_file_alloc(storage);
    bool ok = false;

    do {
        if(!flipper_format_file_open_existing(ff, SAVE_PATH)) break;

        FuriString* header = furi_string_alloc();
        uint32_t version = 0;
        if(!flipper_format_read_header(ff, header, &version)) {
            furi_string_free(header);
            break;
        }
        furi_string_free(header);

        uint32_t v;
        #define READ(key, field) \
            if(!flipper_format_read_uint32(ff, key, &v, 1)) break; \
            c->field = (typeof(c->field))v;

        READ("species",     species)
        READ("stage",       stage)
        READ("hp",          hp)
        READ("hunger",      hunger)
        READ("happiness",   happiness)
        READ("discipline",  discipline)
        READ("age_hours",   age_hours)
        READ("battles_won", battles_won)
        READ("battles_lost",battles_lost)
        READ("total_xp",    total_xp)
        READ("attack",      attack)
        READ("defense",     defense)
        READ("speed",       speed)
        READ("is_sick",     is_sick)
        #undef READ

        ok = true;
    } while(false);

    flipper_format_free(ff);
    storage_close(storage);
    return ok;
}

bool save_exists(void) {
    Storage* storage = storage_open();
    bool exists = storage_file_exists(storage, SAVE_PATH);
    storage_close(storage);
    return exists;
}

void save_delete(void) {
    Storage* storage = storage_open();
    storage_common_remove(storage, SAVE_PATH);
    storage_close(storage);
}
