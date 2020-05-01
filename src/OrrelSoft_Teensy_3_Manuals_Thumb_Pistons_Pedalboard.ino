#include <MIDI.h> // MIDI v4.2
#include <SPI.h>

// Manuals are placed in order Choir, Great, Swell, Solo on console
#define MIDI1_CHANNEL 1 // MIDI channel for choir manual (lowest)
#define MIDI2_CHANNEL 2 // MIDI channel for great manual (middle)
#define MIDI3_CHANNEL 3 // MIDI channel for swell manual (upper)

// User defined settings
//#define DEBUG 1
#define NUM_INPUTS 64  // 32*3: 32 for thumb pistons & stops, 32 for AGO Pedalboard C2-G4, 32 for toe pistons 

// Teensy Pins
#define LATCHPIN PIN_SPI_SS // 10

// General settings
#define NUM_REGISTERS (NUM_INPUTS / 8)
static const unsigned long Btn_Debounce = 100; // 100 milliseconds

// General MIDI Settings
#define MIDI_NOTE_ON_VELOCITY 127

// Thumb Piston Settings (1st Shift Register)
#define THUMB_PISTON_MIDI_CHANNEL 8
#define THUMB_PISTON_START_REGISTER 0
#define THUMB_PISTON_END_REGISTER 3

// Pedal Manual settings (2nd Shift Register)
#define PEDAL_MANUAL_MIDI_CHANNEL 4
#define PEDAL_MANUAL_MIDI_NOTE_OFFSET 36  // MIDI note 36 = C2
#define PEDAL_MANUAL_START_REGISTER 4
#define PEDAL_MANUAL_END_REGISTER 7

// Toe Piston Settings (3rd Shift Register)
#define TOE_PISTON_MIDI_CHANNEL 8
#define TOE_PISTON_START_REGISTER 8
#define TOE_PISTON_END_REGISTER 11

// Pedal Settings
#define PEDAL_MIDI_CHANNEL 8
#define PEDAL_PIN 0 // Pin array position
#define PEDAL_CTRL_NUM 1 // Control number array position
#define PEDAL_ACTIVE 2 // Pedal state array position
// Pedal pins
#define PEDAL_CRESCENDO_PIN PIN_A5 // 19
#define PEDAL_EXPRESSION_PIN PIN_A4 // 18
#define PEDAL_VOLUME_PIN PIN_A3 // 17
// MIDI Control Change numbers for pedals
#define CRESCENDO_CTRL_NUM 16 // General Purpose Controller 1
#define EXPRESSION_CTRL_NUM 11  // Expression Controller
#define VOLUME_CTRL_NUM 7  // Channel Volume (formerly Main Volume)
// Pedal state ON=true/OFF=false
#define CRESCENDO_ACTIVE false // General Purpose Controller 1
#define EXPRESSION_ACTIVE true  // Expression Controller
#define VOLUME_ACTIVE false  // Channel Volume (formerly Main Volume)

// Pedal configuration array
static int pedal[3][3] = {
  {PEDAL_CRESCENDO_PIN, CRESCENDO_CTRL_NUM, CRESCENDO_ACTIVE},
  {PEDAL_EXPRESSION_PIN, EXPRESSION_CTRL_NUM, EXPRESSION_ACTIVE},
  {PEDAL_VOLUME_PIN, VOLUME_CTRL_NUM, VOLUME_ACTIVE}
};
int pedalState[3];  // Hold the current value of the pedal 0-127

static const unsigned ledPin = LED_BUILTIN;

// Variables for shift registers
byte midiState[NUM_REGISTERS];    // Hold last register state HIGH=OFF
byte switchState[NUM_REGISTERS];  // Hold shift register state HIGH=OFF
unsigned long lastSwitch = 0;     // Holds last switch read time for debouncing switches

MIDI_CREATE_INSTANCE( HardwareSerial, Serial1, midi1 );  // Choir manual RX0 (0)
MIDI_CREATE_INSTANCE( HardwareSerial, Serial2, midi2 );  // Great manual RX1 (9)
MIDI_CREATE_INSTANCE( HardwareSerial, Serial3, midi3 );  // Swell manual RX2 (7)

