/// Title: Ardrando
// VERSION:  3.1
boolean DEBUG = 0;
boolean NOTEDEBUG = 0;
#include <Wire.h>
//-------------------- Ins and outs -------------------------
// ---------------------- ANALOG

#define  clock_in A1 // analog clock input
#define  SDA_pin  A4 // non-negotiable, see pinout
#define  SCL_pin  A5 // non-negotiable, see pinout
//     analog inputs for pots 1-3      Shift      |   Normal
//                                   -------------|---------------
#define  pot1  A3   //              quant scale   |   Random lock
#define  pot2  A6   //                  bpm       |   Random Scale
#define  pot3  A7   //              note length   |   Sequence length

// ---------------------- DIGITAL
#define gateOutPin 5  // uses PWM out
#define led_red 2
#define led_blue 3
#define led_green 4
#define seqPin 6      // this reads toggle state, uses internal pullup resistor
#define shift 7     // shift button
#define MCP4725 0x60  //MCP4725 address as 0x61 Change yours accordingly; see http://henrysbench.capnfatz.com/henrys-bench/arduino-projects-tips-and-more/arduino-quick-tip-find-your-i2c-address/

//These are the valid values for scales
// can generate new ones in R like:  paste0(sort(c(c((0:8 * 12) + 0), c((0:8 * 12) + 5))), collapse=", ")
// roots only
uint8_t scale1[9]  = {0, 12, 24, 36, 48, 60, 72, 84, 96};
// roots fifths
uint8_t scale2[18] =   {0, 7, 12, 19, 24, 31, 36, 43, 48,
                        55, 60, 67, 72, 79, 84, 91, 96, 103
                       };
// twos and fifths
uint8_t scale3[27] =   {0, 2, 7, 12, 14, 19, 24, 26, 31,
                        36, 38, 43, 48, 50, 55, 60, 62, 67,
                        72, 74, 79, 84, 86, 91, 96, 98, 103
                       };
// major
uint8_t scale4[27] =   {0, 5, 7, 12, 17, 19, 24, 29, 31,
                        36, 41, 43, 48, 53, 55, 60, 65, 67,
                        72, 77, 79, 84, 89, 91, 96, 101, 103
                       };
//minor
uint8_t scale5[27] =   {0, 4, 7, 12, 16, 19, 24, 28, 31,
                        36, 40, 43, 48, 52, 55, 60, 64, 67,
                        72, 76, 79, 84, 88, 91, 96, 100, 103
                       };
// major 6
uint8_t scale6[36] =   {0, 5, 7, 9, 12, 17, 19, 21, 24,
                        29, 31, 33, 36, 41, 43, 45, 48, 53,
                        55, 57, 60, 65, 67, 69, 72, 77, 79,
                        81, 84, 89, 91, 93, 96, 101, 103, 105
                       };
// major 7
uint8_t scale7[36] =   {0, 5, 7, 10, 12, 17, 19, 22, 24,
                        29, 31, 34, 36, 41, 43, 46, 48, 53,
                        55, 58, 60, 65, 67, 70, 72, 77, 79,
                        82, 84, 89, 91, 94, 96, 101, 103, 106
                       };
// semitones
uint8_t scale8[128] =   {1, 2, 3, 4, 5, 6, 7, 8, 9,
                         10, 11, 12, 13, 14, 15, 16, 17, 18,
                         19, 20, 21, 22, 23, 24, 25, 26, 27,
                         28, 29, 30, 31, 32, 33, 34, 35, 36,
                         37, 38, 39, 40, 41, 42, 43, 44, 45,
                         46, 47, 48, 49, 50, 51, 52, 53, 54,
                         55, 56, 57, 58, 59, 60, 61, 62, 63,
                         64, 65, 66, 67, 68, 69, 70, 71, 72,
                         73, 74, 75, 76, 77, 78, 79, 80, 81,
                         82, 83, 84, 85, 86, 87, 88, 89, 90,
                         91, 92, 93, 94, 95, 96, 97, 98, 99,
                         100, 101, 102, 103, 104, 105, 106, 107, 108,
                         109, 110, 111, 112, 113, 114, 115, 116, 117,
                         118, 119, 120, 121, 122, 123, 124, 125, 126,
                         127, 128
                        };

// needed for toggle cause it is conductive
unsigned long lastDebounceTime = 0;  // the last time the output pin was toggled
unsigned long debounceDelay = 50;    // the debounce time; increase if the output flickers


byte buffer[3];    // for dac

