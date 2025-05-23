#include <stdio.h>
#include <avr/io.h>
#include <util/delay.h>
#include "usart.h"
#include "i2cmaster.h"
#include "lcd.h"
#include <avr/interrupt.h>
#include <stdlib.h>

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

int main(void)
{
    // LCD init
    i2c_init();
    LCD_init();

    DDRD |= (1 << PD6) | (1 << PD7);


    EICRA |= (1 << ISC00); 
    EIMSK |= (1 << INT0);


    lcd_set_cursor(0, 0);
    lcd_print("Angle:");


    while (1)
    {
        angle = ((float)encoder_count / pulses_per_rev) * 360.0;

        char buffer[16];
        dtostrf(angle, 6, 2, buffer);

        char last_position[16];
        dtostrf(last_saved_angle, 6, 2, last_position);


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
