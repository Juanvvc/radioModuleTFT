
/* A Radio/Nav/Hdg/Baro controller for an Arduino Uno/Micro with woth encoders and a seven segments

Juan Vera, July 2020 */

// Left encoder: A:3 B:4 BUTTON:2
// Right encoder: A:12 B:11 BUTTON:10
// 7SEG (5 digits): pins:1AF2345-ED.CGB DIGITS:{5, 6, 7, 8, 9} SEGMENTS:{A5, A4, A3, A2, A1, A0, 13}

// left encoder: select mode. Click=swap
// right encoder: change current value mode. Click=toggle fast

#include <Encoder.h>
#include <SevSeg.h>

// my encoder has hardware steps of 4 internal positions. They are also used to avoid bouncing
#define ENCODER_STEP 4
// time to show the name of the current mode
#define SHOW_MODE_MILLIS 1000
// avoid button bouncing
#define BUTTON_DOWN_MILLIS 100
// max number of modes (COM1, COM1STB, COM2, COM2STB, NAV1, NAV1STB, OBS1, NAV2, NAV2STB, OBS2, ADF, HDG, BARO). The ADF is not controlled with a Swap frequency
#define MAX_MODES 13
// A shortcut to the currently selected mode
#define currentMode (knobSetMode.value)
// A shotcut to send a command Co1=123.45 to the serial line
#define sendCommand(COMMAND,VALUE) Serial.print(COMMAND); Serial.print("="); Serial.print(VALUE); Serial.println()

SevSeg sevseg;

Encoder leftEncoder(3, 4);
Encoder rightEncoder(12, 11);
long rightEncoderPos = 0;
long leftEncoderPos = 0;

// each ENCODER_STEP change this numbers of steps the currently selected value
byte speed = 0;

// manages a value
struct Knob {
  long position; // position, from 0 to steps
  int steps;     // max number of steps
  long offset;   // offset
  int factor;    // each step has this real value. Set to 0 to avoid changing the selected value
  int fast;      // a movement in the encoder changes the position this time during fast mode
  long value;    // current value=offset+position*factor
  int swap;      // swap with this (relative) mode
};

// the knob that selects modes: from 0 to MAX_MODES, fast mode is the same that slow mode
Knob knobSetMode = { 0, MAX_MODES, 0, 1, 1, 0 };
// mode names
char mode_names[][MAX_MODES] = {"co1", "co1St", "co2", "co2St", "nA1", "na1St", "rad1", "na2", "na2St", "rad2", "AdF", "hdG", "BAro"};

Knob knobAvailableData[MAX_MODES] = {
  // position, steps, offset, factor, fast, initial value(=offset+position*factor), swap (+1=next, -1=prev, 0=no swap)
  { 0, 760, 118000, 0, 0, 118000, 1 }, // COM1: from 118.000 to 136.00. No change allowed (factor=0)
  { 0, 760, 118000, 25, 40, 118000, -1 }, // COM1-STB: from 118.000 to 136.00. slow=25Khz (760 steps), fast=40*25=1000Khz
  { 0, 760, 118000, 0, 0, 118000, 1 }, // COM2: from 118.000 to 136.00. No change allowed (factor=0)
  { 0, 760, 118000, 25, 40, 118000, -1 }, // COM2-STB
  { 0, 200, 108000, 0, 0, 108000, 1 }, // NAV1: from 108.000 to 118.00. No change allowed (factor=0)
  { 0, 200, 108000, 50, 20, 108000, -1 }, // NAV1-STB: from 108.000 to 118.00. slow=50Khz (200 steps), fast=20*50=1000Khz
  { 0, 360, 0, 1, 30, 0, 0 }, // OBS1: from 0 to 360, slow=1, fast=30
  { 0, 200, 108000, 0, 0, 108000, 1 }, // NAV2: from 108.000 to 118.00. No change allowed (factor=0)
  { 0, 200, 108000, 50, 20, 108000, -1 }, // NAV2-STB: from 108.000 to 118.00. slow=50Khz (200 steps), fast=20*50=1000Khz
  { 0, 360, 0, 1, 30, 0, 0 }, // OBS2: from 0 to 360, slow=1, fast=30
  { 0, 1560, 190, 1, 100, 0, 0 }, // ADF: from 190Hz to 1750Hz, slow=1Hz, fast=100Hz
  { 0, 360, 0, 1, 30, 0, 0 }, // HDG: from 0 to 360, slow=1, fast=30
  // { 0, 120, 945, 1, 10, 945, 0 } // BARO (hPA): from 945 to 1065, slow=1, fast=10
  { 0, 315, 2790, 1, 10, 2790, 0 } // BARO (inHg): from 2790 to 3105, slow=1, fast=10
};
// the value currently selected
Knob *knobData;

// Buttons on the encoder, with anti-bouncing
struct Button {
  int pin;
  unsigned long lastDebounceTime;
  unsigned long debounceDelay;
  bool alreadyTriggered;
  int lastState;
};
Button leftButton = {2, 0, BUTTON_DOWN_MILLIS, false, 1};
Button rightButton = {10, 0, BUTTON_DOWN_MILLIS, false, 1};

void setup() {
  Serial.begin(9600);

  byte segmentPins[] = {A5, A4, A3, A2, A1, A0, 13};
  byte digitPins[] = {5, 6, 7, 8, 9};
  sevseg.begin(COMMON_CATHODE, 5, digitPins, segmentPins, true, false, false, true);
  sevseg.setBrightness(100);

  // buttons
  pinMode(leftButton.pin, INPUT_PULLUP);
  pinMode(rightButton.pin, INPUT_PULLUP);

  loadMode(0);
}

