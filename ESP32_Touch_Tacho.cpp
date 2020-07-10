 /* Routines to add Tacho capability to TouchDRO Interface
Mates with ESP32Encoder as adapted by romeo
Prepared June 2020
*/


#include "ESP32_Touch_Tacho.h"

const int minRPM = 20; //RPM below which tacho will decide shaft is stopped

extern ESP32Tacho Tacho;  

portMUX_TYPE TachoMux = portMUX_INITIALIZER_UNLOCKED;  //despite vs squiggly line, compiles ok
const int32_t microsPerMinute = 60000000;

ESP32Tacho::ESP32Tacho() {
	pulsesPerRev = 0;
}
ESP32Tacho::ESP32Tacho(int aP, int bP, float ppr, pullupType uip) {
	tachoPinA = (gpio_num_t)aP;
	tachoPinB = (gpio_num_t)bP;
	pulsesPerRev = ppr;
	puType = uip;
	pulsesPerMinute = 0;
	maxPulseInterval = microsPerMinute / (minRPM * pulsesPerRev);
	directionEnabled = false;
}

void ESP32Tacho::attachTacho() {
	setupPin(tachoPinA);
	if (tachoPinB != 0 && tachoPinB != tachoPinA) {
		setupPin(tachoPinB);
		directionEnabled = true;
		running = false;
	}
	attachInterrupt(tachoPinA, tacho_isr, RISING);
}
void ESP32Tacho::setupPin(gpio_num_t a) {
	gpio_pad_select_gpio(a);
	gpio_set_direction(a, GPIO_MODE_INPUT);
	if (puType == down) {
		gpio_pulldown_en(a);
	}
	else {
		if (puType == up) {
			gpio_pullup_en(a);
		}
	}
}

void IRAM_ATTR tacho_isr() {
	unsigned long isr_micros = micros();
	if (!Tacho.running) {
		portENTER_CRITICAL_ISR(&TachoMux);
		Tacho.running = true;
		Tacho.pulseCount = 0;
		Tacho.direction = digitalRead(Tacho.tachoPinB);
		Tacho.timeOfLastPulse = isr_micros;
		Tacho.aggregateMicros = 0;
		portEXIT_CRITICAL_ISR(&TachoMux);
	}
	else {
		portENTER_CRITICAL_ISR(&TachoMux);
		Tacho.pulseCount++;
		Tacho.aggregateMicros += (isr_micros - Tacho.timeOfLastPulse);
		Tacho.direction = digitalRead(Tacho.tachoPinB);
		Tacho.timeOfLastPulse = isr_micros;
		portEXIT_CRITICAL_ISR(&TachoMux);
	}
}

float ESP32Tacho::get_ppm() {
	int temp_pulseCount;
	unsigned long temp_aggregateMicros;
	unsigned long temp_timeOfLastPulse;

	if (!Tacho.running) {
		return 0;
	}
	portENTER_CRITICAL(&TachoMux);
	temp_pulseCount = pulseCount;
	pulseCount = 0;
	temp_aggregateMicros = aggregateMicros;
	aggregateMicros = 0;
	temp_timeOfLastPulse = timeOfLastPulse;
	portEXIT_CRITICAL(&TachoMux);

	if (temp_pulseCount == 0) { //if no actual pulses received, display an apparent slowdown in accordance with the report interval
		pulseInterval = micros() - temp_timeOfLastPulse;
		if (pulseInterval > maxPulseInterval) {
			running = false;
			direction = 0;
			return 0;
		}
		temp_pulseCount = 1;
	}
	else {
		pulseInterval = temp_aggregateMicros / temp_pulseCount;
	}
	return (microsPerMinute / pulseInterval);
}


