/**
 * @file
 * @author Ryan Jolliffe (ryanjolliffe@hotmail.co.uk)
 * @brief Use hall effect sensors to determine and display vehicle gear selection
 * on an 8x8 LED display.
 * @version v0.6
 * @copyright Copyright (c) 2021
 */

/** @mainpage Introduction and Installation
 *
 * @section intro_sec Introduction
 *
 * This is the documentation for GearShift6_8x8.ino - named as such due
 * to it being code that handles Gear Shifting using 6 (by default) hall
 * effect sensors which determines the current gear position then displays
 * the result on an 8x8 LED display matrix with relevant (and not so
 * relevant) animations. 
 *
 * @subsection note_sub Please Note
 * This code was originally developed for my Dad's scratch-built Locost 7.
 * As such, whilst this code was designed with flexibility and customisation
 * in mind, it of course comes with a few caveats. For example:
 * - Whilst the gear/sensor numbers and values can be adjusted,
 * it is still assumed that there are only 2 directions for gear changes - Up and Down.
 *     + Other configurations will still work, but the animations rely on
 *     this structure and as such will still slide up or down only.
 *     This is on my TODO list for future changes!
 *
 * @section install_sec Software Installation
 *
 * @subsection step1 Step 1: Install Arduino IDE and relevant libraries.
 * 3 additional libraries are used in this code and need to be installed
 * via Tools > Manage Libraries in the Arduino IDE. Searching for their
 * names should allow you to find them with ease.
 * Library          | Used For:
 * -------------    | -------------
 * MD_MAX72xx.h     | Interacting with display(s) 
 * MD_Parola.h      | Text and Sprite Animation 
 * CircularBuffer.h | Tracking gear change history 
 *
 * @subsection step2 Step 2: Adjust variables.
 * Most variables are able to be changed to suit your particular setup.
 * Take care to read the documentation for each, as some must meet
 * particular conditions to work as expected (in particular; pin numbers).
 *
 * @subsection step3 Step 3: Verify changes.
 * Use the Verify button (looks like a check/tick underneath the "File" menu heading)
 * to verify that any changes have been implemented correctly. The IDE will
 * warn of any errors it encounters when compiling the code.
 * For additional information, ensure the "Show verbose output" checkboxes
 * are marked in the IDE Preferences (File > Preferences, Settings Tab).
 * - This step is obviously not necessary if no changes are made to the code.
 *
 * @subsection step4 Step 4: Connect Arduino and upload.
 * If connecting via USB, the appropriate port and board should be automatically
 * selected by the IDE, but can be confirmed or manually adjusted under
 * the Tools menu heading (see "Board" and "Port" subsections).
 *
 * @subsection step5 Step 5: Wire up Arduino and test!
 * Currently, no instructions are available from us but this will be changed
 * in the future. Ensure the pins used match what is written in the code's
 * variables.
 *
 * @section anim_sec Animations
 *
 * As the [MD_Parola library](https://github.com/MajicDesigns/MD_Parola) is used,
 * animations using the [relevant sprites](https://arduinoplusplus.wordpress.com/2018/04/19/parola-a-to-z-sprite-text-effects/)
 * can be used if desired and a few select ones are already implemented.
 */


/* Necessary libraries, ensure they are
  installed in Arduino IDE before uploading */
#include <SPI.h>                                                                // should be included in default Arduino IDE
#include <MD_Parola.h>
#include <MD_MAX72xx.h>
#include <CircularBuffer.h>

/** Change if using PAROLA_HW or other LED hardware,
* incorrect setting can cause orientation issues */
#define HARDWARE_TYPE MD_MAX72XX::GENERIC_HW
#define ARRAY_SIZE(a) (sizeof(a) / sizeof(a[0]))

/* Settings: */

