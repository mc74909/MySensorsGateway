// Enable debug prints to serial monitor
//#define MY_DEBUG

// Enable serial gateway
#define MY_GATEWAY_SERIAL

#define MY_NODE_ID 0

#include <MySensors.h>
#include <Bounce2.h>
#include <AnalogMultiButton.h>

#define DIGITAL_BUTTONS_CHILD_ID_OFFSET 0 // The buttons connected to the digital pins will be presented as of 1
#define DIGITAL_BUTTONS 47 // pins 2-48 (0-1 are used for serial communication)
unsigned int digital_button_pins[DIGITAL_BUTTONS] = {2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48};
Bounce digital_button_debouncers[DIGITAL_BUTTONS];

#define ANALOG_BUTTONS_CHILD_ID_OFFSET 100 // The buttons connected to the analog pins will be presented as of 101
// Buttons will be connected to the analog ports of a arduino mega.
#define ANALOG_PINS 11
// set how many buttons you have connected per analog pin
#define ANALOG_BUTTONS_PER_PIN 4
#define ANALOG_BUTTONS ANALOG_PINS * ANALOG_BUTTONS_PER_PIN
unsigned char analog_button_pins[ANALOG_PINS] = {A0, A1, A2, A3, /*A4, */A5, A6, A7, A8, A9, A10, A11/*, A12, A13, A14, A15*/};
const int ANALOG_BUTTONS_VALUES[ANALOG_BUTTONS_PER_PIN + 1] = {1, 230, 505, 760, 1023}; // fake 1st button because library always detected button 1, now the real buttons are 2,3,4,5
AnalogMultiButton analog_button_debouncers[ANALOG_PINS] = AnalogMultiButton(A0, ANALOG_BUTTONS_PER_PIN, ANALOG_BUTTONS_VALUES, 20, 1250); // had to initiate otherwise the compile fails

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
    // analog_buttons_per_pin + 1 because of the fake first button
    analog_button_debouncers[pin-1] = AnalogMultiButton(analog_button_pins[pin - 1], ANALOG_BUTTONS_PER_PIN + 1, ANALOG_BUTTONS_VALUES, 20, 1250); // analogResolution above 1024 to 1250 to get fool the system to think 1023 is between ~880 and 1250 to detect the last button. This is because I don't have a pull up resistor in use.
  }
}

void presentation() {
  // this gateway has 192 buttons connected via resistors to the analog ports of an arduino mega. The buttons present themselves as child ids 1 to 192
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
      // onPress button start from 1 as first button 0 is always pressed
      if (analog_button_debouncers[pin-1].onPress(button)) {
        msg.setSensor(ANALOG_BUTTONS_CHILD_ID_OFFSET+ANALOG_BUTTONS_PER_PIN * (pin - 1) + button);
        send(msg.set(1));
      }
      if (analog_button_debouncers[pin-1].onRelease(button)) {
        msg.setSensor(ANALOG_BUTTONS_CHILD_ID_OFFSET+ANALOG_BUTTONS_PER_PIN * (pin - 1) + button);
        send(msg.set(0)); 
      }
    }
  }
  wait(10);
}
