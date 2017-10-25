/* Buttons to USB MIDI Example

   You must select MIDI from the "Tools > USB Type" menu

   To view the raw MIDI data on Linux: aseqdump -p "Teensy MIDI"

   This example code is in the public domain.
*/
#include <NewPing.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Audio.h>
#include "notes.h"
#include <math.h>

// If using software SPI (the default case):
#define OLED_MOSI  27
#define OLED_CLK   26
#define OLED_DC    25
#define OLED_CS    10
#define OLED_RESET 24
Adafruit_SSD1306 display(OLED_MOSI, OLED_CLK, OLED_DC, OLED_RESET, OLED_CS);

#if (SSD1306_LCDHEIGHT != 32)
#error("Height incorrect, please fix Adafruit_SSD1306.h!");
#endif

AudioOutputAnalog        dac1;
AudioSynthKarplusStrong  string1;
AudioEffectEnvelope      envelope1;
AudioEffectReverb        reverb1;
AudioMixer4              mixer1;
AudioConnection          patchCord1(mixer1, 0, dac1, 0);
AudioSynthWaveformSine   sine1;
AudioConnection          patchCord2(sine1, 0, envelope1, 0);
AudioConnection          patchCord3(envelope1, 0, reverb1, 0);
AudioConnection          patchCord4(sine1, 0, mixer1, 0);
 
#include <Bounce.h>

NewPing right(1, 0, 50);
NewPing center(3, 2, 50);
NewPing left(5, 4, 50);
IntervalTimer updater;

#define MAX_WINDOW_SIZE 15
#define ZEROES_PART 0.1

struct midi_state {
    bool on;
    float curr_value, prev_value;
    int curr_note, curr_pitch;
    int prev_note, prev_pitch;
  } state;

int main_volume = 0;

class USC : public NewPing{

public:
  
  USC(int mn_nt, int mx_nt, int w_size,
      void (* enabled)(midi_state), void (* disabled)(midi_state), void (* notechanged)(midi_state), void (* pitchchanged)(midi_state), void (* valuechanged)(midi_state),
      int pinTrig, int pinEcho, int maxRange) : NewPing(pinTrig, pinEcho, maxRange){
        state = new midi_state;

        state->on = false;
  
        min_note = mn_nt;
        max_note = mx_nt;

        window_size = w_size - 1;
        zeroes = w_size;
  
        max_value = maxRange * 57;
        value_step = 5;
        note_step = max_value/value_step/(float)(max_note - min_note - 1);
 
        onEnabled = enabled;
        onDisabled = disabled;
        onNoteChanged = notechanged;
        onPitchChanged = pitchchanged;
        onValueChanged = valuechanged;
      }
  
  void update();

private:
  int window_size;
  float value[MAX_WINDOW_SIZE + 1];
  
  midi_state * state;
  
  float value_step, note_step, max_value;
  int zeroes_threshold;
  int min_note, max_note;
  int zeroes;

  void (* onEnabled)(midi_state);
  void (* onDisabled)(midi_state);
  void (* onNoteChanged)(midi_state);
  void (* onPitchChanged)(midi_state);
  void (* onValueChanged)(midi_state);
};

//float linear(float val, float base_val){ return val - base_val; }
float logarythmic(float val, float base_val){ return log(val/base_val)/log(2)*12.0; }

void USC::update(){
  float val = 0;

  if ((int)value[0] == 0) zeroes --;
  for (int i = 0; i < window_size; ++i){
    value[i] = value[i+1];
    val += value[i];
  }
  value[window_size] = (float)ping()/value_step;
  val += value[window_size];
  val /= window_size;

  if (value[window_size] >= max_value) value[window_size] = 0;//state->prev_value; 


  // Binary search note and pitch
  /*int note, pitch;
  int l, r;
  l = min_note, r = max_note;
  while (l != r){
    int c = (l + r)/2;
    if (freq < notes[c]) r = c;
    else l = c;
  }
  note = l;
  pitch = 0;*/

  //note = logarythmic(base_note, 

  state->curr_note = val/note_step + min_note;
  state->curr_pitch = ((int)val%(int)note_step)*(127.0/note_step);
  
//  if (((int)average != (int)sensors[i].value[0]) && (average != 0)) sensors[i].onChanged(sensors[i].value[0], average);


  if ((int)value[window_size] == 0) zeroes ++;
  //Serial.println(zeroes);
  if (state->on && (zeroes > (float)ZEROES_PART*(float)window_size)) state->on = false, onDisabled(*state);
  if (!state->on && (zeroes < (float)ZEROES_PART*(float)window_size)) state->on = true, onEnabled(*state);

  // On note changed
  if (state->curr_note != state->prev_note) onNoteChanged(*state);
  // On pitch changed
  if (state->curr_pitch != state->prev_pitch) onPitchChanged(*state);
  // On value changed
  if (state->curr_value != state->prev_value) onValueChanged(*state);

  state->prev_value = state->curr_value;
  state->prev_note = state->curr_note;
  state->prev_pitch = state->curr_pitch;
}