// GEAR SETTINGS
/** How many gears are used - must match the number of gear characters in GearChars array */
const byte    NUM_GEARS                        = 7;
/** Used for loop counting etc when starting with 0 */
const int8_t  NUM_LOOPS                        = NUM_GEARS - 1;
/** Set number of stored previous gear states - 4 used here used to detect 'sequence' such as 1-2-1-2, which can then be acted upon */
const uint8_t BUFFER_SIZE                      = 4;
/** Layout here from left to right should match gear order on vehicle from top to bottom/start to finish */
const char    GearChars[NUM_GEARS]             = {'P', 'R', 'N', 'D', '3', '2', '1'};
// DISPLAY AND SENSOR SETTINGS
/** Hall Effect sensor pins in the same order as GearChars array - 'Park' position is assumed to not have a sensor and so the first pin represents "R" */
const uint8_t Hall[NUM_LOOPS]                  = {      3,   4,   5,   6,   7,   8 };
/** SPI data pin (MOSI) */
const uint8_t DATA_PIN                         = 11;
/** SPI clock pin (SCK) */
const uint8_t CLK_PIN                          = 13;
/** SPI chip-select pin for the MAX72xx */
const uint8_t CS_PIN                           = 10;
/** Number of daisy-chained MAX72xx display modules */
const uint8_t MAX_DEVICES                      = 1;

// CUSTOMISATION
/** Text scrolled upon boot */
const char    StartupText[]                    = {"Scratch built by Kevin Jolliffe"};
/** Sequence of gears necessary to display and loop animations (i.e. D-N-D-N), must be same length as BUFFER_SIZE */
const char    ANIM_SEQUENCE[BUFFER_SIZE]       = {'D', 'N', 'D', 'N'};
/** Sequence of gears necessary to display and loop the startup text (i.e. R-N-R-N), must be same length as BUFFER_SIZE */
const char    SCROLLTEXT_SEQUENCE[BUFFER_SIZE] = {'R', 'N', 'R', 'N'};
/** Speed that StartupText and animations are scrolled, number is milliseconds between frame updates */
const uint16_t SCROLL_SPEED                    = 75;
/** Set brightness of LEDs (using range 0-15) */
const byte    BRIGHTNESS                       = 4;
/** Debounce delay (ms) between the two sensor reads in getGear() - filters
*  electrical noise and transient readings during gear transitions.
*  5ms is imperceptible to the driver given display animations take 500ms+. */
const uint8_t DEBOUNCE_DELAY                   = 5;

// POTENTIOMETER BRIGHTNESS (optional)
/** Set to true to control LED brightness via a potentiometer on BRIGHTNESS_POT_PIN.
*  When false, BRIGHTNESS above is used and the analog pin is never read — the
*  compiler eliminates the dead branch entirely so there is no runtime cost. */
const bool    USE_POT_BRIGHTNESS               = false;
/** Analog pin for the brightness potentiometer (A1 by default; A0 is used for RNG seed) */
const uint8_t BRIGHTNESS_POT_PIN               = A1;

// POTENTIOMETER SCROLL SPEED (optional)
/** Set to true to control animation scroll speed via a potentiometer on SCROLL_POT_PIN.
*  When false, SCROLL_SPEED is used and the analog pin is never read; zero runtime cost. */
const bool    USE_POT_SCROLL                   = false;
/** Analog pin for the scroll speed potentiometer */
const uint8_t SCROLL_POT_PIN                   = A2;
/** Minimum scroll speed in ms per frame when using potentiometer (lower = faster) */
const uint16_t SCROLL_SPEED_MIN                = 30;
/** Maximum scroll speed in ms per frame when using potentiometer (higher = slower) */
const uint16_t SCROLL_SPEED_MAX                = 250;

// NEUTRAL REMINDER (optional)
/** Set to true to activate a brightness-based visual reminder after the vehicle has
*  been in Neutral for longer than NEUTRAL_REMINDER_DELAY. The display brightens to
*  NEUTRAL_REMINDER_BRIGHTNESS and returns to normal as soon as another gear is selected.
*  When false, no timer or pin reads occur; zero runtime cost. */
const bool     NEUTRAL_REMINDER                = false;
/** Index of the Neutral gear in GearChars[]; update if the gear layout is changed */
const int8_t   NEUTRAL_INDEX                   = 2;
/** Time in milliseconds before the neutral reminder activates (default: 5 seconds) */
const uint32_t NEUTRAL_REMINDER_DELAY          = 5000;
/** LED brightness applied when the neutral reminder is active (0-15; default: max) */
const uint8_t  NEUTRAL_REMINDER_BRIGHTNESS     = 15;

