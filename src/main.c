#include <Arduino.h>


#include <stdio.h>
#include <avr/io.h>
#include <util/delay.h>

#define F_CPU 16000000UL
#include <avr/interrupt.h>
#include <stdlib.h>
#include "i2cmaster.h"
#include "lcd.h"  

volatile long encoder_count = 0;
volatile int8_t direction = 0;
float angle = 0.0;
float last_saved_angle = 0.0;
const float pulses_per_rev = 500.0; // Da go proverq che eba li mu ....

void motor_rotate_to(float target_angle);

void motor_stop() {
    PORTD &= ~((1 << PD6) | (1 << PD7));
}

void motor_forward() {
    PORTD |= (1 << PD6);
    PORTD &= ~(1 << PD7);
}

void motor_backward() {
    PORTD |= (1 << PD7);
    PORTD &= ~(1 << PD6);
}

// External interrupt on INT0 (PD2)
ISR(INT0_vect) {
    uint8_t b = PIND & (1 << PD4);
    direction = (b) ? -1 : 1;
    encoder_count += direction;
}

// Main
int main(void) {
    // LCD init
    i2c_init();
    LCD_init();
    LCD_backlight();
    LCD_clear();

    // Encoder pins
    DDRD &= ~((1 << PD2) | (1 << PD4)); 
    PORTD |= (1 << PD2) | (1 << PD4);   

    // Index pin (опционално)
    DDRD &= ~(1 << PD5);
    PORTD |= (1 << PD5);

    // Button pins
    DDRB &= ~((1 << PB0) | (1 << PB1));
    PORTB |= (1 << PB0) | (1 << PB1);  
    // Motor pins
    DDRD |= (1 << PD6) | (1 << PD7);

   
    EICRA |= (1 << ISC00);
    EIMSK |= (1 << INT0);  

    sei(); 

    LCD_set_cursor(0, 0);
    printf("Angle:");

    while (1) {
        // ъгъл - да питам чата за оптимизация на формулата!
        angle = ((float)encoder_count / pulses_per_rev) * 360.0;

        char buffer[16];
        dtostrf(angle, 6, 2, buffer);
        LCD_set_cursor(0, 1);
        printf("              "); 
        LCD_set_cursor(0, 1);
        printf("%s",buffer);
        printf(" deg");

    
        if (!(PINB & (1 << PB0))) {
            _delay_ms(50); 
            motor_rotate_to(last_saved_angle);
        }

        if (!(PINB & (1 << PB1))) {
            _delay_ms(50); 
            motor_rotate_to(0.0);
        }


        if (!(PIND & (1 << PD5))) {
            last_saved_angle = angle;
            _delay_ms(300); 
        }
    }
}

// clear the err, saved in file 28.05.2025 (dekstopa!!!!)

void motor_rotate_to(float target_angle) {
    float error;
    while (1) {
        error = target_angle - angle;

        if (fabs(error) < 1.0) break; 

        if (error > 0) {
            motor_forward();
        } else {
            motor_backward();
        }
    }
    motor_stop();
}

