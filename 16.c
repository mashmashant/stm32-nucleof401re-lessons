#include <stdint.h>

/* ======================== RCC ======================== */

#define RCC_AHB1ENR   (*(volatile uint32_t *)0x40023830U)
#define RCC_APB1ENR   (*(volatile uint32_t *)0x40023840U)

/* ======================= GPIOA ======================= */

#define GPIOA_MODER   (*(volatile uint32_t *)0x40020000U)
#define GPIOA_AFRL    (*(volatile uint32_t *)0x40020020U)

/* ======================= GPIOC ======================= */

#define GPIOC_MODER   (*(volatile uint32_t *)0x40020800U)
#define GPIOC_PUPDR   (*(volatile uint32_t *)0x4002080CU)
#define GPIOC_IDR     (*(volatile uint32_t *)0x40020810U)

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

#define PWM_MAX_VALUE       999U
#define PWM_LEVEL_COUNT     5U

#define BUTTON_PIN          13U
#define BUTTON_MASK         (1U << BUTTON_PIN)

/*
 * Значення CCR1 для:
 *
 * 0%, 25%, 50%, 75%, 100%.
 *
 * При ARR = 999 період містить 1000 станів.
 * CCR1 = 1000 формує постійний високий рівень.
 */
static const uint32_t pwm_values[PWM_LEVEL_COUNT] =
{
    0U,
    250U,
    500U,
    750U,
    1000U
};

static const uint32_t brightness_values[PWM_LEVEL_COUNT] =
{
    0U,
    25U,
    50U,
    75U,
    100U
};

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
    /* Тактування GPIOA та USART2 */
    RCC_AHB1ENR |= (1U << 0);
    RCC_APB1ENR |= (1U << 17);

    /*
     * PA2 — альтернативна функція.
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
     * APB1 = 16 MHz.
     * Baud rate = 115200.
     */
    USART2_BRR = 0x008BU;

    /*
     * TE = 1 — передавач увімкнений.
     * UE = 1 — USART2 увімкнений.
     */
    USART2_CR1 = (1U << 3) | (1U << 13);
}

/* ======================== Button ===================== */

static void button_init(void)
{
    /* Увімкнути тактування GPIOC */
    RCC_AHB1ENR |= (1U << 2);

    /*
     * PC13 — цифровий вхід.
     * MODER13 = 00.
     */
    GPIOC_MODER &= ~(3U << 26);

    /*
     * PC13 — внутрішня підтяжка до землі.
     * PUPDR13 = 10.
     *
     * Ненатиснута кнопка: 0.
     * Натиснута кнопка:   1.
     */
    GPIOC_PUPDR &= ~(3U << 26);
    GPIOC_PUPDR |=  (2U << 26);
}

static uint32_t button_is_pressed(void)
{
    if ((GPIOC_IDR & BUTTON_MASK) != 0U)
    {
        return 1U;
    }

    return 0U;
}

/* ========================= PWM ======================= */

static void pwm_init(void)
{
    /* Тактування GPIOA та TIM2 */
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
     * fTIM = 16 MHz
     * PSC  = 15
     * ARR  = 999
     *
     * fPWM =
     * 16 000 000 / ((15 + 1) × (999 + 1))
     *
     * fPWM = 1000 Hz.
     */
    TIM2_PSC = 15U;
    TIM2_ARR = PWM_MAX_VALUE;

    /* Початково LD2 вимкнений */
    TIM2_CCR1 = 0U;

    /*
     * OC1M = 110 — PWM mode 1.
     * OC1PE = 1 — preload CCR1.
     */
    TIM2_CCMR1 &= ~((7U << 4) | (1U << 3));
    TIM2_CCMR1 |=  (6U << 4) | (1U << 3);

    /* CC1E = 1 — увімкнути канал 1 */
    TIM2_CCER |= (1U << 0);

    /* ARPE = 1 — preload ARR */
    TIM2_CR1 |= (1U << 7);

    /* Згенерувати подію оновлення */
    TIM2_EGR = (1U << 0);

    /* CEN = 1 — запустити таймер */
    TIM2_CR1 |= (1U << 0);
}

/* ================== Виведення стану ================= */

static void print_pwm_state(uint32_t level)
{
    uart_send_string("Mode = ");
    uart_send_uint(level);

    uart_send_string(" | CCR1 = ");
    uart_send_uint(pwm_values[level]);

    uart_send_string(" | Brightness = ");
    uart_send_uint(brightness_values[level]);
    uart_send_string(" %");

    if (level == 0U)
    {
        uart_send_string(" | LD2 OFF");
    }
    else if (level == (PWM_LEVEL_COUNT - 1U))
    {
        uart_send_string(" | LD2 MAX");
    }
    else
    {
        uart_send_string(" | LD2 PWM");
    }

    uart_send_string("\r\n");
}

/* ========================= Main ====================== */

int main(void)
{
    uint32_t current_level = 0U;
    uint32_t button_now;
    uint32_t button_previous = 0U;

    uart_init();
    button_init();
    pwm_init();

    uart_send_string("\r\n");
    uart_send_string("STM32F401RE button PWM control\r\n");
    uart_send_string("B1: PC13, active HIGH\r\n");
    uart_send_string("LD2: PA5 / TIM2_CH1\r\n");
    uart_send_string("Press B1 to change brightness\r\n\r\n");

    TIM2_CCR1 = pwm_values[current_level];
    print_pwm_state(current_level);

    while (1)
    {
        button_now = button_is_pressed();

        /*
         * Обробляємо тільки перехід:
         *
         * кнопка була відпущена: 0
         * кнопка стала натиснута: 1
         */
        if ((button_now != 0U) &&
            (button_previous == 0U))
        {
            /*
             * Програмне усунення брязкоту контактів.
             */
            delay(200000U);

            /*
             * Повторно перевіряємо кнопку.
             * Якщо вона досі натиснута —
             * це справжнє натискання.
             */
            if (button_is_pressed() != 0U)
            {
                current_level++;

                if (current_level >= PWM_LEVEL_COUNT)
                {
                    current_level = 0U;
                }

                TIM2_CCR1 = pwm_values[current_level];

                print_pwm_state(current_level);
            }
        }

        /*
         * Запам’ятати поточний стан для
         * виявлення наступного фронту.
         */
        button_previous = button_now;
    }
}