// DEBUG SWITCH (optional)
/** Set to true to allow a physical normally-open switch wired between DEBUG_SWITCH_PIN
*  and GND to enable debug mode without reflashing. If the pin reads LOW at boot
*  (INPUT_PULLUP), debugFunction() runs exactly as if DEBUG_MODE=1. When false, this
*  pin is never initialised and DEBUG_MODE governs debug behaviour; zero runtime cost. */
const bool    USE_DEBUG_SWITCH                 = false;
/** Digital pin for the optional debug enable switch; pulled LOW to activate.
*  Pin 9 is chosen as it is free on the Uno given the default sensor and SPI pin usage. */
const uint8_t DEBUG_SWITCH_PIN                 = 9;

// DEBUGGING
/** Set to 1 to enable debugging via Serial, 0 to disable.
*
*  Intended workflow: reflash with DEBUG_MODE=1 before a debugging session,
*  then reflash with DEBUG_MODE=0 when finished. When enabled, the sketch
*  runs debugFunction() once in setup() then halts in an infinite loop —
*  it will not enter the normal gear-reading loop. Reboot the Arduino to
*  restart (e.g. power-cycle the car). This is by design so that debug
*  output is captured cleanly without interleaving live readings. */
const byte    DEBUG_MODE                       = 0;
/** Number of readings in debug mode, recommended to match buffer size or larger */
const uint8_t DEBUG_READS                      = BUFFER_SIZE;
/** Delay between debug readings in milliseconds (3 seconds by default) */
const int     DEBUG_DELAY                      = 3000;
/** Set baud rate here for Serial communication */
const int     BAUD_SPEED                       = 9600;

// HARDWARE SPI
/** Creates display instance using given settings (HARDWARE_TYPE, CS_PIN, MAX_DEVICES) */
MD_Parola Parola = MD_Parola(HARDWARE_TYPE, CS_PIN, MAX_DEVICES);

/* Forward declarations (auto-generated by the Arduino IDE; required for
   standard C++ compilers used in testing and non-IDE builds) */
void     hallSetup();
void     displaySetup();
int8_t   getGear();
void     displayGear(int8_t gearValue);
void     checkHistory();
boolean  checkArrays(char arrayA[], const char arrayB[], long numItems);
void     displayAnimation(byte selection);
void     debugFunction();
uint8_t  getBrightness();
uint16_t getScrollSpeed();
bool     neutralReminderActive();
// SOFTWARE SPI
//MD_Parola P = MD_Parola(HARDWARE_TYPE, DATA_PIN, CLK_PIN, CS_PIN, MAX_DEVICES);
    

/* Sprite definitions - stored in RAM/PROGMEM to save memory */

const uint8_t F_PMAN = 6;
const uint8_t W_PMAN = 8;
const uint8_t PROGMEM pacman[F_PMAN * W_PMAN] = {
  0x00, 0x81, 0xc3, 0xe7, 0xff, 0x7e, 0x7e, 0x3c,
  0x00, 0x42, 0xe7, 0xe7, 0xff, 0xff, 0x7e, 0x3c,
  0x24, 0x66, 0xe7, 0xff, 0xff, 0xff, 0x7e, 0x3c,
  0x3c, 0x7e, 0xff, 0xff, 0xff, 0xff, 0x7e, 0x3c,
  0x24, 0x66, 0xe7, 0xff, 0xff, 0xff, 0x7e, 0x3c,
  0x00, 0x42, 0xe7, 0xe7, 0xff, 0xff, 0x7e, 0x3c,
};
/**< Sprite definition for gobbling pacman animation */

const uint8_t F_PMANGHOST = 6;
const uint8_t W_PMANGHOST = 18;
static const uint8_t PROGMEM pacmanghost[F_PMANGHOST * W_PMANGHOST] = {
  0x00, 0x81, 0xc3, 0xe7, 0xff, 0x7e, 0x7e, 0x3c, 0x00, 0x00, 0x00, 0xfe, 0x7b, 0xf3, 0x7f, 0xfb, 0x73, 0xfe,
  0x00, 0x42, 0xe7, 0xe7, 0xff, 0xff, 0x7e, 0x3c, 0x00, 0x00, 0x00, 0xfe, 0x7b, 0xf3, 0x7f, 0xfb, 0x73, 0xfe,
  0x24, 0x66, 0xe7, 0xff, 0xff, 0xff, 0x7e, 0x3c, 0x00, 0x00, 0x00, 0xfe, 0x7b, 0xf3, 0x7f, 0xfb, 0x73, 0xfe,
  0x3c, 0x7e, 0xff, 0xff, 0xff, 0xff, 0x7e, 0x3c, 0x00, 0x00, 0x00, 0xfe, 0x73, 0xfb, 0x7f, 0xf3, 0x7b, 0xfe,
  0x24, 0x66, 0xe7, 0xff, 0xff, 0xff, 0x7e, 0x3c, 0x00, 0x00, 0x00, 0xfe, 0x73, 0xfb, 0x7f, 0xf3, 0x7b, 0xfe,
  0x00, 0x42, 0xe7, 0xe7, 0xff, 0xff, 0x7e, 0x3c, 0x00, 0x00, 0x00, 0xfe, 0x73, 0xfb, 0x7f, 0xf3, 0x7b, 0xfe,
};
/**< Sprite definition for ghost pursued by pacman */

