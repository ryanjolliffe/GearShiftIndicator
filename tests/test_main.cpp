/**
 * Unit tests for GearShift6_8x8.ino
 *
 * Arduino-specific dependencies are stubbed by the headers in tests/mocks/.
 * The .ino is included directly so we test the real source with no modifications.
 *
 * Test groups:
 *   1.  checkArrays              — pure comparison logic
 *   2.  getGear                  — sensor-reading → gear-index mapping
 *   3.  Sprite data              — frame/width constants match actual array sizes
 *   4.  Configuration            — array size relationships enforced by constants
 *   5.  displayGear buffer       — buffer pushed only on gear change
 *   6.  checkHistory             — sequence detection triggers correct display path
 *   7.  getGear debounce         — two-read scheme filters transient noise
 *   8.  getBrightness helper     — constant vs pot-derived brightness
 *   9.  getScrollSpeed helper    — constant vs pot-derived scroll speed
 *   10. neutralReminderActive    — timer logic and configuration validity
 *   11. Feature pin config       — pot and switch pin conflict checks
 */

#include <cstdio>
#include <cstring>

// Pull in mocks before the sketch so angle-bracket includes resolve here
#include "mocks/Arduino.h"
#include "mocks/CircularBuffer.h"
#include "mocks/MD_MAX72xx.h"
#include "mocks/MD_Parola.h"
#include "mocks/SPI.h"

// mock_pin_state storage (declared extern in Arduino.h)
// Covers digital pins 0-13 and analog pins A0-A5 (14-19)
int  mock_pin_state[20]            = {};
int  mock_pin_state_post_delay[20] = {};
bool mock_pin_change_on_delay      = false;
unsigned long mock_millis_value    = 0;

// Include the actual sketch — gives us all its globals and functions
#include "../GearShift6_8x8.ino"

// ---------------------------------------------------------------------------
// Minimal test framework
// ---------------------------------------------------------------------------
static int g_tests_run    = 0;
static int g_tests_passed = 0;
static int g_tests_failed = 0;

#define TEST(name, condition)                                            \
    do {                                                                 \
        g_tests_run++;                                                   \
        if (condition) {                                                 \
            g_tests_passed++;                                            \
            printf("  PASS  %s\n", name);                               \
        } else {                                                         \
            g_tests_failed++;                                            \
            printf("  FAIL  %s  (line %d)\n", name, __LINE__);          \
        }                                                                \
    } while (0)

#define GROUP(name) printf("\n--- %s ---\n", name)

// Reset all mutable test state between test groups
static void reset_state() {
    previousGears.clear();
    previousGears.push(0);   // mirror setup(): start at Park
    currentGear       = 0;
    neutralSinceMs    = 0;
    mock_millis_value = 0;
    parola_reset_counts();
    mock_pin_change_on_delay = false;
    for (int i = 0; i < 20; i++) mock_pin_state[i]            = HIGH;
    for (int i = 0; i < 20; i++) mock_pin_state_post_delay[i] = HIGH;
}

// ---------------------------------------------------------------------------
// 1. checkArrays
// ---------------------------------------------------------------------------
static void test_checkArrays() {
    GROUP("checkArrays");

    // Identical arrays
    char a1[] = {'D', 'N', 'D', 'N'};
    char b1[] = {'D', 'N', 'D', 'N'};
    TEST("identical arrays → true",  checkArrays(a1, b1, 4) == true);

    // Mismatch at position 0
    char a2[] = {'X', 'N', 'D', 'N'};
    char b2[] = {'D', 'N', 'D', 'N'};
    TEST("mismatch at pos 0 → false", checkArrays(a2, b2, 4) == false);

    // Mismatch at last position
    char a3[] = {'D', 'N', 'D', 'X'};
    char b3[] = {'D', 'N', 'D', 'N'};
    TEST("mismatch at last pos → false", checkArrays(a3, b3, 4) == false);

    // All mismatched
    char a4[] = {'A', 'B', 'C', 'D'};
    char b4[] = {'W', 'X', 'Y', 'Z'};
    TEST("all mismatched → false", checkArrays(a4, b4, 4) == false);

    // Single element — match
    char a5[] = {'N'};
    char b5[] = {'N'};
    TEST("single element match → true", checkArrays(a5, b5, 1) == true);

    // Single element — mismatch
    char a6[] = {'N'};
    char b6[] = {'D'};
    TEST("single element mismatch → false", checkArrays(a6, b6, 1) == false);

    // numItems = 0: vacuously true (nothing to compare)
    char a7[] = {'X'};
    char b7[] = {'Y'};
    TEST("numItems=0 → true (vacuous)", checkArrays(a7, b7, 0) == true);

    // Sub-length comparison: first N elements match even if rest differ
    char a8[] = {'D', 'N', 'X', 'X'};
    char b8[] = {'D', 'N', 'D', 'N'};
    TEST("first 2 of 4 match when numItems=2", checkArrays(a8, b8, 2) == true);
}

