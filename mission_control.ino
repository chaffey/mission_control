#include <Adafruit_LEDBackpack.h>
#include <Adafruit_MCP23017.h>
#include <Wire.h>
#include <stdio.h>

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
/* Global Variables                                                      */
/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

// const unsigned long BAUD_RATE = 9600;
// const unsigned long BAUD_RATE = 115200;
// const unsigned long BAUD_RATE = 128000;
// const unsigned long BAUD_RATE = 153600;
// const unsigned long BAUD_RATE = 230400;
// const unsigned long BAUD_RATE = 460800;
// const unsigned long BAUD_RATE = 921600;
// const unsigned long BAUD_RATE = 1500000;
const unsigned long BAUD_RATE = 2000000;

const uint8_t BACKPACK_COUNT = 5;
const uint8_t EXPANDER_COUNT = 5;
const uint8_t FLUSH_RATE_LIMIT = 30;

Adafruit_LEDBackpack backpacks[BACKPACK_COUNT] = {
  Adafruit_LEDBackpack(), Adafruit_LEDBackpack(), Adafruit_LEDBackpack()
};

// 3 x 8 (cathodes w/ 16 bit anode value)
uint16_t led_buffers[BACKPACK_COUNT][8] = {
  { 0b0000000000000000, 0b0000000000000000, 0b0000000000000000,
    0b0000000000000000, 0b0000000000000000, 0b0000000000000000,
    0b0000000000000000, 0b0000000000000000
  },
  { 0b0000000000000000, 0b0000000000000000, 0b0000000000000000,
    0b0000000000000000, 0b0000000000000000, 0b0000000000000000,
    0b0000000000000000, 0b0000000000000000
  },
  { 0b0000000000000000, 0b0000000000000000, 0b0000000000000000,
    0b0000000000000000, 0b0000000000000000, 0b0000000000000000,
    0b0000000000000000, 0b0000000000000000
  }
};

Adafruit_MCP23017 expanders[EXPANDER_COUNT] = {
  Adafruit_MCP23017(), Adafruit_MCP23017(), Adafruit_MCP23017(),
  Adafruit_MCP23017(), Adafruit_MCP23017()
};

// 5 - 16 bit switch values
uint16_t switch_buffers[EXPANDER_COUNT] = {0, 0, 0, 0, 0};

// State flags and global constants
char buf[32];
unsigned long messageTime = millis() + (random(20, 40) * 1000);
unsigned long now;
unsigned long last;
unsigned long fireSuppressOffTime;
unsigned long mealPrepCutoff;
unsigned long compactorCutoff;
unsigned long suit1Cutoff;
unsigned long suit2Cutoff;
unsigned long toff;
unsigned long healthCutoff;
unsigned long musicChangeCutoff;

uint8_t healthVal;
uint8_t flushCount = 0;

float dim = 3;
float dimDir = 0;

bool launchSequence = false;
bool masterAlarm = false;
bool musicEnabled = false;
bool launchActive = false;
bool flushing = false;
bool thrustState[3] = {0, 0, 0};
bool rcsState[3] = {0, 0, 0};
bool o2FanOn = false;
bool h2FanOn = false;
bool heaterOn = false;
bool o2PumpOn = false;
bool h2PumpOn = false;
bool co2ScrubberOn = false;
bool cabinHeadOn = false;
bool systemControlSwitches[3][8] = {{0, 0, 0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, 0, 0}
};

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

int looper = 0;

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
/* Arduino setup()                                                       */
/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

