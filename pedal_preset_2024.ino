// Arduino IDE 2.3.2
// Arduino AVR Boards 1.8.6

// #include <TM1637Display.h> // https://github.com/avishorp/TM1637 1.2.0
// #include <TM1637.h> // https://github.com/AKJ7/TM1637 2.2.1
#include <TM1637TinyDisplay.h> // https://github.com/jasonacox/TM1637TinyDisplay 1.11.0
#include <JC_Button.h> // https://github.com/JChristensen/JC_Button 2.1.4
#include <USB-MIDI.h> // https://github.com/lathoub/Arduino-USBMIDI 1.1.2
#include <EEPROM.h> // Arduino AVR Boards 1.8.6
#include <CircularBuffer.hpp> // https://github.com/rlogiacco/CircularBuffer 1.4.0

// USB-MIDI
USBMIDI_CREATE_DEFAULT_INSTANCE();
CircularBuffer<byte,100> MIDI_Notes_On;

// TM1637 DISPLAY
#define CLK 6
#define DIO 5
// TM1637Display display(CLK, DIO);
// TM1637 display(CLK, DIO);
TM1637TinyDisplay display(CLK, DIO);

// BUTTONS
#define SW_MINUS 4
#define SW_PLUS 3
#define DB_TIME 200
Button Sw_Minus(SW_MINUS, DB_TIME);
Button Sw_Plus(SW_PLUS, DB_TIME);
#define LONG_PRESS_TIME 1000

//// GLOBAL VARS ////
byte Cue = 0;
byte Menu = 0;
bool connected = false;

byte Channel = 1; // Current MIDI Channel
byte Channel_Set = 1; // Menu MIDI Channel
const byte Channel_Addr = 1; // Channel Address in EEPROM

enum Types {CC, PC, Note}; // Ctrl Change, Program Change, Note On
Types Type = CC; // Current MIDI Type msg : 0=Ctl Change / 1=Prg Change / 2=Note
Types Type_Set = CC; // Menu MIDI Type
const byte Type_Addr = 2; // MIDI Type Address in EEPROM

byte CtrlNr = 127; // Current Ctrl Nr
byte CtrlNr_Set = 127; // Menu Ctrl Nr
const byte CtrlNr_Addr = 3; // Ctrl Nr Address in EEPROM

/*enum Notes {
  C_2, Cs_2, D_2, Ds_2, E_2, F_2, Fs_2, G_2, Gs_2, A_2, As_2, B_2,
  C_1, Cs_1, D_1, Ds_1, E_1, F_1, Fs_1, G_1, Gs_1, A_1, As_1, B_1,
  C0, Cs0, D0, Ds0, E0, F0, Fs0, G0, Gs0, A0, As0, B0,
  C1, Cs1, D1, Ds1, E1, F1, Fs1, G1, Gs1, A1, As1, B1,
  C2, Cs2, D2, Ds2, E2, F2, Fs2, G2, Gs2, A2, As2, B2,
  C3, Cs3, D3, Ds3, E3, F3, Fs3, G3, Gs3, A3, As3, B3,
  C4, Cs4, D4, Ds4, E4, F4, Fs4, G4, Gs4, A4, As4, B4,
  C5, Cs5, D5, Ds5, E5, F5, Fs5, G5, Gs5, A5, As5, B5,
  C6, Cs6, D6, Ds6, E6, F6, Fs6, G6, Gs6, A6, As6, B6,
  C7, Cs7, D7, Ds7, E7, F7, Fs7, G7, Gs7, A7, As7, B7,
  C8, Cs8, D8, Ds8, E8, F8, Fs8, G8
};*/
byte NoteNr = 0; // Current Note Nr
byte NoteNr_Set = 0; // Menu Note Nr
const byte NoteNr_Addr = 4; // Note Nr Address in EEPROM

// PRIORITY //
byte Loop = 0;
bool Priority = false;
#define LOW_PRIORITY_LOOPS 10

// DISPLAY //
byte Disp_Cue = 255; // // Currently displayed Cue
bool Disp_ConnLed = false; // Currently displayed
bool Disp_MidiInLed = false; // Currently displayed
bool Disp_MidiOutLed = false; // Currently displayed
bool EcoMode = false; // Current Eco Mode
bool Disp_EcoMode = false; // Currently displayed Eco Mode
String Disp = ""; // String to display

// TIMERS //
unsigned long PressRepeat = 0;
unsigned long EcoTime = 0;
unsigned long MenuTimer = 0;
unsigned long Conn_Time = 0;
unsigned long In_Blink = 0;
unsigned long Out_Blink = 0;
#define CONNECTION_DELAY 5000;
unsigned long Note_Off_Time = 0;


