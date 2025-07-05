#include <bsp_sht3x.h>
#include <esp_log.h>

static i2c_bus_handle_t i2c_bus = nullptr;
static sht3x_handle_t sht3x_handle = nullptr;

sht3x::sht3x(){
    
}


void sht3x::init(){
    i2c_config_t conf = {}; // Zero initialize first
    conf.mode = I2C_MODE_MASTER;
    conf.sda_io_num = I2C_MASTER_SDA_IO;
    conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
    conf.scl_io_num = I2C_MASTER_SCL_IO;
    conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
    conf.master.clk_speed = I2C_MASTER_FREQ_HZ;

    i2c_bus = i2c_bus_create(I2C_MASTER_NUM, &conf);
    sht3x_handle = sht3x_create(i2c_bus, SHT3x_ADDR_PIN_SELECT_VSS);
    esp_err_t err = sht3x_set_measure_mode(sht3x_handle, SHT3x_PER_2_MEDIUM);     /*!< here read data in periodic mode*/
    //判断
    if(err != ESP_OK){
        ESP_LOGE("SHT3X", "sht3x_set_measure_mode failed");
    }
}

//析构函数
sht3x::~sht3x() {
    sht3x_delete(&sht3x_handle);
    i2c_bus_delete(&i2c_bus);
}

void sht3x::read_temperature_humidity(){
    if(!sht3x_handle) {
        ESP_LOGE("SHT3X", "SHTC3 sensor not initialized");
        return;
    }

    if (sht3x_get_humiture(sht3x_handle, &Tem_val, &Hum_val) == 0 ) {
    printf("temperature %.2f°C    ", Tem_val);
    printf("humidity:%.2f %%\n", Hum_val);
    }


}