// ---------------------------------------------------------------------------
// 2. getGear
// ---------------------------------------------------------------------------
static void test_getGear() {
    GROUP("getGear");

    // All sensors HIGH → Park (index 0)
    for (int i = 0; i < 14; i++) mock_pin_state[i] = HIGH;
    TEST("all sensors HIGH → gear 0 (Park)", getGear() == 0);

    // Only Hall[0] LOW → gear 1 (R)
    for (int i = 0; i < 14; i++) mock_pin_state[i] = HIGH;
    mock_pin_state[Hall[0]] = LOW;
    TEST("Hall[0] LOW only → gear 1 (R)", getGear() == 1);

    // Only Hall[1] LOW → gear 2 (N)
    for (int i = 0; i < 14; i++) mock_pin_state[i] = HIGH;
    mock_pin_state[Hall[1]] = LOW;
    TEST("Hall[1] LOW only → gear 2 (N)", getGear() == 2);

    // Only Hall[2] LOW → gear 3 (D)
    for (int i = 0; i < 14; i++) mock_pin_state[i] = HIGH;
    mock_pin_state[Hall[2]] = LOW;
    TEST("Hall[2] LOW only → gear 3 (D)", getGear() == 3);

    // Only Hall[5] LOW → gear 6 (1st gear, highest index)
    for (int i = 0; i < 14; i++) mock_pin_state[i] = HIGH;
    mock_pin_state[Hall[5]] = LOW;
    TEST("Hall[5] LOW only → gear 6 (1st)", getGear() == 6);

    // Two sensors LOW simultaneously: highest index wins
    // Hall[0]=R and Hall[3]=3rd both LOW → should return 4 (the higher index)
    for (int i = 0; i < 14; i++) mock_pin_state[i] = HIGH;
    mock_pin_state[Hall[0]] = LOW;
    mock_pin_state[Hall[3]] = LOW;
    TEST("Hall[0] + Hall[3] both LOW → gear 4 (higher index wins)", getGear() == 4);

    // All sensors LOW → gear 6 (first LOW hit scanning from top)
    for (int i = 0; i < 14; i++) mock_pin_state[i] = LOW;
    TEST("all sensors LOW → gear 6 (highest wins)", getGear() == 6);

    // Return value always in valid GearChars index range [0, NUM_LOOPS]
    for (int i = 0; i < 14; i++) mock_pin_state[i] = HIGH;
    int8_t g = getGear();
    TEST("getGear() returns value in [0, NUM_LOOPS]", g >= 0 && g <= NUM_LOOPS);

    // Reset pins
    for (int i = 0; i < 14; i++) mock_pin_state[i] = HIGH;
}

// ---------------------------------------------------------------------------
// 3. Sprite data integrity
// ---------------------------------------------------------------------------
static void test_sprite_data() {
    GROUP("Sprite data integrity");

    // Each sprite array must contain exactly F_* * W_* bytes
    TEST("pacman size == F_PMAN * W_PMAN",
         sizeof(pacman) == (size_t)(F_PMAN * W_PMAN));

    TEST("pacmanghost size == F_PMANGHOST * W_PMANGHOST",
         sizeof(pacmanghost) == (size_t)(F_PMANGHOST * W_PMANGHOST));

    TEST("sailboat size == F_SAILBOAT * W_SAILBOAT",
         sizeof(sailboat) == (size_t)(F_SAILBOAT * W_SAILBOAT));

    TEST("steamboat size == F_STEAMBOAT * W_STEAMBOAT",
         sizeof(steamboat) == (size_t)(F_STEAMBOAT * W_STEAMBOAT));

    TEST("beatingheart size == F_HEART * W_HEART",
         sizeof(beatingheart) == (size_t)(F_HEART * W_HEART));

    TEST("spaceinvader size == F_INVADER * W_INVADER",
         sizeof(spaceinvader) == (size_t)(F_INVADER * W_INVADER));

    TEST("fire size == F_FIRE * W_FIRE",
         sizeof(fire) == (size_t)(F_FIRE * W_FIRE));

    TEST("walker size == F_WALKER * W_WALKER",
         sizeof(walker) == (size_t)(F_WALKER * W_WALKER));

    // All 8 sprites must be registered in the sprite struct array
    TEST("sprite[] has 8 entries", ARRAY_SIZE(sprite) == 8);

    // Verify sprite struct entries have non-zero width and frames
    bool all_valid = true;
    for (size_t i = 0; i < ARRAY_SIZE(sprite); i++) {
        if (sprite[i].width == 0 || sprite[i].frames == 0 || sprite[i].data == nullptr)
            all_valid = false;
    }
    TEST("all sprite[] entries have non-zero width, frames, and data pointer",
         all_valid);

    // random() used to select sprite: valid range is [0, ARRAY_SIZE(sprite)-1]
    // ARRAY_SIZE(sprite) must be > 0 or random() would be called with 0
    TEST("ARRAY_SIZE(sprite) > 0 so random() call is safe",
         ARRAY_SIZE(sprite) > 0);
}