// -------------  Variables for the sequencer -------------------
uint8_t def_sequencer_steps[16] = {60, 67, 72, 79, 84, 79, 72, 67, 60, 55, 48, 43, 36, 43, 48, 55};
//uint8_t def_sequencer_steps[16] = {36, 48, 60, 72, 84, 96, 36, 48, 60, 72, 84, 96, 36, 48, 60, 72 };
uint8_t sequencer_steps[16] ;
float sequencer_lens[16];
uint8_t seq_counter;
uint8_t seq_length;
boolean seq_running ;
boolean gate_is_high ;
uint8_t thisscale[128];
uint8_t thisscaleid = 8;
uint8_t thisscalelen = 128;
// you could just take the counter - 1 of the seuqencer to get the last note, but what if the seqeunce changes?
int prev_note;
int base;

// ------------------                   Clock stuff
// These keep track of the toggle state, debounced
int internal_clock ; // int, ext midi, ext cv
int this_clock_state ;
int last_clock_state ; // int, ext midi, ext cv, save this for debouncing

//      internal clock
unsigned long time_next_note;  //needs to be unsigned for millis() comparison
unsigned long time_prev_note;
unsigned long time_to_restart;
unsigned long time_to_reset_seq;
unsigned long time_to_reset_bpm;
boolean shift_already = false;
boolean already_reset_seq = false;
boolean already_reset_bpm = false;
int time_between_quarter_notes;
int bpm = 120;



// For Turing Functions
int random_scale;
int random_amt;
int random_rest;
// function declarations
void sequencer_mode();
void edit_mode();
void update_key(int i, int val);
void noteOn(int pitch);
void noteOff(int pitch);

void process_shift_change();
void setSequence();
void toggleClock();
void success_leds();
void waiting_leds();
void set_time_between_quarter_notes(float bpm);
void update_param(int button, int param = 0);

int process_pot(int a_input);
float noteToVolt(int note);
void read_seq_and_params();
void vpoOut(unsigned int pitch);
void gateOut(unsigned int state);


// restart function
void(* resetFunc) (void) = 0;

void setup() {
  Wire.begin();                    //Begins the I2C communication
  time_prev_note = 0;
  pinMode(led_red, OUTPUT);
  pinMode(led_blue, OUTPUT);
  pinMode(led_green, OUTPUT);
  pinMode(gateOutPin, OUTPUT);

  pinMode(pot1, INPUT);
  pinMode(pot2, INPUT);
  pinMode(pot3, INPUT);
  pinMode(seqPin, INPUT_PULLUP);
  pinMode(shift, INPUT_PULLUP);

  digitalWrite(led_red, LOW);
  digitalWrite(led_blue, LOW);
  digitalWrite(led_green, LOW);
  digitalWrite(gateOutPin, LOW);
  internal_clock =  digitalRead(seqPin);
  last_clock_state = internal_clock ;

  // -------------  Variables for the sequencer -------------------
  // these are in the setup() because we want to be able to reset the seqeunce when the unit is reset
  for (int k = 0; k < 16; k++) {
    sequencer_steps[k] = def_sequencer_steps[k];
    sequencer_lens[k] = .5;
  }
  for (int k = 0; k < thisscalelen; k++) {
    thisscale[k] = scale8[k];
  }
  seq_counter = 0;
  seq_length = 16;
  seq_running = false;
  gate_is_high = false;
  // you could just take the counter - 1 of the seuqencer to get the last note, but what if the seqeunce changes?
  int prev_note;


  delay(50);

  if (DEBUG) {
    Serial.begin(9600);
    Serial.print("setup done\n");
  } else {
    Serial.begin(31250);
  }
  success_leds();

}


void loop() {
  // base note to start on.
  base = (12 * 3);
  // start by checking if the shift key is pressed.  If so ignore any note changes
  // note shift is connected as a pull up, so activated means 0, deactivated means 1
  int this_clock_state = digitalRead(seqPin);
  if (this_clock_state != last_clock_state) {
    lastDebounceTime = millis();
  }

  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (this_clock_state != internal_clock) { // still
      if (!internal_clock) {
        digitalWrite(led_blue, HIGH);
        delay(100);
        digitalWrite(led_blue, LOW);
        delay(50);
        digitalWrite(led_red, HIGH);
        delay(100);
        digitalWrite(led_red, LOW);
        delay(50);
        digitalWrite(led_green, HIGH);
        delay(100);
        digitalWrite(led_green, LOW);
        delay(50);
        digitalWrite(led_red, HIGH);
        delay(100);
        digitalWrite(led_red, LOW);
        internal_clock = 1;
      } else {
        internal_clock = 0;
      }
    }
  }

  last_clock_state = this_clock_state;
  // pause sequencer if you need to
  if (digitalRead(shift) == 0 ) {
    digitalWrite(led_green, HIGH);
    process_shift_change();
    seq_running = false;
  } else {
    digitalWrite(led_green, LOW);
    seq_running = true;
    shift_already = false;
    already_reset_bpm = false;
    already_reset_seq = false;
  }

  if (seq_running) {
    sequencer_mode();
  } else {
    // otheriwse, if shift key is not pressed,
    digitalWrite(led_blue, LOW);
  }
}

