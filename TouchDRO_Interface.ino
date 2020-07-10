/*
	Name:       DRO_front_end.ino
	Created:	05-May-20 4:49:02 PM
	Author:     romano
	Emulates the original TouchDRO Launchpad interface (https://www.yuriystoys.com/p/android-dro.html)
	between quadrature scales and the android TouchDRO application, but permits the higher pulse rates
	required by high resolution scales. Also emulates the Tacho function (with improved resolution), and the
	Probe function.
	This version contains numerous conditionally compiled debug lines.
*/
#include "ESP32_Touch_Tacho.h"
#include "ESP32_Quad_Encoder.h"
#include <BluetoothSerial.h>

//Define Bluetooth device name. This is the name that will appear in the tablet's BT device list
#define BTString "Maximat V-10P" //Put your preferred name here 

/*Define 4 encoders here in any order. At least one active instance would be good.

Format for active instances is "ESP32Encoder(axis, aPin, bPin, Type, Pullup, Filter)"
where:
axis			One of (x,y,z,w) used to identify data transmitted by BT to TouchDRO. Any order.
				Encoder can be deactivated by assigning another encoder (lower down) to the same axis
aPin			These are the actual ESP32 GPIO Pin numbers
bPin
Encoder Type	(single, half or full quadrature). Encoder can be de-activated by making type = "na"
Pull-up/down	(up, down, none) applied to both a and b pins. Note that pins 34-39 are always "none".
filter time		Filter to reject short pulses. 0-1023 increments of 12.5nS. 0 is no filter.

Unused positions must define a null instance "ESPEncoder()"
*/

ESP32Encoder Encoder[MAX_ENCODERS] = {
ESP32Encoder(y, 32, 33, full, none, 40),  //define encoders in these lines
ESP32Encoder(x, 39, 36, full, none, 20),//....
ESP32Encoder(z, 27, 14, full, none, 1023),//....
ESP32Encoder()
};

/* Define tacho here.

Reports pulses per minute.
Pulses per rev is included as a proxy to disable the tacho, but otherwise to (potentially; not
implemented in this version) average the values over a complete rev in case multiple sensors are
not equally spaced. Algorithm works by measuring micros() between pulses,
so will give best resolution if number of sensors is small.

If used, format is  "ESP32Tacho Tacho(Pulse PinNumber, Direction Pin Number, PulsesPerRev, Pullup)"
If direction is not used, make the Pin 0 or same as pin A.
If not used, define null instance "ESP32Tacho Tacho"
If pulsesPerRev <= 0, tacho will be ignored.
Pull up/down as for encoder above.
*/

ESP32Tacho Tacho(15, 4, 1, down);
//ESP32Tacho Tacho;

//**************End of User Configuration*************************

//#define debug  //echo data to serial for debug
//#define debug2 //turn off BT to speed things up during debug
#define LEDPin 2
#define probePin 23 

#ifndef debug2
BluetoothSerial SerialBT;
#endif
int loopcounter;
int tachoReportCounter;
int stringOrder[MAX_ENCODERS]{ -1,-1,-1,-1 };//ordering of installed encoders to construct output string
String outputString;
const char axisLabel[5]{ "xyzw" };//NB inverse relationship of this and axisType enum!!
unsigned long prevMillis;
bool probeActive;

void setup()
{
#ifdef debug
#define TestPin 2
	Serial.begin(115200);
	pinMode(TestPin, OUTPUT);

	loopcounter = 0;
#endif
#ifndef debug2
	SerialBT.begin(BTString);
	pinMode(LEDPin, OUTPUT);
#endif
	pinMode(probePin, INPUT_PULLUP);
	probeActive = false;
	prevMillis = 0;
	tachoReportCounter = 0;
	// Attach installed encoders
	int i = 0, j = 0;
	for (i = 0; i < MAX_ENCODERS; i++) {
		if (Encoder[i].type != na) {
			Encoder[i].attachEncoder(i);
			Encoder[i].clearCount();
			j = (int)Encoder[i].axis;
			stringOrder[j] = i;
		}
	}
	if (Tacho.pulsesPerRev > 0) {
		Tacho.attachTacho();
	}
}
void loop() {
	//assemble dataBlock for periodic output to TouchDRO app
	//transmit in x-y-z-w-p-t order regardless of encoder assignment order. This may not be necessary.

	unsigned long currentMillis = millis();
	int probeState;
	if (currentMillis - prevMillis >= reportPeriod) {
		prevMillis += reportPeriod;
		int i = 0;
		outputString = "";
		for (i = 0; i < MAX_ENCODERS; i++) {
			outputString += axisLabel[i];
			if (stringOrder[i] == -1) {
				outputString += "0";
			}
			else {
				outputString += String(Encoder[stringOrder[i]].getCount());
			}
			outputString += ";";
		}
		if (digitalRead(probePin) == 0) {
			probeActive = true;
		}
		if (probeActive == true) {	//only report probe state after it has awoken
			probeState = digitalRead(probePin);
			outputString += ("p" + String(probeState) + ";");
		}
		if (++tachoReportCounter >= tachoReportInterval) {
			tachoReportCounter = 0;
			if (Tacho.direction == 0) {
				outputString += ("t" + String((int)Tacho.get_ppm()) + ";");
			}
			else {
				outputString += ("t-" + String((int)Tacho.get_ppm()) + ";");
			}
		}

#ifndef debug2
		//print dataBlock via BT takes around 106uS, regardless of string length
		if (SerialBT.hasClient()) {
			digitalWrite(LEDPin, HIGH);
			SerialBT.print(outputString);
		}
		else {
			digitalWrite(LEDPin, LOW);
		}
#endif
#ifdef debug	
		if (++loopcounter == 26) {
			loopcounter = 0;
			Serial.println(outputString);
		}
#endif	
		}
	}

