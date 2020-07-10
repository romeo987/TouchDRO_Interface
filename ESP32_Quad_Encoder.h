// ESP32_Quad_Encoder.h

#ifndef _ESP32_QUAD_ENCODER_h
#define _ESP32_QUAD_ENCODER_h

#if defined(ARDUINO) && ARDUINO >= 100
	#include "arduino.h"
#else
	#include "WProgram.h"
#endif
#pragma once
#include <Arduino.h>
#include <driver/gpio.h>
#include "driver/pcnt.h"
#define MAX_ENCODERS 4

enum encType { single, half, full, na };
enum axisType { x, y, z, w };
enum pullupType { none, up, down };
unsigned long const reportPeriod = 40;//mS interval for basic report to TouchDRO app
const int tachoReportInterval = 12; //tacho value will be reported only every <this> instance of basic report

class ESP32Encoder {
private:
	int aPin;
	int bPin;
	enum pullupType puType;
	int filter = 0;
	bool fullQuad = false;
	gpio_num_t aPinNumber;
	gpio_num_t bPinNumber;
	pcnt_unit_t unit;
	pcnt_config_t enc_config;
	static int numEncoders;
	static bool attachedInterrupt;

public:
	ESP32Encoder(axisType axi, int aP, int bP, encType ty, pullupType uip, int fil);
	ESP32Encoder();
	void attachEncoder(int index);
	int32_t getCount();
	int32_t clearCount();
	enum axisType axis;
	enum encType type;
	volatile int32_t count = 0;
	static ESP32Encoder* encoders[MAX_ENCODERS];
};



#endif