void setup() {
  Serial.begin(BAUD_RATE);

  while (!Serial) {
    ;  // wait for serial port to connect. Needed for native USB
  }

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
  delay(1000);
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
  }

  processStatusPanel();
  processCrewSafetyPanel();
  processLaunchControlPanel();
  processCrewLifePanel();
  processPyrotechnicsPanel();
  processCryogenicsPanel();
  processSystemControlPanel();
  processCommunicationsPanel();

  if(getBlinkState(1000) == HIGH) {
    backpackOn(1);
  } else {
    backpackOff(1);
  }

  // update the lights
  for (int i = 0; i < BACKPACK_COUNT; ++i) {
    displayMatrix(i);
  }

  delay(20);
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

  // meal prep
  if (getSwitchState(2, 3) && mealPrepCutoff == 0) {
    mealPrepCutoff = now + 30000;
    setLedState(0, 3, 4, HIGH);
    messagePi("FOOD_PREP");
  } else if (mealPrepCutoff > 0 && mealPrepCutoff < now) {
    mealPrepCutoff = 0;
    setLedState(0, 3, 4, LOW);
  }

  // compactor
  if (getSwitchState(2, 1) && compactorCutoff == 0) {
    compactorCutoff = now + 30000;
    setLedState(0, 3, 5, HIGH);
    messagePi("TRASH");
  } else if (compactorCutoff > 0 && compactorCutoff < now) {
    compactorCutoff = 0;
    setLedState(0, 3, 5, LOW);
  }

  // Suit 1
  if (getSwitchState(2, 2) && suit1Cutoff == 0) {
    suit1Cutoff = now + 30000;
    messagePi("SUIT:PRESSURIZE");
    setLedState(0, 3, 6, HIGH);
  } else if (suit1Cutoff > 0 && suit1Cutoff < now) {
    suit1Cutoff = 0;
    setLedState(0, 3, 6, LOW);
  }

  // Suit 2
  if (getSwitchState(2, 0) && suit2Cutoff == 0) {
    suit2Cutoff = now + 30000;
    messagePi("SUIT:PRESSURIZE");
    setLedState(0, 3, 7, HIGH);
  } else if (suit2Cutoff < now) {
    suit2Cutoff = 0;
    setLedState(0, 3, 7, LOW);
  }

  if (getSwitchState(2, 4) && healthCutoff == 0) {
    healthCutoff = now + 30000;
    healthVal = random(1, 5);
    if (healthVal >= 3) {
      s_health = true;
      activateMasterAlarm();
    }
  } else if (healthCutoff > 0 && healthCutoff < now) {
    healthCutoff = 0;
    s_health = false;
  }

  if (!flushing && getSwitchState(2, 5)) {
    flushCount += 20;
    if (flushCount > FLUSH_RATE_LIMIT) {
      activateMasterAlarm();
      flushCount += 80;
      s_wasteSystem = true;
    } else if (!s_wasteSystem) {
      flushing = true;
      messagePi("FLUSH");
    }
  }

  for (int i = 0; i < 4; ++i) {
    setLedState(0, 3, 3 - i, healthCutoff > now && i == healthVal - 1);
  }
}

void processPyrotechnicsPanel() {
  if (getSwitchState(2, 10) && !thrustState[0]) {
    thrustState[0] = true;
    messagePi("THRUST:ON");
  } else if (!getSwitchState(2, 10) && thrustState[0]) {
    thrustState[0] = false;
    messagePi("THRUST:OFF");
  }
  if (getSwitchState(2, 9) && !thrustState[1]) {
    thrustState[1] = true;
    messagePi("THRUST:ON");
  } else if (!getSwitchState(2, 9) && thrustState[1]) {
    thrustState[1] = false;
    messagePi("THRUST:OFF");
  }
  if (getSwitchState(2, 8) && !thrustState[2]) {
    thrustState[2] = true;
    messagePi("THRUST:ON");
  } else if (!getSwitchState(2, 8) && thrustState[2]) {
    thrustState[2] = false;
    messagePi("THRUST:OFF");
  }

  if (getSwitchState(2, 13) && !rcsState[0]) {
    rcsState[0] = true;
    messagePi("RCS:ON");
  } else if (!getSwitchState(2, 13) && rcsState[0]) {
    rcsState[0] = false;
    messagePi("RCS:OFF");
  }
  if (getSwitchState(2, 12) && !rcsState[1]) {
    rcsState[1] = true;
    messagePi("RCS:ON");
  } else if (!getSwitchState(2, 12) && rcsState[1]) {
    rcsState[1] = false;
    messagePi("RCS:OFF");
  }
  if (getSwitchState(2, 11) && !rcsState[2]) {
    rcsState[2] = true;
    messagePi("RCS:ON");
  } else if (!getSwitchState(2, 11) && rcsState[2]) {
    rcsState[2] = false;
    messagePi("RCS:OFF");
  }

  s_thrust = false;
  for (int i = 0; i < 3; ++i) {
    if (thrustState[i] || rcsState[i]) {
      s_thrust = true;
      break;
    }
  }
}

