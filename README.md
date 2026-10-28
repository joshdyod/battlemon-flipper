# BattleMon — Virtual pet for the Flipper Zero

A Digimon-style virtual pet with IR battles between devices, built for the Flipper Zero.

```
          ██████████          
        ██          ██        
      ████  ████      ██      
  ████        ████    ██      
██          ██████      ██    
████████                ██    
  ██                    ██    
    ██████████        ██      
      ████        ██  ██      
    ██  ██      ██      ██    
    ██████      ██████  ██    
        ████        ██    ██  
    ████    ████████      ██  
  ██  ██    ██  ██  ██  ██  ██
  ████████████  ██████████████
```

---

## Install (pre-compiled)

The easiest way — no build tools needed.

### Option A — qFlipper
1. Download **[battlemon_flipper.fap](dist/battlemon_flipper.fap)** from this repo
2. Open **qFlipper** and connect your Flipper Zero via USB
3. Go to **File Manager → SD Card → apps → Games**
4. Drag `battlemon_flipper.fap` into that folder
5. On your Flipper: **Apps → Games → BattleMon**

### Option B — SD card
1. Download **[battlemon_flipper.fap](dist/battlemon_flipper.fap)**
2. Copy it to your SD card at `/apps/Games/battlemon_flipper.fap`
3. Eject and insert the SD card into your Flipper
4. On your Flipper: **Apps → Games → BattleMon**

### Option C — Flipper Lab (web, no cables)
1. Go to **[lab.flipper.net](https://lab.flipper.net)** and connect your Flipper via WebUSB
2. Click **Files → SD Card → apps → Games**
3. Upload `battlemon_flipper.fap`

> **Note:** The `.fap` was compiled against Flipper firmware **1.5.1**. If you're on a different version and it won't load, build from source (see below).

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

## Controls

| Button | Action |
|--------|--------|
| OK | Open action menu |
| Up | Toggle sleep |
| Left / Right | Navigate line select |
| Back | Go back / exit app |

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

Evolution triggers when age threshold is reached (2h / 12h / 24h) and Hunger ≥ 20, Happiness ≥ 20.

---

## Build from source

```bash
# Install ufbt (macOS/Linux)
pipx install ufbt

# Clone this repo
git clone https://github.com/joshdyod/battlemon-flipper.git
cd battlemon-flipper

# Download the Flipper SDK and build
ufbt update
ufbt

# Output: dist/battlemon_flipper.fap
```

---

## IR battle protocol

1. Both players select **Battle!** from the menu
2. Both devices broadcast a challenge signal
3. First to receive replies with ACK — handshake complete
4. Both exchange an 8-byte creature packet (species, stage, ATK, DEF, SPD, wins, discipline, checksum)
5. Both run the same deterministic battle calculation locally — no further communication needed
6. Result applied: XP gained, happiness adjusted, HP reduced by fatigue

---

## Save file

`/ext/apps_data/battlemon_flipper/save.bmon` — plain text, readable on any device.
