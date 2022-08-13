#include <EverydayCalendar_lights.h>
#include <EverydayCalendar_touch.h>

typedef struct {
   int8_t    x;
   int8_t    y;
} Point;

EverydayCalendar_touch cal_touch;
EverydayCalendar_lights cal_lights;
int16_t brightness = 128;


typedef struct {
  Point p;
  // Max means this burst is deactivated and the struct
  // can be reused. Set it to 0 to activate the burst.
  uint16_t tick = UINT16_MAX;
} Burst;
const size_t BURST_COUNT = 8;
Burst bursts[BURST_COUNT];

typedef struct {
  Point offset;
  uint16_t tickStart;
  uint16_t tickEnd;
} AnimPattern;
const AnimPattern ANIM[] = {
  // 1st ring
  {{ -1,  0},   0,  3},
  {{  1,  0},   0,  3},
  {{  0, -1},   1,  3},
  {{  0,  1},   1,  3},

  // 2nd ring
  {{  1, -1},   2,  6},
  {{  1,  1},   2,  6},
  {{ -1, -1},   2,  6},
  {{ -1,  1},   2,  6},
  {{  0, -2},   2,  5},
  {{  0,  2},   2,  5},
  {{ -2,  0},   5,  36 }, // these lingering are #s between 24-46, spaced 2 apart, shuffled randomly
  {{  2,  0},   5,  32 },

  // 3rd ring
  {{ -2, -1},   7,  34 },
  {{ -2,  1},   7,  24 },
  {{  2, -1},   7,  44 },
  {{  2,  1},   7,  42 },
  {{ -1, -2},   6,  26 },
  {{ -1,  2},   6,  46 },
  {{  1, -2},   6,  28 },
  {{  1,  2},   6,  38 },
  {{  0, -3},   4,  40 },
  {{  0,  3},   4,  30 },
};
const size_t ANIM_COUNT = sizeof(ANIM)/sizeof(ANIM[0]);

uint16_t ANIM_MAX_TICK = 0;


void doBurst(Point p) {
  for (size_t i = 0; i < BURST_COUNT; i++)
  {
    Burst* burst = &bursts[i];
    if (burst->tick == UINT16_MAX) {
      burst->p = p;
      burst->tick = 0;
      return;
    }
  }
}

void setup() {
  Serial.begin(9600);
  Serial.println("Sketch setup started");

  // Init data
  for (size_t a = 0; a < ANIM_COUNT; a++)
  {
    if (ANIM[a].tickEnd > ANIM_MAX_TICK) {
      ANIM_MAX_TICK = ANIM[a].tickEnd;
    }
  }
  
  // Initialize LED functionality
  cal_lights.configure();
  cal_lights.setBrightness(200);
  cal_lights.begin();

  // Perform startup animation
  honeyDrip();

  // Fade out
  for(int b = 200; b >= 0; b--){
    cal_lights.setBrightness(b);
    delay(4);
  }

  // Initialize touch functionality
  cal_touch.configure();
  cal_touch.begin();
  cal_lights.loadLedStatesFromMemory();
  delay(1500);

  // Fade in
  for(int b = 0; b <= brightness; b++){
    cal_lights.setBrightness(b);
    delay(4);
  }
  
  Serial.println("Sketch setup complete");
}

void loop() {
  static Point previouslyHeldButton = {(char)0xFF, (char)0xFF}; // 0xFF and 0xFF if no button is held
  static uint16_t touchCount = 1;
  static const uint8_t debounceCount = 3;
  static const uint16_t clearCalendarCount = 1300; // ~40 seconds.  This is in units of touch sampling interval ~= 30ms.  
  Point buttonPressed = {(char)0xFF, (char)0xFF};
  bool touch = cal_touch.scanForTouch();
  // Handle a button press
  if(touch)
  {
    // Brightness Buttons
    if(cal_touch.y == 31){
      if(cal_touch.x == 4){
        brightness = 0;
      }else if(cal_touch.x == 6){
        brightness = 200;
      }
      brightness = constrain(brightness, 0, 200);
      Serial.print("Brightness: ");
      Serial.println(brightness);
      cal_lights.setBrightness((uint8_t)brightness);
    }
    // If all buttons aren't touched, reset debounce touch counter
    if(previouslyHeldButton.x == -1){
      touchCount = 0;
    }

    // If this button is been held, or it's just starting to be pressed and is the only button being touched
    if(((previouslyHeldButton.x == cal_touch.x) && (previouslyHeldButton.y == cal_touch.y))
    || (debounceCount == 0))
    {
      // The button has been held for a certain number of consecutive checks 
      // This is called debouncing
      if (touchCount == debounceCount){
        // Button is activated
        bool on = cal_lights.toggleLED((uint8_t)cal_touch.x, (uint8_t)cal_touch.y);
        cal_lights.saveLedStatesToMemory();
        Serial.print("x: ");
        Serial.print(cal_touch.x);
        Serial.print("\ty: ");
        Serial.println(cal_touch.y);

        // Burst if toggled on
        if (on) {
          doBurst({cal_touch.x, cal_touch.y});
        }
      }

      // Check if the special "Reset" January 1 button is being held
      if((cal_touch.x == 0) && (cal_touch.y == 0) && (touchCount == clearCalendarCount)){
        Serial.println("Resetting all LED states");
        clearAnimation();
      }
      
      if(touchCount < 65535){
        touchCount++;
        Serial.println(touchCount);
      }
    }
  }

  previouslyHeldButton.x = cal_touch.x;
  previouslyHeldButton.y = cal_touch.y;

  // Bursts
  bool needsClearing = true;
  for (size_t i = 0; i < BURST_COUNT; i++)
  {
    // increment tick for active bursts
    Burst* burst = &bursts[i];
    if (burst->tick == UINT16_MAX) {
      continue;
    }
    burst->tick++;
    
    // clear previous overrides if necessary
    if (needsClearing) {
      cal_lights.clearOverrideLEDs();
      needsClearing = false;
    }

    // end burst when at max tick
    if (burst->tick >= ANIM_MAX_TICK) {
      burst->tick = UINT16_MAX;
      continue;
    }

    // enable the overrides for this tick
    for (size_t a = 0; a < ANIM_COUNT; a++) {
      const AnimPattern* anim = &ANIM[a];
      if (burst->tick < anim->tickStart || burst->tick >= anim->tickEnd) {
        continue;
      }
      cal_lights.setOverrideLED(burst->p.x + anim->offset.x, burst->p.y + anim->offset.y, true);
    }
  }
}

void honeyDrip(){
  uint16_t interval_ms = 25;
  static const uint8_t monthDayOffset[12] = {0, 3, 0, 1, 0, 1, 0, 0, 1, 0, 1, 0};
  // Turn on all LEDs one by one in the span of a few second
  for(int day = 0; day < 31; day++){
    for(int month = 0; month < 12; month++){
      int8_t adjustedDay = day - monthDayOffset[month];
      if(adjustedDay >= 0 ){
        cal_lights.setLED(month, adjustedDay, true);
      }
    }
    delay(interval_ms);
    interval_ms = interval_ms + 2;
  }
}

void clearAnimation(){
  uint16_t interval_ms = 25;
  static const uint8_t monthMaxDay[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  for(int month = 11; month >= 0; month--){
    for(int day = monthMaxDay[month]-1; day >=0; day--){
       cal_lights.setLED(month, day, false);
       delay(interval_ms);
    }
  }
  cal_lights.saveLedStatesToMemory();
}
