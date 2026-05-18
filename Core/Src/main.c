#include <stdint.h>

/* ================================
STM32F303 Register Base Addresses
================================ */
#define RCC_BASE 0x40021000UL
#define GPIOA_BASE 0x48000000UL
#define GPIOB_BASE 0x48000400UL
#define USART2_BASE 0x40004400UL
#define SYSCFG_BASE 0x40010000UL
#define EXTI_BASE 0x40010400UL

/* RCC Registers */
#define RCC_AHBENR (*(volatile uint32_t *)(RCC_BASE + 0x14))
#define RCC_APB2ENR (*(volatile uint32_t *)(RCC_BASE + 0x18))
#define RCC_APB1ENR (*(volatile uint32_t *)(RCC_BASE + 0x1C))

/* GPIOA Registers */
#define GPIOA_MODER (*(volatile uint32_t *)(GPIOA_BASE + 0x00))
#define GPIOA_PUPDR (*(volatile uint32_t *)(GPIOA_BASE + 0x0C))
#define GPIOA_IDR (*(volatile uint32_t *)(GPIOA_BASE + 0x10))
#define GPIOA_ODR (*(volatile uint32_t *)(GPIOA_BASE + 0x14))
#define GPIOA_AFRL (*(volatile uint32_t *)(GPIOA_BASE + 0x20))

/* GPIOB Registers */
#define GPIOB_MODER (*(volatile uint32_t *)(GPIOB_BASE + 0x00))
#define GPIOB_ODR (*(volatile uint32_t *)(GPIOB_BASE + 0x14))

/* USART2 Registers */
#define USART2_CR1 (*(volatile uint32_t *)(USART2_BASE + 0x00))
#define USART2_BRR (*(volatile uint32_t *)(USART2_BASE + 0x0C))
#define USART2_ISR (*(volatile uint32_t *)(USART2_BASE + 0x1C))
#define USART2_TDR (*(volatile uint32_t *)(USART2_BASE + 0x28))

/* SYSCFG / EXTI Registers */
#define SYSCFG_EXTICR1 (*(volatile uint32_t *)(SYSCFG_BASE + 0x08))

#define EXTI_IMR (*(volatile uint32_t *)(EXTI_BASE + 0x00))
#define EXTI_RTSR (*(volatile uint32_t *)(EXTI_BASE + 0x08))
#define EXTI_FTSR (*(volatile uint32_t *)(EXTI_BASE + 0x0C))
#define EXTI_PR (*(volatile uint32_t *)(EXTI_BASE + 0x14))

/* NVIC Register */
#define NVIC_ISER0 (*(volatile uint32_t *)(0xE000E100UL))

/* ================================
Pin Assignments
================================ */
#define TRIG_PIN 0 // PA0 / A0
#define ECHO_PIN 1 // PA1 / A1

/*
* Button is on A2.
* On NUCLEO-F303K8, A2 maps to PA3.
*/
#define BUTTON_PIN 3 // PA3 / A2 calibration button

#define LED_PIN 3 // PB3 / D13 / LD3
#define BUZZER_PIN 4 // PB4 / D12

/* ================================
Project Thresholds
================================ */
#define MAX_VALID_ECHO 300000

/*
* Calibration-based conversion:
* Around 3200 echo counts was measured around 50 cm.
*/
#define CAL_COUNTS_AT_50CM 3200

/*
* If current distance is this many cm larger than baseline,
* classify it as a possible drop / negative obstacle.
*/
#define DELTA_THRESHOLD_CM 10

/*
* Number of consecutive hazard readings required before alert.
* Higher = fewer false alarms.
*/
#define HAZARD_HITS_REQUIRED 5

/* ================================
Global Interrupt Flag
================================ */
volatile int button_interrupt_flag = 0;

/* ================================
Delay
================================ */
void delay(volatile uint32_t count)
{
while (count--)
{
__asm__("nop");
}
}

/* ================================
LED / Buzzer Control
================================ */
void led_on(void)
{
GPIOB_ODR |= (1 << LED_PIN);
}

void led_off(void)
{
GPIOB_ODR &= ~(1 << LED_PIN);
}

void buzzer_on(void)
{
GPIOB_ODR |= (1 << BUZZER_PIN);
}

void buzzer_off(void)
{
GPIOB_ODR &= ~(1 << BUZZER_PIN);
}

void alert_on(void)
{
led_on();
buzzer_on();
}

void alert_off(void)
{
led_off();
buzzer_off();
}

/* ================================
UART2 Functions
================================ */
void uart2_init(void)
{
/* Enable GPIOA clock */
RCC_AHBENR |= (1 << 17);

/* Enable USART2 clock on APB1 */
RCC_APB1ENR |= (1 << 17);

/* PA2 = USART2_TX */
GPIOA_MODER &= ~(3 << (2 * 2));
GPIOA_MODER |= (2 << (2 * 2)); // alternate function mode

/* Set PA2 alternate function to AF7 for USART2 */
GPIOA_AFRL &= ~(0xF << (2 * 4));
GPIOA_AFRL |= (7 << (2 * 4));

/*
* Baud rate.
* Assuming default 8 MHz clock:
* 8,000,000 / 115200 ≈ 69
*/
USART2_BRR = 69;

/* Enable transmitter and USART */
USART2_CR1 |= (1 << 3); // TE
USART2_CR1 |= (1 << 0); // UE
}