// ---------------------------------------------------------------------------
// 4. Configuration validation
// ---------------------------------------------------------------------------
static void test_config() {
    GROUP("Configuration validation");

    // NUM_GEARS must equal the number of characters in GearChars
    TEST("sizeof(GearChars) == NUM_GEARS",
         sizeof(GearChars) == (size_t)NUM_GEARS);

    // Hall sensor array must have exactly NUM_LOOPS entries
    TEST("sizeof(Hall)/sizeof(Hall[0]) == NUM_LOOPS",
         sizeof(Hall) / sizeof(Hall[0]) == (size_t)NUM_LOOPS);

    // NUM_LOOPS must be NUM_GEARS - 1
    TEST("NUM_LOOPS == NUM_GEARS - 1",
         NUM_LOOPS == NUM_GEARS - 1);

    // Sequence arrays must be BUFFER_SIZE long (documented requirement)
    TEST("sizeof(ANIM_SEQUENCE) == BUFFER_SIZE",
         sizeof(ANIM_SEQUENCE) == (size_t)BUFFER_SIZE);

    TEST("sizeof(SCROLLTEXT_SEQUENCE) == BUFFER_SIZE",
         sizeof(SCROLLTEXT_SEQUENCE) == (size_t)BUFFER_SIZE);

    // BUFFER_SIZE must be at least 2 for sequences to be meaningful
    TEST("BUFFER_SIZE >= 2", BUFFER_SIZE >= 2);

    // NUM_GEARS must be at least 2 (Park + at least one drive gear)
    TEST("NUM_GEARS >= 2", NUM_GEARS >= 2);

    // GearChars indices used by getGear() go up to NUM_LOOPS; must be < NUM_GEARS
    TEST("NUM_LOOPS < NUM_GEARS (no out-of-bounds on GearChars)",
         NUM_LOOPS < NUM_GEARS);

    // MAX_DEVICES must be >= 1
    TEST("MAX_DEVICES >= 1", MAX_DEVICES >= 1);

    // ANIM_SEQUENCE and SCROLLTEXT_SEQUENCE must differ (else scrolltext unreachable)
    bool sequences_differ = false;
    for (uint8_t i = 0; i < BUFFER_SIZE; i++) {
        if (ANIM_SEQUENCE[i] != SCROLLTEXT_SEQUENCE[i]) {
            sequences_differ = true;
            break;
        }
    }
    TEST("ANIM_SEQUENCE != SCROLLTEXT_SEQUENCE (else scrolltext is unreachable)",
         sequences_differ);

    // All gear chars in sequences must appear in GearChars
    auto in_gears = [](char c) {
        for (int i = 0; i < NUM_GEARS; i++)
            if (GearChars[i] == c) return true;
        return false;
    };
    bool anim_valid = true, scroll_valid = true;
    for (uint8_t i = 0; i < BUFFER_SIZE; i++) {
        if (!in_gears(ANIM_SEQUENCE[i]))       anim_valid   = false;
        if (!in_gears(SCROLLTEXT_SEQUENCE[i])) scroll_valid = false;
    }
    TEST("All chars in ANIM_SEQUENCE exist in GearChars",       anim_valid);
    TEST("All chars in SCROLLTEXT_SEQUENCE exist in GearChars", scroll_valid);

    // Hall pins must not overlap with SPI pins (DATA_PIN, CLK_PIN, CS_PIN)
    bool no_pin_conflict = true;
    for (int8_t i = 0; i < NUM_LOOPS; i++) {
        if (Hall[i] == DATA_PIN || Hall[i] == CLK_PIN || Hall[i] == CS_PIN)
            no_pin_conflict = false;
    }
    TEST("Hall sensor pins do not conflict with SPI/LED pins", no_pin_conflict);
}

