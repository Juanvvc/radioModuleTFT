
/* A Radio/Nav/Hdg/Baro controller for an Arduino Uno/Micro with woth encoders and a the TFT shield
Juan Vera, July 2020 */


#include <TFT.h> // Hardware-specific library
#include <SPI.h>
#include <Encoder.h>

///////////////////////////// PINS

// I'm using the now retired TFT shield: https://www.arduino.cc/en/Guide/TFT
#define TFT_CS   10
#define TFT_DC   9
#define TFT_RESET  8
// This TFT uses SPI. At the end, the TFT shield it uses pins 8-13 and 4 on an Arduino UNO
// On an Arduino Micro, some of the SPI pins are accessed through the ICSP interface.
// Documentation: https://www.arduino.cc/en/Guide/TFTtoBoards
#define LEFT_ENCODER_BUTTON A3
#define LEFT_ENCODER_A A5
#define LEFT_ENCODER_B A4
#define RIGHT_ENCODER_BUTTON A2
#define RIGHT_ENCODER_A A0
#define RIGHT_ENCODER_B A1


/////////////////////////////// THEMES
// My TFT only has 16 colors, even if its channels can be configured 0-255. Values are scaled automatically by the library.

#define LINE1_ABS 1
#define LINE1_OFFSET2 7
#define LINE1_OFFSET3 4
#define LINE2_ABS 31
#define LINE2_OFFSET2 37
#define LINE2_OFFSET3 34
#define LINE3_ABS 67
#define LINE3_OFFSET2 73
#define LINE3_OFFSET3 70
#define LINE4_ABS 97
#define LINE4_OFFSET2 103
#define LINE4_OFFSET3 100
#define LINE_HEIGHT 30
#define COLUMN1 0
#define COLUMN1_OFFSET 2
#define COLUMN2 50
#define COLUMN2_OFFSET 52
#define SCREEN_WIDTH myScreen.width()

// Blue theme
#define BACKGROUND 0,0,0
#define PANEL_BK 0,0,255
#define PANEL_COLOR 255,255,255
#define PANEL_BORDER 255,255,255
#define PANEL_SELBK 255,255,255
#define PANEL_SELCO 0,0,0

// Green theme
//#define BACKGROUND 0,0,0
//#define PANEL_BK 0,150,0
//#define PANEL_COLOR 255,255,255
//#define PANEL_BORDER 255,255,255
//#define PANEL_SELBK 255,255,255
//#define PANEL_SELCO 0,0,0

// Red theme
//#define BACKGROUND 0,0,0
//#define PANEL_BK 0,0,0
//#define PANEL_COLOR 255,200,0
//#define PANEL_BORDER 255,255,255
//#define PANEL_SELBK 255,255,255
//#define PANEL_SELCO 0,0,0

#define COLUMN2_WIDTH 110

////////////////////////////////////// Encoders and buttons

// my encoder has hardware stops every 4 internal positions. They are also used to avoid bouncing
#define ENCODER_STEP 4
// avoid button bouncing
#define BUTTON_DOWN_MILLIS 100
// max number of modes (COM1, COM1STB, COM2, COM2STB, NAV1, NAV1STB, OBS1, NAV2, NAV2STB, OBS2, ADF, TRANS, HDG, BARO). The ADF is not controlled with a Swap frequency
#define MAX_MODES 14
// A shotcut to send a command Co1=123.45 to the serial line
#define sendCommand(COMMAND,VALUE) Serial.print(COMMAND); Serial.print("="); Serial.print(VALUE); Serial.println();


//////////////////////////////// VARIABLES

TFT myScreen = TFT(TFT_CS, TFT_DC, TFT_RESET);

Encoder leftEncoder(LEFT_ENCODER_B, LEFT_ENCODER_A);
Encoder rightEncoder(RIGHT_ENCODER_B, RIGHT_ENCODER_A);
long rightEncoderPos = 0;
long leftEncoderPos = 0;

// each ENCODER_STEP change this numbers of steps the currently selected value
byte speed = 0;

// Buttons on the encoder, with anti-bouncing
struct Button {
  int pin;
  unsigned long lastDebounceTime;
  unsigned long debounceDelay;
  bool alreadyTriggered;
  int lastState;
};
Button leftButton = {LEFT_ENCODER_BUTTON, 0, BUTTON_DOWN_MILLIS, false, 1};
Button rightButton = {RIGHT_ENCODER_BUTTON, 0, BUTTON_DOWN_MILLIS, false, 1};


