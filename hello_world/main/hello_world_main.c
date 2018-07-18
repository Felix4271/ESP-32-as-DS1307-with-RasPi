#include <stdio.h>
#include "driver/i2c.h"

#define DATA_LENGTH                        512              /*!<Data buffer length for test buffer*/
#define RW_TEST_LENGTH                     6                /*!<Data length for r/w test, any value from 0-DATA_LENGTH*/
#define DELAY_TIME_BETWEEN_ITEMS_MS        1234             /*!< delay time between different test items */

#define I2C_EXAMPLE_SLAVE_SCL_IO           22               /*!<gpio number for i2c slave clock  */
#define I2C_EXAMPLE_SLAVE_SDA_IO           21               /*!<gpio number for i2c slave data */
#define I2C_EXAMPLE_SLAVE_NUM              I2C_NUM_0        /*!<I2C port number for slave dev */
#define I2C_EXAMPLE_SLAVE_TX_BUF_LEN       (2*DATA_LENGTH)  /*!<I2C slave tx buffer size */
#define I2C_EXAMPLE_SLAVE_RX_BUF_LEN       (2*DATA_LENGTH)  /*!<I2C slave rx buffer size */

#define ESP_SLAVE_ADDR                     0x68             /*!< ESP32 slave address, you can set any 7bit value */
#define WRITE_BIT                          I2C_MASTER_WRITE /*!< I2C master write */
#define READ_BIT                           I2C_MASTER_READ  /*!< I2C master read */
#define ACK_CHECK_EN                       0x1              /*!< I2C master will check ack from slave*/
#define ACK_CHECK_DIS                      0x0              /*!< I2C master will not check ack from slave */
#define ACK_VAL                            0x0              /*!< I2C ack value */
#define NACK_VAL                           0x1              /*!< I2C nack value */

SemaphoreHandle_t print_mux = NULL;

static uint8_t bcd_to_dec(uint8_t a) {
    return (a>>4)*10+(((uint8_t) (a<<4))>>4);
}

static uint8_t dec_to_bcd(uint8_t a) {
    return ((a/10)<<4) + (a%10);
}
/*
static uint8_t[6] microseconds_to_time(uint32_t a) {
    uint8_t res[6];
    uint8_t set[] = {365, 24, 60, 60, 1000}
    uint8_t b = 1000*60*60*24*365;
    res[0] = a/b;
    a%=b;
    b/=365;

}*/

/**
 * @brief i2c slave initialization
 */
static void i2c_example_slave_init()
{
    int i2c_slave_port = I2C_EXAMPLE_SLAVE_NUM;
    i2c_config_t conf_slave;
    conf_slave.sda_io_num = I2C_EXAMPLE_SLAVE_SDA_IO;
    conf_slave.sda_pullup_en = GPIO_PULLUP_ENABLE;
    conf_slave.scl_io_num = I2C_EXAMPLE_SLAVE_SCL_IO;
    conf_slave.scl_pullup_en = GPIO_PULLUP_ENABLE;
    conf_slave.mode = I2C_MODE_SLAVE;
    conf_slave.slave.addr_10bit_en = 0;
    conf_slave.slave.slave_addr = ESP_SLAVE_ADDR;
    i2c_param_config(i2c_slave_port, &conf_slave);
    i2c_driver_install(i2c_slave_port, conf_slave.mode,
                       I2C_EXAMPLE_SLAVE_RX_BUF_LEN,
                       I2C_EXAMPLE_SLAVE_TX_BUF_LEN, 0);
}

static void i2c_test_task(void* arg)
{
    while (1) {
        printf("test cnt: \n");
        vTaskDelay(( DELAY_TIME_BETWEEN_ITEMS_MS * ( (uint32_t) arg + 1 ) ) / portTICK_RATE_MS);
    }
}

void app_main()
{
    uint8_t* data = (uint8_t*) malloc(512);
    uint8_t* __time = (uint8_t*) malloc(512);
    print_mux = xSemaphoreCreateMutex();
    i2c_example_slave_init();
    int size = 0;
    for (int i=0;i<64;i++) {
        data[i]=0x00;
        __time[i]=0x01;
    }
    int t=0x00;
    while (1) {
        size = 0;
        while (size == 0) {
            size = i2c_slave_read_buffer( I2C_EXAMPLE_SLAVE_NUM, data, DATA_LENGTH, 1000 / portTICK_RATE_MS);
            printf("NAY\n");
        }
        for (int i=0;i<20;i++) {
            printf("%d: ", i);
            printf("%d\n", data[i]);
        }
        printf("%d\n",size);
        if (size==0&&data[0]==0) {
            size = i2c_slave_read_buffer( I2C_EXAMPLE_SLAVE_NUM, __time, DATA_LENGTH, 1000 / portTICK_RATE_MS);
            for (int i=0;i<10;i++) {
                printf("%d", __time[i]);
            }
            printf("\n");
        } else if (size==1) {
            i2c_slave_write_buffer(I2C_EXAMPLE_SLAVE_NUM, __time, 8, 1000 / portTICK_RATE_MS);
        } else {
            printf("Hmmmmmm......ERRROR%d\n",size);
        }
        vTaskDelay(1000/portTICK_RATE_MS);
    }
    fflush(stdout);
    //xTaskCreate(i2c_test_task, "i2c_test_task_0", 1024 * 2, (void* ) 0, 10, NULL);
}