#include <Wire.h>
#include <Adafruit_LEDBackpack.h>

const uint8_t MATRIX_COUNT = 4;
const uint8_t BP_A = 0;
const uint8_t BP_B = 1;
const uint8_t BP_C = 2;
const uint8_t BP_D = 3;

Adafruit_8x16matrix backpacks[MATRIX_COUNT] = {
  Adafruit_8x16matrix(),
  Adafruit_8x16matrix(),
  Adafruit_8x16matrix(),
  Adafruit_8x16matrix()
};

uint16_t buffers [MATRIX_COUNT][8] = { 
  0b00000000, 
  0b00000000, 
  0b00000000, 
  0b00000000, 
};

void setup() {
  Serial.begin(9600);

  backpacks[BP_A].begin(0x70);
  backpacks[BP_A].setBrightness(7);

  backpacks[BP_B].begin(0x71);
  backpacks[BP_B].setBrightness(7);

  backpacks[BP_C].begin(0x72);
  backpacks[BP_C].setBrightness(7);

  backpacks[BP_D].begin(0x74);
  backpacks[BP_D].setBrightness(7);

  for (int i = 0; i < MATRIX_COUNT; ++i) {
    backpacks[i].clear();
    backpacks[i].writeDisplay();
  }
}

void loop() {
}

void ledOn (Adafruit_LEDBackpack matrix, uint16_t buffer[8], uint8_t cathode, uint8_t anode) {
  buffer[cathode] |= _BV(anode);
  dispMat(matrix, buffer);
}

void ledOff (Adafruit_LEDBackpack matrix, uint16_t buffer[8], uint8_t cathode, uint8_t anode) {
  if (buffer[cathode] & _BV(anode)) {
    buffer[cathode] ^= _BV(anode);
  }
  dispMat(matrix, buffer);
}

void ledToggle (Adafruit_LEDBackpack matrix, uint16_t buffer[8], uint8_t cathode, uint8_t anode) {
  if (buffer[cathode] & _BV(anode)) {
    ledOff(matrix, buffer, cathode, anode);
  } else {
    ledOn(matrix, buffer, cathode, anode);
  }
}

void matrixOn(Adafruit_LEDBackpack matrix, uint16_t buffer[8]) {
  for (uint8_t i = 0; i < 8; i++) {
    buffer[i] = 0b1111111111111111;
  }
  dispMat(matrix, buffer);
}

void matrixOff(Adafruit_LEDBackpack matrix, uint16_t buffer[8]) {
  for (uint8_t i = 0; i < 8; i++) {
    buffer[i] = 0;
  }
  dispMat(matrix, buffer);
}

void dispMat (Adafruit_LEDBackpack matrix, uint16_t buffer[8])
{
  for (uint8_t i = 0; i < 8; i++) {
    matrix.displaybuffer[i] = buffer[i];
  }
  matrix.writeDisplay();
}
