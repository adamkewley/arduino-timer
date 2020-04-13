// Pins for 74HC595. Controls which segments of 4
// seven-segment displays are enabled
#define PIN_SER 12
#define PIN_RCLK 11
#define PIN_SRCLK 10

// Ground pins for the 4 seven-segment displays. Controls
// which of the four are enabled/disabled.
#define PIN_DIGIT0_GND 9
#define PIN_DIGIT1_GND 8
#define PIN_DIGIT2_GND 7
#define PIN_DIGIT3_GND 6

// User control buttons
#define PIN_PROGRAM_BTN 2  // this pin must support ISRs
#define PIN_ADD_MIN_BTN 5
#define PIN_ADD_SEC_BTN 4

// Goes high during alarm
#define PIN_ALARM 3


// A 74HC595 serial-to-parallel shift register.
//
// At the hardware level, the Arduino can "transmit" a single byte 
// to the storage area of the IC. Once transmitted, the Arduino can
// "flip" the byte from the storage area to the shift register, which
// will enable/disable 8 output pins on the IC, based on which bits were
// set in the transmitted byte.
class ic_74hc595 final {
public:
  ic_74hc595(int _pin_ser, int _pin_rclk, int _pin_srclk) :
    pin_ser{_pin_ser}, pin_rclk{_pin_rclk}, pin_srclk{_pin_srclk} {
      
    pinMode(this->pin_ser, OUTPUT);
    digitalWrite(this->pin_ser, LOW);
      
    pinMode(this->pin_rclk, OUTPUT);
    digitalWrite(this->pin_rclk, LOW);
      
    pinMode(this->pin_srclk, OUTPUT);
    digitalWrite(this->pin_srclk, LOW);
  }

  void transmit(byte b) {
    shiftOut(this->pin_ser, this->pin_srclk, MSBFIRST, b);
  }

  void flip() {
    digitalWrite(this->pin_rclk, HIGH);
    digitalWrite(this->pin_rclk, LOW);
  }
  
private:
  int pin_ser;
  int pin_rclk;
  int pin_srclk;
};

// Low-level abstraction of a four-digit 7-segment display, assembled 
// from a 74HC595 SR and a prefabricated 4-digit 7-seg display.
//
// The display contains 7 input pins and 4 output pins. The input pins
// enable/disable segments in *all* digits of the display. The output 
// pins enable/disable each of the 4 digits in the display.
// 
// Because of this pin arrangement, this display can only display the same
// number (permutation of segments) for *all* activated (output == grounded)
// digits in the display. In order to display four different digits, users of
// this class should repeatably call `show` with each digit that should be 
// shown by the display - if called frequently enough, the animation effect
// will make it appear as though all four digits are silmultanously displaying
// different numbers (it can't, but human eyes can't tell the difference).
class four_digit_7seg final {
public:
  four_digit_7seg(ic_74hc595 _sr, int _pin_digit0, int _pin_digit1, int _pin_digit2, int _pin_digit3) :
    sr{_sr}, pin_digit0{_pin_digit0}, pin_digit1{_pin_digit1}, pin_digit2{_pin_digit2}, pin_digit3{_pin_digit3} {      
      pinMode(this->pin_digit0, OUTPUT);
      digitalWrite(this->pin_digit0, LOW);

      pinMode(this->pin_digit1, OUTPUT);
      digitalWrite(this->pin_digit1, LOW);
      
      pinMode(this->pin_digit2, OUTPUT);
      digitalWrite(this->pin_digit2, LOW);

      pinMode(this->pin_digit3, OUTPUT);
      digitalWrite(this->pin_digit3, LOW);
  }

  void show(byte v, int digit0, int digit1, int digit2, int digit3) {
      this->transmit(v);
      this->set_digit_states(HIGH, HIGH, HIGH, HIGH);
      this->flip();
      this->set_digit_states(digit0, digit1, digit2, digit3);
  }
    
private:
  void transmit(byte b) {
    this->sr.transmit(b);
  }
  
  void flip() {
    this->sr.flip();
  }

  void set_digit_states(int digit0, int digit1, int digit2, int digit3) {
    digitalWrite(this->pin_digit0, digit0);
    digitalWrite(this->pin_digit1, digit1);
    digitalWrite(this->pin_digit2, digit2);
    digitalWrite(this->pin_digit3, digit3);
  }

  ic_74hc595 sr;
  int pin_digit0;
  int pin_digit1;
  int pin_digit2;
  int pin_digit3;
};

// Higher-level non-blocking abstraction over a 4-digit display that
// can only display one digit at a time.
//
// Users of this class can "set" a multi-digit number as the value that
// the display should show. Users then must call `tick()` frequently 
// enough to ensure that the display shows all digits of the number without
// lag.
class nonblocking_4digit_display {
public:
  nonblocking_4digit_display(four_digit_7seg _disp) : disp{_disp} {
  }

  void set(unsigned v) {
    // How numerals correspond to bytes that should be sent to
    // the display via the shift register.
    constexpr byte digits[] = {
      0b11101011,
      0b01001000,
      0b11010011,
      0b11011010,
      0b01111000,
      0b10111010,
      0b10111011,
      0b11001000,
      0b11111011,
      0b11111000,
    };

    const byte dot = this->showing_dots ? 0b00000100 : 0b00000000;

    this->d1 = digits[(v/1000) % 10] | dot;
    this->d2 = digits[(v/100) % 10] | dot;
    this->d3 = digits[(v/10) % 10] | dot;
    this->d4 = digits[v % 10] | dot;
  }

  void show_dots() {
    this->showing_dots = true;
  }

  void hide_dots() {
    this->showing_dots = false;
  }