void setup() {
  // put your setup code here, to run once:

  // Read EEPROM
  if((EEPROM.read(Channel_Addr) >= 1) && (EEPROM.read(Channel_Addr) <= 16)) Channel = EEPROM.read(Channel_Addr);
  if(EEPROM.read(Type_Addr) <= 1) Type = EEPROM.read(Type_Addr);
  if(EEPROM.read(CtrlNr_Addr) <= 127) CtrlNr = EEPROM.read(CtrlNr_Addr);
  if(EEPROM.read(NoteNr_Addr) <= 127) NoteNr = EEPROM.read(NoteNr_Addr);
  Channel_Set = Channel;
  Type_Set = Type;
  CtrlNr_Set = CtrlNr;
  NoteNr_Set = NoteNr;
  EEPROM.end();
  
  // SETUP USB-MIDI
  MIDI.begin(MIDI_CHANNEL_OMNI);
  MIDI.setHandleNoteOff(handleNoteOff);
  MIDI.setHandleNoteOn(handleNoteOn);
  MIDI.setHandleAfterTouchPoly(handleAfterTouchPoly);
  MIDI.setHandleControlChange(handleControlChange);
  MIDI.setHandleProgramChange(handleProgramChange);
  MIDI.setHandleAfterTouchChannel(handleAfterTouchChannel);
  MIDI.setHandlePitchBend(handlePitchBend);
  MIDI.setHandleSystemExclusive(handleSystemExclusive);
  MIDI.setHandleTimeCodeQuarterFrame(handleTimeCodeQuarterFrame);
  MIDI.setHandleSongPosition(handleSongPosition);
  MIDI.setHandleSongSelect(handleSongSelect);
  MIDI.setHandleTuneRequest(handleTuneRequest);
  MIDI.setHandleClock(handleClock);
  MIDI.setHandleStart(handleStart);
  MIDI.setHandleContinue(handleContinue);
  MIDI.setHandleStop(handleStop);
  MIDI.setHandleActiveSensing(handleActiveSensing);
  MIDI.setHandleSystemReset(handleSystemReset);

  // SETUP BUTTONS
  Sw_Minus.begin();
  Sw_Plus.begin();

  // SETUP SERIAL DEBUG
  Serial.begin(115200);

  // INIT ECO
  Init_Eco();
  

  // INIT DISPLAY
  // display.init();
  display.begin();
}

void loop() {
  // put your main code here, to run repeatedly:
  if(Menu == 0) { // Normal Mode
    handleButtons();
    Display_Status();
    Display_Cue();
    handleMIDI();
    Eco();
  } else {
    handleMenu();
  }
  MIDI_Note_Off();
  handlePriority();
}

void handlePriority() {
  Loop = (Loop + 1) % LOW_PRIORITY_LOOPS;
  Priority = (Loop == 0);
}

void handleMIDI() {
  MIDI.read();
}

void handleButtons() {
  Sw_Minus.read();
  Sw_Plus.read();
  if(Sw_Minus.wasPressed()) {
    Minus();
  }
  if(Sw_Plus.wasPressed()) {
    Plus();
  }
  if(Sw_Plus.pressedFor(LONG_PRESS_TIME) && (millis() - PressRepeat > 200)) {
    Plus();
  }
  if(Sw_Minus.pressedFor(LONG_PRESS_TIME) && (Cue != 0)) {
    Cue = 0;
    MIDI.sendControlChange(CtrlNr, Cue, Channel);
    OutBlink();
  }
  if(Sw_Minus.pressedFor(LONG_PRESS_TIME) && Sw_Plus.wasPressed()) {
    Menu++;
  }
}

void Plus() {
  Cue = (Cue +1)%128;
  PressRepeat = millis();
  MIDI.sendControlChange(CtrlNr, Cue, Channel);
  OutBlink();
}

void Minus() {
  Cue = (Cue -1)%128;
  if(Cue == 255) Cue = 127;
  PressRepeat = millis();
  MIDI.sendControlChange(CtrlNr, Cue, Channel);
  OutBlink();
}

void SendCue() {
  switch(Type) {
    case CC: // Ctrl Change
      MIDI.sendControlChange(CtrlNr, Cue, Channel);
      break;
    case PC: //
      MIDI.sendProgramChange(Cue, Channel);
      break;
    case Note:
      MIDI_Notes_send(Cue);
      break;
  }
  OutBlink();
}

void MIDI_Notes_send(byte note) {
  MIDI.sendNoteOn(note, 100, Channel);
  delay(50);
  Note_Off_Time = millis() + 50;
  MIDI_Notes_On.push(note);
}

