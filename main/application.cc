#include <application.h>
#include <esp_log.h>
//wifi
#include <nvs_flash.h>
#include <esp_wifi.h>
#include "ssid_manager.h"
#include <wifi_station.h>
#include <wifi_configuration_ap.h>
//mqtt
#include <mqtt_protocol.h>
#include "cJSON.h"

#define MQTT_BROKER_URI CONFIG_MQTT_BROKER_URI
#define MQTT_USERNAME CONFIG_MQTT_USERNAME
#define MQTT_PASSWORD CONFIG_MQTT_PASSWORD

#define MQTT_TOPIC_TEMP "home/sensor/temperature" // 温度发布主题
#define MQTT_TOPIC_HUMI "home/sensor/humidity"    // 湿度发布主题
#define DEVICE_NAME "esp32_temp_sensor" // 设备名称，用于 Home Assistant 发现


static const char *TAG = "MAIN_APP";

Application::Application() {
    // 初始化成员变量或执行其他必要的操作
    // 创建主循环任务
}

Application::~Application() {
    // 释放资源或执行其他必要的操作
}

void Application::MainLoop(){
    float temperature, humidity;
    while (true) {
        // sht3x_.read_temperature_humidity();
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }

}
//wifi init 
void Application::wifi_init(){

    // Get the Wi-Fi configuration
    auto& ssid_list = SsidManager::GetInstance().GetSsidList();
    if (ssid_list.empty()) {
        // Start the Wi-Fi configuration AP
        auto& ap = WifiConfigurationAp::GetInstance();
        ap.SetSsidPrefix("ESP32");
        ap.Start();
        return;
    }

    // Otherwise, connect to the Wi-Fi network
    WifiStation::GetInstance().Start();

}


void Application::Start() {

    wifi_init();

    //传感器初始化
    sht3x_=new sht3x();
    // 启动应用程序
    sht3x_->init();

    vTaskDelay(3000 / portTICK_PERIOD_MS);//等待wifi初始化

    //mqtt init
    mqttclient_=new MqttClient(MQTT_BROKER_URI, MQTT_USERNAME, MQTT_PASSWORD);

    //启动 MQTT 客户端
    mqttclient_->start();


    // ESP_LOGE("MIAN", "creating MainLoop task");
    // xTaskCreate([](void* arg) {
    //     Application* app = (Application*)arg;
    //     app->MainLoop();
    //     vTaskDelete(NULL);
    // }, "MainLoop", 4096, NULL, 5, NULL);

    while(true){   
        //读取温度
        sht3x_->read_temperature_humidity();
        char temp_str[16];
        char humi_str[16];
        snprintf(temp_str, sizeof(temp_str), "%.2f", sht3x_->Tem_val);
        snprintf(humi_str, sizeof(humi_str), "%.2f", sht3x_->Hum_val);
        //数据整理


        // 发布温度数据
        if (mqttclient_) {
            int msg_id_temp = mqttclient_->publish(MQTT_TOPIC_TEMP, temp_str, 0, 0, 0);

            // 发布湿度数据
            int msg_id_humi = mqttclient_->publish(MQTT_TOPIC_HUMI, humi_str, 0, 0, 0);
        }

        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}