// Handlers to relay Note On/Off messages from manuals into USB MIDI channels
void NoteOn1( byte channel, byte note, byte velocity ) {
  usbMIDI.sendNoteOn( note, velocity, MIDI1_CHANNEL );
  #if defined(DEBUG)
  Serial.println(String("1-Note On:  ch=") + channel + ", note=" + note + ", velocity=" + velocity);
  #endif
}

void NoteOff1( byte channel, byte note, byte velocity ) {
  usbMIDI.sendNoteOff( note, velocity, MIDI1_CHANNEL );
}

void NoteOn2( byte channel, byte note, byte velocity ) {
  usbMIDI.sendNoteOn( note, velocity, MIDI2_CHANNEL );
  #if defined(DEBUG)
  Serial.println(String("2-Note On:  ch=") + channel + ", note=" + note + ", velocity=" + velocity);
  #endif
}

void NoteOff2( byte channel, byte note, byte velocity ) {
  usbMIDI.sendNoteOff( note, velocity, MIDI2_CHANNEL );
}

void NoteOn3( byte channel, byte note, byte velocity ) {
  usbMIDI.sendNoteOn( note, velocity, MIDI3_CHANNEL );
  #if defined(DEBUG)
  Serial.println( String( "3-Note On:  ch=" ) + channel + ", note=" + note + ", velocity=" + velocity );
  #endif
}

void NoteOff3( byte channel, byte note, byte velocity ) {
  usbMIDI.sendNoteOff( note, velocity, MIDI3_CHANNEL );
}

void setup() {
  
  // Initiate MIDI communications
  // Listen to all channels but only handle NoteOn/Off messages from manuals
  midi1.begin( MIDI_CHANNEL_OMNI );
  midi1.setHandleNoteOn( NoteOn1 );
  midi1.setHandleNoteOff( NoteOff1 );
  
  midi2.begin( MIDI_CHANNEL_OMNI );
  midi2.setHandleNoteOn( NoteOn2 );
  midi2.setHandleNoteOff( NoteOff2 );
  
  midi3.begin( MIDI_CHANNEL_OMNI );
  midi3.setHandleNoteOn( NoteOn3 );
  midi3.setHandleNoteOff( NoteOff3 );

  analogReadRes( 10 ); // Read 0-1023 8,
  analogReadAveraging( 8 );  // 16,8,4
  
  delay( 200 );
  
  // Initialize last switch state array
  for ( byte i = 0; i < NUM_REGISTERS; i++ ) {
    midiState[i] = 0;  // 00000000
  }
  
  #if defined(DEBUG)
  Serial.begin( 57600 );
  Serial.println( "Begin" );
  #endif

  pinMode( LATCHPIN, OUTPUT );
  digitalWrite( LATCHPIN, HIGH ); // Don't load inputs
  SPI.begin();
}

void loop() {
  // Handle MIDI inputs from manuals
  midi1.read();
  midi2.read();
  midi3.read();

  // Read shift registers for pistons & pedalboard
  readShiftRegisters();  // Read current switch positions

  // Read pedal positions
  readPedals(); // Read analog values on pedal potentiometers
}

// A function that prints all the 1's and 0's of a byte, so 8 bits +or- 2
#if defined(DEBUG)
void print_byte( byte val ) {
  for( byte i = 0; i <= 7; i++ ) {
    Serial.print( val >> i & 1, BIN ); // Magic bit shift, if you care look up the <<, >>, and & operators
  }
  Serial.print( "\n" );
}
#endif

