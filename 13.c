#include <stdint.h>

/* ============================================================
 * RCC
 * ============================================================ */

#define RCC_AHB1ENR    (*(volatile uint32_t *)0x40023830U)
#define RCC_APB1ENR    (*(volatile uint32_t *)0x40023840U)
#define RCC_APB2ENR    (*(volatile uint32_t *)0x40023844U)

/* ============================================================
 * GPIOA
 * ============================================================ */

#define GPIOA_MODER    (*(volatile uint32_t *)0x40020000U)
#define GPIOA_AFRL     (*(volatile uint32_t *)0x40020020U)

/* ============================================================
 * USART2
 * ============================================================ */

#define USART2_SR      (*(volatile uint32_t *)0x40004400U)
#define USART2_DR      (*(volatile uint32_t *)0x40004404U)
#define USART2_BRR     (*(volatile uint32_t *)0x40004408U)
#define USART2_CR1     (*(volatile uint32_t *)0x4000440CU)

/* ============================================================
 * ADC1
 * ============================================================ */

#define ADC1_SR        (*(volatile uint32_t *)0x40012000U)
#define ADC1_CR1       (*(volatile uint32_t *)0x40012004U)
#define ADC1_CR2       (*(volatile uint32_t *)0x40012008U)
#define ADC1_SMPR1     (*(volatile uint32_t *)0x4001200CU)
#define ADC1_SQR1      (*(volatile uint32_t *)0x4001202CU)
#define ADC1_SQR3      (*(volatile uint32_t *)0x40012034U)
#define ADC1_DR        (*(volatile uint32_t *)0x4001204CU)

#define ADC_CCR        (*(volatile uint32_t *)0x40012304U)

/* ============================================================
 * Біти регістрів
 * ============================================================ */

#define USART_SR_TXE       (1U << 7)
#define USART_CR1_TE       (1U << 3)
#define USART_CR1_UE       (1U << 13)

#define ADC_SR_EOC         (1U << 1)
#define ADC_CR2_ADON       (1U << 0)
#define ADC_CR2_SWSTART    (1U << 30)
#define ADC_CCR_TSVREFE    (1U << 23)

/* Кількість вимірювань для усереднення */
#define ADC_SAMPLE_COUNT   16U

/* ============================================================
 * Програмна затримка
 * ============================================================ */

static void delay(volatile uint32_t count)
{
    while (count > 0U)
    {
        count--;
        __asm volatile ("nop");
    }
}

/* ============================================================
 * Функції UART
 * ============================================================ */

