#include <stdint.h>

/* ====================== RCC ====================== */

#define RCC_AHB1ENR   (*(volatile uint32_t *)0x40023830U)
#define RCC_APB1ENR   (*(volatile uint32_t *)0x40023840U)
#define RCC_APB2ENR   (*(volatile uint32_t *)0x40023844U)

/* ===================== GPIOA ===================== */

#define GPIOA_MODER   (*(volatile uint32_t *)0x40020000U)
#define GPIOA_AFRL    (*(volatile uint32_t *)0x40020020U)

/* ===================== USART2 ==================== */

#define USART2_SR     (*(volatile uint32_t *)0x40004400U)
#define USART2_DR     (*(volatile uint32_t *)0x40004404U)
#define USART2_BRR    (*(volatile uint32_t *)0x40004408U)
#define USART2_CR1    (*(volatile uint32_t *)0x4000440CU)

/* ====================== ADC1 ===================== */

#define ADC1_SR       (*(volatile uint32_t *)0x40012000U)
#define ADC1_CR1      (*(volatile uint32_t *)0x40012004U)
#define ADC1_CR2      (*(volatile uint32_t *)0x40012008U)
#define ADC1_SMPR1    (*(volatile uint32_t *)0x4001200CU)
#define ADC1_SQR1     (*(volatile uint32_t *)0x4001202CU)
#define ADC1_SQR3     (*(volatile uint32_t *)0x40012034U)
#define ADC1_DR       (*(volatile uint32_t *)0x4001204CU)

#define ADC_CCR       (*(volatile uint32_t *)0x40012304U)

/* =================== Константи =================== */

#define ADC_CHANNEL_VREFINT       17U
#define ADC_CHANNEL_TEMPERATURE   18U

#define ADC_MAX_VALUE             4095U
#define VREFINT_TYP_MV            1210U
#define ADC_SAMPLES               16U

/* ====================== Delay ==================== */

static void delay(volatile uint32_t count)
{
    while (count > 0U)
    {
        count--;
        __asm volatile ("nop");
    }
}

/* ====================== UART ===================== */

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

static void uart_send_int(int32_t value)
{
    if (value < 0)
    {
        uart_send_char('-');
        value = -value;
    }

    uart_send_uint((uint32_t)value);
}

static void uart_send_voltage(uint32_t millivolts)
{
    uart_send_uint(millivolts / 1000U);
    uart_send_char('.');

    uart_send_char((char)('0' + ((millivolts / 100U) % 10U)));
    uart_send_char((char)('0' + ((millivolts / 10U) % 10U)));
    uart_send_char((char)('0' + (millivolts % 10U)));
}

static void uart_send_temperature(int32_t temperature_tenth_c)
{
    if (temperature_tenth_c < 0)
    {
        uart_send_char('-');
        temperature_tenth_c = -temperature_tenth_c;
    }

    uart_send_uint((uint32_t)temperature_tenth_c / 10U);
    uart_send_char('.');
    uart_send_char(
        (char)('0' + ((uint32_t)temperature_tenth_c % 10U))
    );
}

static void uart_init(void)
{
    /* Тактування GPIOA та USART2 */
    RCC_AHB1ENR |= (1U << 0);
    RCC_APB1ENR |= (1U << 17);

    /* PA2 — альтернативна функція */
    GPIOA_MODER &= ~(3U << 4);
    GPIOA_MODER |=  (2U << 4);

    /* PA2 — AF7, USART2_TX */
    GPIOA_AFRL &= ~(0xFU << 8);
    GPIOA_AFRL |=  (7U << 8);

    /*
     * Частота APB1 = 16 MHz.
     * Baud rate = 115200.
     */
    USART2_BRR = 0x008BU;

    /* Увімкнути передавач і USART2 */
    USART2_CR1 = (1U << 3) | (1U << 13);
}

/* ======================= ADC ===================== */

static uint32_t adc_read_single(void)
{
    /* Запуск перетворення */
    ADC1_CR2 |= (1U << 30);

    /* Очікувати EOC */
    while ((ADC1_SR & (1U << 1)) == 0U)
    {
    }

    return ADC1_DR;
}