// Read all the current switch positions from the shift registers (for pedalboard & thumb/toe pistons)
void readShiftRegisters() {

  if ( millis() - lastSwitch > Btn_Debounce ) {  // Ignore if we're in the debounce time
    lastSwitch = millis();
  
    // dataMode : SPI_MODE0, SPI_MODE1, SPI_MODE2, or SPI_MODE3
    // dataOrder: MSBFIRST or LSBFIRST
    SPI.beginTransaction( SPISettings( 5000000, MSBFIRST, SPI_MODE0 ) );
    
    digitalWrite( LATCHPIN, LOW); // Trigger loading the state of the A-H data lines into the shift register
    delayMicroseconds( 5 ); // Requires a delay here according to the datasheet timing diagram
    digitalWrite( LATCHPIN, HIGH );
    delayMicroseconds( 5 );
  
    // While the shift register is in serial mode collect each shift register 
    // into a byte. NOTE: the register attached to the chip comes in first.
    for ( byte i = 0; i < NUM_REGISTERS; i++ ) {
      
      switchState[i] = SPI.transfer( 0x00 );
   
      #if defined( DEBUG )
      Serial.print( "Register #" );
      Serial.print( i );
      Serial.print( ": " );
      Serial.print( switchState[i] );
      Serial.print( "-" );
      print_byte( switchState[i] );
      Serial.println();
      #endif
    }
    
    SPI.endTransaction();

    #if defined(DEBUG)
    Serial.println("~~~~~~~~~~~~~~~");
    #endif
  
    processSwitches();  // Process switch positions
  }
}

// Read thumb pistons on manuals via registers and send MIDI program change messages via USB MIDI
void processSwitches() {

  // Process all registers
  for ( byte i = 0; i < NUM_REGISTERS; i++ ) {
    if ( switchState[i] != midiState[i] ) {  // Don't process if no switches changed
      for ( byte j = 0; j < 8; j++ ) {
        // Read old & new switch states
        // byte switchStateValue = ( switchState[i] & (1 << j) ) == (1 << j) );
        // byte midiStateValue   = ( midiState[i] & (1 << j) ) == (1 << j) );
        byte switchStateValue = bitRead( switchState[i], j );
        byte midiStateValue   = bitRead( midiState[i], j );
        
        // Switch has changed, send MIDI command
        if ( switchStateValue != midiStateValue ) {

          if ( i >= THUMB_PISTON_START_REGISTER && i <= THUMB_PISTON_END_REGISTER ) {
            byte program = ( ( i - THUMB_PISTON_START_REGISTER ) * 8 ) + j;
            if (switchStateValue == 1) {  // HIGH = switch closed
              usbMIDI.sendProgramChange( program, THUMB_PISTON_MIDI_CHANNEL );
            }
          } else if ( i >= TOE_PISTON_START_REGISTER && i <= TOE_PISTON_END_REGISTER ) {
            byte program = ( ( i - THUMB_PISTON_START_REGISTER ) * 8 ) + j;
            if ( switchStateValue == 1 ) {  // HIGH = switch closed
              usbMIDI.sendProgramChange(program, PEDAL_MANUAL_MIDI_CHANNEL );
            }
          } else if ( i >= PEDAL_MANUAL_START_REGISTER && i <= PEDAL_MANUAL_END_REGISTER ) {
            byte note = ( ( i - PEDAL_MANUAL_START_REGISTER ) * 8 + j ) + PEDAL_MANUAL_MIDI_NOTE_OFFSET;  // MIDI note for current switch
            if ( switchStateValue == 1 ) {  // HIGH = switch closed
              // Keyboard wired wrong possibly, flip notes
              usbMIDI.sendNoteOn( 103 - note, MIDI_NOTE_ON_VELOCITY, TOE_PISTON_MIDI_CHANNEL );
            } else {  // LOW = switch open
              usbMIDI.sendNoteOff( 103 - note, 0, TOE_PISTON_MIDI_CHANNEL );
            }
          }
        }
      }
      midiState[i] = switchState[i];  // Save current switch state
    }
  }
}

// Read analog values from pedals and send MIDI Control Change messages via USB MIDI
void readPedals() {

    // Read the position of all 3 pedals
    for ( byte p = 0; p < 3; p++ ) {
        // Don't bother reading pedal if not in use
        if (! pedal[p][PEDAL_ACTIVE] ) {
            continue;
        }
        // Read analog value 0-1023 from pedal pin
        int pedalVal = analogRead( pedal[p][PEDAL_PIN] ) / 8;
    
        // If pedalposition has changed, save the new value and send a MIDI control change
        if (pedalVal != pedalState[p] ) {
            usbMIDI.sendControlChange( pedal[p][PEDAL_CTRL_NUM], pedalVal, PEDAL_MIDI_CHANNEL );
            pedalState[p] = pedalVal;
        }
    }
}
