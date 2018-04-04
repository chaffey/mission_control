#include <Wire.h>
#include <Adafruit_LEDBackpack.h>
#include <Adafruit_MCP23017.h>

//#define DEBUG

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
/* Global Variables                                                      */
/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

const PROGMEM uint8_t BACKPACK_COUNT = 3;
const PROGMEM uint8_t EXPANDER_COUNT = 5;

const char SERIAL_DELIMITER = '|';

char buf[17];

Adafruit_8x16matrix backpacks[BACKPACK_COUNT] = {
  Adafruit_8x16matrix(),
  Adafruit_8x16matrix(),
  Adafruit_8x16matrix()
};

// 3 x 8 (cathodes w/ 16 bit anode value)
uint16_t led_buffers [BACKPACK_COUNT][8] = {
  {
    0b0000000000000000,
    0b0000000000000000,
    0b0000000000000000,
    0b0000000000000000,
    0b0000000000000000,
    0b0000000000000000,
    0b0000000000000000,
    0b0000000000000000
  }, {
    0b0000000000000000,
    0b0000000000000000,
    0b0000000000000000,
    0b0000000000000000,
    0b0000000000000000,
    0b0000000000000000,
    0b0000000000000000,
    0b0000000000000000
  }, {
    0b0000000000000000,
    0b0000000000000000,
    0b0000000000000000,
    0b0000000000000000,
    0b0000000000000000,
    0b0000000000000000,
    0b0000000000000000,
    0b0000000000000000
  }
};

Adafruit_MCP23017 expanders[EXPANDER_COUNT] = {
  Adafruit_MCP23017(),
  Adafruit_MCP23017(),
  Adafruit_MCP23017(),
  Adafruit_MCP23017(),
  Adafruit_MCP23017()
};

// 4 - 16 bit switch values
uint16_t switch_buffers [EXPANDER_COUNT] = { 0, 0, 0, 0 };

// State flags and global constants
bool launchSequence = false;
bool masterAlarm = false;
bool musicEnabled = false;
bool message = false;
bool messageActive = false;
unsigned long messageTime;
unsigned long now;

bool flushing = false;
bool launchActive = false;
uint8_t flushCount = 0;
unsigned long flushTime;
float dim = 7;
float dimDir = 0;

bool s_standby = true;
bool s_uplink = false;
bool s_thrust = false;
bool s_mainChute = false;
bool s_mechFailure = false;
bool s_hatch = false;
bool s_health = false;
bool s_eject = false;
bool s_wasteSystem = false;
bool s_battery = false;
bool s_cabinTemp = false;
bool s_engineFault = false;
bool s_o2Level = false;
bool s_h2Level = false;
bool s_o2Press = false;
bool s_h2Press = false;
bool s_fire = false;
unsigned long fireSuppressOffTime;
bool s_temp = false;

unsigned long mealPrepCutoff;
unsigned long compactorCutoff;
unsigned long suit1Cutoff;
unsigned long suit2Cutoff;
unsigned long healthCutoff;
uint8_t healthVal;

bool thrustState[3] = { 0, 0, 0 };
bool rcsState[3] = { 0, 0, 0 };
unsigned long musicChangeCutoff;

int iteration = 0;

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
/* Arduino Methods                                                       */
/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

void setup() {
  Serial.begin(9600);

  while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB
  }

#ifdef DEBUG
  Serial.println("Starting setup");
#endif

  expanders[0].begin(0);
  expanders[1].begin(1);
  expanders[2].begin(2);
  expanders[3].begin(4);
  expanders[4].begin(3);

  backpacks[0].begin(0x70);
  backpacks[0].setBrightness(dim);

  backpacks[1].begin(0x71);
  backpacks[1].setBrightness(dim);

  backpacks[2].begin(0x72);
  backpacks[2].setBrightness(dim);

  //  for (int i = 0; i < BACKPACK_COUNT; ++i) {
  //    backpackOn(i);
  //    displayMatrix(i);
  //    delay(1000);
  //    backpackOff(i);
  //    displayMatrix(i);
  //  }

  for (int i = 0; i < EXPANDER_COUNT; ++i) {
    for (int j = 0; j < 16; ++j) {
      expanders[i].pinMode(j, INPUT);
      expanders[i].pullUp(j, HIGH);
    }
  }

  delay(20);

#ifdef DEBUG
  Serial.println("Setup complete");
#endif
}