// ---------------------------------------------------------------------------
// 5. displayGear — buffer push behaviour
// ---------------------------------------------------------------------------
static void test_displayGear_buffer() {
    GROUP("displayGear buffer tracking");

    reset_state();

    // Initial state: previousGears has one entry: 0 (Park)
    TEST("initial previousGears.last() == 0 (Park)", previousGears.last() == 0);

    // Gear changes: 0→1 (Park→R), buffer should update
    parola_reset_counts();
    displayGear(1);
    TEST("gear change 0→1 updates previousGears.last() to 1",
         previousGears.last() == 1);

    // Same gear again: 1→1, buffer must NOT push
    parola_reset_counts();
    displayGear(1);
    TEST("same gear 1→1 does not change previousGears.last()",
         previousGears.last() == 1);

    // Gear down: 1→0 (R→Park), buffer should update
    displayGear(0);
    TEST("gear change 1→0 updates previousGears.last() to 0",
         previousGears.last() == 0);

    // Rapid multi-step changes fill the buffer correctly
    reset_state();
    displayGear(1);  // Park→R
    displayGear(2);  // R→N
    displayGear(3);  // N→D
    TEST("after P→R→N→D, previousGears.last() == 3 (D)",
         previousGears.last() == 3);
    TEST("after P→R→N→D, buffer is full",
         previousGears.isFull() == true);

    // Buffer contents in order: P(0), R(1), N(2), D(3)
    TEST("buffer[0] == 0 (Park)",  previousGears[0] == 0);
    TEST("buffer[1] == 1 (R)",     previousGears[1] == 1);
    TEST("buffer[2] == 2 (N)",     previousGears[2] == 2);
    TEST("buffer[3] == 3 (D)",     previousGears[3] == 3);

    // After buffer is full, a new gear push evicts the oldest entry
    displayGear(2);  // D→N (scroll up)
    TEST("after 5th unique gear, oldest (Park) evicted; buffer[0]==R",
         previousGears[0] == 1);

    reset_state();
}

// ---------------------------------------------------------------------------
// 6. checkHistory — sequence detection
// ---------------------------------------------------------------------------
static void test_checkHistory() {
    GROUP("checkHistory sequence detection");

    // Buffer not full → no animation, no scroll text
    reset_state();
    parola_reset_counts();
    // Only one entry in buffer (Park), not full
    checkHistory();
    TEST("buffer not full → no animation triggered",   parola_setSpriteData_count == 0);
    TEST("buffer not full → no scroll text triggered", parola_displayScroll_count == 0);

    // Buffer full but no matching sequence
    reset_state();
    // Push P(0), R(1), N(2), D(3) — does not match D-N-D-N or R-N-R-N
    previousGears.push(1);
    previousGears.push(2);
    previousGears.push(3);
    parola_reset_counts();
    checkHistory();
    TEST("non-matching full buffer → no animation",   parola_setSpriteData_count == 0);
    TEST("non-matching full buffer → no scroll text", parola_displayScroll_count == 0);

    // Buffer matches ANIM_SEQUENCE (D-N-D-N = indices 3,2,3,2)
    reset_state();
    previousGears.clear();
    previousGears.push(3);  // D
    previousGears.push(2);  // N
    previousGears.push(3);  // D
    previousGears.push(2);  // N
    parola_reset_counts();
    checkHistory();
    TEST("ANIM_SEQUENCE match → setSpriteData called (animation played)",
         parola_setSpriteData_count == 1);
    TEST("ANIM_SEQUENCE match → displayScroll NOT called",
         parola_displayScroll_count == 0);

    // Buffer matches SCROLLTEXT_SEQUENCE (R-N-R-N = indices 1,2,1,2)
    reset_state();
    previousGears.clear();
    previousGears.push(1);  // R
    previousGears.push(2);  // N
    previousGears.push(1);  // R
    previousGears.push(2);  // N
    parola_reset_counts();
    checkHistory();
    TEST("SCROLLTEXT_SEQUENCE match → displayScroll called",
         parola_displayScroll_count == 1);
    TEST("SCROLLTEXT_SEQUENCE match → setSpriteData NOT called",
         parola_setSpriteData_count == 0);

    // Sequence re-triggers on next call while gear is unchanged (intended behaviour)
    parola_reset_counts();
    checkHistory();  // buffer unchanged; should trigger again
    TEST("unchanged buffer re-triggers animation on next checkHistory call",
         parola_displayScroll_count == 1);

    reset_state();
}