// manages a value
struct Value {
  char name[6];  // Name. Notice only 5 characters are available
  long position; // position, from 0 to steps
  unsigned long steps;     // max number of steps
  long offset;   // offset for position=0
  unsigned long factor;    // each step has this effect on value. Set to 0 to avoid changing the selected value
  int fast;      // a movement in the encoder changes the position this time during fast mode
  long value;    // current value=offset+position*factor
  int swap;      // swap with this (relative) mode
};

// current mode, from 0 to MAX_NODES
// some modes are not possible: 0, 2, 4, 7: they are NOT editable COM1/COM2/NAV1/NAV2 frequencies
// see setMode() and selectNextMode()
int currentMode;

Value availableValues[MAX_MODES] = {
  // name, position, steps, offset, factor, fast, value(the initialization value is NOT calculated automatically. Set value=offset+position*factor), swap (+1=next, -1=prev, 0=no swap)
  { "CO1", 0, 760, 118000, 0, 0, 118000, 1 }, // COM1: from 118.000 to 136.00. No change allowed (factor=0)
  { "CO1S", 0, 760, 118000, 25, 40, 118000, -1 }, // COM1-STB: from 118.000 to 136.00. slow=25Khz (760 steps), fast=40*25=1000Khz
  { "CO2", 380, 760, 118000, 0, 0, 127500, 1 }, // COM2: from 118.000 to 136.00. No change allowed (factor=0)
  { "CO2S", 380, 760, 118000, 25, 40, 127500, -1 }, // COM2-STB: from 118.000 to 136.00. slow=25Khz (760 steps), fast=40*25=1000Khz
  { "NA1", 0, 200, 108000, 0, 0, 108000, 1 }, // NAV1: from 108.000 to 118.00. No change allowed (factor=0)
  { "NA1S", 0, 200, 108000, 50, 20, 108000, -1 }, // NAV1-STB: from 108.000 to 118.00. slow=50Khz (200 steps), fast=20*50=1000Khz
  { "OBS1", 0, 360, 0, 1, 30, 0, 0 }, // OBS1: from 0 to 360, slow=1, fast=30
  { "NA2", 100, 200, 108000, 0, 0, 113000, 1 }, // NAV2: from 108.000 to 118.00. No change allowed (factor=0)
  { "NA2S", 100, 200, 108000, 50, 20, 113000, -1 }, // NAV2-STB: from 108.000 to 118.00. slow=50Khz (200 steps), fast=20*50=1000Khz
  { "OBS2", 180, 360, 0, 1, 30, 180, 0 }, // OBS2: from 0 to 360, slow=1, fast=30
  { "ADF", 0, 1560, 190, 1, 100, 190, 0 }, // ADF: from 190Hz to 1750Hz, slow=1Hz, fast=100Hz
  { "TRANS", 0, 10000, 0, 1, 1000, 0, 0 }, // TRANSPONDER: from 0 to 9999, slow=1, fast=1000
  { "HDG", 0, 360, 0, 1, 30, 0, 0 }, // HDG: from 0 to 360, slow=1, fast=30
  // { "BARO", 0, 120, 945, 1, 10, 945, 0 } // BARO (hPA): from 945 to 1065, slow=1, fast=10
  { "BARO", 0, 315, 2790, 1, 10, 2790, 0 } // BARO (inHg): from 2790 to 3105, slow=1, fast=10
};
// the value currently selected
Value *currentValue;