void loop() {
  now = millis();

  pullSwitchValues();

  if (iteration >= 100) {
    iteration = 0;

#ifdef DEBUG
    debugInputs();
    //  debugOutputs();
#endif
  }

  processStatusPanel();
  processCrewSafetyPanel();
  processLaunchControlPanel();
  processCrewLifePanel();
  processPyrotechnicsPanel();
  processCryogenicsPanel();
  processSystemControlPanel();
  processCommunicationsPanel();

  // update the lights
  for (int i = 0; i < BACKPACK_COUNT; ++i) {
    displayMatrix(i);
  }

  iteration++;

  handleMessageFromPi();
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
/* Panel Methods                                                         */
/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

void processCrewSafetyPanel() {
  if (masterAlarm) {
    if (switchState(3, 0) == HIGH) {
      deactivateMasterAlarm();
    }
  }
  setLed(0, 1, 12, masterAlarm);

  if (s_fire) {
    if (switchState(3, 1) == HIGH) {
      if (fireSuppressOffTime == 0) {
        fireSuppressOffTime = now + random(3000, 6000);
        messagePi("FIRE_SUPPRESSION");
      }

      if (fireSuppressOffTime <= now) {
        s_fire = false;
        fireSuppressOffTime = 0;
      }
    } else if (fireSuppressOffTime <= now) {
      fireSuppressOffTime = 0;
    }
  }

  setLed(0, 1, 13, switchState(3, 2));
  setLed(0, 1, 14, isBlinkOn(123) && switchState(3, 3));

  if (switchState(3, 5) && switchState(3, 4)) {
    dim = dim + dimDir;
    if (dim < 1) {
      dim = 1;
    } else if (dim > 7) {
      dim = 7;
    }
    for (int i = 0; i < 3; ++i) {
      backpacks[i].setBrightness(dim);
    }
  } else if (switchState(3, 5) && !switchState(3, 4)) {
    dimDir = -0.2;
  } else if (!switchState(3, 5) && switchState(3, 4)) {
    dimDir = 0.2;
  }
}

void processLaunchControlPanel() {
  if (!launchActive && switchState(1, 8)) {
    launchActive = true;
    messagePi("LAUNCH");
  }
  if (launchActive && switchState(1, 9)) {
    launchActive = false;
    messagePi("ABORT");
  }
  setLed(0, 1, 4, launchActive);
  setLed(0, 1, 5, !launchActive || isBlinkOn(127));
}

void processCrewLifePanel() {
  if (!musicEnabled && switchState(2, 7)) {
    musicEnabled = true;
    messagePi("MUSIC:ON");
  } else if (musicEnabled && !switchState(2, 7)) {
    musicEnabled = false;
    messagePi("MUSIC:OFF");
  }

  if (musicEnabled && switchState(2, 6) && musicChangeCutoff < now) {
    messagePi("MUSIC:CHANGE");
    musicChangeCutoff = now + 3000;
  }

  // music light
  setLed(0, 1, 9, musicEnabled);

  // unsigned long mealPrepCutoff;
  // unsigned long compactorCutoff;

  if (switchState(2, 0) && suit2Cutoff == 0) {
    suit2Cutoff = now + 30000;
  } else if (suit2Cutoff < now) {
    suit2Cutoff = 0;
  }

  if (switchState(2, 1) && compactorCutoff == 0) {
    compactorCutoff = now + 30000;
  } else if (compactorCutoff < now) {
    compactorCutoff = 0;
  }

  if (switchState(2, 2) && suit1Cutoff == 0) {
    suit1Cutoff = now + 30000;
  } else if (suit1Cutoff < now) {
    suit1Cutoff = 0;
  }

  if (switchState(2, 3) && mealPrepCutoff == 0) {
    mealPrepCutoff = now + 30000;
  } else if (mealPrepCutoff < now) {
    mealPrepCutoff = 0;
  }

  if (switchState(2, 4) && healthCutoff == 0) {
    healthCutoff = now + 30000;
    healthVal = random(1, 5);
  } else if (healthCutoff < now) {
    healthCutoff = 0;
  }

  for (int i = 0; i < 4; ++i) {
    setLed(0, 3, 3 - i, i == healthVal - 1);
  }

  setLed(0, 3, 4, mealPrepCutoff != 0);
  setLed(0, 3, 5, compactorCutoff != 0);
  setLed(0, 3, 6, suit1Cutoff != 0);
  setLed(0, 3, 7, suit2Cutoff != 0);

  if (!flushing && switchState(2, 5)) {
    flushing = true;
    messagePi("FLUSH");
  }
}

void processPyrotechnicsPanel() {
  if (switchState(2, 10) && !thrustState[0]) {
    thrustState[0] = true;
    messagePi("THRUST_RIGHT:ON");
  } else if (!switchState(2, 10) && thrustState[0]) {
    thrustState[0] = false;
    messagePi("THRUST_RIGHT:OFF");
  }
  if (switchState(2, 9) && !thrustState[1]) {
    thrustState[1] = true;
    messagePi("THRUST_CENTER:ON");
  } else if (!switchState(2, 9) && thrustState[1]) {
    thrustState[1] = false;
    messagePi("THRUST_CENTER:OFF");
  }
  if (switchState(2, 8) && !thrustState[2]) {
    thrustState[2] = true;
    messagePi("THRUST_LEFT:ON");
  } else if (!switchState(2, 8) && thrustState[2]) {
    thrustState[2] = false;
    messagePi("THRUST_LEFT:OFF");
  }

  if (switchState(2, 13) && !rcsState[0]) {
    rcsState[0] = true;
    messagePi("RCS_RIGHT:ON");
  } else if (!switchState(2, 13) && rcsState[0]) {
    rcsState[0] = false;
    messagePi("RCS_RIGHT:OFF");
  }
  if (switchState(2, 12) && !rcsState[1]) {
    rcsState[1] = true;
    messagePi("RCS_CENTER:ON");
  } else if (!switchState(2, 12) && rcsState[1]) {
    rcsState[1] = false;
    messagePi("RCS_CENTER:OFF");
  }
  if (switchState(2, 11) && !rcsState[2]) {
    rcsState[2] = true;
    messagePi("RCS_LEFT:ON");
  } else if (!switchState(2, 11) && rcsState[2]) {
    rcsState[2] = false;
    messagePi("RCS_LEFT:OFF");
  }
}

void processCryogenicsPanel() {
  // o2 fan
  setLed(0, 0, 0, switchState(3, 8));
  // h2 fan
  setLed(0, 0, 2, switchState(3, 9));
  // heater
  setLed(0, 0, 3, switchState(3, 10));
  // o2 pump
  setLed(0, 0, 4, switchState(3, 11));
  // h2 pump
  setLed(0, 0, 5, switchState(3, 12));
}

void processSystemControlPanel() {
  for (int i = 0; i < 8; ++i) {
    // 9 - 16
    setLed(0, 2, i, switchState(1, i));
    // 1 - 8
    setLed(0, 3, 8 + i, switchState(0, i));
    // 17 - 24
    setLed(0, 2, 15 - i, switchState(0, 8 + i));
  }
}

void processCommunicationsPanel() {
  uint8_t currentCommRegister = 0;
  if (switchState(3, 13) == HIGH) {
    currentCommRegister = 1;
  } else if (switchState(3, 14) == HIGH) {
    currentCommRegister = 2;
  } else if (switchState(3, 15) == HIGH) {
    currentCommRegister = 3;
  }

  if (messageTime < now) {
    if (switchState(2, 14) == HIGH) {
      if (!messageActive) {
        messagePi("MESSAGE");
        messageActive = true;
      }

      messageTime = now + random(60000, 180000);
    } else {
      setLed(0, 0, 0, isBlinkOn(500));
    }
  }

  // set uplink light
  s_uplink = messageActive;
}

void processStatusPanel() {
  // standby
  setLed(0, 0, 0, s_standby ? HIGH : LOW);
  // uplink
  setLed(0, 0, 0, s_uplink ? HIGH : LOW);
  // thrust
  setLed(0, 0, 0, s_thrust ? HIGH : LOW);
  // mainChute
  setLed(0, 0, 0, s_mainChute ? HIGH : LOW);
  // mechFailure
  setLed(0, 0, 0, s_mechFailure ? HIGH : LOW);
  // hatch
  setLed(0, 0, 0, s_hatch ? HIGH : LOW);
  // health
  setLed(0, 0, 0, s_health ? HIGH : LOW);
  // eject
  setLed(0, 0, 0, s_eject ? HIGH : LOW);
  // wasteSystem
  setLed(0, 0, 0, s_wasteSystem ? HIGH : LOW);
  // battery
  setLed(0, 0, 0, s_battery ? HIGH : LOW);
  // cabinTemp
  setLed(0, 0, 0, s_cabinTemp ? HIGH : LOW);
  // engineFault
  setLed(0, 0, 0, s_engineFault ? HIGH : LOW);
  // o2Level
  setLed(0, 0, 0, s_o2Level ? HIGH : LOW);
  // h2Level
  setLed(0, 0, 0, s_h2Level ? HIGH : LOW);
  // o2Press
  setLed(0, 0, 0, s_o2Press ? HIGH : LOW);
  // h2Press
  setLed(0, 0, 0, s_h2Press ? HIGH : LOW);
  // fire
  setLed(0, 0, 0, s_fire ? HIGH : LOW);
  // temp
  setLed(0, 0, 0, s_temp ? HIGH : LOW);
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
/* Helper Methods                                                        */
/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

void activateMasterAlarm() {
  masterAlarm = true;
  messagePi("MASTER_ALARM:ON");
}

void deactivateMasterAlarm() {
  masterAlarm = false;
  messagePi("MASTER_ALARM:OFF");
}

bool isBlinkOn(uint16_t milliDelay) {
  if (milliDelay < 100) {
    milliDelay = 100;
  }
  return millis() % (2 * milliDelay) < milliDelay;
}

uint8_t switchState(uint8_t expander, uint16_t sswitch) {
  return ((switch_buffers[expander] & _BV(sswitch)) > 0) ? HIGH : LOW;
}

void pullSwitchValues() {
  for (uint8_t i = 0; i < EXPANDER_COUNT; ++i) {
    switch_buffers[i] = ~expanders[i].readGPIOAB();
  }
}

void ledOn(uint8_t backpack, uint8_t cathode, uint16_t anode) {
  led_buffers[backpack][cathode] |= _BV(anode);
}

void ledOff(uint8_t backpack, uint8_t cathode, uint16_t anode) {
  if (led_buffers[backpack][cathode] & _BV(anode)) {
    led_buffers[backpack][cathode] ^= _BV(anode);
  }
}

void setLed(uint8_t backpack, uint8_t cathode, uint16_t anode, uint8_t state) {
  if (state == HIGH) {
    ledOn(backpack, cathode, anode);
  } else {
    ledOff(backpack, cathode, anode);
  }
}

void backpackOn(uint8_t backpack) {
  for (uint8_t i = 0; i < 8; i++) {
    led_buffers[backpack][i] = 0b1111111111111111;
  }
}

void backpackOff(uint8_t backpack) {
  for (uint8_t i = 0; i < 8; i++) {
    led_buffers[backpack][i] = 0;
  }
}

void displayMatrix(int backpack) {
  for (uint8_t i = 0; i < 8; i++) {
    backpacks[backpack].displaybuffer[i] = led_buffers[backpack][i];
  }
  backpacks[backpack].writeDisplay();
}

void intToBinaryCharArray(uint16_t val, char* o_buffer) {
  for (int8_t i = 15; i >= 0 ; --i) {
    if (val > 0) {
      uint16_t b = val % 2;
      o_buffer[i] = (char) (b + 48);
      val = val >> 1;
    } else {
      o_buffer[i] = '0';
    }
  }
  o_buffer[16] = 0;
}

void debugInputs() {
  Serial.println();
  Serial.print("Inputs: ");
  for (uint16_t i = 0; i < EXPANDER_COUNT; ++i) {
    Serial.print(i);
    Serial.print(":");
    Serial.print(switch_buffers[i], BIN);
    Serial.print(" ");
  }
  Serial.println();
}

void debugOutputs() {
  Serial.println("Outputs:");
  for (uint8_t i = 0; i < BACKPACK_COUNT; ++i) {
    Serial.print("  - Backpack #");
    Serial.print(i + 1);
    Serial.println(" -");
    for (uint8_t j = 0; j < 8; ++j) {
      Serial.print("    - Cathode: ");
      Serial.print(j);
      Serial.print(" - ");
      Serial.print(led_buffers[i][j], BIN);
      Serial.println(buf);
    }
  }
}

void messagePi(const char* message) {
  Serial.println(message);
}

void handleMessageFromPi() {
  int x;
  String str;

  if (Serial.available() > 0) {
    str = Serial.readStringUntil('\n');
    if (str.equalsIgnoreCase("FLUSH_COMPLETE")) {
      flushing = false;
    } else if (str.equalsIgnoreCase("FIRE")) {
      s_fire = true;
    }
  }
}

