#include <stdint.h>

/* ============================================================
 * RCC
 * ============================================================ */

#define RCC_AHB1ENR    (*(volatile uint32_t *)0x40023830U)
#define RCC_APB1ENR    (*(volatile uint32_t *)0x40023840U)

/* ============================================================
 * GPIOA
 * ============================================================ */

#define GPIOA_MODER    (*(volatile uint32_t *)0x40020000U)
#define GPIOA_AFRL     (*(volatile uint32_t *)0x40020020U)
#define GPIOA_BSRR     (*(volatile uint32_t *)0x40020018U)

/* ============================================================
 * USART2
 * ============================================================ */

#define USART2_SR      (*(volatile uint32_t *)0x40004400U)
#define USART2_DR      (*(volatile uint32_t *)0x40004404U)
#define USART2_BRR     (*(volatile uint32_t *)0x40004408U)
#define USART2_CR1     (*(volatile uint32_t *)0x4000440CU)

#define NVIC_ISER1     (*(volatile uint32_t *)0xE000E104U)

#define USART_SR_RXNE      (1U << 5)
#define USART_SR_TXE       (1U << 7)

#define USART_CR1_RE       (1U << 2)
#define USART_CR1_TE       (1U << 3)
#define USART_CR1_RXNEIE   (1U << 5)
#define USART_CR1_UE       (1U << 13)

/* ============================================================
 * Кільцевий буфер USART2 RX
 * ============================================================ */

#define RX_BUFFER_SIZE    64U
#define RX_BUFFER_MASK    (RX_BUFFER_SIZE - 1U)

static volatile char rx_buffer[RX_BUFFER_SIZE];
static volatile uint32_t rx_head = 0U;
static volatile uint32_t rx_tail = 0U;
static volatile uint32_t rx_overflow = 0U;

/* ============================================================
 * Буфер текстової команди
 * ============================================================ */

#define COMMAND_SIZE      32U

static char command_buffer[COMMAND_SIZE];
static uint32_t command_index = 0U;

static uint32_t led_state = 0U;

/* ============================================================
 * Передавання через UART
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

/* ============================================================
 * Кільцевий буфер
 * ============================================================ */

static uint32_t uart_rx_available(void)
{
    return (rx_head != rx_tail);
}

static char uart_rx_get_char(void)
{
    char character;

    character = rx_buffer[rx_tail];
    rx_tail = (rx_tail + 1U) & RX_BUFFER_MASK;

    return character;
}

/* ============================================================
 * Порівняння двох текстових рядків
 * Повертає 1, якщо рядки однакові
 * ============================================================ */

static uint32_t strings_equal(const char *first, const char *second)
{
    while ((*first != '\0') && (*second != '\0'))
    {
        if (*first != *second)
        {
            return 0U;
        }

        first++;
        second++;
    }

    return ((*first == '\0') && (*second == '\0'));
}

/* ============================================================
 * Керування LD2
 * ============================================================ */

static void led_set(uint32_t state)
{
    led_state = state;

    if (led_state != 0U)
    {
        GPIOA_BSRR = (1U << 5);
    }
    else
    {
        GPIOA_BSRR = (1U << 21);
    }
}

/* ============================================================
 * Запрошення для наступної команди
 * ============================================================ */

static void print_prompt(void)
{
    uart_send_string("\r\n> ");
}

/* ============================================================
 * Обробка готової текстової команди
 * ============================================================ */

static void process_command(const char *command)
{
    if (strings_equal(command, "led on") != 0U)
    {
        led_set(1U);
        uart_send_string("LD2 switched ON\r\n");
    }
    else if (strings_equal(command, "led off") != 0U)
    {
        led_set(0U);
        uart_send_string("LD2 switched OFF\r\n");
    }
    else if (strings_equal(command, "led toggle") != 0U)
    {
        led_set(led_state ^ 1U);

        if (led_state != 0U)
        {
            uart_send_string("LD2 switched ON\r\n");
        }
        else
        {
            uart_send_string("LD2 switched OFF\r\n");
        }
    }
    else if (strings_equal(command, "status") != 0U)
    {
        if (led_state != 0U)
        {
            uart_send_string("LD2 state: ON\r\n");
        }
        else
        {
            uart_send_string("LD2 state: OFF\r\n");
        }
    }
    else if (strings_equal(command, "help") != 0U)
    {
        uart_send_string(
            "Available commands:\r\n"
            "  led on      - switch LD2 on\r\n"
            "  led off     - switch LD2 off\r\n"
            "  led toggle  - change LD2 state\r\n"
            "  status      - show LD2 state\r\n"
            "  help        - show commands\r\n"
        );
    }
    else if (command[0] != '\0')
    {
        uart_send_string("Unknown command. Type help.\r\n");
    }
}