void processCryogenicsPanel() {
  // o2 fan1
  if (getSwitchState(3, 8)) {
    if (!o2FanOn) {
      o2FanOn = true;
      messagePi("O2_FAN:ON");
    }
    setLedState(0, 0, 1, HIGH);
  } else if (o2FanOn) {
    o2FanOn = false;
    messagePi("O2_FAN:OFF");
    setLedState(0, 0, 1, LOW);
  }

  // h2 fan
  if (getSwitchState(3, 9)) {
    if (!h2FanOn) {
      h2FanOn = true;
      messagePi("H2_FAN:ON");
    }
    setLedState(0, 0, 2, HIGH);
  } else if (h2FanOn) {
    h2FanOn = false;
    messagePi("H2_FAN:OFF");
    setLedState(0, 0, 2, LOW);
  }

  // heater
  if (getSwitchState(3, 10)) {
    if (!heaterOn) {
      heaterOn = true;
      messagePi("HEATER:ON");
    }
    setLedState(0, 0, 3, HIGH);
  } else if (heaterOn) {
    heaterOn = false;
    messagePi("HEATER:OFF");
    setLedState(0, 0, 3, LOW);
  }

  // o2 pump
  if (getSwitchState(3, 11)) {
    if (!o2PumpOn) {
      o2PumpOn = true;
      messagePi("O2_PUMP:ON");
    }
    setLedState(0, 0, 4, HIGH);
  } else if (o2PumpOn) {
    o2PumpOn = false;
    messagePi("O2_PUMP:OFF");
    setLedState(0, 0, 4, LOW);
  }

  // h2 pump
  if (getSwitchState(3, 12)) {
    if (!h2PumpOn) {
      h2PumpOn = true;
      messagePi("H2_PUMP:ON");
    }
    setLedState(0, 0, 5, HIGH);
  } else if (h2PumpOn) {
    h2PumpOn = false;
    messagePi("H2_PUMP:OFF");
    setLedState(0, 0, 5, LOW);
  }
}

void processSystemControlPanel() {
  for (int i = 0; i < 8; ++i) {
    // 1 - 8
    bool on = getSwitchState(0, i);
    if (on != systemControlSwitches[0][i]) {
      systemControlSwitches[0][i] = on;
      sprintf(buf, "SWITCH_%2d:%s", i + 1, on ? "ON" : "OFF");
      messagePi(buf);
      setLedState(0, 3, 8 + i, on ? HIGH : LOW);
    }

    // 9 - 16
    on = getSwitchState(1, i);
    if (on != systemControlSwitches[1][i]) {
      systemControlSwitches[1][i] = on;
      sprintf(buf, "SWITCH_%2d:%s", i + 9, on ? "ON" : "OFF");
      messagePi(buf);
      setLedState(0, 2, i, on ? HIGH : LOW);
    }

    // 17 - 24
    on = getSwitchState(0, 8 + i);
    if (on != systemControlSwitches[2][i]) {
      systemControlSwitches[2][i] = on;
      sprintf(buf, "SWITCH_%2d:%s", i + 17, on ? "ON" : "OFF");
      messagePi(buf);
      setLedState(0, 2, 15 - i, on ? HIGH : LOW);
    }
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
        setLedState(0, 0, 0, LOW);
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
  // fire
  // setLedState(0, 4, 0, s_fire ? HIGH : LOW);
  // temp
  // setLedState(0, 4, 0, s_temp ? HIGH : LOW);
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

void setLedState(uint8_t backpack, uint8_t cathode, uint16_t anode,
                 uint8_t state) {
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

void messagePi(const String message) {
  Serial.println(message);
}