const uint8_t F_SAILBOAT = 1;
const uint8_t W_SAILBOAT = 11;
const uint8_t PROGMEM sailboat[F_SAILBOAT * W_SAILBOAT] = {
  0x10, 0x30, 0x58, 0x94, 0x92, 0x9f, 0x92, 0x94, 0x98, 0x50, 0x30,
};
/**< Sprite definition for sail boat */

const uint8_t F_STEAMBOAT = 2;
const uint8_t W_STEAMBOAT = 11;
const uint8_t PROGMEM steamboat[F_STEAMBOAT * W_STEAMBOAT] = {
  0x10, 0x30, 0x50, 0x9c, 0x9e, 0x90, 0x91, 0x9c, 0x9d, 0x90, 0x71,
  0x10, 0x30, 0x50, 0x9c, 0x9c, 0x91, 0x90, 0x9d, 0x9e, 0x91, 0x70,
};
/**< Sprite definition for steam boat */

const uint8_t F_HEART = 5;
const uint8_t W_HEART = 9;
const uint8_t PROGMEM beatingheart[F_HEART * W_HEART] = {
  0x0e, 0x11, 0x21, 0x42, 0x84, 0x42, 0x21, 0x11, 0x0e,
  0x0e, 0x1f, 0x33, 0x66, 0xcc, 0x66, 0x33, 0x1f, 0x0e,
  0x0e, 0x1f, 0x3f, 0x7e, 0xfc, 0x7e, 0x3f, 0x1f, 0x0e,
  0x0e, 0x1f, 0x33, 0x66, 0xcc, 0x66, 0x33, 0x1f, 0x0e,
  0x0e, 0x11, 0x21, 0x42, 0x84, 0x42, 0x21, 0x11, 0x0e,
};
/**< Sprite definition for beating heart */

const uint8_t F_INVADER = 2;
const uint8_t W_INVADER = 10;
const uint8_t PROGMEM spaceinvader[F_INVADER * W_INVADER] = {
  0x0e, 0x98, 0x7d, 0x36, 0x3c, 0x3c, 0x36, 0x7d, 0x98, 0x0e,
  0x70, 0x18, 0x7d, 0xb6, 0x3c, 0x3c, 0xb6, 0x7d, 0x18, 0x70,
};
/**< Sprite definition for space invader */

const uint8_t F_FIRE = 2;
const uint8_t W_FIRE = 11;
const uint8_t PROGMEM fire[F_FIRE * W_FIRE] = {
  0x7e, 0xab, 0x54, 0x28, 0x52, 0x24, 0x40, 0x18, 0x04, 0x10, 0x08,
  0x7e, 0xd5, 0x2a, 0x14, 0x24, 0x0a, 0x30, 0x04, 0x28, 0x08, 0x10,
};
/**< Sprite definition for fire */

const uint8_t F_WALKER = 5;
const uint8_t W_WALKER = 7;
const uint8_t PROGMEM walker[F_WALKER * W_WALKER] = {
  0x00, 0x48, 0x77, 0x1f, 0x1c, 0x94, 0x68,
  0x00, 0x90, 0xee, 0x3e, 0x38, 0x28, 0xd0,
  0x00, 0x00, 0xae, 0xfe, 0x38, 0x28, 0x40,
  0x00, 0x00, 0x2e, 0xbe, 0xf8, 0x00, 0x00,
  0x00, 0x10, 0x6e, 0x3e, 0xb8, 0xe8, 0x00,
};
/**< Sprite definition for walking stick figure */