void setfreq(midi_state state){
  //sine1.frequency(state.curr_value);
  
  //Serial.println(cfreq);
}

void doNothing(midi_state state){
//  Serial.print(state.curr_note);
//  Serial.print(" ");  
//  Serial.println(state.curr_pitch);
}

void setPitch(midi_state state){
  sine1.frequency(pow(2, 1 + (float)state.curr_note/12.0 + (float)state.curr_pitch/127.0/12.0) * 65.41);
  
  usbMIDI.sendPitchBend(state.curr_pitch, 1);
  //Serial.println(state.curr_pitch);
}

void setnoteOn(midi_state state){
  sine1.frequency(pow(2, 1 + (float)state.curr_note/12.0 + (float)state.curr_pitch/127.0/12.0) * 65.41);
//  usbMIDI.sendNoteOff(state.prev_note, 0, 1);
  usbMIDI.sendNoteOn(state.curr_note, main_volume, 1);
  usbMIDI.sendControlChange(64,127,1);
  Serial.println(main_volume);
}

void setnoteOff(midi_state state){
  sine1.amplitude(0);
  usbMIDI.sendControlChange(64,0,1);
  usbMIDI.sendNoteOff(50, 0, 1);
  usbMIDI.sendNoteOff(state.prev_note, 0, 1);
  usbMIDI.sendNoteOff(state.curr_note, 0, 1);
}

void setamplitude(midi_state state){
  sine1.amplitude((float)state.curr_pitch/127.0);
  main_volume = state.curr_pitch;
}

USC sensors[2] = {
  //USC(10, 50, 5, doNothing, doNothing, doNothing, doNothing, doNothing, 5, 4, 50),
  USC(1, 3, 5, doNothing, doNothing, doNothing, setamplitude, doNothing, 5, 4, 50),
  USC(40, 60, 5, doNothing, setnoteOff, setnoteOn, setPitch, doNothing, 3, 2, 50),
};

void scan_sensors(){
  for (int i = 0; i < 2; i++){
    sensors[i].update();
  }
}


// the MIDI channel number to send messages
const int channel = 1;

// Create Bounce objects for each button.  The Bounce object
// automatically deals with contact chatter or "bounce", and
// it makes detecting changes very simple.
Bounce button0 = Bounce(6, 5);
Bounce button1 = Bounce(7, 5);  // 5 = 5 ms debounce time
Bounce button2 = Bounce(8, 5);  // which is appropriate for good
Bounce button3 = Bounce(9, 5);

int mod_period = 1;
bool sine_on = false;
int cnt = 0;
float amp = 0;
void generate(void) {
  cnt++;
  if (cnt%mod_period == 0){
   if (sine_on){
      sine1.amplitude(0);
      sine_on = false;
   } else {
      sine1.amplitude(0.4);
      sine_on = true;
   }
   cnt = 0;
  }
}

float allnotes[60];
double eigth_part = 1.0145453349375236414538678576629;

void setup() {
  // Configure the pins for input mode with pullup resistors.
  // The pushbuttons connect from each pin to ground.  When
  // the button is pressed, the pin reads LOW because the button
  // shorts it to ground.  When released, the pin reads HIGH
  // because the pullup resistor connects to +5 volts inside
  // the chip.  LOW for "on", and HIGH for "off" may seem
  // backwards, but using the on-chip pullup resistors is very
  // convenient.  The scheme is called "active low", and it's
  // very commonly used in electronics... so much that the chip
  // has built-in pullup resistors!
  pinMode(6, INPUT_PULLUP);
  pinMode(7, INPUT_PULLUP);
  pinMode(8, INPUT_PULLUP);
  pinMode(9, INPUT_PULLUP);
  Serial.begin(115200);

  allnotes[0] = notes[10];
  for(int i = 1; i < 60; i ++){
    allnotes[i] = allnotes[i-1]*eigth_part;
  }

  display.begin(SSD1306_SWITCHCAPVCC);
  // init done
 
  updater.begin(scan_sensors, 10000);  
  // Show image buffer on the display hardware.
  // Since the buffer is intialized with an Adafruit splashscreen
  // internally, this will display the splashscreen.
  //display.display();
  //delay(2000);

  // Clear the buffer.
  display.clearDisplay();

  //analogWriteResolution(12);
  AudioMemory(25);

  dac1.analogReference(INTERNAL);   // normal volume
  //dac1.analogReference(EXTERNAL); // louder
  mixer1.gain(0, 1);
  //drawlogo();
  display.clearDisplay();
  display.display();
  sine1.amplitude(0.6);

  usbMIDI.sendControlChange(64,127,1);

  envelope1.sustain(1);
}

