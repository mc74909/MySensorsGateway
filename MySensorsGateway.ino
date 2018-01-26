/**
  The MySensors Arduino library handles the wireless radio link and protocol
  between your home built sensors/actuators and HA controller of choice.
  The sensors forms a self healing radio network with optional repeaters. Each
  repeater and gateway builds a routing tables in EEPROM which keeps track of the
  network topology allowing messages to be routed to nodes.

  Created by Henrik Ekblad <henrik.ekblad@mysensors.org>
  Copyright (C) 2013-2015 Sensnology AB
  Full contributor list: https://github.com/mysensors/Arduino/graphs/contributors

  Documentation: http://www.mysensors.org
  Support Forum: http://forum.mysensors.org

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License
  version 2 as published by the Free Software Foundation.

*******************************

  DESCRIPTION
  The ArduinoGateway prints data received from sensors on the serial link.
  The gateway accepts input on seral which will be sent out on radio network.

  The GW code is designed for Arduino Nano 328p / 16MHz

  Wire connections (OPTIONAL):
  - Inclusion button should be connected between digital pin 3 and GND
  - RX/TX/ERR leds need to be connected between +5V (anode) and digital pin 6/5/4 with resistor 270-330R in a series

  LEDs (OPTIONAL):
  - To use the feature, uncomment any of the MY_DEFAULT_xx_LED_PINs
  - RX (green) - blink fast on radio message recieved. In inclusion mode will blink fast only on presentation recieved
  - TX (yellow) - blink fast on radio message transmitted. In inclusion mode will blink slowly
  - ERR (red) - fast blink on error during transmission error or recieve crc error

*/

// Enable debug prints to serial monitor
#define MY_DEBUG

// Enable and select radio type attached
#define MY_RADIO_NRF24
//#define MY_RADIO_RFM69
#define MY_RF24_CE_PIN 49
#define MY_RF24_CS_PIN 53

// Set LOW transmit power level as default, if you have an amplified NRF-module and
// power your radio separately with a good regulator you can turn up PA level.
#define MY_RF24_PA_LEVEL RF24_PA_LOW

// Enable serial gateway
#define MY_GATEWAY_SERIAL

// Enable inclusion mode
//#define MY_INCLUSION_MODE_FEATURE
// Enable Inclusion mode button on gateway
//#define MY_INCLUSION_BUTTON_FEATURE

// Inverses behavior of inclusion button (if using external pullup)
//#define MY_INCLUSION_BUTTON_EXTERNAL_PULLUP

// Set inclusion mode duration (in seconds)
//#define MY_INCLUSION_MODE_DURATION 60
// Digital pin used for inclusion mode button
//#define MY_INCLUSION_MODE_BUTTON_PIN  2

// Set blinking period
//#define MY_DEFAULT_LED_BLINK_PERIOD 300

// Inverses the behavior of leds
//#define MY_WITH_LEDS_BLINKING_INVERSE

// Flash leds on rx/tx/err
// Uncomment to override default HW configurations
//#define MY_DEFAULT_ERR_LED_PIN 4  // Error led pin
//#define MY_DEFAULT_RX_LED_PIN  6  // Receive led pin
//#define MY_DEFAULT_TX_LED_PIN  5  // the PCB, on board LED

#include <SPI.h>
#include <MySensors.h>
#include <Bounce2.h>
#include <TimerOne.h>

#define DIGITAL_BUTTONS_CHILD_ID_OFFSET 0 // The buttons connected to the digital pins will be presented as 1-43 (on pins 11-53)
#define DIGITAL_BUTTONS 3 // pins 11-53 (0-1 are used for serial communication, 2-10 are used for the dimmers)
//unsigned int digital_button_pins[DIGITAL_BUTTONS] = {11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53};
unsigned int digital_button_pins[DIGITAL_BUTTONS] = {11, 12, 13};
MyMessage digital_button_msg[DIGITAL_BUTTONS];
Bounce digital_button_debouncers[DIGITAL_BUTTONS];
 

#define ANALOG_BUTTONS_CHILD_ID_OFFSET 100 // The buttons connected to the analog pins will be presented as 101-164
// Buttons will be connected to the analog ports of a arduino mega.
// 4 buttons will be connected per pin (through the R-2R DAC principle, http://www.tek.com/blog/tutorial-digital-analog-conversion-%E2%80%93-r-2r-dac)
#define ANALOG_PINS 16 // 4 buttons x 16 analog pins
#define ANALOG_BUTTONS 64 // 4 buttons x 16 analog pins
unsigned int analog_button_pins[ANALOG_PINS] = {A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15};
MyMessage analog_button_msg[ANALOG_BUTTONS];

#define DIMMERS_CHILD_ID_OFFSET 200 // the dimmers connected via the KRIDA board and are presented as child-sensor-id 201-208
#define DIMMERS 8
unsigned int dimmer_pins[DIMMERS] = {3, 4, 5, 6, 7, 8, 9, 10};
unsigned int dimmer_levels[DIMMERS]; // going from 5 (MAX) to 90 (LOW) and 95 (OFF)
unsigned int clock_tick; // counting the percentages (0-100) after every cross of zero in the sinus wave of AC.
unsigned int zero_cross_pin = 2;

