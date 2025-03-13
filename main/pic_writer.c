/* UART Echo Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "sdkconfig.h"
#include "esp_log.h"
#include "esp_timer.h"

#include "pic_writer.h"

/**
 * This is an example which echos any data it receives on configured UART back to the sender,
 * with hardware flow control turned off. It does not use UART driver event queue.
 *
 * - Port: configured UART
 * - Receive (Rx) buffer: on
 * - Transmit (Tx) buffer: off
 * - Flow control: off
 * - Event queue: off
 * - Pin assignment: see defines below (See Kconfig)
 */

#define ECHO_TEST_TXD (CONFIG_EXAMPLE_UART_TXD)
#define ECHO_TEST_RXD (CONFIG_EXAMPLE_UART_RXD)
#define ECHO_TEST_RTS (UART_PIN_NO_CHANGE)
#define ECHO_TEST_CTS (UART_PIN_NO_CHANGE)

#define PIN_VPP_CTRL 10
#define PIN_ICSP_CLK 8
#define PIN_ICSP_DAT 9

#define ECHO_UART_PORT_NUM (CONFIG_EXAMPLE_UART_PORT_NUM)
#define ECHO_UART_BAUD_RATE (CONFIG_EXAMPLE_UART_BAUD_RATE)
#define ECHO_TASK_STACK_SIZE (CONFIG_EXAMPLE_TASK_STACK_SIZE)

static const char *TAG = "UART TEST";

#define BUF_SIZE (16)
#define BAUD_RATE 2e5
#define ICPS_CLK_PERIOD (1e6 / BAUD_RATE)

typedef enum
{
    WRITER_STATUS_WAIT_COMMAND,
    WRITER_STATUS_CHECK_COMMAND,
    WRITER_STATUS_TO_WRITE,
} writer_status_t;

typedef struct
{
    uint32_t d;
    uint8_t pos;
    uint8_t len;
} icsp_dat_t;

static void func_init(void);
static esp_timer_handle_t icsp_init(esp_timer_create_args_t *args);
static void icsp_proc(void *);

// variable
static uint8_t *data;
static int read_bytes;
static int icsp_clk_potensial;
static void *(*func[])(void *);
static int remains;
static icsp_dat_t current_data;

static void echo_task(void *arg)
{
    /* Configure parameters of an UART driver,
     * communication pins and install the driver */
    uart_config_t uart_config = {
        .baud_rate = ECHO_UART_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    writer_status_t status = WRITER_STATUS_WAIT_COMMAND;
    void *arg = NULL;
    int intr_alloc_flags = 0;
    void *result;
    esp_timer_create_args_t timer_args = {
        .callback = icsp_proc,
        .name = "periodic",
    };
    esp_timer_handle_t timer_handle;

    func_init();
    timer_handle = icsp_init(&timer_args);

    ESP_ERROR_CHECK(uart_driver_install(ECHO_UART_PORT_NUM, BUF_SIZE * 2, 0, 0, NULL, intr_alloc_flags));
    ESP_ERROR_CHECK(uart_param_config(ECHO_UART_PORT_NUM, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(ECHO_UART_PORT_NUM, ECHO_TEST_TXD, ECHO_TEST_RXD, ECHO_TEST_RTS, ECHO_TEST_CTS));

    // Configure a temporary buffer for the incoming data
    data = (uint8_t *)malloc(BUF_SIZE);

    while (1)
    {
        result = func[status](arg);
    }
}

void app_main(void)
{
    xTaskCreate(echo_task, "uart_echo_task", ECHO_TASK_STACK_SIZE, NULL, 10, NULL);
}

static void *read_command(void *arg)
{
    read_bytes = uart_read_bytes(ECHO_UART_PORT_NUM, data, (BUF_SIZE - 1), 20 / portTICK_PERIOD_MS);
    return NULL;
}
static void *check_command(void *arg)
{
    return NULL;
}
static void *write_command(void *arg)
{
    int len = read_bytes;
    uart_write_bytes(ECHO_UART_PORT_NUM, (const char *)data, len);
    if (len)
    {
        data[len] = '\0';
        ESP_LOGI(TAG, "Recv str: %s", (char *)data);
    }
    return NULL;
}
static void func_init(void)
{
    func[WRITER_STATUS_WAIT_COMMAND] = read_command;
    func[WRITER_STATUS_CHECK_COMMAND] = check_command;
    func[WRITER_STATUS_TO_WRITE] = write_command;
}

static esp_timer_handle_t icsp_init(esp_timer_create_args_t *args)
{
    esp_timer_handle_t timer;
    esp_err_t status;
    status = esp_timer_create(args, &timer);

    ERR_RETn(status != ESP_OK);

    status = esp_timer_start_periodic(timer, (uint64_t)ICPS_CLK_PERIOD);

error_return:
    return timer;
}

static void icsp_proc(void *arg)
{
    icsp_clk_potensial = !icsp_clk_potensial;
    gpio_set_level(PIN_ICSP_CLK, icsp_clk_potensial);
    if (current_data.pos < current_data.len)
    {
        uint32_t v = (current_data.d & (1 << current_data.pos)) ? 1 : 0;
        gpio_set_level(PIN_ICSP_DAT, v);
        current_data.pos++;
    }
    else
    {
        if (remains > 0)
        {
            current_data = *p_data++;
        }
    }
}