int readEncoder(Encoder *encoder, long *oldpos, int speed) {
  /* Reads the value of an encoder. The encoder is considered changed if its value changed at least ENCODER_STEP
   *  Returns: 1*speed if the encoder goes UP, -1*speed if the encoder goes DOWN, 0 if the encoder didn't change
  */

  // remember: *p++ means *(p++)
  // As a result, this line *oldpos -= ENCODER_STEP changes something else and makes the code of OTHER functions inestable!
  
  long newpos = encoder->read();  
  if(newpos == *(oldpos)) return 0;
  if(newpos - *(oldpos) >= ENCODER_STEP) {
    *(oldpos) = (*(oldpos) + ENCODER_STEP);
    return speed;
  }
  if(*(oldpos) - newpos >= ENCODER_STEP) {
    *(oldpos) = (*oldpos - ENCODER_STEP);
    return -speed;
  }
  return 0;
}

bool buttonClicked(Button *b) {
  /* Returns true on the event CLICK_DOWN */
  int reading = digitalRead(b->pin);
  if(reading != b->lastState) {
    b->lastDebounceTime = millis();
  }
  b->lastState = reading;
  if(reading == 0) {
    if((millis() - b->lastDebounceTime) > b->debounceDelay) {
      if(!b->alreadyTriggered) {
        b->alreadyTriggered = true;
        return true;
      } else {
        return false;
      }
    }
  }
  b->alreadyTriggered = false;
  return false;
}

bool updateKnob(Knob *k, int move) {
  /* Updates the position and value of a knob by moving it "move" positions.
   * If move==0, do knothing
   * value =  k->offset + k->position * k->factor;
   * 
   * Returns true if the value changed.
   */
  int newposition;
  if(move == 0 || k->factor == 0) return false;
  newposition = k->position + move;
  // check bounds
  if(newposition < 0) {
    newposition = k->steps + newposition;
  } else if(newposition >= k->steps) {
    newposition = newposition - k->steps;
  }
  k->position = newposition;
  // calculate value
  k->value = k->offset + newposition * k->factor;
  return true;
}

void setKnob(Knob *k, long value) {
  /* set a value (and its position) for a knob.
  If value = 0, do not change the knob */
  if(value != 0) {
    k->position = ( value - k->offset ) / k->factor;
    k->value = value;
  }
}

void showModeName(int mode) {
  /* Show the mode name during SHOW_MODE_MILLIS */
  sevseg.setChars(mode_names[mode]);
  for(int i=0; i<SHOW_MODE_MILLIS; i++) {
    sevseg.refreshDisplay();
    delay(1);
  }
}

void readInitialValues() {
  /* Read initial values from the serial connection.
   *  Valuas are a list of integers separated using any non-numeric number (spaces or new line, for example).
   *  The number and order is exactly the same than knobAvailableData
   *  Example: 123450 118150 123450 118150 102025 102025 0 102050 102050 180 90 1015
   */
  long newvalue;
  for(int i=0; i<MAX_MODES; i++) {
    newvalue = Serial.parseInt();
    sendCommand("read", newvalue);
    setKnob(&knobAvailableData[i], newvalue);
  }
}

void loadMode(int mode) {
  /* Loads a mode: shoe the mode name on the screen, load data and update display */
  sendCommand("mode", mode_names[mode]);
  showModeName(mode);
  knobData = &knobAvailableData[mode];
  speed = 1;
  printState(true);
}

void printState(bool alsoSend) {
  /** Send a command to the serial channel name=value and show the value on the seven segment screen.
   *  Values of more than 5 digits are trimmed (less significant values are not represented)
   *  Decimal point is not represented: only integer values are supported.
   */
  long value = knobData->value;  
  if(value>99999)
    sevseg.setNumber(value / 10);
  else
    sevseg.setNumber(value);
  if(alsoSend) {
    sendCommand(mode_names[currentMode], value);
  }
}

void changeSpeed() {
  sendCommand("speed", mode_names[currentMode]);
  if(speed == 1) {
    speed = knobData->fast;
  } else {
    speed = 1;
  }
}

void swapFreqs() {
  // swap the current mode with the swap mode, if exists
  int swap = knobData->swap;
  int swapWith = currentMode + knobData->swap;
  sendCommand("swap", mode_names[currentMode]);
  if(swap == 0) return;
  long tmp = knobData->value;
  setKnob(knobData, knobAvailableData[swapWith].value);
  printState(true);
  setKnob(&knobAvailableData[swapWith], tmp);
  sendCommand(mode_names[swapWith], tmp);
}

void loop() {
  // manage knobs
  if(updateKnob(&knobSetMode, readEncoder(&leftEncoder, &leftEncoderPos, 1))) {
    // the right knob changed: change mode
    loadMode(currentMode); 
  } else if(updateKnob(knobData, readEncoder(&rightEncoder, &rightEncoderPos, speed))) {
    // the left knob changed: update value, display and send command
    printState(true);
  }
  // check rigth button: change speed
  if(buttonClicked(&rightButton)) changeSpeed();
  // check left button: swap
  if(buttonClicked(&leftButton)) swapFreqs();
  // refresh display
  sevseg.refreshDisplay();
  // if there is available data in the serial port, read it
  if(Serial.available() > 0) {
    readInitialValues();
    printState(false);
  }
}
