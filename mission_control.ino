#include <Wire.h>
#include <Adafruit_LEDBackpack.h>
#include <Adafruit_MCP23017.h>

//#define DEBUG

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
/* Global Variables                                                      */
/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

const uint8_t BACKPACK_COUNT = 3;
const uint8_t EXPANDER_COUNT = 5;
const uint8_t FLUSH_RATE_LIMIT = 30;

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

// 5 - 16 bit switch values
uint16_t switch_buffers [EXPANDER_COUNT] = { 0, 0, 0, 0, 0 };

// State flags and global constants
unsigned long messageTime = millis() + (random(20, 40) * 1000);
unsigned long now;
unsigned long last;
unsigned long fireSuppressOffTime;
unsigned long mealPrepCutoff;
unsigned long compactorCutoff;
unsigned long suit1Cutoff;
unsigned long suit2Cutoff;
unsigned long healthCutoff;
unsigned long musicChangeCutoff;

uint8_t healthVal;
uint8_t flushCount = 0;

float dim = 7;
float dimDir = 0;

bool launchSequence = false;
bool masterAlarm = false;
bool musicEnabled = false;
bool launchActive = false;
bool flushing = false;
bool thrustState[3] = { 0, 0, 0 };
bool rcsState[3] = { 0, 0, 0 };