/* Struct used for storing/retrieving sprite settings. */
struct
{
  const uint8_t *data;
  uint8_t width;
  uint8_t frames;
}
sprite[] =
{
  { fire, W_FIRE, F_FIRE },
  { pacman, W_PMAN, F_PMAN },
  { walker, W_WALKER, F_WALKER },
  { beatingheart, W_HEART, F_HEART },
  { sailboat, W_SAILBOAT, F_SAILBOAT },
  { spaceinvader, W_INVADER, F_INVADER },
  { steamboat, W_STEAMBOAT, F_STEAMBOAT },
  { pacmanghost, W_PMANGHOST, F_PMANGHOST }
};

/* Variables that will change during runtime: */

/** An integer representing the current gear position
* (as given in the GearChars array). */
int8_t currentGear;
/** A Circular Buffer (array of length BUFFER_SIZE)
* used to store the previous gear positions. */
CircularBuffer<byte, BUFFER_SIZE> previousGears;                                // a buffer to store previous gear positions
/** Timestamp (ms) recorded when the vehicle entered Neutral; 0 = not in Neutral.
*  Only written when NEUTRAL_REMINDER is true. */
unsigned long neutralSinceMs = 0;

/** @brief Returns the target LED brightness.
*
*  When USE_POT_BRIGHTNESS is true, reads BRIGHTNESS_POT_PIN and maps the
*  result (0–1023) to the valid intensity range (0–15). When false, returns
*  the BRIGHTNESS constant — the dead branch is eliminated by the compiler. */
uint8_t getBrightness() {
  if (USE_POT_BRIGHTNESS)
    return (uint8_t)map(analogRead(BRIGHTNESS_POT_PIN), 0, 1023, 0, 15);
  return BRIGHTNESS;
}

/** @brief Returns the target scroll/animation speed in ms per frame.
*
*  When USE_POT_SCROLL is true, reads SCROLL_POT_PIN and maps the result
*  to [SCROLL_SPEED_MIN, SCROLL_SPEED_MAX]. When false, returns SCROLL_SPEED
*  with no analog read; zero runtime cost. */
uint16_t getScrollSpeed() {
  if (USE_POT_SCROLL)
    return (uint16_t)map(analogRead(SCROLL_POT_PIN), 0, 1023, SCROLL_SPEED_MIN, SCROLL_SPEED_MAX);
  return SCROLL_SPEED;
}

/** @brief Returns true when the neutral reminder should be active.
*
*  Active when NEUTRAL_REMINDER is enabled, the current gear is Neutral,
*  the neutral timer has been started (neutralSinceMs != 0), and the vehicle
*  has remained in Neutral for at least NEUTRAL_REMINDER_DELAY milliseconds.
*  Extracted as a helper to keep loop() readable and to allow unit testing. */
bool neutralReminderActive() {
  return NEUTRAL_REMINDER
    && (currentGear == NEUTRAL_INDEX)
    && (neutralSinceMs != 0)
    && (millis() - neutralSinceMs >= NEUTRAL_REMINDER_DELAY);
}

/**
 * @brief Initialises sensors and LED display.
 * 
 * Function that runs *once* when Arduino is first
 * booted; initialises sensors and LED display,
 * then loads known state (Park position) and
 * checks if DEBUG_MODE is enabled.
 * Also adds randomness to random() calls via
 * an analogue 'Pin 0' read.
 *
 */
void setup() {
  hallSetup();                                                                  // initialise sensors
  displaySetup();                                                               // initialise display
  currentGear = 0;                                                              // set current gear to 'Parked' position until first sensor read to establish known state
  previousGears.push(currentGear);                                              // push 'Park' {"P"} position to buffer also, which is translated to *char via GearChars[0]
  bool runDebug = false;
  if (USE_DEBUG_SWITCH) {                                                       // if physical debug switch enabled, read the pin at boot to decide
    pinMode(DEBUG_SWITCH_PIN, INPUT_PULLUP);
    runDebug = (digitalRead(DEBUG_SWITCH_PIN) == LOW);                         // switch closed (pulled to GND) → enable debug
  } else {
    runDebug = (DEBUG_MODE == 1);                                               // fall back to the reflash-based DEBUG_MODE flag
  }
  if (runDebug) {
    Serial.begin(BAUD_SPEED);
    debugFunction();
  }
  randomSeed(analogRead(0));                                                    // take 'noisy' reading (i.e. hopefully random) as the seed for our random() calls; adds randomness
}