  void tick() {    
    switch ((this->state++) % 4) {
      case 0:
        this->disp.show(this->d1, LOW, HIGH, HIGH, HIGH);
        break;
      case 1:
        this->disp.show(this->d2, HIGH, LOW, HIGH, HIGH);
        break;
      case 2:
        this->disp.show(this->d3, HIGH, HIGH, LOW, HIGH);
        break;
      case 3: 
        this->disp.show(this->d4, HIGH, HIGH, HIGH, LOW);
    }
  }
private:
  four_digit_7seg disp;
  unsigned char state = 0;
  byte d1 = 0x00;
  byte d2 = 0x00;
  byte d3 = 0x00;
  byte d4 = 0x00;
  bool showing_dots = false;
};

class debouncer {
public:
  debouncer(unsigned long _window_millis) : 
    window_millis{_window_millis} {
  }

  template<typename Func>
  void exec_debounced(Func f) {
    auto t = millis();
    
    if (t >= this->debounce_end) {
      f();
      this->debounce_end = t + this->window_millis;
    }
  }

  void reset() {
    this->debounce_end = 0;
  }
private:
  const unsigned long window_millis;
  unsigned long debounce_end = 0;
};

enum class Clock_state {
  programming,
  counting_down,
  alarming,
};


// GLOBAL STATE: mutated by ISRs, setup(), and loop()
nonblocking_4digit_display disp{{ 
  {PIN_SER, PIN_RCLK, PIN_SRCLK}, 
  PIN_DIGIT0_GND, 
  PIN_DIGIT1_GND, 
  PIN_DIGIT2_GND, 
  PIN_DIGIT3_GND 
}};
Clock_state clock_state = Clock_state::programming;
unsigned long secs_remaining = 0;
unsigned long alarm_start = 0;
debouncer add_time_debouncer{1000};
debouncer program_button_debouncer{200};


void enter_programming_state() {
  clock_state = Clock_state::programming;
  secs_remaining = 0;
  disp.show_dots();
  disp.set(secs_remaining);
}

void enter_counting_down_state() {
  clock_state = Clock_state::counting_down;
  disp.hide_dots();
  disp.set(secs_remaining);
}

void enter_alarming_state() {
  clock_state = Clock_state::alarming;
  alarm_start = millis();
  secs_remaining = 0;
}

// 1 Hz timer ISR that "ticks" time down while the clock is
// in the counting_down state
ISR(TIMER1_COMPA_vect){
  switch (clock_state) {
    case Clock_state::counting_down:
      if (secs_remaining > 0) {
        disp.set(--secs_remaining);
        if (secs_remaining == 0) {
          enter_alarming_state();
        }
      }
      break;
    case Clock_state::alarming:
      digitalWrite(PIN_ALARM, !digitalRead(PIN_ALARM));
      break;
  }
}

// 2 kHz timer ISR that "ticks" the nonblocking display
ISR(TIMER2_COMPA_vect){
  disp.tick();
}

// ISR that triggers whenever the user presses the "program" 
// button
void isr_program_button() {
  program_button_debouncer.exec_debounced([]() {
    switch (clock_state) {
      case Clock_state::programming:
        enter_counting_down_state();
        break;
      case Clock_state::counting_down:
        enter_programming_state();
        break;
      case Clock_state::alarming:
        digitalWrite(PIN_ALARM, LOW);
        enter_programming_state();
        break;
    }
  });
}

void setup() {
  enter_programming_state();
  
  cli();
  
  TCCR1A = 0;// set entire TCCR1A register to 0
  TCCR1B = 0;// same for TCCR1B
  TCNT1  = 0;//initialize counter value to 0
  // set compare match register for 1hz increments
  OCR1A = 15624;// = (16*10^6) / (1*1024) - 1 (must be <65536)
  // turn on CTC mode
  TCCR1B |= (1 << WGM12);
  // Set CS12 and CS10 bits for 1024 prescaler
  TCCR1B |= (1 << CS12) | (1 << CS10);  
  // enable timer compare interrupt
  TIMSK1 |= (1 << OCIE1A); 

  //set timer2 interrupt at 8kHz
  TCCR2A = 0;// set entire TCCR2A register to 0
  TCCR2B = 0;// same for TCCR2B
  TCNT2  = 0;//initialize counter value to 0
  // set compare match register for 2khz increments
  OCR2A = 124;// = (16*10^6) / (2000*64) - 1 (must be <256)
  // turn on CTC mode
  TCCR2A |= (1 << WGM21);
  // Set CS21 bit for 8 prescaler
  TCCR2B |= (1 << CS01) | (1 << CS00);
  // enable timer compare interrupt
  TIMSK2 |= (1 << OCIE2A);
  
  sei();

  pinMode(PIN_PROGRAM_BTN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(PIN_PROGRAM_BTN), isr_program_button, FALLING);
  pinMode(PIN_ADD_MIN_BTN, INPUT_PULLUP);
  pinMode(PIN_ADD_SEC_BTN, INPUT_PULLUP);
  pinMode(PIN_ALARM, OUTPUT);
  digitalWrite(PIN_ALARM, LOW);
}

void loop() {
  if (clock_state == Clock_state::programming) {
    if (digitalRead(PIN_ADD_MIN_BTN) == LOW) {
      add_time_debouncer.exec_debounced([]() {
        secs_remaining += 60;
        disp.set(secs_remaining);
      });
    } else if (digitalRead(PIN_ADD_SEC_BTN) == LOW) {
      add_time_debouncer.exec_debounced([]() {
        secs_remaining += 1;
        disp.set(secs_remaining);
      });
    } else {
      add_time_debouncer.reset();
    }    
  }
}
