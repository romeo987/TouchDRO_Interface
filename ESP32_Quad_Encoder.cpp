/*
 * Adapted from ESP32Encoder.cpp
 Original Author: @Author github:madhephaestus mad.hephaestus@gmail.com
 Adapted by romeo for specific application to TouchDRO
 */
 
#include "ESP32_Quad_Encoder.h"

int32_t HighLimit = INT16_MAX;
int32_t LowLimit = INT16_MIN;

bool ESP32Encoder::attachedInterrupt = false;
int ESP32Encoder::numEncoders = 0;
//The following is an array of pointers to the individual instances of ESP32Encoder 
ESP32Encoder* ESP32Encoder::encoders[MAX_ENCODERS] = { nullptr, nullptr, nullptr, nullptr };

ESP32Encoder::ESP32Encoder() {
	type = na;
}
ESP32Encoder::ESP32Encoder(axisType axi, int aP, int bP, encType ty, pullupType uip, int fil) {
	axis = axi;
	aPinNumber = (gpio_num_t)aP;
	bPinNumber = (gpio_num_t)bP;
	type = ty;
	puType = uip;
	filter = fil;
	encoders[numEncoders++] = this;
}

/* HW pulse counters count continuously between preset high and low limits.
Interrupt is raised whenever any counter hits either limit, at which point the appropriate
(limit) value is (algebraically) added to the software counter.
Counter limit is around 32000, so interrupt rate is very low.
If more than one counter interrupts on exactly the same cycle (!) each interrupt is dealt with sequentially.
While interrupt(s) is being serviced, the counter(s) has automatically reset to zero and continues to count,
so no pulses will be missed.
At any instant, the encoder position is the sum of the software and hardware counters.
The ISR decodes which PCNT unit(s) originated (an) interrupt(s), and whether it was high or low limit.
 */
static void IRAM_ATTR pcnt_isr(void* arg) {
	ESP32Encoder* ptr;
	int intr_status = PCNT.int_st.val;
	int i;
	for (i = 0; i < MAX_ENCODERS; i++) {
		if (intr_status & (BIT(i))) {
			ptr = ESP32Encoder::encoders[i];
			if (PCNT.status_unit[i].h_lim_lat) {
				ptr->count = HighLimit + ptr->count;
			}
			if (PCNT.status_unit[i].l_lim_lat) {
				ptr->count = LowLimit + ptr->count;
			}
			PCNT.int_clr.val = BIT(i); // clear the interrupt
		}
	}
}

/* This function "attachEncoder" initialises the essential quadrature counting function of the encoders only
Error and Index management, if used, is unique to encoder types and is elsewhere
*/

void ESP32Encoder::attachEncoder(int i) {
	int index = i;
	fullQuad = type != single; //"fullQuad" is a misnomer should be "notSingle" or "fullOrHalf"
	unit = (pcnt_unit_t)index;

	//Set up the IO state of the pin
	gpio_pad_select_gpio(aPinNumber);
	gpio_pad_select_gpio(bPinNumber);
	gpio_set_direction(aPinNumber, GPIO_MODE_INPUT);
	gpio_set_direction(bPinNumber, GPIO_MODE_INPUT);
	if (puType == down) {
		gpio_pulldown_en(aPinNumber);
		gpio_pulldown_en(bPinNumber);
	}
	else {
		if (puType == up) {
			gpio_pullup_en(aPinNumber);
			gpio_pullup_en(bPinNumber);
		}
	}
	// Set up encoder PCNT configuration
	enc_config.pulse_gpio_num = aPinNumber;		//Encoder Chan A
	enc_config.ctrl_gpio_num = bPinNumber;		//Encoder Chan B

	enc_config.unit = unit;
	enc_config.channel = PCNT_CHANNEL_0;

	enc_config.pos_mode = fullQuad ? PCNT_COUNT_DEC : PCNT_COUNT_DIS; //Count Only On Rising-Edges
	enc_config.neg_mode = PCNT_COUNT_INC;

	enc_config.lctrl_mode = PCNT_MODE_KEEP;    // Rising A on HIGH B = Fwd
	enc_config.hctrl_mode = PCNT_MODE_REVERSE; // Rising A on LOW B = Rev
	enc_config.counter_h_lim = HighLimit;
	enc_config.counter_l_lim = LowLimit;

	pcnt_unit_config(&enc_config);

	if (type == full) {
		// set up second channel for full quad
		enc_config.pulse_gpio_num = bPinNumber; //make prior control into signal
		enc_config.ctrl_gpio_num = aPinNumber;    //and prior signal into control

		enc_config.unit = unit;
		enc_config.channel = PCNT_CHANNEL_1; // channel 1

		enc_config.pos_mode = PCNT_COUNT_DEC; //Count On Rising and Falling Edges
		enc_config.neg_mode = PCNT_COUNT_INC;

		enc_config.lctrl_mode = PCNT_MODE_REVERSE;    // prior high mode is now low
		enc_config.hctrl_mode = PCNT_MODE_KEEP; // prior low mode is now high

		enc_config.counter_h_lim = HighLimit;
		enc_config.counter_l_lim = LowLimit;

		pcnt_unit_config(&enc_config);

	}
	else { // make sure channel 1 is not set when not full quad
		enc_config.pulse_gpio_num = bPinNumber; //make prior control into signal
		enc_config.ctrl_gpio_num = aPinNumber;    //and prior signal into control

		enc_config.unit = unit;
		enc_config.channel = PCNT_CHANNEL_1; // channel 1

		enc_config.pos_mode = PCNT_COUNT_DIS; //disabling channel 1
		enc_config.neg_mode = PCNT_COUNT_DIS;   // disabling channel 1

		enc_config.lctrl_mode = PCNT_MODE_DISABLE;    // disabling channel 1
		enc_config.hctrl_mode = PCNT_MODE_DISABLE; // disabling channel 1

		enc_config.counter_h_lim = HighLimit;
		enc_config.counter_l_lim = LowLimit;

		pcnt_unit_config(&enc_config);
	}
	// Filter out bounces and noise
	if (filter != 0) {
		pcnt_set_filter_value(unit, filter);  // Filter Runt Pulses
		pcnt_filter_enable(unit);
	}
	else {
		pcnt_filter_disable(unit);
	};
	/* Enable events on  maximum and minimum limit values */
	pcnt_event_enable(unit, PCNT_EVT_H_LIM);
	pcnt_event_enable(unit, PCNT_EVT_L_LIM);

	pcnt_counter_pause(unit);
	pcnt_counter_clear(unit);

	/* Register ISR handler and enable interrupts for PCNT unit - but only once with first attach */
	if (ESP32Encoder::attachedInterrupt == false) {
		ESP32Encoder::attachedInterrupt = true;
		pcnt_isr_register(pcnt_isr, (void*)NULL, (int)0, NULL);
	}
	pcnt_intr_enable(unit);
	pcnt_counter_resume(unit);
}
int32_t ESP32Encoder::getCount() {
	int16_t c;
	pcnt_get_counter_value(unit, &c);
	return c + count;
}
int32_t ESP32Encoder::clearCount() {
	count = 0;
	return pcnt_counter_clear(unit);
}


