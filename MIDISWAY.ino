/*
  MIDISWAY
  MIDI Analog Wow Simulator
  by Hugo Swift

  This program is a wow effect implemented via MIDI rather than audio
  by directly modifying the pitchbend parameter of a target keyboard or synthesiser.
  The effect aims to simulate the pitch variation on vinyl or tape due to eccentricity
  in the spools.

  The code sends MIDI messages to an external MIDI device that modifies its pitch with
  a sine wave, the frequency and amplitude of which are controlled by two potentiometers.
  A switch turns the effect on or off and an LED gently pulses at the speed of the sine
  wave when the effect is turned on.

  The limitation of this code is that it directly affects the pitchbend control of the
  target device, meaning you are not able to use the pitch wheel while the effect is on.

  Wiring:

  MIDI port
    Pin 5 - 220ohm resistor - Digital pin 1
    Pin 2 - GND
    Pin 4 - 220ohm resistor - +5V
  10k linear pot
    Left pin - +5V
    Centre pin - Analog pin 6
    Right pin - GND
  10k linear pot with integrated switch (these can also be separate parts)
    Left pin - GND
    Centre pin - Analog pin 7
    Right pin - +5V
    GND - Switch - Digital pin 2
  GND - LED - Digital pin 3
  Optional: GND - DC barrel jack - Vin
  
  Find out more about this project at hugoswift.co.uk  
  
  This work is licensed under the Creative Commons Attribution-ShareAlike 4.0 International License:
  https://creativecommons.org/licenses/by-sa/4.0/
*/

// Instantiate the variables used throughout the program.
int i;
const int tableSize = 256;

const int switchPin = 2;
const int ledPin = 3;
const int ratePin = A6;
const int depthPin = A7;

// Sets the range of values possible for the rate in microseconds. 1000 - 10000 microseconds is approximately equal to 4hz - 40hz.
int rateMin = 1000;
int rateMax = 10000;

// Sets the strength of the effect at full. Enter a value between 0 and 1.
float effectStrength = 0.4;

// Sets the range of values possible for the pitch bend effect. 0 is no effect and 16383 is max effect.
int depthMin = 8192 - ((effectStrength * 8192));
int depthMax = 8192 + ((effectStrength * 8192) -1);

int rate;
int depth;
int change;

boolean previousState;
boolean state;

unsigned char low;
unsigned char high;

/* This is a lookup table for a sine wave. It is important that the values center around
   zero as they will be multiplied by the depth value later on.
   The lookup table was generated using the Sine Look Up Table Generator Calculator by
   Daycounter Inc. Engineering Services:
   http://www.daycounter.com/Calculators/Sine-Generator-Calculator.phtml */
static int sinTable [256] = {
  0, 3, 6, 9, 12, 15, 18, 21,
  24, 27, 30, 34, 37, 39, 42, 45,
  48, 51, 54, 57, 60, 62, 65, 68,
  70, 73, 75, 78, 80, 83, 85, 87,
  90, 92, 94, 96, 98, 100, 102, 104,
  106, 107, 109, 110, 112, 113, 115, 116,
  117, 118, 120, 121, 122, 122, 123, 124,
  125, 125, 126, 126, 126, 127, 127, 127,
  127, 127, 127, 127, 126, 126, 126, 125,
  125, 124, 123, 122, 122, 121, 120, 118,
  117, 116, 115, 113, 112, 110, 109, 107,
  106, 104, 102, 100, 98, 96, 94, 92,
  90, 87, 85, 83, 80, 78, 75, 73,
  70, 68, 65, 62, 60, 57, 54, 51,
  48, 45, 42, 39, 37, 34, 30, 27,
  24, 21, 18, 15, 12, 9, 6, 3,
  0, -4, -7, -10, -13, -16, -19, -22,
  - 25, -28, -31, -35, -38, -40, -43, -46,
  - 49, -52, -55, -58, -61, -63, -66, -69,
  - 71, -74, -76, -79, -81, -84, -86, -88,
  - 91, -93, -95, -97, -99, -101, -103, -105,
  - 107, -108, -110, -111, -113, -114, -116, -117,
  - 118, -119, -121, -122, -123, -123, -124, -125,
  - 126, -126, -127, -127, -127, -128, -128, -128,
  - 128, -128, -128, -128, -127, -127, -127, -126,
  - 126, -125, -124, -123, -123, -122, -121, -119,
  - 118, -117, -116, -114, -113, -111, -110, -108,
  - 107, -105, -103, -101, -99, -97, -95, -93,
  - 91, -88, -86, -84, -81, -79, -76, -74,
  - 71, -69, -66, -63, -61, -58, -55, -52,
  - 49, -46, -43, -40, -38, -35, -31, -28,
  - 25, -22, -19, -16, -13, -10, -7, -4,
};

void setup() {
  //  Set the MIDI serial speed.
  Serial.begin(31250);

  //Assign pin modes
  pinMode(switchPin, INPUT_PULLUP);
  pinMode(ledPin, OUTPUT);
  pinMode(ratePin, INPUT);
  pinMode(depthPin, INPUT);
}

void loop() {
  // Reads the state of the inputs and maps the analog values to useful ranges.
  state = digitalRead(switchPin);
  rate = map(analogRead(ratePin), 0, 1023, rateMin, rateMax);
  depth = map(analogRead(depthPin), 0, 1023, 0, 255);
  /* Multiplies the depth value by the current value of the sin lookup table and maps it
     to a 14 bit integer required for the pitchbend MIDI message. The table will be
     iterated through as the loop() repeats. */
  change = map(depth * sinTable[i], -32768, 32767, depthMin, depthMax);

  /* If the switch has been turned off, set the pitchbend value to 8192 (zero change) and
     turn off the LED. This only needs to be done once when the switch is toggled to prevent
     a stream of zero-change messages being sent. This allows the pitch wheel to be used when
     the effect is off. */
  if (state == HIGH && previousState == LOW) {
    sendPitch(8192);
    analogWrite(ledPin, 0);
  }

  // If the switch is on, send a pitchbend message with the current depth value and change the LED brightness.
  else if (state == LOW) {
    sendPitch(change);
    analogWrite(ledPin, map(change, depthMin, depthMax, 0, 255));
  }

  // Delay the loop from repeating by the preset amount of time to control rate of advancement through the sine table.
  delayMicroseconds(rate);

  // Move to the next value in the sine lookup table.
  i++;
  // Once the end of the table is reached, loop back to the first value.
  if (i == tableSize)
    i = 0;

  // Reset the switch state.
  previousState = state;
}

// Send the pitchbend data in MIDI protocol format over Serial.
void sendPitch(int change) {
  // Splits the 14 bit change value into the least and most significant bits; the format required by the MIDI protocol for pitchbend.
  low = change & 0x7F;
  high = (change >> 7) & 0x7F;
  // Sends the MIDI message for controlling the pitch bend parameter in hexadecimal.
  Serial.write(0xE0);
  Serial.write(low);
  Serial.write(high);
}
