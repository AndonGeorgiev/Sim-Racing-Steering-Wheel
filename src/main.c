#define BAUD 9600
#define BAUD_PRESCALER (((F_CPU / (BAUD * 16UL))) - 1)

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
const float pulses_per_rev = 500.0 * 4.0;
volatile uint8_t last_A = 0;
volatile uint8_t last_B = 0;
uint8_t speed;
unsigned long lastUpdate = 0;

ISR(INT0_vect)
{
    uint8_t A = (PIND >> PD2) & 1;
    uint8_t B = (PIND >> PD4) & 1;

    if (A != last_A)
    {
        if (A == B)
            encoder_count++;
        else
            encoder_count--;
        last_A = A;
    }
}

ISR(PCINT2_vect) // PCINT[23:16] for PORTD
{
    uint8_t A = (PIND >> PD2) & 1;
    uint8_t B = (PIND >> PD4) & 1;

    if (B != last_B)
    {
        if (A != B)
            encoder_count++;
        else
            encoder_count--;
        last_B = B;
    }
}

void pwm_init()
{
    DDRD |= (1 << PD6); 
    
    TCCR0A |= (1 << WGM01) | (1 << WGM00); 
    TCCR0A |= (1 << COM0A1);               
    TCCR0B |= (1 << CS01);                 
}

void motor_rotate_to(float target_angle);

void motor_stop()
{
    OCR0A = 0;
}

void motor_forward()
{
    OCR0A = speed;
    PORTD &= ~(1 << PD7);
}

void motor_backward()
{
    PORTD |= (1 << PD7);
    OCR0A = speed;
}

// ---------- UART ----------
void usart_init(void) 
{
    UBRR0H = (uint8_t)(BAUD_PRESCALER>>8);
    UBRR0L = (uint8_t)(BAUD_PRESCALER);
    UCSR0B = (1 << RXEN0) | (1 << TXEN0); // Enable RX and TX
    UCSR0C = (1 << UCSZ01) | (1 << UCSZ00); // 8-bit data
}

void USART_Transmit(unsigned char data) 
{
    while (!(UCSR0A & (1<<UDRE0)));
    UDR0 = data;
}

unsigned char USART_Receive(void) 
{
    while (!(UCSR0A & (1<<RXC0)));
    return UDR0;
}

void sendValToNextion(const char *component, int value) 
{
    char buffer[32];
    // Build command string: component.txt="value"
    sprintf(buffer, "%s.val=%d", component, value);
    
    // Send each character
    for (int i = 0; buffer[i] != '\0'; i++) 
	{
        USART_Transmit(buffer[i]);
    }

    // Send end-of-command bytes
    USART_Transmit(0xFF);
    USART_Transmit(0xFF);
    USART_Transmit(0xFF);
}

void handleButton(uint8_t id) 
{
    switch(id) 
	{
        case 0x01: // Set Center
			last_saved_angle = angle;
            sendValToNextion("CurrCenter", last_saved_angle);
            break;
        case 0x02: // Center Wheel
            motor_rotate_to(last_saved_angle);
            break;
        case 0x03: // Go to 0
            motor_rotate_to(0.0);
            break;
    }
}

void updateCurrAngle(void)
{
		uint32_t millis = (uint32_t) TCNT1;
		if (millis - lastUpdate >= 62500) //62500 250ms at 16M/256 pre
		{
    		lastUpdate = millis;

    		sendValToNextion("CurrAngle", angle); // Update current angle label
  		}
}

void receiveNextionInput(void)
{
	if (UCSR0A & (1<<RXC0)) 
	{
		if (USART_Receive() == 0x23) 
		{
			if (USART_Receive() == 0x02) 
			{
				uint8_t btn_id = USART_Receive();
				handleButton(btn_id);
			}
		}
	}
}

int main(void)
{
    usart_init();
    
	TCCR1B |= (1 << CS12);

    DDRD &= ~((1 << PD2) | (1 << PD4) | (1 << PD5));
    PORTD |= (1 << PD2) | (1 << PD4) | (1 << PD5);

    last_A = (PIND >> PD2) & 1;
    last_B = (PIND >> PD4) & 1;

    DDRC = 0xF0;
    PORTC = 0x3F;

    // motor pins
    DDRD |= (1 << PD6) | (1 << PD7);

    pwm_init();

    EICRA |= (1 << ISC00);
    EIMSK |= (1 << INT0);

    PCICR |= (1 << PCIE2);
    PCMSK2 |= (1 << PCINT20);

    sei();

    while (1)
    {
        angle = ((float)encoder_count / pulses_per_rev) * 360.0;

        char buffer[16];
        dtostrf(angle, 6, 2, buffer);

        char last_position[16];
        dtostrf(last_saved_angle, 6, 2, last_position);

        updateCurrAngle();
		receiveNextionInput();

        // if (!(PINC & (1 << PC0)))
        // {
        //     _delay_ms(50);
        //     motor_rotate_to(last_saved_angle);
        // }

        // if (!(PINC & (1 << PC1)))
        // {
        //     _delay_ms(50);
        //     motor_rotate_to(0.0);
        // }

        // if (!(PINC & (1 << PC2)))
        // {
        //     last_saved_angle = angle;
        //     _delay_ms(300);
        // }
    }
}

void motor_rotate_to(float target_angle)
{
    float error;
    float total_distance = fabs(target_angle - angle);
    float current_distance;

    while (1)
    {
        error = target_angle - angle;
        current_distance = fabs(error);

        if (current_distance < 1.0)
        {
            break;
        }

        if (total_distance < 0.2 * current_distance)
        {
            speed = 80;
        }
        else
        {
            speed = 180;
        }

        if (error > 0)
        {
            motor_forward();
        }
        else
        {
            motor_backward();
        }
    }
    motor_stop();
}


/*

motor driver to arduino conection 

ENA → PD6 (PWM)

IN1 → +12V 

IN2 → PD7 

*/