#define INPUT_FREQUENCY 50

#define CHANNEL_MAX  2000
#define CHANNEL_MIN 1000
#define CHANNEL_MID 1500
#define CHANNEL_NUMBER 7

#define ANALOG_INPUTS 4
#define DIGITAL_INPUTS 2

#define FRAME_LENGTH 22500  //set the PPM frame length in microseconds (1ms = 1000µs)
#define PULSE_LENGTH 300  //set the pulse length
#define onState 1  //set polarity of the pulses: 1 is positive, 0 is negative
#define sigPin 3  //set PPM signal output pin on the arduino

int analogInputPins[ANALOG_INPUTS] = {A2, A3, A1, A0};
int digitalInputPins[DIGITAL_INPUTS] = {12, 11};
int switchPins[1] = {10};

int channel_input[ANALOG_INPUTS] = {};
int channel_center[ANALOG_INPUTS] = {};
int channel_min[ANALOG_INPUTS] = {};
int channel_max[ANALOG_INPUTS] = {};

int ppm[CHANNEL_NUMBER];

int prevSwitch = LOW;
byte currentState = LOW;

void setup() {
  delay(300);
  Serial.begin(57600);
  
  //initiallize default ppm values
  for (int i=0; i<CHANNEL_NUMBER; i++){
      ppm[i]= CHANNEL_MID;
  }

  ppm[CHANNEL_NUMBER - 1] = 1000;

  for (int i = 0; i < sizeof(digitalInputPins); i++) {
    pinMode(digitalInputPins[i], INPUT_PULLUP);
  }

  for (int i = 0; i < ANALOG_INPUTS; i++) {
    channel_center[i] = analogRead(analogInputPins[i]);
  }

  pinMode(switchPins[0], INPUT_PULLUP);

  pinMode(sigPin, OUTPUT);
  digitalWrite(sigPin, !onState);  //set the PPM signal pin to the default state (off)

  cli();
  TCCR1A = 0; // set entire TCCR1 register to 0
  TCCR1B = 0;
  
  OCR1A = 100;  // compare match register, change this
  TCCR1B |= (1 << WGM12);  // turn on CTC mode
  TCCR1B |= (1 << CS11);  // 8 prescaler: 0,5 microseconds at 16mhz
  TIMSK1 |= (1 << OCIE1A); // enable timer compare interrupt
  sei();
}

void process_analog_channel(int channel) {
  channel_input[channel] = analogRead(analogInputPins[channel]);

  int diff = channel_center[channel] - channel_input[channel];

  if (diff > channel_max[channel]) {
    channel_max[channel] = diff;
  }

  if (diff < channel_min[channel]) {
    channel_min[channel] = diff;
  }

  if (diff > 0) {
    ppm[channel] = map(diff, 0, channel_max[channel], CHANNEL_MID, CHANNEL_MAX);
  } else {
    ppm[channel] = map(diff, channel_min[channel], 0, CHANNEL_MIN, CHANNEL_MID);
  }
  
}

void loop() {
  
  for (int i = 0; i < ANALOG_INPUTS; i++) {
    process_analog_channel(i);
  }

  for (int i = 0; i < DIGITAL_INPUTS; i++) {
    if (digitalRead(digitalInputPins[i]) == LOW) {
      ppm[i + ANALOG_INPUTS] = 2000;
    } else {
      ppm[i + ANALOG_INPUTS] = 1000;
    }
  }

  int swPin = digitalRead(switchPins[0]);

  if (swPin == LOW && prevSwitch == HIGH) {
    currentState = !currentState;
  }

  if (currentState == HIGH) {
    ppm[CHANNEL_NUMBER - 1] = 2000;
  } else {
    ppm[CHANNEL_NUMBER - 1] = 1000;
  }

  prevSwitch = swPin;
  
  delay(1000 / INPUT_FREQUENCY);

}

ISR(TIMER1_COMPA_vect){  //leave this alone
  static boolean state = true;
  
  TCNT1 = 0;
  
  if (state) {  //start pulse
    digitalWrite(sigPin, onState);
    OCR1A = PULSE_LENGTH * 2;
    state = false;
  } else{  //end pulse and calculate when to start the next pulse
    static byte cur_chan_numb;
    static unsigned int calc_rest;
  
    digitalWrite(sigPin, !onState);
    state = true;

    if(cur_chan_numb >= CHANNEL_NUMBER){
      cur_chan_numb = 0;
      calc_rest = calc_rest + PULSE_LENGTH;// 
      OCR1A = (FRAME_LENGTH - calc_rest) * 2;
      calc_rest = 0;
    }
    else{
      OCR1A = (ppm[cur_chan_numb] - PULSE_LENGTH) * 2;
      calc_rest = calc_rest + ppm[cur_chan_numb];
      cur_chan_numb++;
    }     
  }
}