/**
 * @brief Main loop.
 * 
 * The main loop that runs the core components
 * repeatedly until power-off.
 *
 */
void loop() {
  currentGear = getGear();                                                      // read hall effect sensors and calculate current gear position

  // Track how long the vehicle has been in Neutral (only when NEUTRAL_REMINDER enabled)
  if (NEUTRAL_REMINDER) {
    if (currentGear == NEUTRAL_INDEX) {
      if (neutralSinceMs == 0) neutralSinceMs = millis() | 1UL;               // record entry time; '| 1' ensures we never store 0 (0 is the "not in neutral" sentinel)
    } else {
      neutralSinceMs = 0;                                                       // left Neutral — reset timer
    }
  }

  // Update display brightness when pot control or neutral reminder is in use.
  // A static sentinel (255 = uninitialised) means setIntensity() is only called
  // when the value changes, avoiding unnecessary SPI writes each loop iteration.
  if (USE_POT_BRIGHTNESS || NEUTRAL_REMINDER) {
    static uint8_t lastIntensity = 255;
    uint8_t intensity = neutralReminderActive() ? NEUTRAL_REMINDER_BRIGHTNESS : getBrightness();
    if (intensity != lastIntensity) {
      Parola.setIntensity(intensity);
      lastIntensity = intensity;
    }
  }

  displayGear(currentGear);                                                     // display the current gear, with appropriate animation if different from previous gear
  checkHistory();                                                               // checks gear history for defined sequences and calls relevant functions
}

/** @brief Initialize the hall effect sensor pins as inputs.
*
* INPUT is used here assuming sensors have push-pull outputs or external
* pull-up/pull-down resistors are present. If your sensors have open-drain
* outputs and no external pull-ups, consider changing INPUT to INPUT_PULLUP
* to enable the Arduino's internal pull-up resistors and prevent floating
* inputs when no sensor is triggered. */
void hallSetup() {
  for (int8_t i = 0; i < NUM_LOOPS; i++) {
    pinMode(Hall[i], INPUT);
  }
}

/** @brief Setup LED display */
void displaySetup() {
  Parola.begin();                                                               // initialise display
  Parola.setIntensity(BRIGHTNESS);                                              // set display intensity/brightness
  Parola.displayClear();
  Parola.displayScroll(StartupText, PA_LEFT, PA_SCROLL_LEFT, getScrollSpeed()); // display message on startup
  while (!Parola.displayAnimate())                                              // play animation once until complete
    ;
  Parola.displayReset();
  Parola.displayClear();
}

/**
 * @brief Loop through sensors until LOW reading detected
 * 
 * @return gear
 * A numeric value representing the current gear,
 * matching the gear's position in the GearChars array.
 */
int8_t getGear() {
  int8_t gear = NUM_LOOPS;
  while ((gear) && (digitalRead(Hall[gear - 1]))) {
    gear--;
  }
  delay(DEBOUNCE_DELAY);                                                        // short pause then re-read to confirm; filters noise and transition glitches
  int8_t gearConfirm = NUM_LOOPS;
  while ((gearConfirm) && (digitalRead(Hall[gearConfirm - 1]))) {
    gearConfirm--;
  }
  return (gear == gearConfirm) ? gear : previousGears.last();                  // only accept reading if both agree; otherwise hold last known gear
}

/**
 * @brief Displays current gear on LED, and checks if animations
 * should be used depending on previous gear value.
 *
 * @param gearValue A numeric value representing the current gear,
 * matching the gear's position in the GearChars array.
 */
void displayGear(int8_t gearValue) {
  char curGearChar[2] = {GearChars[gearValue]};                                 // convert gearValue to c-string character for display purposes by pulling from null terminated array using pointers
  if (gearValue == previousGears.last()) {                                      // if current gear is same as previous, simply print
    Parola.displayText(curGearChar, PA_CENTER, 0, 0, PA_PRINT, PA_NO_EFFECT);   // set display settings
    Parola.displayAnimate();                                                    // display appropriate character
  }
  else if ((previousGears.last() < gearValue)) {                                // if the previous gear is situated to the left of current gear (in char array) then scroll down
    Parola.displayText(
      curGearChar, PA_CENTER, getScrollSpeed(), 1, PA_SCROLL_DOWN, PA_NO_EFFECT // set scrolling text settings
    );
    while (!Parola.displayAnimate())                                            // play once animation until complete
      ;
    previousGears.push(gearValue);                                              // push current gear to buffer as it is different
  } else {                                                                      // if the previous gear is not situated left (i.e. is to the right of current gear in char array) then scroll up
    Parola.displayText(
      curGearChar, PA_CENTER, getScrollSpeed(), 1, PA_SCROLL_UP, PA_NO_EFFECT
    );
    while (!Parola.displayAnimate())
      ;
    previousGears.push(gearValue);                                              // push current gear to buffer as it is different
  }
}