static void adc_select_channel(uint32_t channel)
{
    ADC1_SQR3 &= ~0x1FU;
    ADC1_SQR3 |= channel & 0x1FU;

    /*
     * Перше вимірювання після перемикання каналу відкидаємо.
     */
    (void)adc_read_single();
}

static uint32_t adc_read_average(uint32_t channel)
{
    uint32_t sum = 0U;

    adc_select_channel(channel);

    for (uint32_t i = 0U; i < ADC_SAMPLES; i++)
    {
        sum += adc_read_single();
    }

    return (sum + (ADC_SAMPLES / 2U)) / ADC_SAMPLES;
}

static void adc_init(void)
{
    /* Тактування ADC1 */
    RCC_APB2ENR |= (1U << 8);

    /*
     * ADCPRE = 01: тактова частота ADC = APB2 / 4.
     * TSVREFE = 1: увімкнути температурний сенсор і VREFINT.
     */
    ADC_CCR &= ~(3U << 16);
    ADC_CCR |=  (1U << 16);
    ADC_CCR |=  (1U << 23);

    /*
     * Канал 17: 480 циклів.
     * Канал 18: 480 циклів.
     */
    ADC1_SMPR1 &= ~((7U << 21) | (7U << 24));
    ADC1_SMPR1 |=   (7U << 21) | (7U << 24);

    /* Одне перетворення в регулярній послідовності */
    ADC1_SQR1 &= ~(0xFU << 20);

    ADC1_CR1 = 0U;

    /* Увімкнути ADC */
    ADC1_CR2 = (1U << 0);

    /* Час стабілізації внутрішніх каналів */
    delay(10000U);
}

/* ======================= Main ==================== */

int main(void)
{
    uint32_t adc_vref;
    uint32_t adc_temperature;
    uint32_t vdda_mv;
    uint32_t temperature_voltage_tenth_mv;
    int32_t temperature_tenth_c;

    uart_init();
    adc_init();

    uart_send_string("\r\n");
    uart_send_string("STM32F401RE ADC VDDA compensation\r\n");
    uart_send_string("VREFINT channel 17\r\n");
    uart_send_string("Temperature channel 18\r\n\r\n");

    while (1)
    {
        adc_vref =
            adc_read_average(ADC_CHANNEL_VREFINT);

        adc_temperature =
            adc_read_average(ADC_CHANNEL_TEMPERATURE);

        /*
         * VDDA = VREFINT_TYP * 4095 / ADC_VREFINT
         *
         * Додавання adc_vref / 2 виконує округлення.
         */
        if (adc_vref != 0U)
        {
            vdda_mv =
                (VREFINT_TYP_MV * ADC_MAX_VALUE +
                 adc_vref / 2U) / adc_vref;
        }
        else
        {
            vdda_mv = 0U;
        }

        /*
         * Напруга температурного сенсора у десятих частках mV.
         *
         * Vsense[mV] =
         * ADC_TEMP * VDDA[mV] / 4095
         */
        temperature_voltage_tenth_mv =
            (uint32_t)(
                ((uint64_t)adc_temperature *
                 (uint64_t)vdda_mv * 10ULL) /
                ADC_MAX_VALUE
            );

        /*
         * Наближена формула:
         *
         * T = 25 + (760 mV - Vsense) / 2.5 mV/°C
         *
         * Результат зберігається у десятих частках °C.
         */
        temperature_tenth_c =
            250 +
            ((7600 - (int32_t)temperature_voltage_tenth_mv) *
             10) / 25;

        uart_send_string("VREF ADC = ");
        uart_send_uint(adc_vref);

        uart_send_string(" | VDDA = ");
        uart_send_voltage(vdda_mv);
        uart_send_string(" V");

        uart_send_string(" | TEMP ADC = ");
        uart_send_uint(adc_temperature);

        uart_send_string(" | Temperature ~ ");
        uart_send_temperature(temperature_tenth_c);
        uart_send_string(" C\r\n");

        delay(3000000U);
    }
}
