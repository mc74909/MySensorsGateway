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

#define MY_NODE_ID 0

#include <MySensors.h>
#include <Bounce2.h>
#include <AnalogMultiButton.h>

#define DIGITAL_BUTTONS_CHILD_ID_OFFSET 0 // The buttons connected to the digital pins will be presented as of 1
#define DIGITAL_BUTTONS 47 // pins 2-48 (0-1 are used for serial communication, 49-53 are used for wireless)
unsigned int digital_button_pins[DIGITAL_BUTTONS] = {2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48};
Bounce digital_button_debouncers[DIGITAL_BUTTONS];
 

#define ANALOG_BUTTONS_CHILD_ID_OFFSET 100 // The buttons connected to the analog pins will be presented as of 101
// Buttons will be connected to the analog ports of a arduino mega.
#define ANALOG_PINS 11
// set how many buttons you have connected per analog pin
#define ANALOG_BUTTONS_PER_PIN 4
#define ANALOG_BUTTONS ANALOG_PINS * ANALOG_BUTTONS_PER_PIN
unsigned char analog_button_pins[ANALOG_PINS] = {A0, A1, A2, A3, /*A4, */A5, A6, A7, A8, A9, A10, A11/*, A12, A13, A14, A15*/};
const int ANALOG_BUTTONS_VALUES[ANALOG_BUTTONS_PER_PIN] = {256, 512, 768, 1023};
AnalogMultiButton analog_button_debouncers[ANALOG_PINS];

MyMessage msg = MyMessage(0,V_STATUS);

void setup()
{
  for (unsigned char i = 1; i <= DIGITAL_BUTTONS; i++) {
    pinMode(digital_button_pins[i-1], INPUT_PULLUP);
    digital_button_debouncers[i-1] = Bounce();
    digital_button_debouncers[i-1].attach(digital_button_pins[i-1]);
    digital_button_debouncers[i-1].interval(5);
  }

  // configure the pins used by the buttons connected to the analog pins
  for (unsigned char pin = 1; pin <= ANALOG_PINS; pin++) {
    analog_button_debouncers[pin-1] = AnalogMultiButton(analog_button_pins[pin - 1], ANALOG_BUTTONS_PER_PIN, ANALOG_BUTTONS_VALUES, 20, 1024, 1);
  }
}

void presentation() {
  // this gateway has 192 buttons connected via resistors to the analog ports of an arduino mega. The buttons present themselves as child ids 1 to 192
  // this gateway has 8 dimmer modules connected to the digital i/o pins 3 - 12 (3 being the zero cross detection pin)
  // this gateway uses pin 2 for inclusion mode trigger
  // this gateway has a nrf24 connected to pins 49 - 53
  sendSketchInfo("MarcEnChristel Gateway", "1.0");
  for (unsigned char sensor = 1; sensor <= DIGITAL_BUTTONS; sensor++) {
    present(DIGITAL_BUTTONS_CHILD_ID_OFFSET+sensor, S_BINARY);
  }
  for (unsigned char sensor = 1; sensor <= ANALOG_BUTTONS; sensor++) {
    present(ANALOG_BUTTONS_CHILD_ID_OFFSET+sensor, S_BINARY);
  }
}

//  Check if digital input has changed and send in new value
void loop()
{
  for (unsigned char i = 1; i <= DIGITAL_BUTTONS; i++) {
    if (digital_button_debouncers[i-1].update()) {
      msg.setSensor(DIGITAL_BUTTONS_CHILD_ID_OFFSET+i);
      send(msg.set(digital_button_debouncers[i-1].read() == HIGH ? 0 : 1));
    }
  }
  for (unsigned char pin = 1; pin <= ANALOG_PINS; pin++) {
    analog_button_debouncers[pin-1].update();
    for (unsigned char button = 1; button <= ANALOG_BUTTONS_PER_PIN; button++) {
      if (analog_button_debouncers[pin-1].onPress(button-1)) {
        msg.setSensor(ANALOG_BUTTONS_CHILD_ID_OFFSET+ANALOG_BUTTONS_PER_PIN * (pin - 1) + button);
        send(msg.set(1));
      }
      if (analog_button_debouncers[pin-1].onRelease(button-1)) {
        msg.setSensor(ANALOG_BUTTONS_CHILD_ID_OFFSET+ANALOG_BUTTONS_PER_PIN * (pin - 1) + button);
        send(msg.set(0));        
      }
    }
//    delay(5);  # don't know what this was for
  }
  delay(10);
}