// ---------------------------------------------------------------------------
// 7. getGear debounce
// ---------------------------------------------------------------------------
static void test_debounce() {
    GROUP("getGear debounce");

    // Stable read: both reads agree → accepted
    reset_state();
    mock_pin_state[Hall[2]] = LOW;              // D on first read
    // post-delay state is same (stable)
    mock_pin_state_post_delay[Hall[2]] = LOW;
    mock_pin_change_on_delay = true;
    TEST("stable read (both agree) → gear accepted", getGear() == 3);

    // Transient glitch: first read sees a triggered sensor, second read does not
    // → should hold last known gear (Park = 0)
    reset_state();
    mock_pin_state[Hall[3]] = LOW;              // '3' triggered on first read
    // post-delay: all HIGH again (glitch cleared)
    mock_pin_change_on_delay = true;
    TEST("transient glitch (reads disagree) → last known gear returned",
         getGear() == previousGears.last());

    // Transient in reverse: first read clean, second read picks up noise
    reset_state();
    // first read: all HIGH (Park)
    mock_pin_state_post_delay[Hall[1]] = LOW;   // N appears after delay (noise)
    mock_pin_change_on_delay = true;
    TEST("noise after delay (reads disagree) → last known gear returned",
         getGear() == previousGears.last());

    // Both reads agree on Park (all HIGH) → returns 0
    reset_state();
    mock_pin_change_on_delay = false;
    TEST("both reads HIGH → gear 0 (Park)", getGear() == 0);

    reset_state();
}

// ---------------------------------------------------------------------------
// 8. getBrightness helper
// ---------------------------------------------------------------------------
static void test_getBrightness() {
    GROUP("getBrightness helper");

    // USE_POT_BRIGHTNESS=false (compile-time default) → always returns BRIGHTNESS constant
    reset_state();
    TEST("pot disabled → returns BRIGHTNESS constant", getBrightness() == BRIGHTNESS);

    // Analog pin value is irrelevant when pot is disabled
    mock_pin_state[BRIGHTNESS_POT_PIN] = 1023;
    TEST("pot disabled → analog pin not read, still returns BRIGHTNESS",
         getBrightness() == BRIGHTNESS);

    // Verify map() produces correct values (used internally when pot enabled)
    TEST("map(0, 0, 1023, 0, 15) == 0",    map(0,    0, 1023, 0, 15) == 0);
    TEST("map(1023, 0, 1023, 0, 15) == 15", map(1023, 0, 1023, 0, 15) == 15);
    TEST("map(512, 0, 1023, 0, 15) maps to 7 or 8",
         map(512, 0, 1023, 0, 15) >= 7 && map(512, 0, 1023, 0, 15) <= 8);

    reset_state();
}

// ---------------------------------------------------------------------------
// 9. getScrollSpeed helper
// ---------------------------------------------------------------------------
static void test_getScrollSpeed() {
    GROUP("getScrollSpeed helper");

    // USE_POT_SCROLL=false (compile-time default) → always returns SCROLL_SPEED constant
    reset_state();
    TEST("pot disabled → returns SCROLL_SPEED constant", getScrollSpeed() == SCROLL_SPEED);

    // Analog pin value is irrelevant when pot is disabled
    mock_pin_state[SCROLL_POT_PIN] = 1023;
    TEST("pot disabled → analog pin not read, still returns SCROLL_SPEED",
         getScrollSpeed() == SCROLL_SPEED);

    // Speed range constants are sane
    TEST("SCROLL_SPEED_MIN < SCROLL_SPEED_MAX", SCROLL_SPEED_MIN < SCROLL_SPEED_MAX);
    TEST("SCROLL_SPEED_MIN >= 1 (non-zero prevents divide-by-zero in Parola)",
         SCROLL_SPEED_MIN >= 1);
    TEST("SCROLL_SPEED within [SCROLL_SPEED_MIN, SCROLL_SPEED_MAX]",
         SCROLL_SPEED >= SCROLL_SPEED_MIN && SCROLL_SPEED <= SCROLL_SPEED_MAX);

    reset_state();
}