/* -------------------------------    Main Logic     --------------------------- */
// one of the three following modes is run each time the main loop executes:
// playing the keyboard, running the sequencer, and editing stuff


void check_note_length_int() {
  if (gate_is_high) {
    if (millis() > (time_prev_note - (sequencer_lens[seq_counter] * time_between_quarter_notes))) {
      noteOff(prev_note);
    }
  }
}

void check_note_length_ext() {
  if (gate_is_high) {
    if (millis() > (time_prev_note - (sequencer_lens[seq_counter] * time_between_quarter_notes))) {
      noteOff(prev_note);
    }
  }
}

void sequencer_mode() {

  // first, check for BPM changes
  // next, checks clock mode
  // next, checks if enouggh time has ellapsed between notes

  //  Serial.print(bpm);
  //  Serial.print("bpm; ");
  //  Serial.print(time_between_quarter_notes);
  //  Serial.print(" ms between nodes;");
  //  Serial.print(time_next_note);
  //  Serial.print(" ");
  //  Serial.println(millis());


  set_time_between_quarter_notes(bpm);
  // internal
  if (internal_clock) {
    time_prev_note = millis();

    if (millis() > time_next_note) {
      //      Serial.print(time_prev_note);
      //      Serial.print(" ");
      //      Serial.print(time_between_quarter_notes);
      //      Serial.print(" ");
      //      Serial.print(time_next_note);
      //      Serial.print(" ");
      //      Serial.println(millis());
      play_sequence();
      time_next_note = time_prev_note + time_between_quarter_notes;
    } else {
      check_note_length_int();
    }
    //   external cv
  } else {
    // check for a CV pulse. USA A VOLTAGE DIVIDER TO ATTENUATE INPUT!!!
    // also, ignore long pulses:  for instance, STARTUP by music thing modular outputs clock triggers for 10MS
    // so for safety lets wait for a 5x that (100)
    //     Serial.print(analogRead(clock_in));
    //     Serial.print("\n");
    if (analogRead(clock_in) > 400) {
      //      Serial.print(time_prev_note);
      //      Serial.print(" ");
      //      Serial.println(millis());
      if ((millis() - time_prev_note)  > 100) {
        //        Serial.print("playing note\n");
        time_prev_note = millis();
        play_sequence();
      }
    } else {
      check_note_length_ext();
    }
  }
}


//-------------------------------   Functions  ---------------------------------
int scale_centered_sequence_note(int thisnote, int scaling, int half_scaling) {
  // this scales the seqeunced centered around the middle of the keyboard, key 64
  // for instace, scale to 1/3 of total notes(42) and the sequence is  48 60 72
  // centered around 1 (really 64) this is 48/64 60/64, 72/64
  //                                     .75       .9375      1.125
  // scaled to 42's center: 21           .75*21    .9375*21   1.125*21
  //                                   15.75     19.6875     23.625
  //but thats not centered around the middle of the keyboard, so
  // we add the difference between new middle and old middle: 64 - 21 = 43
  // New sequence is                    58.75    62.6875     66.625
  return (ceil(float(thisnote / 64.0) * half_scaling + (64 - half_scaling)));

}