void MIDI_Note_Off() {
  if(millis() > Note_Off_Time && Priority) {
    if(MIDI_Notes_On.size() > 0) {
      byte note = MIDI_Notes_On.shift();
      MIDI.sendNoteOff(note, 100, Channel);
      delay(50);
      OutBlink();
    }
  }
}

void handleMenu() {
  Sw_Minus.read();
  Sw_Plus.read();
  if(Sw_Minus.pressedFor(LONG_PRESS_TIME) && Sw_Plus.wasPressed()) {
    Menu++;
  }
  switch(Menu) {
    case 1:
      display.showString("CHAn");
      break;
    case 2:
      display.showString("TYPE");
      break;
    case 3:
      display.showString("nbr");
      break;
    default:
      Menu = 0;
      Disp_Cue = 255;
      break;
  }

}

void Display_Cue() {
  if(Priority) {
    if(Disp_Cue != Cue) {
      //display.display(Cue, false, false, 0);
      display.showNumber(Cue, false, 3, 1);
      Disp_Cue = Cue;
      Init_Eco();
    }
  }
}

void Display_Status() {
  if(Priority) {
    uint8_t stat[] = {0};
    // Connexion Status
    connected = (millis() < Conn_Time);
    bool ConnLed = (connected || (millis() % 1000 <500));
    stat[0] = ConnLed <<3;

    // MIDI In Blink
    bool MidiInLed = (millis() < In_Blink);
    stat[0] |= MidiInLed <<6;

    // MIDI Out Blink
    bool MidiOutLed = (millis() < Out_Blink);
    stat[0] |= MidiOutLed;

    // Test display changings
    bool changed = (Disp_ConnLed != ConnLed) || (Disp_MidiInLed != MidiInLed) || (Disp_MidiOutLed != MidiOutLed);

    // Save Display setting
    Disp_ConnLed = ConnLed;
    Disp_MidiInLed = MidiInLed;
    Disp_MidiOutLed = MidiOutLed;

      
    // Display
    if(changed && (Menu == 0)) {
      display.setSegments(stat, 1, 0);
    }  
  }
}

void Init_Eco() {
  EcoTime = millis() +10000;
}

void Eco() {
  if(Priority) {
    EcoMode = (millis() > EcoTime);
    if(Disp_EcoMode != EcoMode) {
      display.setBrightness((1 - EcoMode) *0xE +1);
      display.showString("", 0, 0);
      Disp_EcoMode = EcoMode;
    }
  }
}

void inBlink() {
  In_Blink = millis() + 100;
  Conn_Time = millis() + CONNECTION_DELAY;
}

void OutBlink() {
  Out_Blink = millis() + 100;
}

void handleNoteOff(byte channel, byte note, byte velocity) {
  inBlink();
}

void handleNoteOn(byte channel, byte note, byte velocity) {
  inBlink();
  if((channel == Channel) && (Type == 2)) {
    if(!Sw_Minus.isPressed() && !Sw_Plus.isPressed()) Cue = note;
  }
}

void handleAfterTouchPoly(byte channel, byte note, byte pressure) {
  inBlink();
}

void handleControlChange(byte channel, byte number, byte value) {
  inBlink();
  if((channel == Channel) && (number == CtrlNr) && (Type == 0)) {
    if(!Sw_Minus.isPressed() && !Sw_Plus.isPressed()) Cue = value;
  }
}

void handleProgramChange(byte channel, byte number) {
  inBlink();
  if((channel == Channel) && (Type == 1)) {
    if(!Sw_Minus.isPressed() && !Sw_Plus.isPressed()) Cue = number;
  }
}

void handleAfterTouchChannel(byte channel, byte pressure) {
  inBlink();
}

void handlePitchBend(byte channel, int bend) {
  inBlink();
}

void handleSystemExclusive(byte *data, unsigned Size) {
  inBlink();
} 

void handleTimeCodeQuarterFrame(byte data) {
  inBlink();
}

void handleSongPosition(unsigned beats) {
  inBlink();
}

void handleSongSelect(byte songnumber) {
  inBlink();
}

void handleTuneRequest(void) {
  inBlink();
}

void handleClock(void) {
  //inBlink();
  Conn_Time = millis() + CONNECTION_DELAY;
}

void handleStart(void) {
  inBlink();
}

void handleContinue(void) {
  inBlink();
}

void handleStop(void) {
  inBlink();
}

void handleActiveSensing(void) {
  //inBlink();
  Conn_Time = millis() + 300;
}

void handleSystemReset(void) {
  inBlink();
}
