#include "EverydayCalendar_lights.h"
#include <Arduino.h>
#include <SPI.h>
#include <EEPROM.h>

#define UINT16_MSB(uint16) ((uint8_t)(uint16 >> 8))
#define UINT16_LSB(uint16) ((uint8_t)(uint16 & 0xFF))
#define ARRAY_SIZE(array) (sizeof((array))/sizeof((array[0])))
#define EEPROM_START_ADR  0x00000000

static const int BRIGHTNESS_INITIAL_X10 = 2180; // x10 is to give us room to animate more smoothly without using floats
static const int BRIGHTNESS_DARKEST_X10 = 2400;
static const int BRIGHTNESS_LIGHTEST_X10 = 600;
static const int BRIGHTNESS_SPEED = 20;
static const int BRIGHTNESS_DELAY_DIST_TICKS = 40;

static const int csPin = 10;
static const int tickle_pin = 9;
static const int outputEnablePin = 8;
// set up the speed, data order and data mode
static SPISettings spiSettings(4000000, MSBFIRST, SPI_MODE0);
static uint32_t ledValues[12] = {0}; // Months are the integers in range [0,11], Days are bits within the integers, in range [0,31]
static bool loaded = false;

enum AnimState {
  Still = 0, // rest state at medium bright
  Delayed = 1, // waiting a number of ticks
  Brightening = 2, // go up to max bright
  Dimming = 3, // go down to min bright
  Resetting = 4, // go back up to initial bright
}; 

struct BrightnessRecord {
  int brightnessX10 = 0;
  int delayTicks = 0;
  AnimState state = Still;
};
static BrightnessRecord brights[12];
static int animMonth = 0;

static const int BRIGHT_SHIFT_TICKS = 1;

void EverydayCalendar_lights::configure(){
  // LED configurations
  SPI.begin();
  pinMode (csPin, OUTPUT);
  digitalWrite (csPin, HIGH);
  pinMode (tickle_pin, OUTPUT);
  digitalWrite(tickle_pin, HIGH);
  pinMode (outputEnablePin, OUTPUT);
  digitalWrite(outputEnablePin, HIGH);
  // Enable Timer2 interrupt.
  // We want CTC mode (mode 2) where timer resets after compare
  TCCR2A = (TCCR2A & ~0x03) | 0x00;
  TCCR2B = (TCCR2B & ~0x08) | 0x00;
  TCCR2B = (TCCR2B & ~0x07) | 0x02; // selects a clock prescaler of 8. That's a frequency of 31372.55
  OCR2A = BRIGHTNESS_INITIAL_X10 / 10;
  clearAllLEDs();
}

void EverydayCalendar_lights::begin(){
  TIMSK2 = (1<<OCIE2A) | (1<<TOIE2); // Enable Timers
}

void EverydayCalendar_lights::clearAllLEDs(){
  memset(ledValues, 0, sizeof(ledValues));
}

// Month is in range [0,11]
// Day is in range [0,30]
void EverydayCalendar_lights::setLED(uint8_t month, uint8_t day, bool enable){
  if (month > 11 || day > 30){
    return;
  }

  if (enable){
      ledValues[month] = ledValues[month] | ((uint32_t)1 << day);

    if (loaded) {
      // Run animation for this month
      animMonth = month;
      for (size_t i = 0; i < 12; i++)
      {
        int dist = abs((int)month - (int)i);
        brights[i].brightnessX10 = dist == 0 ? BRIGHTNESS_LIGHTEST_X10 : BRIGHTNESS_INITIAL_X10;
        brights[i].delayTicks = dist * BRIGHTNESS_DELAY_DIST_TICKS;
        brights[i].state = dist == 0 ? Brightening : Delayed;
      }
    }
  }else{
      ledValues[month] = ledValues[month] & ~((uint32_t)1 << day);
  }
}

void EverydayCalendar_lights::toggleLED(uint8_t month, uint8_t day){
   bool ledState = (*(ledValues+month) & ((uint32_t)1 << (day)));
   setLED(month, day, !ledState);
}

void EverydayCalendar_lights::saveLedStatesToMemory(){
  for(uint8_t month=0; month<12; month++){
    int addr = EEPROM_START_ADR + (month * sizeof(uint32_t));
    EEPROM.put(addr, ledValues[month]);
  }
}

