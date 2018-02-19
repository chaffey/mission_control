#include <Wire.h>
#include <Adafruit_LEDBackpack.h>
#include <Adafruit_MCP23017.h>

#define DEBUG

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
/* Global Variables                                                      */
/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

const PROGMEM uint8_t BACKPACK_COUNT = 4;
const PROGMEM uint8_t BACKPACK_A = 0;
const PROGMEM uint8_t BACKPACK_B = 1;
const PROGMEM uint8_t BACKPACK_C = 2;
const PROGMEM uint8_t BACKPACK_D = 3;

const PROGMEM uint8_t EXPANDER_COUNT = 4;
const PROGMEM uint8_t EXPANDER_A = 0;
const PROGMEM uint8_t EXPANDER_B = 1;
const PROGMEM uint8_t EXPANDER_C = 2;
const PROGMEM uint8_t EXPANDER_D = 3;

const PROGMEM uint8_t BRIGHTNESS_A = 7;
const PROGMEM uint8_t BRIGHTNESS_B = 7;
const PROGMEM uint8_t BRIGHTNESS_C = 7;
const PROGMEM uint8_t BRIGHTNESS_D = 7;

char buf[17];

Adafruit_8x16matrix backpacks[BACKPACK_COUNT] = {
  Adafruit_8x16matrix(),
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
  Adafruit_MCP23017()
};

// 4 - 16 bit switch values
uint16_t switch_buffers [EXPANDER_COUNT] = { 0, 0, 0, 0 };

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

  expanders[EXPANDER_A].begin(0);
  expanders[EXPANDER_B].begin(1);
  expanders[EXPANDER_C].begin(2);
  expanders[EXPANDER_D].begin(4);

  backpacks[BACKPACK_A].begin(0x70);
  backpacks[BACKPACK_A].setBrightness(BRIGHTNESS_A);

  backpacks[BACKPACK_B].begin(0x71);
  backpacks[BACKPACK_B].setBrightness(BRIGHTNESS_B);

  backpacks[BACKPACK_C].begin(0x72);
  backpacks[BACKPACK_C].setBrightness(BRIGHTNESS_C);

  backpacks[BACKPACK_D].begin(0x74);
  backpacks[BACKPACK_D].setBrightness(BRIGHTNESS_D);

  for (int i = 0; i < BACKPACK_COUNT; ++i) {
    backpacks[i].clear();
    backpacks[i].writeDisplay();
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
  pullSwitchValues();

#ifdef DEBUG
  debugInputs();
  debugOutputs();
#endif


  if (switchState(3, 0) == LOW && isBlinkOn()) {
    ledOn(3, 0, 0);
  } else {
    ledOff(3, 0, 0);
  }

  processCrewSafetyPanel();
  processLaunchControlPanel();
  processCrewLifePanel();
  processPyrotechnicsPanel();
  processCryogenicsPanel();
  processSystemControlPanel();
  processCommunicationsPanel();
  processStatusPanel();

  delay(20);
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
/* Panel Methods                                                         */
/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

void processCrewSafetyPanel() {
}

void processLaunchControlPanel() {
}

void processCrewLifePanel() {
}

void processPyrotechnicsPanel() {
}

void processCryogenicsPanel() {
}

void processSystemControlPanel() {
}

void processCommunicationsPanel() {
}

void processStatusPanel() {
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
/* Helper Methods                                                        */
/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

bool isBlinkOn() {
  return (millis() % 1000) < 500;
}

uint8_t switchState(uint8_t backpack, uint16_t sswitch) {
  return (switch_buffers[backpack] & _BV(sswitch) > 0) ? HIGH : LOW;
}

void pullSwitchValues() {
  for (uint8_t i = 0; i < BACKPACK_COUNT; ++i) {
    switch_buffers[i] = expanders[i].readGPIOAB();
  }
}

void ledOn(uint8_t backpack, uint8_t cathode, uint16_t anode) {
  led_buffers[backpack][cathode] |= _BV(anode);
  dispMat(backpack);
}

void ledOff(uint8_t backpack, uint8_t cathode, uint16_t anode) {
  if (led_buffers[backpack][cathode] & _BV(anode)) {
    led_buffers[backpack][cathode] ^= _BV(anode);
  }
  dispMat(backpack);
}

void backpackOn(uint8_t backpack) {
  for (uint8_t i = 0; i < 8; i++) {
    led_buffers[backpack][i] = 0b1111111111111111;
  }
  dispMat(backpack);
}

void backpackOff(uint8_t backpack) {
  for (uint8_t i = 0; i < 8; i++) {
    led_buffers[backpack][i] = 0;
  }
  dispMat(backpack);
}

void dispMat(int backpack) {
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
  Serial.println("Inputs:");
  for (uint8_t i = 0; i < BACKPACK_COUNT; ++i) {
    Serial.print("  - Expander #");
    Serial.print(i + 1);
    Serial.print(" - ");
    Serial.print(switch_buffers[i], BIN);
    Serial.println(buf);
  }
}

void debugOutputs() {
  Serial.println("Outputs:");
  for (uint8_t i = 0; i < BACKPACK_COUNT; ++i) {
    Serial.print("  - Backpack #");
    Serial.print(i + 1);
    Serial.println(" -");
    for (uint8_t j = 0; j < 8; ++j) {
      Serial.print("    - Cathod: ");
      Serial.print(j);
      Serial.print(" - ");
      Serial.print(led_buffers[i][j], BIN);
      Serial.println(buf);
    }
  }
}