static void uart_send_char(char character)
{
    while ((USART2_SR & USART_SR_TXE) == 0U)
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

static void uart_send_uint32(uint32_t number)
{
    char buffer[10];
    uint32_t index = 0U;

    if (number == 0U)
    {
        uart_send_char('0');
        return;
    }

    while (number > 0U)
    {
        buffer[index] = (char)('0' + (number % 10U));
        number /= 10U;
        index++;
    }

    while (index > 0U)
    {
        index--;
        uart_send_char(buffer[index]);
    }
}

/* ============================================================
 * Виведення числа з однією цифрою після крапки
 *
 * Приклад:
 * value = 282 означає 28.2
 * ============================================================ */

static void uart_send_fixed1(int32_t value)
{
    uint32_t absolute_value;

    if (value < 0)
    {
        uart_send_char('-');
        absolute_value = (uint32_t)(-value);
    }
    else
    {
        absolute_value = (uint32_t)value;
    }

    uart_send_uint32(absolute_value / 10U);
    uart_send_char('.');
    uart_send_char((char)('0' + (absolute_value % 10U)));
}

/* ============================================================
 * Одне перетворення ADC
 * ============================================================ */

static uint32_t adc1_read(void)
{
    ADC1_CR2 |= ADC_CR2_SWSTART;

    while ((ADC1_SR & ADC_SR_EOC) == 0U)
    {
    }

    return ADC1_DR;
}

/* ============================================================
 * Усереднення 16 вимірювань
 * ============================================================ */

static uint32_t adc1_read_average(
    uint32_t *first_measurement)
{
    uint32_t sum = 0U;
    uint32_t measurement;
    uint32_t index;

    for (index = 0U;
         index < ADC_SAMPLE_COUNT;
         index++)
    {
        measurement = adc1_read();

        /*
         * Перше вимірювання зберігаємо окремо,
         * щоб показати його як Raw ADC.
         */
        if (index == 0U)
        {
            *first_measurement = measurement;
        }

        sum += measurement;
    }

    /*
     * Додаємо половину дільника перед діленням,
     * щоб виконати округлення до найближчого цілого:
     *
     * average = (sum + 8) / 16.
     */
    return (sum + (ADC_SAMPLE_COUNT / 2U))
           / ADC_SAMPLE_COUNT;
}

/* ============================================================
 * Головна функція
 * ============================================================ */

int main(void)
{
    uint32_t raw_adc;
    uint32_t average_adc;

    /*
     * Напруга у десятих частках мілівольта.
     *
     * Наприклад:
     * 7518 означає 751,8 мВ.
     */
    uint32_t voltage_tenth_mv;

    /*
     * Температура у десятих частках градуса.
     *
     * Наприклад:
     * 282 означає 28,2 °C.
     */
    int32_t temperature_tenth_c;

    /* --------------------------------------------------------
     * 1. Тактування GPIOA, USART2 та ADC1
     * -------------------------------------------------------- */

    RCC_AHB1ENR |= (1U << 0);
    RCC_APB1ENR |= (1U << 17);
    RCC_APB2ENR |= (1U << 8);

    /* --------------------------------------------------------
     * 2. PA2 — USART2_TX, AF7
     * -------------------------------------------------------- */

    GPIOA_MODER &= ~(3U << 4);
    GPIOA_MODER |=  (2U << 4);

    GPIOA_AFRL &= ~(0xFU << 8);
    GPIOA_AFRL |=  (7U << 8);

    /* --------------------------------------------------------
     * 3. USART2: 115200 бод
     * -------------------------------------------------------- */

    USART2_BRR = 0x008BU;
    USART2_CR1 |= USART_CR1_TE;
    USART2_CR1 |= USART_CR1_UE;

    /* --------------------------------------------------------
     * 4. ADC clock = PCLK2 / 4
     * -------------------------------------------------------- */

    ADC_CCR &= ~(3U << 16);
    ADC_CCR |=  (1U << 16);

    /* --------------------------------------------------------
     * 5. Увімкнення температурного сенсора
     * -------------------------------------------------------- */

    ADC_CCR |= ADC_CCR_TSVREFE;
    delay(10000U);

    /* --------------------------------------------------------
     * 6. Один регулярний канал
     * -------------------------------------------------------- */

    ADC1_SQR1 &= ~(0xFU << 20);

    /* SQ1 = channel 18 */
    ADC1_SQR3 &= ~0x1FU;
    ADC1_SQR3 |= 18U;

    /* SMP18 = 111: 480 циклів ADC */
    ADC1_SMPR1 &= ~(7U << 24);
    ADC1_SMPR1 |=  (7U << 24);

    ADC1_CR1 = 0U;

    /* Увімкнення ADC1 */
    ADC1_CR2 |= ADC_CR2_ADON;
    delay(10000U);

    uart_send_string(
        "\r\n"
        "STM32 ADC digital averaging\r\n"
        "Samples per result: 16\r\n\r\n"
    );

    /* --------------------------------------------------------
     * 7. Головний цикл
     * -------------------------------------------------------- */

    while (1)
    {
        average_adc =
            adc1_read_average(&raw_adc);

        /*
         * Напруга у десятих частках мілівольта:
         *
         * U × 10 = ADC × 33000 / 4095.
         */
        voltage_tenth_mv =
            (average_adc * 33000U) / 4095U;

        /*
         * Температура у десятих частках °C:
         *
         * V25 = 760,0 мВ = 7600 одиниць;
         * slope = 2,5 мВ/°C.
         *
         * T × 10 =
         * 250 + (7600 - U×10) × 10 / 25.
         */
        temperature_tenth_c =
            250 +
            (((int32_t)7600 -
              (int32_t)voltage_tenth_mv) * 10) / 25;

        uart_send_string("Raw ADC = ");
        uart_send_uint32(raw_adc);

        uart_send_string(" | Average ADC = ");
        uart_send_uint32(average_adc);

        uart_send_string(" | Voltage = ");
        uart_send_uint32(voltage_tenth_mv / 10U);
        uart_send_char('.');
        uart_send_char(
            (char)('0' + (voltage_tenth_mv % 10U)));
        uart_send_string(" mV");

        uart_send_string(" | Temperature ~ ");
        uart_send_fixed1(temperature_tenth_c);
        uart_send_string(" C\r\n");

        delay(4000000U);
    }
}