/** @brief Checks for given sequence of gear changes using buffer
* functionality and calls other functions as required.
*
* Called every loop iteration. When the gear history buffer is full and
* matches a configured sequence, the corresponding animation plays.
* Re-triggering on subsequent calls with the same buffer contents is
* intentional: animations continue looping until the driver selects a
* different gear, at which point the buffer changes and the match breaks. */
void checkHistory() {
  if (previousGears.isFull()) {
    char gearHistory[BUFFER_SIZE];                                              // create new char array from history for comparison
    for (int8_t i = 0; i < BUFFER_SIZE; i++) {                                  // loop to populate array with char equivalents
      gearHistory[i] = GearChars[previousGears[i]];
    }
    if (checkArrays(gearHistory, ANIM_SEQUENCE, BUFFER_SIZE) == true) {         // compares the two arrays; if buffer history matches ANIM_SEQUENCE, then display animation
      displayAnimation(random(ARRAY_SIZE(sprite)));                              // selects and displays random animation from struct array
    }
    else if (checkArrays(gearHistory, SCROLLTEXT_SEQUENCE, BUFFER_SIZE) == true) {
      Parola.displayClear();
      Parola.displayScroll(StartupText, PA_LEFT, PA_SCROLL_LEFT, getScrollSpeed()); // scroll StartupText
      while (!Parola.displayAnimate())                                          // play animation once until complete
        ;
      Parola.displayReset();
      Parola.displayClear();
    }
  }
}

/** @brief Compares 2 char arrays and returns boolean result. */
boolean checkArrays(char arrayA[], const char arrayB[], long numItems) {
  boolean matchCheck = true;
  long i = 0;
  while (i < numItems && matchCheck) {
    matchCheck = (arrayA[i] == arrayB[i]);
    i++;
  }
  return matchCheck;
}

/** @brief Displays an animation based on the previously
* selected sprite definition from checkHistory function. */
void displayAnimation(byte selection) {
  char curGearChar[2] = {GearChars[previousGears.last()]};
  Parola.displayReset();
  Parola.displayClear();
  Parola.setSpriteData(
    sprite[selection].data, sprite[selection].width, sprite[selection].frames,  // entry sprite
    sprite[selection].data, sprite[selection].width, sprite[selection].frames   // exit sprite
  );
  Parola.displayText(
    curGearChar, PA_CENTER, getScrollSpeed(), 1, PA_SPRITE, PA_SPRITE           // set animation settings
  );
  while (!Parola.displayAnimate())                                              // play animation once until complete
    ;
}

/** @brief Functions useful for debugging.
*
* Writes DEBUG_READS lots of readings (default:4) from
* all hall sensors to Serial - with a delay to allow changing
* gear - then fills & prints buffer; for debugging purposes. */
void debugFunction() {
  String buf = "";
  String bufChars = "";
  for (int8_t i = 0; i < DEBUG_READS; i++) {
    delay(DEBUG_DELAY);                                                         // wait to allow gear changing/hall sensor/magnet position changes
    for (int8_t x = 0; x < NUM_LOOPS; x++) {                                    // loop through all sensors and print values to Serial
      Serial.println(
        String(x + 1) +
        "| Digital: " +
        String(digitalRead(Hall[x])) +
        " Analogue: " +
        String(analogRead(Hall[x]))
      );
    }
    previousGears.push(random(NUM_GEARS));                                      // push pseudorandom GearChar values to buffer
    buf = buf + previousGears.last();                                           // add current gear in numeric form to a string for printing to Serial
    bufChars = bufChars + GearChars[previousGears.last()];                      // add current gear in char form to a string for printing to Serial
  }
  Serial.println("Buffer contents: " + buf + bufChars);                         // ...print buffer contents, to Serial...
  while (true);                                                                 // puts arduino into an infinite loop, reboot to start again
}