void EverydayCalendar_lights::loadLedStatesFromMemory(){
  // If the first month is completely set, then this is the first time we're running the code.
  // Clear the calendar memory
  uint32_t firstInt;
  EEPROM.get(EEPROM_START_ADR, firstInt);
  if(firstInt == 0xFFFFFFFF){
    Serial.println("First time running everyday calendar.  Clearing all days.");
    for(int i=0; i<sizeof(ledValues); i++){
      EEPROM.write(EEPROM_START_ADR + i, 0x00);
    }
  }
  // Load calendar from memory
  for(int i=0; i<sizeof(ledValues); i++){
    *((byte*)ledValues + i) = EEPROM.read(EEPROM_START_ADR + i);
  }

  for(int i=0; i<12; i++){
    Serial.print("LED Column ");
    Serial.print(i);
    Serial.print(" = ");
    Serial.println(ledValues[i]);
  }

  loaded = true;
}

void EverydayCalendar_lights::setBrightness(uint8_t b){
  if(b > 0){
    TIMSK2 |= (1<<OCIE2A);
  } else {
    TIMSK2 &= ~(1<<OCIE2A);
  }
}


// Code to drive the LED multiplexing.
// This code is called at a very fast period, activating each LED column one by one
// This is done faster than the eye can perceive, and it appears that all twelve columns are illuminated at once.
ISR(TIMER2_COMPA_vect) {
  digitalWrite(outputEnablePin, LOW); // Enable
}
ISR(TIMER2_OVF_vect) {
    static uint16_t activeColumn = 0;
    static byte spiBuf[6];
    size_t c = activeColumn;

    digitalWrite(outputEnablePin, HIGH); // Disable


    // Use initial brightness by default
    int brightnessX10 = BRIGHTNESS_INITIAL_X10;

    // Otherwise animate wave
    if (brights[c].state == Delayed) {
      brights[c].delayTicks--;
      if (brights[c].delayTicks <= 0) {
        brights[c].state = Brightening;
      }
    } else if (brights[c].state != Still) {
      
      // Velocity & reversing
      int vel = -BRIGHTNESS_SPEED; // negative means going up since higher numbers are brighter
      if (brights[c].state == Dimming) {
        vel *= -1;
      }

      // Slow down darker (higher) velocity so there's balance between light & dark
      if (brights[c].brightnessX10 > 2100) {
        vel /= 4;
      } else if (brights[c].brightnessX10 > 1800) {
        vel /= 2;
      }
      
      // New brightness
      int prev = brights[c].brightnessX10;
      brights[c].brightnessX10 += vel;

      // Change states
      if (brights[c].state == Brightening && brights[c].brightnessX10 < BRIGHTNESS_LIGHTEST_X10) {
        brights[c].state = Dimming;
      } else if (brights[c].state == Dimming && brights[c].brightnessX10 > BRIGHTNESS_DARKEST_X10) {
        brights[c].state = Resetting;
      } else if (brights[c].state == Resetting && brights[c].brightnessX10 <= BRIGHTNESS_INITIAL_X10) {
        brights[c].state = Still;
      }
      
      // Use anim brightness
      brightnessX10 = brights[c].brightnessX10;
      
      // Special handling for anim month to always stay bright
      if (c == animMonth) {
        if (brights[c].state && brightnessX10 > BRIGHTNESS_INITIAL_X10) {
          brightnessX10 = BRIGHTNESS_INITIAL_X10;
        }
      }
    }

    OCR2A = brightnessX10 / 10;

    // "Tickle" the watchdog circuit to keep the LEDs enabled
    digitalWrite(tickle_pin, !digitalRead(tickle_pin));

    // update the next column
    uint16_t month = (1 << activeColumn);
    uint8_t * pDays = (uint8_t*) (ledValues + activeColumn);

    // Send the LED control values into the shift registers
    digitalWrite (csPin, LOW);
    SPI.beginTransaction(spiSettings);
    memcpy(spiBuf, &month, 2);
    spiBuf[2] = pDays[3];
    spiBuf[3] = pDays[2];
    spiBuf[4] = pDays[1];
    spiBuf[5] = pDays[0];
    SPI.transfer(spiBuf, sizeof(spiBuf));
    SPI.endTransaction();
    digitalWrite (csPin, HIGH);

    activeColumn = (activeColumn + 1) % 12;
}