void uart2_write_char(char c)
{
while (!(USART2_ISR & (1 << 7)))
{
/* Wait until TXE is set */
}

USART2_TDR = c;
}

void uart2_write_string(const char *str)
{
while (*str)
{
uart2_write_char(*str++);
}
}

void uart2_write_number(uint32_t num)
{
char buffer[12];
int i = 0;

if (num == 0)
{
uart2_write_char('0');
return;
}

while (num > 0)
{
buffer[i++] = '0' + (num % 10);
num /= 10;
}

while (i > 0)
{
uart2_write_char(buffer[--i]);
}
}

/* ================================
GPIO Setup
================================ */
void setup_gpio(void)
{
/* Enable GPIOA and GPIOB clocks */
RCC_AHBENR |= (1 << 17); // GPIOA clock
RCC_AHBENR |= (1 << 18); // GPIOB clock

/* PA0 output for HC-SR04 TRIG */
GPIOA_MODER &= ~(3 << (TRIG_PIN * 2));
GPIOA_MODER |= (1 << (TRIG_PIN * 2));

/* PA1 input for HC-SR04 ECHO */
GPIOA_MODER &= ~(3 << (ECHO_PIN * 2));

/*
* PA3 input for calibration button.
* Button wiring:
* A2 / PA3 ---- button ---- GND
*
* Internal pull-up enabled:
* Not pressed = HIGH
* Pressed = LOW
*/
GPIOA_MODER &= ~(3 << (BUTTON_PIN * 2));

GPIOA_PUPDR &= ~(3 << (BUTTON_PIN * 2));
GPIOA_PUPDR |= (1 << (BUTTON_PIN * 2)); // pull-up

/* PB3 output for onboard LED */
GPIOB_MODER &= ~(3 << (LED_PIN * 2));
GPIOB_MODER |= (1 << (LED_PIN * 2));

/* PB4 output for buzzer */
GPIOB_MODER &= ~(3 << (BUZZER_PIN * 2));
GPIOB_MODER |= (1 << (BUZZER_PIN * 2));

/* Start outputs low/off */
GPIOA_ODR &= ~(1 << TRIG_PIN);
alert_off();
}

/* ================================
EXTI Setup for Button on PA3
================================ */
void setup_button_exti(void)
{
/*
* Button is on PA3 / A2.
* Not pressed = HIGH due to pull-up.
* Pressed = LOW.
* Therefore, use falling edge interrupt.
*/

/* Enable SYSCFG clock on APB2 */
RCC_APB2ENR |= (1 << 0);

/*
* EXTI3 source selection:
* EXTI3 uses SYSCFG_EXTICR1 bits [15:12].
* Port A = 0000, so clear bits for PA3.
*/
SYSCFG_EXTICR1 &= ~(0xF << 12);

/* Unmask EXTI line 3 */
EXTI_IMR |= (1 << BUTTON_PIN);

/* Falling edge trigger enabled */
EXTI_FTSR |= (1 << BUTTON_PIN);

/* Rising edge trigger disabled */
EXTI_RTSR &= ~(1 << BUTTON_PIN);

/* Clear pending EXTI3 interrupt */
EXTI_PR |= (1 << BUTTON_PIN);

/*
* Enable EXTI3 interrupt in NVIC.
* EXTI3 IRQ number is 9 on STM32F303.
*/
NVIC_ISER0 |= (1 << 9);
}

/* ================================
EXTI3 Interrupt Handler
================================ */
void EXTI3_IRQHandler(void)
{
if (EXTI_PR & (1 << BUTTON_PIN))
{
/* Clear pending interrupt */
EXTI_PR |= (1 << BUTTON_PIN);

/* Set flag for main loop */
button_interrupt_flag = 1;
}
}

/* ================================
Button Helper
================================ */
int button_pressed(void)
{
/*
* Active-low button:
* pressed = PA3 reads 0
*/
return ((GPIOA_IDR & (1 << BUTTON_PIN)) == 0);
}

void wait_for_button_release(void)
{
while (button_pressed())
{
delay(100000);
}

/* Extra debounce delay */
delay(500000);
}

/* ================================
HC-SR04 Ultrasonic Reading
================================ */
uint32_t get_echo_count(void)
{
uint32_t timeout = 0;
uint32_t count = 0;

/* Start TRIG low */
GPIOA_ODR &= ~(1 << TRIG_PIN);
delay(1000);

/* Send trigger pulse */
GPIOA_ODR |= (1 << TRIG_PIN);
delay(3000);
GPIOA_ODR &= ~(1 << TRIG_PIN);

/* Wait for ECHO to go HIGH */
timeout = 0;
while (((GPIOA_IDR & (1 << ECHO_PIN)) == 0) && timeout < 500000)
{
timeout++;
}

if (timeout >= 500000)
{
return 0; // no echo
}

/* Count how long ECHO stays HIGH */
count = 0;
while ((GPIOA_IDR & (1 << ECHO_PIN)) && count < MAX_VALID_ECHO)
{
count++;
}

return count;
}