void setup()
{
  for (int i = 1; i <= DIGITAL_BUTTONS; i++) {
    pinMode(digital_button_pins[i-1], INPUT_PULLUP);
    digital_button_msg[i-1] = MyMessage(DIGITAL_BUTTONS_CHILD_ID_OFFSET+i, V_STATUS);
    digital_button_debouncers[i-1] = Bounce();
    digital_button_debouncers[i-1].attach(digital_button_pins[i-1]);
    digital_button_debouncers[i-1].interval(5);
  }

  // configure the pins used by the buttons connected to the analog pins
  for (int i = 1; i <= ANALOG_PINS; i++) {
    pinMode(analog_button_pins[i-1], INPUT);
  }  
  for (int i = 1; i <= ANALOG_BUTTONS; i++) {
    digital_button_msg[i-1] = MyMessage(ANALOG_BUTTONS_CHILD_ID_OFFSET+i, V_STATUS);
  }

  // configure the pins used by the dimmers
  for (int i = 1; i <= DIMMERS; i++) {
    pinMode(dimmer_pins[i-1], OUTPUT);// Set AC Load pin as output
  }
  attachInterrupt(digitalPinToInterrupt(zero_cross_pin), zero_crosss_int, RISING); // every time the sinus wave of AC passes zero, the clock_tick will start from 0;
  Timer1.initialize(100); // set a timer of length 100 microseconds for 50Hz. To get to 100 slices of the sinus wave of AC.
  Timer1.attachInterrupt( timerIsr ); // attach the service routine here
}

void timerIsr()
{
  clock_tick++;

  for (int i = 0; i < DIMMERS; i++) {
    if (dimmer_levels[i] == clock_tick) { // if the current percentage (clock_tick) is the required dimming percentage then fire the dimmer.
      digitalWrite(dimmer_pins[i], HIGH); // triac firing
      delayMicroseconds(5); // triac On propogation delay
      digitalWrite(dimmer_pins[i], LOW); // triac Off
    }
  }
}

void presentation() {
  // this gateway has 192 buttons connected via resistors to the analog ports of an arduino mega. The buttons present themselves as child ids 1 to 192
  // this gateway has 8 dimmer modules connected to the digital i/o pins 3 - 12 (3 being the zero cross detection pin)
  // this gateway uses pin 2 for inclusion mode trigger
  // this gateway has a nrf24 connected to pins 49 - 53
  sendSketchInfo("MyGateway", "1.0");
  for (int sensor = 1; sensor <= DIGITAL_BUTTONS; sensor++) {
    present(DIGITAL_BUTTONS_CHILD_ID_OFFSET+sensor, S_BINARY);
  }
  for (int sensor = 1; sensor <= ANALOG_BUTTONS; sensor++) {
    present(ANALOG_BUTTONS_CHILD_ID_OFFSET+sensor, S_BINARY);
  }
  for (int sensor = 1; sensor <= DIMMERS; sensor++) {
    present(DIMMERS_CHILD_ID_OFFSET+sensor, S_DIMMER);
  }
}

void zero_crosss_int() // function to be fired at the zero crossing to dim the light
{
  // Every zerocrossing interrupt: For 50Hz (1/2 Cycle) => 10ms ; For 60Hz (1/2 Cycle) => 8.33ms
  // 10ms=10000us , 8.33ms=8330us

  clock_tick = 0;
}

//  Check if digital input has changed and send in new value
void loop()
{
  for (int i = 1; i <= DIGITAL_BUTTONS; i++) {
    if (digital_button_debouncers[i-1].update()) {
      send(digital_button_msg[i-1].set(digital_button_debouncers[i-1].read() == HIGH ? 0 : 1));
    }
  }
}

void receive(const MyMessage &message)
{
  //  Serial.println((String) message.sensor + " " + message.type);
  if ((message.sensor >= DIMMERS_CHILD_ID_OFFSET + 1) and (message.sensor <= DIMMERS_CHILD_ID_OFFSET + DIMMERS)) { // is this a message for a dimmer ?
    if (message.type == V_STATUS) {
      if (message.getBool() == 1) {
        dimmer_levels[message.sensor - DIMMERS_CHILD_ID_OFFSET - 1] = 5;
      } else {
        dimmer_levels[message.sensor - DIMMERS_CHILD_ID_OFFSET - 1] = 95;
      }
    }
    if (message.type == V_PERCENTAGE) {
      int level = message.getUInt();
      level = 100 - level;
      if (level < 5) {
        level = 5;
      }
      if (level > 90) {
        level = 90;
      }
      dimmer_levels[message.sensor - DIMMERS_CHILD_ID_OFFSET - 1] = level;
    }
  }
}

