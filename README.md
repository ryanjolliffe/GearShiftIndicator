# GearShiftIndicator 🚗

Use Hall effect sensors to determine and display vehicle gear selection on an 8×8 LED matrix — complete with animations triggered by specific gear-change sequences.

> Originally built for a scratch-built Locost 7.

---

## How It Works

Six Hall effect sensors are mounted to detect gear position (Reverse through 1st). Park is assumed when no sensor fires. On each loop the firmware:

1. **Reads** all sensors and debounces the result (two reads, 5 ms apart)
2. **Displays** the current gear character on the LED matrix, scrolling up or down to match the direction of travel through the gearbox
3. **Watches** the last four gear changes for special sequences that trigger bonus animations
4. **Reminds** the driver if the vehicle has been sitting in Neutral too long *(optional)*

---

## Hardware

| Component | Detail |
|-----------|--------|
| Microcontroller | Arduino Uno (or any with SPI + 6 free digital pins) |
| Display | 8×8 MAX72xx LED matrix module |
| Sensors | 6× Hall effect sensors (one per selectable gear) |
| SPI pins | DATA → 11 · CLK → 13 · CS → 10 |
| Sensor pins | 3, 4, 5, 6, 7, 8 (R → 1st) |

> **Tip:** If your sensors have open-drain outputs and no external pull-up resistors, change `INPUT` to `INPUT_PULLUP` in `hallSetup()`.

### Optional hardware

The following components are supported but off by default — enable each with its corresponding `const bool` flag in the settings section:

| Component | Pin | Enable flag |
|-----------|-----|-------------|
| Brightness potentiometer | A1 | `USE_POT_BRIGHTNESS` |
| Scroll speed potentiometer | A2 | `USE_POT_SCROLL` |
| Debug enable switch *(normally open, to GND)* | 9 | `USE_DEBUG_SWITCH` |

All three pins are free on the Uno given the default sensor and SPI pin usage. A0 is reserved internally for the RNG seed read.

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

The firmware keeps a rolling buffer of the last 4 gear indices. When the buffer is full and matches a configured sequence, a bonus animation plays — and continues looping until a different gear is selected:

| Sequence (default) | Effect |
|--------------------|--------|
| D → N → D → N | Random sprite animation (Pac-Man, fire, space invader, …) |
| R → N → R → N | Startup text scrolls across the display |

Eight built-in sprites are stored in `PROGMEM` to preserve RAM:

🔥 Fire · 👾 Space Invader · 👻 Pac-Man Ghost · 🟡 Pac-Man · 🚶 Walker · ❤️ Beating Heart · ⛵ Sailboat · 🚢 Steamboat

Both sequences are fully configurable — as long as they are the same length as `BUFFER_SIZE` and use valid gear characters.

---

## Optional Features ✨

All optional features default to **off** (`const bool = false`). When disabled, the compiler eliminates the associated code entirely — there is zero runtime cost to leaving an unused feature compiled in.

Enable a feature by changing its flag to `true` in the settings section of `GearShift6_8x8.ino`, then wire the corresponding hardware.

### Potentiometer Brightness Control

```cpp
const bool    USE_POT_BRIGHTNESS = false;  // set to true to enable
const uint8_t BRIGHTNESS_POT_PIN = A1;
```

A potentiometer wired to A1 continuously adjusts LED brightness (0–15) in real time. The static `BRIGHTNESS` constant is used as a fallback when disabled. `setIntensity()` is only called over SPI when the value actually changes, avoiding unnecessary bus traffic.

### Potentiometer Scroll Speed Control

```cpp
const bool     USE_POT_SCROLL    = false;  // set to true to enable
const uint8_t  SCROLL_POT_PIN    = A2;
const uint16_t SCROLL_SPEED_MIN  = 30;    // ms per frame (fastest)
const uint16_t SCROLL_SPEED_MAX  = 250;   // ms per frame (slowest)
```

A potentiometer wired to A2 adjusts the animation and scroll speed across the configured range. All animation calls (`displayGear`, `checkHistory`, `displayAnimation`, `displaySetup`) use `getScrollSpeed()` so the pot controls every animated element. The static `SCROLL_SPEED` constant is used when disabled.

### Neutral Reminder

```cpp
const bool     NEUTRAL_REMINDER            = false;  // set to true to enable
const int8_t   NEUTRAL_INDEX               = 2;      // index of 'N' in GearChars[]
const uint32_t NEUTRAL_REMINDER_DELAY      = 5000;   // ms before reminder fires
const uint8_t  NEUTRAL_REMINDER_BRIGHTNESS = 15;     // brightness boost (0-15)
```

After the vehicle has been in Neutral for longer than `NEUTRAL_REMINDER_DELAY` (default 5 seconds), the display brightens to `NEUTRAL_REMINDER_BRIGHTNESS` (default maximum). The brightness returns to normal the moment any other gear is selected — no blinking, no interruption to the displayed character.

> If `USE_POT_BRIGHTNESS` is also enabled, the reminder overrides the pot value while active, then hands control back to the pot when the driver selects another gear.

Update `NEUTRAL_INDEX` if you change the order of gears in `GearChars[]`.

### Debug Enable Switch

```cpp
const bool    USE_DEBUG_SWITCH  = false;  // set to true to enable
const uint8_t DEBUG_SWITCH_PIN  = 9;
```