int quantize_to_scale(int thisnote, int thisscalelen) {
  //Quantize if neccesary: check if the note is in the selected scale.
  // If not, increment or decrement closer to center
  int quantized = 0;
  for (int q = 0; q < thisscalelen; q++) {
    if (abs(thisscale[q] - thisnote) < abs(quantized - thisnote)) {
      quantized = thisscale[q];
    }
  }
  return (quantized);
}
void play_sequence() {
  int scaling =  map(process_pot(pot2), 0, 1023, 1, 128);
  int half_scaling =  ceil(float(scaling / 2.0));
  if (DEBUG) {
    if (seq_counter == 0) {
      for (int i = 0 ; i < seq_length; i++) {
        Serial.print(sequencer_steps[i]);
        Serial.print(" ");
      }
      Serial.print(" || ");
      for (int i = 0 ; i < seq_length; i++) {
        Serial.print(scale_centered_sequence_note(sequencer_steps[i], scaling, half_scaling));
        Serial.print(" ");
      }
      Serial.print(" || ");
      for (int i = 0 ; i < seq_length; i++) {
        Serial.print(quantize_to_scale(scale_centered_sequence_note(sequencer_steps[i], scaling, half_scaling), thisscalelen));
        Serial.print(" ");
      }
      Serial.print(thisscaleid);
      Serial.println(" ");
    }
  }
  seq_length = round(map(process_pot(pot3), 0, 1023, 1, 16));
  int thisnote = sequencer_steps[seq_counter];
  int thisscalednote = scale_centered_sequence_note(thisnote, scaling, half_scaling);
  int quantized = quantize_to_scale(thisscalednote, thisscalelen);
  // kill previous note
  if (thisnote == 0) {
    // this is how we play rests
    gateOut(0);
  } else {
    noteOff(prev_note);
    noteOn(quantized);
  }

  // increment, resetting counter if need be
  seq_counter = seq_counter + 1 ;
  if (seq_counter >= seq_length) {
    seq_counter = 0;
  }

  //  turing bit
  random_amt = map(process_pot(pot1), 0, 1023, 100, 0);
  // decide change note or not
  if ( random_amt  > random(100)) {
    int base = ceil(scaling / 2.0);
    int rscale = random(scaling);
    //int new_note = thisnote - half_scaling + rscale; // new note is centered around middle
    int new_note = thisnote - base + rscale;
    if (DEBUG) {
      Serial.print("old step ");

      Serial.print(sequencer_steps[seq_counter]);
      Serial.print("; new step ");
      Serial.print(new_note);
      Serial.print("; base ");
      Serial.print(base);
      Serial.print("; rand ");
      Serial.println(rscale);
    }
    sequencer_steps[seq_counter] = constrain(new_note, 1, 128);
  }
}


void set_time_between_quarter_notes(float bpm) {
  float bps = 60 / bpm ;                    // beats per second (2 @120)
  time_between_quarter_notes = bps * 1000 ; // milliseconds per beat
}


int process_pot(int a_input) {
  int thisval = analogRead(a_input);
  //  if (DEBUG) {
  //    Serial.print("Analog input:");
  //    Serial.println(a_input);
  //    Serial.print(" ");
  //    Serial.println(thisval);
  //  }
  return (thisval);
}


///////////////////////////    Functions for CV   /////////////////
float noteToVolt(int note) {
  // returns value 0-4096 to sent to DAC
  // note to freq
  // https://en.wikipedia.org/wiki/Piano_key_frequencies
  //float freq = pow(2, ((note - 49) / 12.0)) * 440 ;
  int constr_note = constrain(note, 36, 97);       // DAC only can do 5 octaves
  int mvolts = map(constr_note, 36, 97, 0, 5000) ; // start at A220
  float volts = (mvolts / 1000.0);
  //  if (DEBUG) {
  //    Serial.print("Freq: ");
  //    Serial.print(freq);
  //    Serial.print("  Note: ");
  //    Serial.print(note);
  //    Serial.print("  ConstrNote: ");
  //    Serial.print(constr_note);
  //    Serial.print("   mVolts: ");
  //    Serial.print(mvolts);
  //    Serial.print("   Volts: ");
  //    Serial.println(volts);
  //  }
  return (volts);
}

void gateOut(unsigned int state) {
  if (state == 1) {
    digitalWrite(gateOutPin, HIGH);
  } else {
    digitalWrite(gateOutPin, LOW);
  }
}

void vpoOut(unsigned int pitch) {
  buffer[0] = 0b01000000;            //Sets the buffer0 with control byte (010-Sets in Write mode)
  //Serial.print("ADC:");
  //Serial.println(adc);
  int adc =  map(noteToVolt(pitch) * 1000, 0, 5000, 0, 4096);
  buffer[1] = adc >> 4;              //Puts the most significant bit values
  buffer[2] = adc << 4;              //Puts the Least significant bit values
  Wire.beginTransmission(MCP4725);         //Joins I2C bus with MCP4725 with 0x61 address

  Wire.write(buffer[0]);            //Sends the control byte to I2C
  Wire.write(buffer[1]);            //Sends the MSB to I2C
  Wire.write(buffer[2]);            //Sends the LSB to I2C

  Wire.endTransmission();           //Ends the transmission
}

