// ESP32_Touch_Tacho.h

#ifndef _ESP32_TOUCH_TACHO_h
#define _ESP32_TOUCH_TACHO_h

#if defined(ARDUINO) && ARDUINO >= 100
	#include "arduino.h"
#else
	#include "WProgram.h"
#endif

#ifndef _ESP32TACHO_h
#define _ESP32TACHO_h
#endif
#include "ESP32_Quad_Encoder.h"

class ESP32Tacho {
private:
	unsigned long maxPulseInterval;
	//int nopulseCount;
	gpio_num_t tachoPinA;
	enum pullupType puType;
	void(setupPin(gpio_num_t pin));

public:
	ESP32Tacho(int pinA, int pinB, float pulsePerRev, pullupType uip);
	ESP32Tacho();
	void attachTacho();
	float get_ppm();
	float pulsesPerRev;
	float pulsesPerMinute;
	volatile unsigned long timeOfLastPulse;
	volatile unsigned long aggregateMicros;
	unsigned long pulseInterval;
	volatile int pulseCount;
	bool running;
	bool directionEnabled;
	int direction;
	gpio_num_t tachoPinB;
};

void IRAM_ATTR tacho_isr();




#endif