bool buttonClicked(Button *b) {
  /* Returns true only on the event CLICK_DOWN */
  int reading = digitalRead(b->pin);
  if (reading != b->lastState) {
    b->lastDebounceTime = millis();
  }
  b->lastState = reading;
  if (reading == 0) {
    if ((millis() - b->lastDebounceTime) > b->debounceDelay) {
      if (!b->alreadyTriggered) {
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

bool updateValue(Value *k, int move) {
  /* Updates the position and value of a knob by moving it "move" positions.
     If move==0, do nothing
     new value =  k->offset + k->position * k->factor;

     Returns true if the value changed.
  */
  long newposition;
  if (move == 0 || (k->factor) == 0) return false;
  newposition = (k->position) + move;
  // check bounds. TODO: maybe MOD would be a better solution
  if (newposition < 0) {
    newposition = (k->steps) + newposition;
  } else if (newposition >= (k->steps)) {
    newposition = newposition - (k->steps);
  }
  (k->position) = newposition;
  // calculate value
  (k->value) = (k->offset) + newposition * (k->factor);
  return true;
}

void setup() {
  Serial.begin(9600);

  Serial.println("status=setup");

  myScreen.begin();
  myScreen.background(0, 0, 0); // clear the screen
  myScreen.stroke(255, 255, 255);

  // buttons
  pinMode(leftButton.pin, INPUT_PULLUP);
  pinMode(rightButton.pin, INPUT_PULLUP);

  // load the first editable mode
  selectNextEditableMode(1);
}

void drawGenericPanel(Value *value1, Value *value2, Value *value3, Value *value4, boolean focusOnFirst, boolean complete) {
  /* Draws a panel with 4 values in two groups.
     A group: two values that can be swapped (for example: COM1, COM1-STB)
     If value2(value4) are not zero: assume value2(value4) is editable.
     If value2(value4) are zero: assume value1(value3) is editable. Draw the value using two lines (it is nicer)
     If focusOnFirst, the first group is currently selected
     If complete, redraw everything. If not, redraw only selection
  */
  char buffer[6];

  if (complete) {
    // groups background
    myScreen.stroke(PANEL_BORDER);
    myScreen.fill(PANEL_BK); myScreen.rect(COLUMN1, LINE1_ABS, SCREEN_WIDTH, LINE_HEIGHT * 2);
    myScreen.fill(PANEL_BK); myScreen.rect(COLUMN1, LINE3_ABS, SCREEN_WIDTH, LINE_HEIGHT * 2);

    myScreen.setTextSize(2);
    myScreen.stroke(PANEL_COLOR);
    myScreen.text(value1->name, COLUMN1_OFFSET, LINE1_OFFSET2);
    myScreen.text(value3->name, COLUMN1_OFFSET, LINE3_OFFSET2);

    myScreen.setTextSize(3);
    if (value2 != 0) {
      ltoa(value1->value, buffer, 10); myScreen.text(buffer, COLUMN2_OFFSET, LINE1_OFFSET3);
      ltoa(value2->value, buffer, 10); myScreen.text(buffer, COLUMN2_OFFSET, LINE2_OFFSET3);
    } else {
      ltoa(value1->value, buffer, 10); myScreen.text(buffer, COLUMN2_OFFSET, LINE2_OFFSET3);
    }
    if (value4 != 0) {
      ltoa(value3->value, buffer, 10); myScreen.text(buffer, COLUMN2_OFFSET, LINE3_OFFSET3);
      ltoa(value4->value, buffer, 10); myScreen.text(buffer, COLUMN2_OFFSET, LINE4_OFFSET3);
    } else {
      ltoa(value3->value, buffer, 10); myScreen.text(buffer, COLUMN2_OFFSET, LINE4_OFFSET3);
    }
  }

  if (focusOnFirst) {
    if (value2 == 0) {
      ltoa(value1->value, buffer, 10); updateSelection(buffer, PANEL_SELBK, PANEL_SELCO, LINE2_ABS, LINE2_OFFSET3);
    } else {
      ltoa(value2->value, buffer, 10); updateSelection(buffer, PANEL_SELBK, PANEL_SELCO, LINE2_ABS, LINE2_OFFSET3);
    }
  } else {
    if (value4 == 0) {
      ltoa(value3->value, buffer, 10); updateSelection(buffer, PANEL_SELBK, PANEL_SELCO, LINE4_ABS, LINE4_OFFSET3);
    } else {
      ltoa(value4->value, buffer, 10); updateSelection(buffer, PANEL_SELBK, PANEL_SELCO, LINE4_ABS, LINE4_OFFSET3);
    }
  }
}

void updateSelection(char *buffer, byte br, byte bg, byte bb, int sr, int sg, int sb, int y, int yt) {
  myScreen.fill(br, bg, bb); myScreen.rect(COLUMN2, y, COLUMN2_WIDTH, LINE_HEIGHT);
  myScreen.stroke(sr, sg, sb); myScreen.text(buffer, COLUMN2_OFFSET, yt);
}

void drawPanel(bool complete) {
  /** Draw the panel corresponding to the currently selected mode
      Some modes are not possible: 0, 2, 4, 7: they are NOT editable COM1/COM2/NAV1/NAV2 frequencies
      If complete, redraw everything. If not, redraw only selection
  */
  switch (currentMode) {
    // COM1/COM2, COM1-STB selected
    case 1: drawGenericPanel(&availableValues[0], &availableValues[1], &availableValues[2], &availableValues[3], true, complete); break;
    // COM1/COM2, COM2-STB selected
    case 3: drawGenericPanel(&availableValues[0], &availableValues[1], &availableValues[2], &availableValues[3], false, complete); break;
    // NAV1/OBS1, NAV1-STB selected
    case 5: drawGenericPanel(&availableValues[4], &availableValues[5], &availableValues[6], 0, true, complete); break;
    // NAV1/OBS1, OBS1 selected
    case 6: drawGenericPanel(&availableValues[4], &availableValues[5], &availableValues[6], 0, false, complete); break;
    // NAV2/OBS2, NAV2-STB selected
    case 8: drawGenericPanel(&availableValues[7], &availableValues[8], &availableValues[9], 0, true, complete); break;
    // NAV2/OBS2, OBS2 selected
    case 9: drawGenericPanel(&availableValues[7], &availableValues[8], &availableValues[9], 0, false, complete); break;
    // ADF/TRANS. ADF selected
    case 10: drawGenericPanel(&availableValues[10], 0, &availableValues[11], 0, true, complete); break;
    // ADF/TRANS. TRANS selected
    case 11: drawGenericPanel(&availableValues[10], 0, &availableValues[11], 0, false, complete); break;
    // HDG/BARO. HDG selected
    case 12: drawGenericPanel(&availableValues[12], 0, &availableValues[13], 0, true, complete); break;
    // HDG/BARO. BARO selected
    case 13: drawGenericPanel(&availableValues[12], 0, &availableValues[13], 0, false, complete); break;
    default: sendCommand("error", "bad mode");
  }
}

void loadMode(int mode) {
  /* Loads a mode: show the mode name on the screen, load data and update display */
  currentMode = mode;
  currentValue = &availableValues[mode];
  sendCommand(F("mode"), currentValue->name);
  speed = 1;
  drawPanel(true);
}

void selectNextEditableMode(int direction) {
  /* Loads the next editable mode in a direction */
  if (direction == 0) return;
  int cm = currentMode;
  int nm = (cm + direction) % MAX_MODES;
  if (nm < 0) nm = MAX_MODES + nm; // notice nm is negative
  while ((&availableValues[nm])->factor == 0) {
    nm = (nm + direction) % MAX_MODES;
    if (nm < 0) nm = MAX_MODES + nm; // notice nm is negative
  }
  loadMode(nm);
}

int readEncoder(Encoder *encoder, long *oldpos, int speed) {
  /* Reads the value of an encoder. The encoder is considered changed if its value changed at least ENCODER_STEP
      Returns: 1*speed if the encoder goes UP, -1*speed if the encoder goes DOWN, 0 if the encoder didn't change
  */

  // remember: *p++ means *(p++)
  // As a result, this line *oldpos -= ENCODER_STEP changes something else and makes the code of OTHER functions inestable!

  long newpos = encoder->read();
  if (newpos == *(oldpos)) return 0;
  if (newpos - * (oldpos) >= ENCODER_STEP) {
    *(oldpos) = (*(oldpos) + ENCODER_STEP);
    return speed;
  }
  if (*(oldpos) - newpos >= ENCODER_STEP) {
    *(oldpos) = (*oldpos - ENCODER_STEP);
    return -speed;
  }
  return 0;
}

void setValue(Value *k, long value) {
  /* set a value (and its position) for a Value.
    If value = 0, do not change anything */
  if (value != 0) {
    k->position = ( value - k->offset ) / k->factor;
    k->value = value;
  }
}

void changeSpeed() {
  sendCommand("speed", currentValue->name);
  if (speed == 1) {
    speed = currentValue->fast;
  } else {
    speed = 1;
  }
}

void swapFreqs() {
  // swap the current mode with the swap mode, if exists
  int swap = currentValue->swap;
  int swapWith = currentMode + currentValue->swap;
  sendCommand("swap", currentValue->name);
  if (swap == 0) return;
  long tmp = currentValue->value;
  setValue(currentValue, availableValues[swapWith].value);
  setValue(&availableValues[swapWith], tmp);
  sendCommand(currentValue->name, currentValue->value);
  sendCommand(availableValues[swapWith].name, tmp);
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
    setValue(&availableValues[i], newvalue);
  }
}

void loop() {
  // if the left encoder changes, change the mode. Notice selectNextEditableMode does nothing if readEncoder() returns 0
  selectNextEditableMode(readEncoder(&leftEncoder, &leftEncoderPos, 1));

  if (updateValue(currentValue, readEncoder(&rightEncoder, &rightEncoderPos, speed))) {
    sendCommand(currentValue->name, currentValue->value);
    // for some reason, sometimes, serial needs to send a number before updating the display or the previous value is not understood correctly!
    // this is not necessary if the display is not updated. I don't know why
    // Serial.println(currentMode);
    drawPanel(false);
  }

  // check rigth button: change speed
  if (buttonClicked(&rightButton)) {
    changeSpeed();
  }
  // check left button: swap
  if (buttonClicked(&leftButton)) {
    swapFreqs();
    drawPanel(true);
  }

  // if there is available data in the serial port, read it
  if(Serial.available() > 0) {
    readInitialValues();
    drawPanel(true);
  }
}
