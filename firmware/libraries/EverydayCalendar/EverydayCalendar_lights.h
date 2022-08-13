#ifndef __EVERYDAYCALENDAR_LIGHTS_H
#define __EVERYDAYCALENDAR_LIGHTS_H
#include <stdint.h>


class EverydayCalendar_lights
{
public:
	void configure();
	void begin();
	void loadLedStatesFromMemory();
	void saveLedStatesToMemory();

	void setLED(uint8_t month, uint8_t day, bool enable);
	bool toggleLED(uint8_t month, uint8_t day);
	void clearAllLEDs();
	void setBrightness(uint8_t brightness);

	void setOverrideLED(uint8_t month, uint8_t day, bool enable);
	void EverydayCalendar_lights::clearOverrideLEDs();
};

#endif