// Status flags...
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
bool s_temp = false;

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
/* Arduino setup()                                                       */
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

  for (int i = 0; i < BACKPACK_COUNT; ++i) {
    backpackOn(i);
    displayMatrix(i);
  }
  delay(500);
  for (int i = 0; i < BACKPACK_COUNT; ++i) {
    backpackOff(i);
    displayMatrix(i);
  }

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

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
/* Arduino loop()                                                        */
/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
void loop() {
  pullSwitchValues();
  handleMessageFromPi();

  now = millis();

  // 1 second timers
  if (now - last > 1000) {
    last = now;

    if (flushCount > 0) {
      flushCount--;
    } else if (flushCount == 0 && s_wasteSystem) {
      s_wasteSystem = false;
    }

#ifdef DEBUG
    debugInputs();
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
}

void handleMessageFromPi() {
  String str;

  if (Serial.available() > 0) {
    str = Serial.readStringUntil('\n');
    if (str.equalsIgnoreCase("FLUSH_COMPLETE")) {
      flushing = false;
    } else if (str.equalsIgnoreCase("FIRE")) {
      s_fire = true;
    } else if (str.equalsIgnoreCase("MESSAGE_COMPLETE")) {
      messageTime = now + random(60000, 180000);
      s_uplink = false;
    }
  }
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
/* Panel Methods                                                         */
/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

void processCrewSafetyPanel() {
  if (masterAlarm) {
    if (getSwitchState(3, 0) == HIGH) {
      deactivateMasterAlarm();
    }
  }
  setLedState(0, 1, 12, masterAlarm);

  if (s_fire) {
    if (getSwitchState(3, 1) == HIGH) {
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

  setLedState(0, 1, 13, getSwitchState(3, 2));
  setLedState(0, 1, 14, getBlinkState(123) && getSwitchState(3, 3));

  if (getSwitchState(3, 5) && getSwitchState(3, 4)) {
    dim = dim + dimDir;
    if (dim < 0) {
      dim = 0;
    } else if (dim > 7) {
      dim = 7;
    }
    for (int i = 0; i < 3; ++i) {
      backpacks[i].setBrightness(dim);
    }
  } else if (getSwitchState(3, 5) && !getSwitchState(3, 4)) {
    dimDir = -0.2;
  } else if (!getSwitchState(3, 5) && getSwitchState(3, 4)) {
    dimDir = 0.2;
  }
}

void processLaunchControlPanel() {
  if (!launchActive && getSwitchState(1, 8)) {
    launchActive = true;
    messagePi("LAUNCH");
  }
  if (launchActive && getSwitchState(1, 9)) {
    launchActive = false;
    messagePi("ABORT");
  }
  setLedState(0, 1, 4, launchActive);
  setLedState(0, 1, 5, !launchActive || getBlinkState(127));
}

void processCrewLifePanel() {
  if (!musicEnabled && getSwitchState(2, 7)) {
    musicEnabled = true;
    messagePi("MUSIC:ON");
    setLedState(0, 1, 9, HIGH);
  } else if (musicEnabled && !getSwitchState(2, 7)) {
    musicEnabled = false;
    messagePi("MUSIC:OFF");
    setLedState(0, 1, 9, LOW);
  }

  if (musicEnabled && getSwitchState(2, 6) && musicChangeCutoff < now) {
    messagePi("MUSIC:CHANGE");
    musicChangeCutoff = now + 3000;
  }

  if (getSwitchState(2, 0) && suit2Cutoff == 0) {
    suit2Cutoff = now + 30000;
  } else if (suit2Cutoff < now) {
    suit2Cutoff = 0;
  }

  if (getSwitchState(2, 1) && compactorCutoff == 0) {
    compactorCutoff = now + 30000;
    setLedState(0, 3, 5, HIGH);
  } else if (compactorCutoff < now) {
    compactorCutoff = 0;
    setLedState(0, 3, 5, LOW);
  }

  if (getSwitchState(2, 2) && suit1Cutoff == 0) {
    suit1Cutoff = now + 30000;
  } else if (suit1Cutoff < now) {
    suit1Cutoff = 0;
  }

  if (getSwitchState(2, 3) && mealPrepCutoff == 0) {
    mealPrepCutoff = now + 30000;
    setLedState(0, 3, 4, HIGH);
  } else if (mealPrepCutoff < now) {
    mealPrepCutoff = 0;
    setLedState(0, 3, 4, LOW);
  }

  if (getSwitchState(2, 4) && healthCutoff == 0) {
    healthCutoff = now + 30000;
    healthVal = random(1, 5);
    if (healthVal >= 4) {
      s_health = true;
      activateMasterAlarm();
      for (int i = 0; i < 4; ++i) {
        setLedState(0, 3, 3 - i, i == healthVal - 1);
      }
    }
  } else if (healthCutoff < now) {
    healthCutoff = 0;
    s_health = false;
  }

  if (!flushing && getSwitchState(2, 5)) {
    flushCount += 20;
    if (flushCount > FLUSH_RATE_LIMIT) {
      activateMasterAlarm();
      flushCount += 60;
      s_wasteSystem = true;
    } else {
      flushing = true;
      messagePi("FLUSH");
    }
  }

  setLedState(0, 3, 6, suit1Cutoff != 0);
  setLedState(0, 3, 7, suit2Cutoff != 0);

}

void processPyrotechnicsPanel() {
  if (getSwitchState(2, 10) && !thrustState[0]) {
    thrustState[0] = true;
    messagePi("THRUST_RIGHT:ON");
  } else if (!getSwitchState(2, 10) && thrustState[0]) {
    thrustState[0] = false;
    messagePi("THRUST_RIGHT:OFF");
  }
  if (getSwitchState(2, 9) && !thrustState[1]) {
    thrustState[1] = true;
    messagePi("THRUST_CENTER:ON");
  } else if (!getSwitchState(2, 9) && thrustState[1]) {
    thrustState[1] = false;
    messagePi("THRUST_CENTER:OFF");
  }
  if (getSwitchState(2, 8) && !thrustState[2]) {
    thrustState[2] = true;
    messagePi("THRUST_LEFT:ON");
  } else if (!getSwitchState(2, 8) && thrustState[2]) {
    thrustState[2] = false;
    messagePi("THRUST_LEFT:OFF");
  }

  if (getSwitchState(2, 13) && !rcsState[0]) {
    rcsState[0] = true;
    messagePi("RCS_RIGHT:ON");
  } else if (!getSwitchState(2, 13) && rcsState[0]) {
    rcsState[0] = false;
    messagePi("RCS_RIGHT:OFF");
  }
  if (getSwitchState(2, 12) && !rcsState[1]) {
    rcsState[1] = true;
    messagePi("RCS_CENTER:ON");
  } else if (!getSwitchState(2, 12) && rcsState[1]) {
    rcsState[1] = false;
    messagePi("RCS_CENTER:OFF");
  }
  if (getSwitchState(2, 11) && !rcsState[2]) {
    rcsState[2] = true;
    messagePi("RCS_LEFT:ON");
  } else if (!getSwitchState(2, 11) && rcsState[2]) {
    rcsState[2] = false;
    messagePi("RCS_LEFT:OFF");
  }
}

void processCryogenicsPanel() {
  // o2 fan
  setLedState(0, 0, 0, getSwitchState(3, 8));
  // h2 fan
  setLedState(0, 0, 2, getSwitchState(3, 9));
  // heater
  setLedState(0, 0, 3, getSwitchState(3, 10));
  // o2 pump
  setLedState(0, 0, 4, getSwitchState(3, 11));
  // h2 pump
  setLedState(0, 0, 5, getSwitchState(3, 12));
}

void processSystemControlPanel() {
  for (int i = 0; i < 8; ++i) {
    // 9 - 16
    setLedState(0, 2, i, getSwitchState(1, i));
    // 1 - 8
    setLedState(0, 3, 8 + i, getSwitchState(0, i));
    // 17 - 24
    setLedState(0, 2, 15 - i, getSwitchState(0, 8 + i));
  }
}

void processCommunicationsPanel() {
  uint8_t currentCommRegister = 0;
  if (getSwitchState(3, 13) == HIGH) {
    currentCommRegister = 1;
  } else if (getSwitchState(3, 14) == HIGH) {
    currentCommRegister = 2;
  } else if (getSwitchState(3, 15) == HIGH) {
    currentCommRegister = 3;
  }

  if (!s_uplink && messageTime < now) {
    if (getSwitchState(2, 14) == HIGH) {
      if (!s_uplink) {
        messagePi("MESSAGE");
        s_uplink = true;
      }
    } else {
      setLedState(0, 0, 0, getBlinkState(500));
    }
  }
}

void processStatusPanel() {
  // standby
  setLedState(0, 4, 0, s_standby ? HIGH : LOW);
  // uplink
  setLedState(0, 4, 1, s_uplink ? HIGH : LOW);
  // thrust
  setLedState(0, 4, 2, s_thrust ? HIGH : LOW);
  // mainChute
  setLedState(0, 4, 3, s_mainChute ? HIGH : LOW);
  // mechFailure
  setLedState(0, 4, 4, s_mechFailure ? HIGH : LOW);
  // hatch
  setLedState(0, 4, 5, s_hatch ? HIGH : LOW);
  // health
  setLedState(0, 4, 6, s_health ? HIGH : LOW);
  // eject
  setLedState(0, 4, 7, s_eject ? HIGH : LOW);
  // wasteSystem
  setLedState(0, 4, 8, s_wasteSystem ? HIGH : LOW);
  // battery
  setLedState(0, 4, 9, s_battery ? HIGH : LOW);
  // cabinTemp
  setLedState(0, 4, 10, s_cabinTemp ? HIGH : LOW);
  // engineFault
  setLedState(0, 4, 11, s_engineFault ? HIGH : LOW);
  // o2Level
  setLedState(0, 4, 12, s_o2Level ? HIGH : LOW);
  // h2Level
  setLedState(0, 4, 13, s_h2Level ? HIGH : LOW);
  // o2Press
  setLedState(0, 4, 14, s_o2Press ? HIGH : LOW);
  // h2Press
  setLedState(0, 4, 15, s_h2Press ? HIGH : LOW);
//  // fire
//  setLedState(0, 4, 0, s_fire ? HIGH : LOW);
//  // temp
//  setLedState(0, 4, 0, s_temp ? HIGH : LOW);
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
/* Helper Methods                                                        */
/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

void activateMasterAlarm() {
  if (!masterAlarm) {
    masterAlarm = true;
    messagePi("MASTER_ALARM:ON");
  }
}

void deactivateMasterAlarm() {
  masterAlarm = false;
  messagePi("MASTER_ALARM:OFF");
}

bool getBlinkState(uint16_t milliDelay) {
  if (milliDelay < 100) {
    milliDelay = 100;
  }
  return millis() % (2 * milliDelay) < milliDelay;
}

uint8_t getSwitchState(uint8_t expander, uint16_t sswitch) {
  return ((switch_buffers[expander] & _BV(sswitch)) > 0) ? HIGH : LOW;
}

void pullSwitchValues() {
  for (uint8_t i = 0; i < EXPANDER_COUNT; ++i) {
    switch_buffers[i] = ~expanders[i].readGPIOAB();
  }
}

void setLedState(uint8_t backpack, uint8_t cathode, uint16_t anode, uint8_t state) {
  if (state == HIGH) {
    led_buffers[backpack][cathode] |= _BV(anode);
  } else if (led_buffers[backpack][cathode] & _BV(anode)) {
    led_buffers[backpack][cathode] ^= _BV(anode);
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

void debugInputs() {
  Serial.print("I: ");
  for (uint16_t i = 0; i < EXPANDER_COUNT; ++i) {
    Serial.print(switch_buffers[i], BIN);
    Serial.print(" ");
  }
  Serial.println();
}

void messagePi(const String message) {
  Serial.println(message);
}