/* ============================================================
 * Обробка одного прийнятого символу
 * ============================================================ */

static void process_received_character(char character)
{
    /* Enter: команда завершена */
    if ((character == '\r') || (character == '\n'))
    {
        /*
         * Якщо рядок не порожній, додаємо нульовий символ
         * і передаємо команду на обробку.
         */
        if (command_index > 0U)
        {
            command_buffer[command_index] = '\0';

            uart_send_string("\r\n");
            process_command(command_buffer);

            command_index = 0U;
        }

        print_prompt();
    }

    /* Backspace */
    else if ((character == '\b') || (character == 127))
    {
        if (command_index > 0U)
        {
            command_index--;

            /*
             * Стерти символ у PuTTY:
             * назад, пробіл, назад.
             */
            uart_send_string("\b \b");
        }
    }

    /* Звичайний символ */
    else
    {
        if (command_index < (COMMAND_SIZE - 1U))
        {
            command_buffer[command_index] = character;
            command_index++;

            /*
             * Echo: повертаємо символ у PuTTY,
             * щоб користувач бачив введений текст.
             */
            uart_send_char(character);
        }
        else
        {
            command_index = 0U;

            uart_send_string(
                "\r\nCommand is too long.\r\n"
            );

            print_prompt();
        }
    }
}

/* ============================================================
 * Обробник переривання USART2
 * ============================================================ */

void USART2_IRQHandler(void)
{
    uint32_t next_head;
    char received_character;

    if ((USART2_SR & USART_SR_RXNE) != 0U)
    {
        received_character = (char)(USART2_DR & 0xFFU);

        next_head = (rx_head + 1U) & RX_BUFFER_MASK;

        if (next_head != rx_tail)
        {
            rx_buffer[rx_head] = received_character;
            rx_head = next_head;
        }
        else
        {
            rx_overflow++;
        }
    }
}

/* ============================================================
 * Головна функція
 * ============================================================ */

int main(void)
{
    char received_character;

    /* Тактування GPIOA і USART2 */
    RCC_AHB1ENR |= (1U << 0);
    RCC_APB1ENR |= (1U << 17);

    /* PA5 — цифровий вихід LD2 */
    GPIOA_MODER &= ~(3U << 10);
    GPIOA_MODER |=  (1U << 10);

    led_set(0U);

    /* PA2 і PA3 — альтернативний режим */
    GPIOA_MODER &= ~((3U << 4) | (3U << 6));
    GPIOA_MODER |=  ((2U << 4) | (2U << 6));

    /* PA2/PA3 — AF7 USART2 */
    GPIOA_AFRL &= ~((0xFU << 8) | (0xFU << 12));
    GPIOA_AFRL |=  ((7U << 8) | (7U << 12));

    /* 115200 бод за PCLK1 = 16 МГц */
    USART2_BRR = 0x008BU;

    /* Передавач, приймач, RXNE interrupt */
    USART2_CR1 |= USART_CR1_TE;
    USART2_CR1 |= USART_CR1_RE;
    USART2_CR1 |= USART_CR1_RXNEIE;

    /* USART2_IRQn = 38; 38 - 32 = 6 */
    NVIC_ISER1 = (1U << 6);

    /* Увімкнення USART2 */
    USART2_CR1 |= USART_CR1_UE;

    uart_send_string(
        "\r\n"
        "STM32 command terminal ready.\r\n"
        "Type help and press Enter.\r\n"
    );

    print_prompt();

    while (1)
    {
        if (uart_rx_available() != 0U)
        {
            received_character = uart_rx_get_char();
            process_received_character(received_character);
        }
    }
}