///////////////////////////    Functions for MIDI /////////////////


void noteOn(int pitch) {
  digitalWrite(led_red, HIGH);
  vpoOut(pitch);
  gateOut( 1 );
  gate_is_high  = true;
  if (NOTEDEBUG) {
    Serial.print("on ");
    Serial.print(pitch);
    Serial.print("\n");
  }

  prev_note = pitch;
}

void noteOff(int pitch) {
  digitalWrite(led_red, LOW);
  gateOut(0);
  gate_is_high = false;
  if (NOTEDEBUG) {
    Serial.print("off ");
    Serial.print(pitch);
    Serial.print("\n");
  }
}


///////////////////////////////////   Functions for Sequencer, ETC ////////////
void process_shift_change() {
  // After 4 seconds, clear the sequence to all Cs
  // After 8, reset the arduino
  seq_running = false;
  if (!shift_already) {

    shift_already = true;
    // restart if held for 8 seconds
    time_to_restart = millis() + (8 * 1000) ;
    time_to_reset_seq = millis() + (4 * 1000) ;
    time_to_reset_bpm = millis() + (2 * 1000) ;
  } else {
    if (time_to_restart < millis()) {
      for (int i = 0; i < 4; i ++) {
        digitalWrite(led_red, HIGH);
        digitalWrite(led_green, HIGH);
        digitalWrite(led_blue, HIGH);
        delay(200);
        digitalWrite(led_red, LOW);
        digitalWrite(led_green, LOW);
        digitalWrite(led_blue, LOW);
        delay(100);
      }
      resetFunc();
    } else if (time_to_reset_bpm < millis() and not already_reset_bpm)  {
			//  Change the BPM
      bpm = map(process_pot(pot2), 0, 1023, 10, 400);
      already_reset_bpm = true;
      digitalWrite(led_blue, HIGH);
      delay(100);
      digitalWrite(led_blue, LOW);
      delay(100);
      digitalWrite(led_blue, HIGH);
      delay(100);
      digitalWrite(led_blue, LOW);
			//  Change the quantization scale, setup thisscale with valid notes
      thisscaleid = map(process_pot(pot1), 0, 1023, 1, 8);
      if (thisscaleid == 1) {
        thisscalelen = 9 ;
        for (int s = 0; s < thisscalelen; s++) {
          thisscale[s] = scale1[s];
        }
      } else if (thisscaleid == 2) {
        thisscalelen = 18 ;
        for (int s = 0; s < thisscalelen; s++) {
          thisscale[s] = scale2[s];
        }
      } else if (thisscaleid == 3) {
        thisscalelen = 27 ;
        for (int s = 0; s < thisscalelen; s++) {
          thisscale[s] = scale3[s];
        }
      } else if (thisscaleid == 4) {
        thisscalelen = 27 ;
        for (int s = 0; s < thisscalelen; s++) {
          thisscale[s] = scale4[s];
        }
      } else if (thisscaleid == 5) {
        thisscalelen = 27 ;
        for (int s = 0; s < thisscalelen; s++) {
          thisscale[s] = scale5[s];
        }
      } else if (thisscaleid == 6) {
        thisscalelen = 36 ;
        for (int s = 0; s < thisscalelen; s++) {
          thisscale[s] = scale6[s];
        }
      } else if (thisscaleid == 7) {
        thisscalelen = 36 ;
        for (int s = 0; s < thisscalelen; s++) {
          thisscale[s] = scale7[s];
        }
      } else {
        thisscalelen = 128 ;
        for (int s = 0; s < thisscalelen; s++) {
          thisscale[s] = scale8[s];
        }
      }
    } else if (time_to_reset_seq < millis() and not already_reset_seq) {
			// restart the arduino
      digitalWrite(led_red, HIGH);
      delay(100);
      digitalWrite(led_red, LOW);
      delay(100);
      digitalWrite(led_red, HIGH);
      delay(100);
      digitalWrite(led_red, LOW);
      already_reset_seq = true;
      for (int k = 0; k < seq_length; k++ ) {
        sequencer_steps[k] = 60;
        sequencer_lens[k] = .5;
      }
    }
  }
}


void success_leds() {
  for (int i = 0; i < 4 ; i++) {
    digitalWrite(led_green, HIGH);
    delay(50);
    digitalWrite(led_green, LOW);
    delay(100);
  }
}
