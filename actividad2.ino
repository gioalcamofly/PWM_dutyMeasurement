#include <Arduino.h>
#include <stdint.h>
#include <avr/interrupt.h>



#define OC2A_PIN 11

#define TIMER1_OVERFLOW_VALUE ((uint64_t)(1<<10))
#define TIMER1_OVERFLOW_HALF_VALUE (1<<9)

volatile uint64_t timer1_overflows = 0;

#define TIMESTAMPS_LENGTH 40+1 // should be less than 256

#define SECS_PER_TIMER1_TICK (double(4e-6))
#define SECS_FRACTIONAL_DIGITS 6
#define HZ_FRACTIONAL_DIGITS 6

volatile uint64_t timestamps[TIMESTAMPS_LENGTH];
volatile uint8_t current_timestamp=0;
uint8_t prescaler;
uint8_t dutyCycle;
uint16_t prescaler_values[7] = {
  0,
  8,
  32,
  64,
  128,
  256,
  1024
};

void set_timer1() {
  TCCR1A |= (1<<(WGM11)) | (1<<(WGM10));

  TCCR1B |= (1<<(ICNC1)) | (1<<(WGM12)) | (1<<(CS11)) | (1<<(CS10));
  TCCR1B &= ~(1<<(CS12));
  TCCR1B &= ~(1<<(ICES1)) & ~(1<<(WGM13));


}

void set_timer2() {
  
  TCCR2A |= (1<<(WGM21)) | (1<<(WGM20));
  TCCR2A |= (1<<(COM2A1)) | (1<<(COM2B1));
  
  TCCR2B = prescaler;
  
  OCR2A = dutyCycle;
  
  TIFR1 |= (1<<(ICF1)) | (1<<(TOV1));
  TIMSK1 |= (1<<(ICIE1)) | (1<<(TOIE1));
}

void setup() {
  pinMode(OC2A_PIN, OUTPUT);

  Serial.begin(9600);

  printSetDutyCycle();
  printSetPrescaler();
  
  
  set_timer1();
  set_timer2();

  interrupts();
}

void printSetPrescaler() {
  Serial.println("Please, enter the prescaler setting --> Prescaler [value]");
  while (!Serial.available()) {}
  String command = Serial.readStringUntil('\n');
  if (command.startsWith("Prescaler")) {
    uint16_t value = command.substring(10).toInt();
    Serial.print("Prescaler = "); Serial.println(value);
    for (int i = 0; i < 7; i++) {
      if (value == prescaler_values[i]) {
        prescaler = i + 1;
        return;  // Este valor se corresponde con el que los bits CS2x del timer 2 requiere
      } 
    }
    Serial.println("Bad Number. Please try again");
  }
  printSetPrescaler();
  
}

void printSetDutyCycle() {
  Serial.println("Please, enter the duty cycle (in percentage) --> Duty [0-100]");
  while (!Serial.available()) {}
  String command = Serial.readStringUntil('\n');
  if (command.startsWith("Duty")) {
    uint8_t value = command.substring(5).toInt();
    if (value >= 0 && value <= 100) {
      Serial.print("Duty Cycle = "); Serial.print(value); Serial.println("%");
      dutyCycle = map(value, 0, 100, 0, 255);
      return;
    }
  }
  Serial.println("Bad command. Please try again");
  printSetDutyCycle();
  
}

void loop() 
{
  if(current_timestamp==(TIMESTAMPS_LENGTH>>1))
  {
    Serial.print("----> "); Serial.print(TIMESTAMPS_LENGTH); Serial.println(" timestamps taken");
    
    uint64_t previous=*timestamps, current;
    uint8_t listCount = 1;
    for(uint8_t i=1; i<TIMESTAMPS_LENGTH; i++)
    {
      current=timestamps[i];
      if (!(i&0b00000001)) {
        uint64_t difference=current-previous;
        double difference_in_seconds= double(difference)*SECS_PER_TIMER1_TICK;
        double frequency=1/difference_in_seconds;
        
        Serial.print("  "); Serial.print(listCount); Serial.print(": "); 
        print_uint64_t(&current); Serial.print('-');
        print_uint64_t(&previous); Serial.print('=');
        print_uint64_t(&difference); Serial.print("(");
        Serial.print(difference_in_seconds,SECS_FRACTIONAL_DIGITS); 
        Serial.print(" s. <-> ");
        Serial.print(frequency,HZ_FRACTIONAL_DIGITS);
        Serial.println(" Hz)");
        listCount++;
      }
      previous=current;
    }
    Serial.println("----> continue? ");
    
    byte key=0;
    while( (key!='y') && (key!='Y') )
    {
      if(Serial.available()>0) key=Serial.read();
    }

    
    printSetDutyCycle();
    printSetPrescaler();
    
    
    noInterrupts();
    set_timer1();
    set_timer2();
    current_timestamp=0;

    interrupts();
    Serial.println("----> capturing signal ...");
    
    
  }
}  

void print_uint64_t(uint64_t* value)
{
  Serial.print("["); Serial.print(uint32_t((*value)>>32),HEX); Serial.print("][");
  Serial.print(uint32_t(*value),HEX); Serial.print("]");  
}
  

ISR(TIMER1_OVF_vect) {
  timer1_overflows +=TIMER1_OVERFLOW_VALUE;
}

ISR(TIMER1_CAPT_vect) {
  uint16_t icr1_register=ICR1;
  TCCR1B ^= (1<<(ICES1));
  TIFR1 |= (1<<(ICF1));

  if(current_timestamp<TIMESTAMPS_LENGTH) {
    timestamps[current_timestamp] = timer1_overflows + icr1_register;

    if ((TIFR1 & (1<<TOV1)) && (icr1_register<TIMER1_OVERFLOW_HALF_VALUE)) {
      timestamps[current_timestamp] += TIMER1_OVERFLOW_VALUE;
    }

    current_timestamp++;
  }
  
}