/* ================================
Echo Count to Estimated Distance
================================ */
uint32_t echo_count_to_cm(uint32_t echo_count)
{
/*
* Calibration-based conversion:
* Around 3200 counts was measured at 50 cm.
*
* distance_cm = echo_count * 50 / 3200
*/
return (echo_count * 50) / CAL_COUNTS_AT_50CM;
}

/* ================================
Baseline Calibration
================================ */
uint32_t calibrate_baseline(void)
{
uint32_t baseline_sum = 0;
uint32_t valid_samples = 0;

alert_off();

uart2_write_string("\r\nCalibrating baseline...\r\n");
uart2_write_string("Hold sensor steady at the SAFE floor/table distance.\r\n");

for (int i = 0; i < 15; i++)
{
uint32_t echo_count = get_echo_count();
uint32_t distance_cm = echo_count_to_cm(echo_count);

uart2_write_string("Calibration sample ");
uart2_write_number(i + 1);
uart2_write_string(": ");

/*
* Keep only realistic baseline samples.
* Readings like 463 cm are often outliers/reflections.
*/
if (echo_count > 0 && distance_cm > 0 && distance_cm < 150)
{
baseline_sum += distance_cm;
valid_samples++;

uart2_write_number(distance_cm);
uart2_write_string(" cm accepted\r\n");
}
else
{
uart2_write_number(distance_cm);
uart2_write_string(" cm rejected\r\n");
}

delay(300000);
}

if (valid_samples > 0)
{
uint32_t baseline = baseline_sum / valid_samples;

uart2_write_string("New Baseline Distance: ");
uart2_write_number(baseline);
uart2_write_string(" cm\r\n\r\n");

return baseline;
}
else
{
uart2_write_string("Calibration failed. Using fallback baseline: 50 cm\r\n\r\n");
return 50;
}
}

/* ================================
Main Program
================================ */
int main(void)
{
setup_gpio();
uart2_init();
setup_button_exti();

uart2_write_string("\r\n==============================\r\n");
uart2_write_string("Haptic Guide Ready\r\n");
uart2_write_string("Mode: Delta-Y Drop Detection with EXTI Button\r\n");
uart2_write_string("==============================\r\n");
uart2_write_string("Button function: EXTI interrupt recalibrates baseline\r\n");
uart2_write_string("Wire button: A2/PA3 ---- button ---- GND\r\n\r\n");

uint32_t baseline_distance_cm = calibrate_baseline();

int hazard_hits = 0;

while (1)
{
/*
* EXTI button interrupt check.
* The interrupt handler sets button_interrupt_flag.
* Main loop performs calibration safely.
*/
if (button_interrupt_flag)
{
button_interrupt_flag = 0;

alert_off();
hazard_hits = 0;

uart2_write_string("\r\nEXTI BUTTON INTERRUPT DETECTED\r\n");
uart2_write_string("Recalibrating baseline now...\r\n");

baseline_distance_cm = calibrate_baseline();

uart2_write_string("Updated baseline is now: ");
uart2_write_number(baseline_distance_cm);
uart2_write_string(" cm\r\n");

uart2_write_string("Recalibration complete.\r\n");

wait_for_button_release();
continue;
}

uint32_t echo_count = get_echo_count();
uint32_t distance_cm = echo_count_to_cm(echo_count);

uart2_write_string("Distance: ");
uart2_write_number(distance_cm);
uart2_write_string(" cm | Baseline: ");
uart2_write_number(baseline_distance_cm);
uart2_write_string(" cm");

if (echo_count == 0)
{
hazard_hits++;

uart2_write_string(" | Delta: NO ECHO\r\n");
uart2_write_string("Status: POSSIBLE DROP - NO SURFACE DETECTED\r\n");

if (hazard_hits >= HAZARD_HITS_REQUIRED)
{
alert_on();
uart2_write_string("ALERT: Drop-off detected\r\n");
}
}
else
{
uint32_t delta_cm = 0;

if (distance_cm > baseline_distance_cm)
{
delta_cm = distance_cm - baseline_distance_cm;
}
else
{
delta_cm = 0;
}

uart2_write_string(" | Delta: ");
uart2_write_number(delta_cm);
uart2_write_string(" cm\r\n");

if (delta_cm >= DELTA_THRESHOLD_CM)
{
hazard_hits++;

uart2_write_string("Status: NEGATIVE OBSTACLE / DROP DETECTED\r\n");

if (hazard_hits >= HAZARD_HITS_REQUIRED)
{
alert_on();
uart2_write_string("ALERT: Significant delta-y drop detected\r\n");
}
}
else
{
hazard_hits = 0;
alert_off();

uart2_write_string("Status: Floor baseline safe\r\n");
}
}

delay(300000);
}
}
