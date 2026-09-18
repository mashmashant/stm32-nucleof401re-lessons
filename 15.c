#include <stdint.h>

/* ======================== RCC ======================== */

#define RCC_AHB1ENR   (*(volatile uint32_t *)0x40023830U)
#define RCC_APB1ENR   (*(volatile uint32_t *)0x40023840U)

/* ======================= GPIOA ======================= */

#define GPIOA_MODER   (*(volatile uint32_t *)0x40020000U)
#define GPIOA_AFRL    (*(volatile uint32_t *)0x40020020U)

/* ======================= USART2 ====================== */

#define USART2_SR     (*(volatile uint32_t *)0x40004400U)
#define USART2_DR     (*(volatile uint32_t *)0x40004404U)
#define USART2_BRR    (*(volatile uint32_t *)0x40004408U)
#define USART2_CR1    (*(volatile uint32_t *)0x4000440CU)

/* ======================== TIM2 ======================= */

#define TIM2_CR1      (*(volatile uint32_t *)0x40000000U)
#define TIM2_EGR      (*(volatile uint32_t *)0x40000014U)
#define TIM2_CCMR1    (*(volatile uint32_t *)0x40000018U)
#define TIM2_CCER     (*(volatile uint32_t *)0x40000020U)
#define TIM2_PSC      (*(volatile uint32_t *)0x40000028U)
#define TIM2_ARR      (*(volatile uint32_t *)0x4000002CU)
#define TIM2_CCR1     (*(volatile uint32_t *)0x40000034U)

/* ====================== Константи ==================== */

#define PWM_MAX_VALUE     999U
#define PWM_STEP          5U

/* ======================== Delay ====================== */

static void delay(volatile uint32_t count)
{
    while (count > 0U)
    {
        count--;
        __asm volatile ("nop");
    }
}

/* ======================== UART ======================= */

static void uart_send_char(char character)
{
    while ((USART2_SR & (1U << 7)) == 0U)
    {
    }

    USART2_DR = (uint32_t)character;
}

static void uart_send_string(const char *text)
{
    while (*text != '\0')
    {
        uart_send_char(*text);
        text++;
    }
}

static void uart_send_uint(uint32_t value)
{
    char buffer[10];
    uint32_t index = 0U;

    if (value == 0U)
    {
        uart_send_char('0');
        return;
    }

    while (value > 0U)
    {
        buffer[index] = (char)('0' + (value % 10U));
        value /= 10U;
        index++;
    }

    while (index > 0U)
    {
        index--;
        uart_send_char(buffer[index]);
    }
}

static void uart_init(void)
{
    /* Увімкнути тактування GPIOA та USART2 */
    RCC_AHB1ENR |= (1U << 0);
    RCC_APB1ENR |= (1U << 17);

    /*
     * PA2 — режим альтернативної функції.
     * MODER2 = 10.
     */
    GPIOA_MODER &= ~(3U << 4);
    GPIOA_MODER |=  (2U << 4);

    /*
     * PA2 — AF7, USART2_TX.
     */
    GPIOA_AFRL &= ~(0xFU << 8);
    GPIOA_AFRL |=  (7U << 8);

    /*
     * Частота APB1 = 16 MHz.
     * Швидкість = 115200 біт/с.
     */
    USART2_BRR = 0x008BU;

    /*
     * TE = 1 — передавач увімкнений.
     * UE = 1 — USART2 увімкнений.
     */
    USART2_CR1 = (1U << 3) | (1U << 13);
}

/* ========================= PWM ======================= */

static void pwm_init(void)
{
    /* Увімкнути тактування GPIOA та TIM2 */
    RCC_AHB1ENR |= (1U << 0);
    RCC_APB1ENR |= (1U << 0);

    /*
     * PA5 — альтернативна функція.
     * MODER5 = 10.
     */
    GPIOA_MODER &= ~(3U << 10);
    GPIOA_MODER |=  (2U << 10);

    /*
     * PA5 — AF1, TIM2_CH1.
     */
    GPIOA_AFRL &= ~(0xFU << 20);
    GPIOA_AFRL |=  (1U << 20);

    /*
     * Частота таймера:
     *
     * fTIM = 16 MHz
     * PSC = 15
     * ARR = 999
     *
     * fPWM = 16 000 000 /
     *        ((15 + 1) × (999 + 1))
     *
     * fPWM = 1000 Hz.
     */
    TIM2_PSC = 15U;
    TIM2_ARR = PWM_MAX_VALUE;

    /* Початково LED вимкнений */
    TIM2_CCR1 = 0U;

    /*
     * OC1M = 110 — PWM mode 1.
     * OC1PE = 1 — preload register CCR1.
     */
    TIM2_CCMR1 &= ~((7U << 4) | (1U << 3));
    TIM2_CCMR1 |=  (6U << 4) | (1U << 3);

    /*
     * CC1E = 1 — увімкнути вихід TIM2_CH1.
     */
    TIM2_CCER |= (1U << 0);

    /*
     * ARPE = 1 — буферизація ARR.
     */
    TIM2_CR1 |= (1U << 7);

    /*
     * Згенерувати подію оновлення.
     */
    TIM2_EGR = (1U << 0);

    /*
     * CEN = 1 — запустити TIM2.
     */
    TIM2_CR1 |= (1U << 0);
}

/* ========================= Main ====================== */

int main(void)
{
    uint32_t pwm_value = 0U;
    uint32_t brightness_percent;
    uint32_t report_counter = 0U;
    int32_t direction = 1;

    uart_init();
    pwm_init();

    uart_send_string("\r\n");
    uart_send_string("STM32F401RE breathing LED\r\n");
    uart_send_string("LD2: PA5 / TIM2_CH1\r\n");
    uart_send_string("PWM frequency: 1000 Hz\r\n\r\n");

    while (1)
    {
        /*
         * Запис поточного коефіцієнта
         * заповнення в регістр CCR1.
         */
        TIM2_CCR1 = pwm_value;

        /*
         * Розрахунок яскравості у відсотках.
         *
         * PWM_MAX_VALUE + 1 = 1000.
         */
        brightness_percent =
            ((pwm_value + 1U) * 100U) /
            (PWM_MAX_VALUE + 1U);

        /*
         * Виводимо інформацію не на кожному кроці,
         * щоб не перевантажувати термінал.
         */
        report_counter++;

        if (report_counter >= 20U)
        {
            report_counter = 0U;

            uart_send_string("PWM CCR1 = ");
            uart_send_uint(pwm_value);

            uart_send_string(" | Brightness = ");
            uart_send_uint(brightness_percent);

            uart_send_string(" % | Direction: ");

            if (direction > 0)
            {
                uart_send_string("UP\r\n");
            }
            else
            {
                uart_send_string("DOWN\r\n");
            }
        }

        /*
         * Збільшення яскравості.
         */
        if (direction > 0)
        {
            if (pwm_value < (PWM_MAX_VALUE - PWM_STEP))
            {
                pwm_value += PWM_STEP;
            }
            else
            {
                pwm_value = PWM_MAX_VALUE;
                direction = -1;
            }
        }
        /*
         * Зменшення яскравості.
         */
        else
        {
            if (pwm_value > PWM_STEP)
            {
                pwm_value -= PWM_STEP;
            }
            else
            {
                pwm_value = 0U;
                direction = 1;
            }
        }

        delay(40000U);
    }
}
