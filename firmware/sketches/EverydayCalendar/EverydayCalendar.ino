#include <EverydayCalendar_lights.h>
#include <EverydayCalendar_touch.h>

typedef struct {
   int8_t    x;
   int8_t    y;
} Point;

EverydayCalendar_touch cal_touch;
EverydayCalendar_lights cal_lights;
int16_t brightness = 128;

void setup() {
  Serial.begin(9600);
  Serial.println("Sketch setup started");
  
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
        cal_lights.toggleLED((uint8_t)cal_touch.x, (uint8_t)cal_touch.y);
        cal_lights.saveLedStatesToMemory();
        Serial.print("x: ");
        Serial.print(cal_touch.x);
        Serial.print("\ty: ");
        Serial.println(cal_touch.y);
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