A normally-open switch wired between pin 9 and GND allows debug mode to be toggled **without reflashing**. The pin is read once at boot with `INPUT_PULLUP`; if it reads LOW (switch closed), `debugFunction()` runs identically to `DEBUG_MODE = 1`. If the switch is open at boot, normal operation proceeds. See the [Debugging](#debugging-) section for the full workflow.

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
| `BRIGHTNESS` | `4` | LED intensity (0 – 15); used when `USE_POT_BRIGHTNESS` is off |

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
| `SCROLL_SPEED` | `75` | Milliseconds between animation frame updates; used when `USE_POT_SCROLL` is off |

### Debounce

| Constant | Default | Description |
|----------|---------|-------------|
| `DEBOUNCE_DELAY` | `5` | Milliseconds between the two sensor reads |

### Optional: Potentiometer Brightness

| Constant | Default | Description |
|----------|---------|-------------|
| `USE_POT_BRIGHTNESS` | `false` | Enable potentiometer brightness control |
| `BRIGHTNESS_POT_PIN` | `A1` | Analog pin for the brightness potentiometer |

### Optional: Potentiometer Scroll Speed

| Constant | Default | Description |
|----------|---------|-------------|
| `USE_POT_SCROLL` | `false` | Enable potentiometer scroll speed control |
| `SCROLL_POT_PIN` | `A2` | Analog pin for the scroll speed potentiometer |
| `SCROLL_SPEED_MIN` | `30` | Fastest scroll speed in ms per frame |
| `SCROLL_SPEED_MAX` | `250` | Slowest scroll speed in ms per frame |

### Optional: Neutral Reminder

| Constant | Default | Description |
|----------|---------|-------------|
| `NEUTRAL_REMINDER` | `false` | Enable the neutral reminder |
| `NEUTRAL_INDEX` | `2` | Index of Neutral in `GearChars[]`; update if gear order changes |
| `NEUTRAL_REMINDER_DELAY` | `5000` | Milliseconds in Neutral before reminder activates |
| `NEUTRAL_REMINDER_BRIGHTNESS` | `15` | Brightness level while reminder is active (0 – 15) |

### Debugging

| Constant | Default | Description |
|----------|---------|-------------|
| `DEBUG_MODE` | `0` | Set to `1` to enable debug mode, `0` to disable |
| `BAUD_SPEED` | `9600` | Serial baud rate |
| `DEBUG_READS` | `4` | Number of sensor readings to log |
| `DEBUG_DELAY` | `3000` | Milliseconds between debug readings (time to change gear) |
| `USE_DEBUG_SWITCH` | `false` | Enable physical debug switch (no reflash needed) |
| `DEBUG_SWITCH_PIN` | `9` | Pin for the debug switch (pulled LOW to activate) |

---

## Function Reference

| Function | Description |
|----------|-------------|
| `setup()` | Initialises sensor pins and LED display; seeds RNG; runs debug if enabled via flag or switch |
| `loop()` | Reads sensors, manages brightness, displays gear, checks history — repeats until power-off |
| `hallSetup()` | Sets all Hall sensor pins to `INPUT` mode |
| `displaySetup()` | Initialises Parola, sets brightness, scrolls startup text |
| `getGear()` | Reads sensors with debounce; returns gear index `[0, NUM_LOOPS]` |
| `getBrightness()` | Returns current brightness: pot-derived when `USE_POT_BRIGHTNESS` is on, else `BRIGHTNESS` |
| `getScrollSpeed()` | Returns current scroll speed: pot-derived when `USE_POT_SCROLL` is on, else `SCROLL_SPEED` |
| `neutralReminderActive()` | Returns `true` when the neutral dwell timer has exceeded `NEUTRAL_REMINDER_DELAY` |
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
    ├── test_main.cpp        # 87-test suite (no Arduino required)
    └── mocks/
        ├── Arduino.h        # Primitive types, pin state, map(), millis() mock
        ├── CircularBuffer.h # Faithful reimplementation of rlogiacco/CircularBuffer
        ├── MD_MAX72xx.h     # Hardware-type enum stub
        ├── MD_Parola.h      # Call-counter mock for animation and intensity assertions
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
Results: 87/87 passed
```

### Test Coverage

| Suite | Tests | What's covered |
|-------|-------|----------------|
| `checkArrays` | 8 | Array comparison edge cases |
| `getGear` | 8 | Sensor reading, priority logic |
| `Sprite data integrity` | 11 | PROGMEM array sizes, sprite table completeness |
| `Configuration validation` | 13 | Array lengths, pin conflicts, sequence validity |
| `displayGear buffer tracking` | 8 | Gear history push/eviction behaviour |
| `checkHistory sequence detection` | 6 | Sequence matching, animation mutual exclusivity |
| `getGear debounce` | 4 | Stable reads accepted, glitches rejected |
| `getBrightness helper` | 5 | Constant fallback; `map()` correctness |
| `getScrollSpeed helper` | 5 | Constant fallback; speed range constraints |
| `neutralReminderActive helper` | 7 | Timer logic, sentinel, configuration validity |
| `Feature pin configuration` | 6 | Pot and switch pin conflict checks |

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

**Reflash workflow:**
1. Reflash with `DEBUG_MODE = 1`
2. Power on the car (or connect USB)
3. Open Serial Monitor at `BAUD_SPEED` (default 9600)
4. Step through gears during the `DEBUG_DELAY` pauses
5. Review the sensor and buffer output
6. Reboot the Arduino to run another capture (power-cycle suffices)
7. Reflash with `DEBUG_MODE = 0` when finished

**Switch workflow** *(optional — requires `USE_DEBUG_SWITCH = true` and a switch on pin 9):*
1. Close the debug switch before powering on
2. Power on — debug mode activates automatically, no reflash needed
3. Open Serial Monitor, step through gears, review output
4. Power off, open the switch, power on again for normal operation

The infinite halt means the car's gear indicator will not function while debug mode is active — intended to make it obvious the sketch needs either reflashing or the switch opening before driving.

---

## License

See repository for licence details.
