#include <Wire.h>
#include <Adafruit_LEDBackpack.h>
#include <Adafruit_MCP23017.h>

//#define DEBUG

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
/* Global Variables                                                      */
/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

const PROGMEM uint8_t BACKPACK_COUNT = 3;
const PROGMEM uint8_t EXPANDER_COUNT = 5;

const PROGMEM uint8_t BRIGHTNESS_0 = 7;
const PROGMEM uint8_t BRIGHTNESS_1 = 7;
const PROGMEM uint8_t BRIGHTNESS_2 = 7;

const byte SERIAL_DELIMITER = '|';

char buf[17];

Adafruit_8x16matrix backpacks[BACKPACK_COUNT] = {
  Adafruit_8x16matrix(),
  Adafruit_8x16matrix(),
  Adafruit_8x16matrix()
};

// 4 x 8 (cathodes w/ 16 bit anode value)
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
uint8_t flushCount = 0;
unsigned long flushTime;

boolean s_standby = true;
boolean s_uplink = false;
boolean s_thrust = false;
boolean s_mainChute = false;
boolean s_mechFailure = false;
boolean s_hatch = false;
boolean s_health = false;
boolean s_eject = false;
boolean s_wasteSystem = false;
boolean s_battery = false;
boolean s_cabinTemp = false;
boolean s_engineFault = false;
boolean s_o2Level = false;
boolean s_h2Level = false;
boolean s_o2Press = false;
boolean s_h2Press = false;
boolean s_fire = false;
unsigned long fireSuppressOffTime;
boolean s_temp = false;

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
  backpacks[0].setBrightness(BRIGHTNESS_0);

  backpacks[1].begin(0x71);
  backpacks[1].setBrightness(BRIGHTNESS_1);

  backpacks[2].begin(0x72);
  backpacks[2].setBrightness(BRIGHTNESS_2);

  for (int i = 0; i < BACKPACK_COUNT; ++i) {
    backpackOn(i);
    displayMatrix(i);
    delay(250);
    backpackOff(i);
    displayMatrix(i);
  }

  for (int i = 0; i < EXPANDER_COUNT; ++i) {
    for (int j = 0; j < 16; ++j) {
      expanders[i].pinMode(j, INPUT);
      expanders[i].pullUp(j, HIGH);
    }
  }

  delay(100);

#ifdef DEBUG
  Serial.println("Setup complete");
#endif
}

void loop() {
  now = millis();

  pullSwitchValues();

  if (iteration >= 1000) {
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
}

void processLaunchControlPanel() {
}

void processCrewLifePanel() {
  if (!musicEnabled && switchState(2, 7) == HIGH) {
    musicEnabled = true;
    messagePi("MUSIC:ON");
  } else if (musicEnabled && switchState(2, 7) == LOW) {
    musicEnabled = false;
    messagePi("MUSIC:OFF");
  }

  // music light
  setLed(0, 1, 9, isBlinkOn(2000) && musicEnabled);
}

void processPyrotechnicsPanel() {
}

void processCryogenicsPanel() {
  // o2 fan
  setLed(0, 4, 1, switchState(3, 8));
  // h2 fan
  setLed(0, 4, 2, switchState(3, 9));
  // heater
  setLed(0, 4, 3, switchState(3, 10));
  // o2 pump
  setLed(0, 4, 4, switchState(3, 11));
  // h2 pump
  setLed(0, 4, 5, switchState(3, 12));
}

void processSystemControlPanel() {
  for (int i = 0; i < 8; ++i) {
    setLed(0, 2, i, switchState(1, i));
    setLed(0, 3, 8 + i, switchState(0, i));
    setLed(0, 2, 15 - i, switchState(1, 8 + i));
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
    if (switchState(2, 15) == HIGH) {
      if (!messageActive) {
        messagePi("MESSAGE");
        messageActive = true;
      }

      messageTime = now + random(60000, 180000);
    } else {
      setLed(0, 4, 0, isBlinkOn(2000));
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
  Serial.print(SERIAL_DELIMITER);
  Serial.print(message);
  Serial.print(SERIAL_DELIMITER);
}

