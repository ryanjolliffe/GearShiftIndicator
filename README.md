# GearShiftIndicator 🚗

Use Hall effect sensors to determine and display vehicle gear selection on an 8×8 LED matrix — complete with animations triggered by specific gear-change sequences.

> Originally built for a scratch-built Locost 7.

---

## How It Works

Six Hall effect sensors are mounted to detect gear position (Reverse through 1st). Park is assumed when no sensor fires. On each loop the firmware:

1. **Reads** all sensors and debounces the result (two reads, 5 ms apart)
2. **Displays** the current gear character on the LED matrix, scrolling up or down to match the direction of travel through the gearbox
3. **Watches** the last four gear changes for special sequences that trigger bonus animations

---

## Hardware

| Component | Detail |
|-----------|--------|
| Microcontroller | Arduino (any with SPI + 6 free digital pins) |
| Display | 8×8 MAX72xx LED matrix module |
| Sensors | 6× Hall effect sensors (one per selectable gear) |
| SPI pins | DATA → 11 · CLK → 13 · CS → 10 |
| Sensor pins | 3, 4, 5, 6, 7, 8 (R → 1st) |

> **Tip:** If your sensors have open-drain outputs and no external pull-up resistors, change `INPUT` to `INPUT_PULLUP` in `hallSetup()`.

---

## Gear Mapping

| Index | Character | Gear |
|-------|-----------|------|
| 0 | `P` | Park *(no sensor — all HIGH)* |
| 1 | `R` | Reverse |
| 2 | `N` | Neutral |
| 3 | `D` | Drive |
| 4 | `3` | 3rd |
| 5 | `2` | 2nd |
| 6 | `1` | 1st |

The character shown on the display comes directly from `GearChars[]`, so adding or reordering gears is a one-line change.

---

## Sensor Debounce

`getGear()` reads the sensor array **twice** with a short pause between reads:

- Both reads agree → the new gear is accepted ✅
- Reads disagree → the last known stable gear is held instead 🔒

This filters electrical noise and transient glitches during gear transitions without any perceptible delay (display animations take 500 ms+).

---

## Sequence-Triggered Animations 🎬

The firmware keeps a rolling buffer of the last 4 gear indices. When the buffer is full and matches a configured sequence, a bonus animation plays:

| Sequence (default) | Effect |
|--------------------|--------|
| D → N → D → N | Random sprite animation (Pac-Man, fire, space invader, …) |
| R → N → R → N | Startup text scrolls across the display |

Eight built-in sprites are stored in `PROGMEM` to preserve RAM:

🔥 Fire · 👾 Space Invader · 👻 Pac-Man Ghost · 🟡 Pac-Man · 🚶 Walker · ❤️ Beating Heart · ⛵ Sailboat · 🚢 Steamboat

Both sequences are fully configurable — as long as they are the same length as `BUFFER_SIZE` and use valid gear characters.

---

## Configuration

All user-facing settings live at the top of `GearShift6_8x8.ino` with Doxygen comments. Nothing else needs touching for a typical installation.

### Display & Hardware

| Constant | Default | Description |
|----------|---------|-------------|
| `DATA_PIN` | `11` | SPI MOSI pin |
| `CLK_PIN` | `13` | SPI SCK pin |
| `CS_PIN` | `10` | SPI chip-select pin |
| `MAX_DEVICES` | `1` | Number of daisy-chained MAX72xx modules |
| `BRIGHTNESS` | `4` | LED intensity (0 – 15) |

### Gear Setup

| Constant | Default | Description |
|----------|---------|-------------|
| `NUM_GEARS` | `7` | Total gear count; must match `GearChars` length |
| `GearChars[]` | `{'P','R','N','D','3','2','1'}` | Display character for each gear index |
| `Hall[]` | `{3,4,5,6,7,8}` | Sensor pin for each gear (R → 1st) |

### Animation & Sequences

| Constant | Default | Description |
|----------|---------|-------------|
| `BUFFER_SIZE` | `4` | Gear-history depth; must match sequence array lengths |
| `StartupText[]` | `"Scratch built by Kevin Jolliffe"` | Text scrolled on boot and by the scroll sequence |
| `ANIM_SEQUENCE[]` | `{'D','N','D','N'}` | Sequence that triggers a random sprite animation |
| `SCROLLTEXT_SEQUENCE[]` | `{'R','N','R','N'}` | Sequence that triggers the startup text scroll |
| `SCROLL_SPEED` | `75` | Milliseconds between animation frame updates |

### Debounce

| Constant | Default | Description |
|----------|---------|-------------|
| `DEBOUNCE_DELAY` | `5` | Milliseconds between the two sensor reads |

