# BattleMon — Virtual pet for the Flipper Zero

A BattleMon-style virtual pet with IR battles between devices, built on the
Flipper Zero's custom firmware using C and the Flipper SDK.

---

## Features

- **30 creatures** across 5 evolution lines (Earth, Insect, Beast, Sea, Mystic)
- **4 evolution stages** per line: Baby → Child → Adult → Ultimate
- **Discipline branching** — your care style determines which Adult/Ultimate you get
- **IR battles** — point two Flippers at each other and fight
- **Deterministic battles** — both devices compute the same result from one packet exchange
- **Sound effects** — evolution fanfare, battle win/loss, death, care actions
- **LED notifications** — RGB flashes red when the creature needs attention
- **Auto-save** every minute via FlipperFormat on the SD card
- **First-launch line select** — scroll through all 5 lines to pick your start

---

## Evolution lines

```
EARTH     Rock Slime → Rockaroo → Adamantoise* / Cursed Shell → Magmalin / Dread Carp
INSECT    Ladybird   → Hornet   → Bug Chimp*   / Spooky Butterfly → Spiker / Harpy
BEAST     SunMeow    → Thundra Doe → Cerberus* / Devil Goat → Flaming Stallion / Heavens Bull
SEA       Octopus    → Sea Spike → Nessie*     / Death Frog → Metal Snake / Baloonor
MYSTIC    Onion Knight → Wizard Bunneh → Specter* / Hypnotic Tapir → Claw King / Fungusaurus Rex

* = high-discipline path (discipline ≥ 50 at Child stage)
```

Evolution triggers when:
- Creature has reached the age threshold for its current stage (2h / 12h / 24h)
- Hunger ≥ 20 and Happiness ≥ 20

---

## Controls

| Button | Action |
|--------|--------|
| OK     | Open action menu |
| Up     | Toggle sleep     |
| Left/Right | Navigate line select |
| Back   | Exit app (home screen) / go back |

---

## Building

### Requirements

- [Flipper Zero firmware repo](https://github.com/flipperdevices/flipperzero-firmware) cloned
- `fbt` build tool (included in the firmware repo)

### Steps

```bash
# 1. Clone the Flipper firmware
git clone --recursive https://github.com/flipperdevices/flipperzero-firmware.git
cd flipperzero-firmware

# 2. Copy this project into the external apps folder
cp -r /path/to/battlemon_flipper applications_user/

# 3. Build the .fap
./fbt fap_battlemon_flipper

# 4. Deploy to Flipper over USB
./fbt launch_app APPSRC=applications_user/battlemon_flipper
```

The compiled `.fap` lands in `build/f7-firmware-D/.extapps/battlemon_flipper.fap`.
Copy to `/ext/apps/Games/` on your SD card for a permanent install.

### Testing logic without a Flipper

All game logic (`game/`) is SDK-free:

```bash
gcc -Wall -Wextra -o test_offline test_offline.c \
    game/battlemon_creature.c \
    game/battlemon_battle.c \
    game/battlemon_evolution.c \
    && ./test_offline
# → All 8 tests passed.
```

---

## IR battle protocol

1. Both players select **Battle!** from the menu
2. Both devices broadcast a `CHALLENGE` byte (0xD1) at 38 kHz
3. First device to receive a challenge replies with `ACK` (0xAC)
4. Both exchange an 8-byte `DigiPacket` (species, stage, ATK, DEF, SPD, wins, discipline, checksum)
5. Both run `battle_resolve()` with identical inputs → same winner, no further comms
6. Result applied: XP gained, happiness adjusted, HP reduced by fatigue

Packet integrity is verified via XOR checksum. A corrupted packet aborts the battle cleanly.

---

## Battle formula

```
power = (ATK × 3) + DEF + (SPD × 2) + (battles_won × 4) + luck
luck  ∈ [0 .. 32 − (discipline × 28/100)]
```

Higher discipline shrinks the luck range — disciplined creatures fight more consistently.
Speed determines who strikes first each round. Up to 5 rounds per battle.

---

## File structure

```
battlemon_flipper/
├── application.fam           App manifest
├── battlemon_app.c             Entry point, global event routing, timers, LED
├── battlemon_app.h             Shared DigiApp struct, view/event enums
│
├── game/                     Pure C — zero SDK dependencies, testable offline
│   ├── battlemon_creature.c/h  Creature struct, stats, evolution, packet encode
│   ├── battlemon_battle.c/h    Deterministic battle resolver (LCG seeded from both creatures)
│   └── battlemon_evolution.c/h Evolution check + hint strings
│
├── scenes/                   One file per screen
│   ├── battlemon_scene.c/h     Scene handler table
│   ├── scene_lineselect.c    First-launch line picker
│   ├── scene_home.c          Idle animation, stat bars, sleep toggle
│   ├── scene_misc.c          Menu, Status, Evolve, GameOver scenes
│   └── scene_battle.c        IR handshake, fight animation, result
│
├── comms/
│   └── battlemon_ir.c/h        Full IR TX/RX with pulse-distance encoding
│
├── storage/
│   └── battlemon_save.c/h      FlipperFormat save/load to /ext/apps_data/
│
├── assets/
│   ├── sprites.h             30 × 32×32 XBM byte arrays (WB sprite pack)
│   └── sounds.h              Speaker-based sound effects
│
└── test_offline.c            8-test offline suite (plain gcc)
```

---

## Save file location

`/ext/apps_data/battlemon_flipper/save.digi` — plain text FlipperFormat, readable on any device.