// Simple DAC sine wave test on Teensy 3.1

void loop() {
  // Update all the buttons.  There should not be any long
  // delays in loop(), so this runs repetitively at a rate
  // faster than the buttons could be pressed and released.
  
  button0.update();
  button1.update();
  button2.update();
  button3.update();

  /*if((c > 0) && (c != lastc)){
    sine1.frequency(allnotes[c%60]);
    Serial.println(allnotes[c%60]);
    
    lastc = c;
    center_off = false;
    //usbMIDI.sendNoteOff(lastc, 0, channel);
    //usbMIDI.sendNoteOn(c, 99, channel);
    //usbMIDI.sendControlChange(64,127,1);
  } else if ((c < 0) && (center_off == false)){
    center_off = true;
    sine1.amplitude(0);
    sine1.frequency(0);
    //note_period = 0;
    //usbMIDI.sendNoteOff(lastc, 0, channel);
    //usbMIDI.sendControlChange(64,0,1);
  } else if (c == lastc){
    //usbMIDI.sendPitchBend(pitch, 1);
  }
  delay(10);*/

  // Check each button for "falling" edge.
  // Send a MIDI Note On message when each button presses
  // Update the Joystick buttons only upon changes.
  // falling = high (not pressed - voltage from pullup resistor)
  //           to low (pressed - button connects pin to ground)
  /*if (button0.fallingEdge()) {
    usbMIDI.sendNoteOn(60, 99, channel);  // 60 = C4
  }
  if (button1.fallingEdge()) {
    usbMIDI.sendNoteOn(61, 99, channel);  // 61 = C#4
  }
  if (button2.fallingEdge()) {
    usbMIDI.sendNoteOn(62, 99, channel);  // 62 = D4
  }
  if (button3.fallingEdge()) {
    usbMIDI.sendNoteOn(63, 99, channel);  // 63 = D#4
  }*/

  // Check each button for "rising" edge
  // Send a MIDI Note Off message when each button releases
  // For many types of projects, you only care when the button
  // is pressed and the release isn't needed.
  // rising = low (pressed - button connects pin to ground)
  //          to high (not pressed - voltage from pullup resistor)
  /*if (button0.risingEdge()) {
    usbMIDI.sendNoteOff(60, 0, channel);  // 60 = C4
  }
  if (button1.risingEdge()) {
    usbMIDI.sendNoteOff(61, 0, channel);  // 61 = C#4
  }
  if (button2.risingEdge()) {
    usbMIDI.sendNoteOff(62, 0, channel);  // 62 = D4
  }
  if (button3.risingEdge()) {
    usbMIDI.sendNoteOff(63, 0, channel);  // 63 = D#4
  }*/

  // MIDI Controllers should discard incoming MIDI messages.
  // http://forum.pjrc.com/threads/24179-Teensy-3-Ableton-Analog-CC-causes-midi-crash
  while (usbMIDI.read()) {
    // ignore incoming messages
  }
}


void draw_triangle(int x, int y, int hght){
  display.fillTriangle(x, y - hght/2,
                       x - hght/2, y + hght/2,
                       x + hght/2, y + hght/2, WHITE);
}

void draw_triforce(int x, int y, int hght){
  draw_triangle(x, y - hght/4, hght/3);
  draw_triangle(x - hght/4, y + hght/4, hght/3);
  draw_triangle(x + hght/4, y + hght/4, hght/3);
}

void drawlogo(void) {
  uint8_t color = WHITE;
  display.clearDisplay();
  draw_triforce(110, 16, 25);
  display.setTextSize(2);
  display.setTextColor(WHITE);
  display.setCursor(0,10);
  display.println("triforce");
  if (color == WHITE) color = BLACK;
  else color = WHITE;
  display.display();
  delay(3000);
}