### Debugging

| Constant | Default | Description |
|----------|---------|-------------|
| `DEBUG_MODE` | `0` | Set to `1` to enable debug mode, `0` to disable |
| `BAUD_SPEED` | `9600` | Serial baud rate |
| `DEBUG_READS` | `4` | Number of sensor readings to log |
| `DEBUG_DELAY` | `3000` | Milliseconds between debug readings (time to change gear) |

---

## Function Reference

| Function | Description |
|----------|-------------|
| `setup()` | Initialises sensor pins and LED display; seeds RNG; runs debug if enabled |
| `loop()` | Reads sensors, displays gear, checks history — repeats until power-off |
| `hallSetup()` | Sets all Hall sensor pins to `INPUT` mode |
| `displaySetup()` | Initialises Parola, sets brightness, scrolls startup text |
| `getGear()` | Reads sensors with debounce; returns gear index `[0, NUM_LOOPS]` |
| `displayGear(gearValue)` | Renders gear character; scrolls up/down based on direction of change |
| `checkHistory()` | Compares gear buffer against configured sequences; fires animations continuously until a different gear is selected |
| `checkArrays(a, b, n)` | Returns `true` if the first `n` elements of two char arrays match |
| `displayAnimation(selection)` | Plays a PROGMEM sprite animation on the LED matrix |
| `debugFunction()` | Logs sensor readings and buffer contents to Serial, then halts in an infinite loop |

---

## Project Structure

```
GearShiftIndicator/
├── GearShift6_8x8.ino      # Main Arduino sketch
├── docs/
│   └── doxygen/html/        # Generated Doxygen documentation
└── tests/
    ├── Makefile             # Build: g++ -std=c++17
    ├── test_main.cpp        # 64-test suite (no Arduino required)
    └── mocks/
        ├── Arduino.h        # Primitive types, digitalRead, delay mock
        ├── CircularBuffer.h # Faithful reimplementation of rlogiacco/CircularBuffer
        ├── MD_MAX72xx.h     # Hardware-type enum stub
        ├── MD_Parola.h      # Call-counter mock for animation assertions
        └── SPI.h            # Empty SPI stub
```

---

## Running the Tests 🧪

No Arduino hardware or IDE needed — tests compile and run natively on any machine with a C++17 compiler.

```bash
cd tests
make
./test_runner
```

Expected output:

```
=== GearShift6_8x8 Unit Tests ===
...
Results: 64/64 passed
```

### Test Coverage

| Suite | Tests | What's covered |
|-------|-------|----------------|
| `checkArrays` | 8 | Array comparison edge cases |
| `getGear` | 8 | Sensor reading, priority logic |
| `Sprite data integrity` | 11 | PROGMEM array sizes, sprite table completeness |
| `Configuration validation` | 11 | Array lengths, pin conflicts, sequence validity |
| `displayGear buffer tracking` | 8 | Gear history push/eviction behaviour |
| `checkHistory sequence detection` | 6 | Sequence matching, animation mutual exclusivity |
| `getGear debounce` | 4 | Stable reads accepted, glitches rejected |

The test runner includes the `.ino` file directly — no modifications to the sketch are needed.

---

## Dependencies

| Library | Purpose |
|---------|---------|
| [MD_Parola](https://github.com/MajicDesigns/MD_Parola) | LED matrix animation |
| [MD_MAX72xx](https://github.com/MajicDesigns/MD_MAX72XX) | MAX72xx hardware driver |
| [CircularBuffer](https://github.com/rlogiacco/CircularBuffer) | Gear-history ring buffer |

Install via the Arduino Library Manager or place in your `libraries/` folder.

---

## Debugging 🔧

Set `DEBUG_MODE = 1` in the sketch and reflash. On boot, `debugFunction()` runs once: it takes `DEBUG_READS` sensor snapshots (with `DEBUG_DELAY` ms between each to give time to move through gears), prints the results over Serial, then **halts in an infinite loop**. This is intentional — it keeps debug output clean and prevents live readings from interleaving with the log.

**Workflow:**
1. Reflash with `DEBUG_MODE = 1`
2. Power on the car (or connect USB)
3. Open Serial Monitor at `BAUD_SPEED` (default 9600)
4. Step through gears during the `DEBUG_DELAY` pauses
5. Review the sensor and buffer output
6. Reboot the Arduino to run another capture (power-cycle suffices)
7. Reflash with `DEBUG_MODE = 0` when finished

The infinite halt means the car's gear indicator will not function while debug mode is active — intended to make it obvious the sketch needs reflashing before use.

---

## License

See repository for licence details.
