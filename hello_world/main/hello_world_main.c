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

int century = 21;

SemaphoreHandle_t print_mux = NULL;

static uint8_t bcd_to_dec(uint8_t a) {
    return (a>>4)*10+(((uint8_t) (a<<4))>>4);
}

static uint8_t dec_to_bcd(uint8_t a) {
    return ((a/10)<<4) + (a%10);
}

typedef struct {
    uint8_t second;
    uint8_t minute;
    uint8_t hour;
    uint8_t date;
    uint8_t month;
    uint8_t year;
    uint8_t day;
} __time_t;

static void increment(__time_t prev) {
    if ((prev.second&0x0F)>=0x09) {
        if (prev.second>=0x59) {
            prev.second = 0;
            if ((prev.minute&0x0F)>=0x09) {
                if (prev.minute>=0x59) {
                    prev.minute=0;
                    if (prev.hour>=0x24) {
                        prev.date++;
                        prev.day++;
                        prev.day%=0x07;
                        uint8_t month_lengths[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
                        uint8_t month = (prev.month>>4)*10+(prev.month&0x0F);
                        if (prev.year%4==0&&(prev.year!=00||century%4==1)) {
                            month_lengths[1] = 29;
                        }
                        if (prev.date>=month_lengths[month-1]) {
                            prev.date=0;
                            if ((prev.month&0x0F)>=0x09) {
                                prev.month+=0x07;
                            } else {
                                prev.month++;
                            }
                            if (prev.month>=0x13) {
                                prev.month=0;
                                if ((prev.year&0x0F)>=0x09) {
                                    prev.year=(prev.year&0xF0)+0x10;
                                    if (prev.year==0x99) {
                                        prev.year=0;
                                        century++;
                                    }
                                } else {
                                    prev.year++;
                                }
                            }
                        } else if ((prev.date&0x0F)>=0x09) {
                            prev.date=(prev.date&0xF0)+0x10;
                        } else {
                            prev.date++;
                        }
                    } else if ((prev.hour&0x0F)>=0x09) {
                        prev.hour=(prev.hour&0xF0)+0x10;
                    } else {
                        prev.hour++;
                    }
                } else {
                    prev.minute=(prev.minute&0xF0)+0x10;
                }
            } else {
                prev.minute++;
            }
        } else {
            prev.second=(prev.second&0xF0)+0x10;
        }
    } else {
        prev.second++;
    }
}

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

static void talk_to_the_pi() {
    uint8_t write_buf[512];
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
    uint8_t cycle_count=0;
    uint8_t cycle_ptr=0;

    if ( i2c_reset_tx_fifo(I2C_EXAMPLE_SLAVE_NUM) != ESP_OK ) {
        printf("Failed to reset tx FIFO - I2C loop will not be started!\n");
        return;
    }

    while (1) {
        size = 0;
        while (size == 0) {
            size = i2c_slave_read_buffer( I2C_EXAMPLE_SLAVE_NUM, data, 1, 1000 / portTICK_RATE_MS);
        }
        if (size==1) {
            for (int i=0;i<8;i++) {
                write_buf[i]=cycle_count;
            }
            write_buf[7]=0x06;
            //if (data[0]==10) {
            //    write_buf[0]=0;
            //}
            if ( i2c_reset_tx_fifo(I2C_EXAMPLE_SLAVE_NUM) != ESP_OK ) {
                printf("Failed to reset tx FIFO - I2C loop will not be started!\n");
            }
            size=i2c_slave_write_buffer(I2C_EXAMPLE_SLAVE_NUM, write_buf, 8, 1000 / portTICK_RATE_MS);
            if ( size != 8) {
                printf("WARN: wrong number of bytes written: %d\n", size);
            }
            cycle_ptr++;
            if ( cycle_ptr > 0 ) {
                cycle_count=(cycle_count%9)+1;
                cycle_ptr=0;
            }
        } else {
            printf("Unexpected read count: %d\n",size);
        }
        // TaskDelay(1000/portTICK_RATE_MS);
    }
}

static void HZ_pulse() {
    gpio_set_direction(2, GPIO_MODE_OUTPUT);
    while (1) {
        gpio_set_level(2, 1);
        vTaskDelay(500/portTICK_RATE_MS);
        gpio_set_level(2, 0);
        vTaskDelay(500/portTICK_RATE_MS);
    }
}

void app_main()
{
    xTaskCreate(HZ_pulse, "Task: HZ_pulse", 1024, NULL, 10, NULL);
    xTaskCreate(talk_to_the_pi, "Task: talk_to_pi", 8192, NULL, 10, NULL);
    fflush(stdout);
}