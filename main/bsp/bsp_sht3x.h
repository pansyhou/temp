#ifndef BSP_SHT3X_H
#define BSP_SHT3X_H

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "i2c_bus.h"
#include "esp_system.h"
#include "sht3x.h"
// #include "driver/i2c.h"

#define I2C_MASTER_SCL_IO          GPIO_NUM_17              /*!< gpio number for I2C master clock */
#define I2C_MASTER_SDA_IO          GPIO_NUM_16              /*!< gpio number for I2C master data  */
#define I2C_MASTER_NUM              I2C_NUM_0              /*!< I2C port number for master dev */
#define I2C_MASTER_FREQ_HZ          100000                 /*!< I2C master clock frequency */


class sht3x {
public:
    sht3x();
    ~sht3x();
    void init();
    void read_temperature_humidity();
    float Tem_val, Hum_val;

private:
    //初始化准备就绪标志位
    bool init_ready_ = false ;
    // static i2c_bus_handle_t i2c_bus ;
    // static sht3x_handle_t sht3x_handle ;
        

};




#endif