// ---------------------------------------------------------------------------
// 10. neutralReminderActive helper
// ---------------------------------------------------------------------------
static void test_neutralReminder() {
    GROUP("neutralReminderActive helper");

    // NEUTRAL_REMINDER=false (compile-time default) → helper always returns false
    reset_state();
    currentGear    = NEUTRAL_INDEX;
    neutralSinceMs = 1;
    mock_millis_value = (unsigned long)NEUTRAL_REMINDER_DELAY + 1000UL;
    TEST("NEUTRAL_REMINDER disabled → neutralReminderActive() always false",
         neutralReminderActive() == false);

    // When disabled, helper returns false even if all other conditions are met
    neutralSinceMs = 0;
    currentGear    = 0;
    TEST("NEUTRAL_REMINDER disabled → false when not in neutral either",
         neutralReminderActive() == false);

    // Configuration constraints (valid regardless of whether feature is enabled)
    TEST("NEUTRAL_INDEX is within GearChars bounds",
         NEUTRAL_INDEX >= 0 && NEUTRAL_INDEX < NUM_GEARS);
    TEST("GearChars[NEUTRAL_INDEX] == 'N' (default config)",
         GearChars[NEUTRAL_INDEX] == 'N');
    TEST("NEUTRAL_REMINDER_DELAY > 0",
         NEUTRAL_REMINDER_DELAY > 0);
    TEST("NEUTRAL_REMINDER_BRIGHTNESS in valid range [0, 15]",
         NEUTRAL_REMINDER_BRIGHTNESS <= 15);

    // neutralSinceMs=0 sentinel: reminder must not fire when timer not started
    // (exercises the neutralSinceMs != 0 guard in neutralReminderActive)
    reset_state();
    currentGear    = NEUTRAL_INDEX;
    neutralSinceMs = 0;
    mock_millis_value = 99999UL;  // millis() far past any delay — but timer not set
    TEST("neutralSinceMs==0 → reminder does not fire (timer not started)",
         neutralReminderActive() == false);

    reset_state();
}

// ---------------------------------------------------------------------------
// 11. Feature pin configuration
// ---------------------------------------------------------------------------
static void test_feature_pin_config() {
    GROUP("Feature pin configuration");

    // Pot pins must be in the analog-capable range (A0=14 … A5=19 on Uno)
    TEST("BRIGHTNESS_POT_PIN is an analog-capable pin (A0-A5)",
         BRIGHTNESS_POT_PIN >= 14 && BRIGHTNESS_POT_PIN <= 19);
    TEST("SCROLL_POT_PIN is an analog-capable pin (A0-A5)",
         SCROLL_POT_PIN >= 14 && SCROLL_POT_PIN <= 19);

    // Pot pins must be distinct
    TEST("BRIGHTNESS_POT_PIN != SCROLL_POT_PIN",
         BRIGHTNESS_POT_PIN != SCROLL_POT_PIN);

    // A0 (pin 14) is used for RNG seed in setup() — pot pins must not reuse it
    TEST("BRIGHTNESS_POT_PIN != A0 (A0 reserved for RNG seed)",
         BRIGHTNESS_POT_PIN != 14);
    TEST("SCROLL_POT_PIN != A0 (A0 reserved for RNG seed)",
         SCROLL_POT_PIN != 14);

    // DEBUG_SWITCH_PIN must not conflict with Hall sensor, SPI, or pot pins
    bool switch_ok = true;
    for (int8_t i = 0; i < NUM_LOOPS; i++)
        if (DEBUG_SWITCH_PIN == Hall[i]) switch_ok = false;
    if (DEBUG_SWITCH_PIN == DATA_PIN        || DEBUG_SWITCH_PIN == CLK_PIN    ||
        DEBUG_SWITCH_PIN == CS_PIN          || DEBUG_SWITCH_PIN == BRIGHTNESS_POT_PIN ||
        DEBUG_SWITCH_PIN == SCROLL_POT_PIN)
        switch_ok = false;
    TEST("DEBUG_SWITCH_PIN does not conflict with Hall, SPI, or pot pins", switch_ok);

    reset_state();
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------
int main() {
    printf("=== GearShift6_8x8 Unit Tests ===\n");

    test_checkArrays();
    test_getGear();
    test_sprite_data();
    test_config();
    test_displayGear_buffer();
    test_checkHistory();
    test_debounce();
    test_getBrightness();
    test_getScrollSpeed();
    test_neutralReminder();
    test_feature_pin_config();

    printf("\n=================================\n");
    printf("Results: %d/%d passed", g_tests_passed, g_tests_run);
    if (g_tests_failed > 0)
        printf("  (%d FAILED)", g_tests_failed);
    printf("\n");

    return g_tests_failed > 0 ? 1 : 0;
}
