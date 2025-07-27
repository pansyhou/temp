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

//ir
#include "bsp_ir_encoder.h"


#define MQTT_BROKER_URI CONFIG_MQTT_BROKER_URI
#define MQTT_USERNAME CONFIG_MQTT_USERNAME
#define MQTT_PASSWORD CONFIG_MQTT_PASSWORD

#define MQTT_TOPIC_TEMP "home/sensor/temperature" // 温度发布主题
#define MQTT_TOPIC_HUMI "home/sensor/humidity"    // 湿度发布主题
#define DEVICE_NAME "esp32_temp_sensor" // 设备名称，用于 Home Assistant 发现


static const char *TAG = "MAIN_APP";

Application::Application() {
}

Application::~Application() {
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

    // 传感器初始化
    sht3x_=new sht3x();
    // 启动应用程序
    sht3x_->init();

    vTaskDelay(3000 / portTICK_PERIOD_MS);//等待wifi初始化

    //mqtt init
    mqttclient_=new MqttClient(MQTT_BROKER_URI, MQTT_USERNAME, MQTT_PASSWORD);

    //启动 MQTT 客户端
    mqttclient_->start();

    bsp_rmt_init();

    //实例化kelon protocol
    kelon_ac_ = new IRKelonAc();
    //绑定发送函数
    kelon_ac_->setSend([](uint64_t data){
        ESP_LOGI(TAG, "send data: %llu", data);
        bsp_ir_send(data);
    });

    //默认24度制冷
    kelon_ac_->setTemp(24);
    kelon_ac_->setMode(kKelonModeCool);

    // 发布HomeAssistant设备发现配置
    const char* ha_config = "{\"name\":\"Kelon AC\","
                            "\"platform\":\"mqtt\","
                            "\"unique_id\":\"kelon_ac_1\","
                            "\"modes\":[\"auto\",\"heat\",\"cool\",\"dry\",\"fan_only\"]," 
                            "\"fan_modes\":[\"auto\",\"low\",\"medium\",\"high\"],"
                            "\"temperature_command_topic\":\"homeassistant/climate/kelon_ac/temperature/command\","
                            "\"temperature_state_topic\":\"homeassistant/climate/kelon_ac/temperature/state\","
                            "\"current_temperature_topic\":\"homeassistant/climate/kelon_ac/current_temperature\","
                            "\"current_humidity_topic\":\"homeassistant/climate/kelon_ac/current_humidity\","
                            "\"mode_command_topic\":\"homeassistant/climate/kelon_ac/mode/command\","
                            "\"mode_state_topic\":\"homeassistant/climate/kelon_ac/mode/state\","
                            "\"fan_mode_command_topic\":\"homeassistant/climate/kelon_ac/fan/command\","
                            "\"fan_mode_state_topic\":\"homeassistant/climate/kelon_ac/fan/state\","
                            "\"min_temp\":16,"
                            "\"max_temp\":32}";
    //entity开关，toggle 空调电源
    const char* ha_switch_config = "{\"name\":\"Kelon AC Power\","
                              "\"unique_id\":\"kelon_ac_power_1\","
                              "\"command_topic\":\"homeassistant/switch/kelon_ac_power/command\","
                              "\"state_topic\":\"homeassistant/switch/kelon_ac_power/state\","
                              "\"payload_on\":\"ON\","
                              "\"payload_off\":\"OFF\","
                              "\"platform\":\"mqtt\"}";


    mqttclient_->publish("homeassistant/climate/kelon_ac/config", ha_config, 0, 1, 0);
    mqttclient_->publish("homeassistant/switch/kelon_ac_power/config", ha_switch_config, 0, 1, 0);

    // 设置MQTT消息回调
    mqttclient_->onMqttEvent([this](esp_mqtt_event_handle_t event) {
        if (event->event_id == MQTT_EVENT_CONNECTED) {
            ESP_LOGI(TAG, "MQTT Connected. Subscribing to command topics...");
            mqttclient_->subscribe("homeassistant/switch/kelon_ac_power/command", 1);
            mqttclient_->subscribe("homeassistant/climate/kelon_ac/temperature/command", 1);
            mqttclient_->subscribe("homeassistant/climate/kelon_ac/mode/command", 1);
            mqttclient_->subscribe("homeassistant/climate/kelon_ac/fan/command", 1);
        }
        else if (event->event_id == MQTT_EVENT_DATA) {
            std::string topic(event->topic, event->topic_len);
            std::string data(event->data, event->data_len);
            ESP_LOGI(TAG, "Received MQTT message: %s = %s", topic.c_str(), data.c_str());

            // 处理温度命令
            if (topic == "homeassistant/climate/kelon_ac/temperature/command") {
                uint8_t temp = std::stoi(data);
                kelon_ac_->setTemp(temp);
                kelon_ac_->send();
                // 发布状态更新
                char temp_str[4];
                sprintf(temp_str, "%d", kelon_ac_->getTemp());
                mqttclient_->publish("homeassistant/climate/kelon_ac/temperature/state", temp_str, 0, 0, 0);
            }
            // 处理模式命令
            else if (topic == "homeassistant/climate/kelon_ac/mode/command") {

                if (data == "heat") kelon_ac_->setMode(kKelonModeHeat);
                else if (data == "cool") kelon_ac_->setMode(kKelonModeCool);
                else if (data == "dry") kelon_ac_->setMode(kKelonModeDry);
                else if (data == "fan_only") kelon_ac_->setMode(kKelonModeFan);
                else if (data == "auto") kelon_ac_->setMode(kKelonModeSmart);

                kelon_ac_->send();
                // 发布状态更新
                mqttclient_->publish("homeassistant/climate/kelon_ac/mode/state", data.c_str(), 0, 0, 0);
            }
            // 处理风扇命令
            else if (topic == "homeassistant/climate/kelon_ac/fan/command") {
                if (data == "low") kelon_ac_->setFan(kKelonFanMin);
                else if (data == "medium") kelon_ac_->setFan(kKelonFanMedium);
                else if (data == "high") kelon_ac_->setFan(kKelonFanMax);
                else if (data == "auto") kelon_ac_->setFan(kKelonFanAuto);
                kelon_ac_->send();
                // 发布状态更新
                mqttclient_->publish("homeassistant/climate/kelon_ac/fan/state", data.c_str(), 0, 0, 0);
            }
            else if (topic == "homeassistant/switch/kelon_ac_power/command") {
                if (data == "ON") {
                    kelon_ac_->setTogglePower(true);  

                    ESP_LOGI(TAG, "Kelon AC Power: ON command received.");
                } else if (data == "OFF") {
                    kelon_ac_->setTogglePower(true); 
                    ESP_LOGI(TAG, "Kelon AC Power: OFF command received.");
                }
                kelon_ac_->send(); // 发送红外指令

                // 发布状态更新，让Home Assistant知道开关状态已改变
                mqttclient_->publish("homeassistant/switch/kelon_ac_power/state", data.c_str(), data.length(), 0, 0);
        }
    }
});




    // ESP_LOGE("MIAN", "creating MainLoop task");
    // xTaskCreate([](void* arg) {
    //     Application* app = (Application*)arg;
    //     app->MainLoop();
    //     vTaskDelete(NULL);
    // }, "MainLoop", 4096, NULL, 5, NULL);

// 定期发布状态更新
    xTaskCreate([](void* arg) {
        Application* app = (Application*)arg;
        if (!app) { // 添加空指针检查
            ESP_LOGE(TAG, "Application instance is NULL");
            vTaskDelete(NULL);
            return;
        }
        while(true){
            if (app->kelon_ac_ && app->mqttclient_ && app->sht3x_) { // 添加对 sht3x_ 的检查
                // 发布设定温度状态
                char set_temp_str[6];
                snprintf(set_temp_str, sizeof(set_temp_str), "%d", app->kelon_ac_->getTemp());
                app->mqttclient_->publish("homeassistant/climate/kelon_ac/temperature/state", set_temp_str, strlen(set_temp_str), 0, 0);

                // 发布模式状态 (包含开关逻辑)
                std::string mode_str;
                switch(app->kelon_ac_->getMode()){
                    case kKelonModeHeat: mode_str = "heat"; break;
                    case kKelonModeCool: mode_str = "cool"; break;
                    case kKelonModeDry: mode_str = "dry"; break;
                    case kKelonModeFan: mode_str = "fan_only"; break;
                    case kKelonModeSmart: mode_str = "auto"; break;
                    default: mode_str = "auto"; // 默认值，确保总有一个模式
                }

                app->mqttclient_->publish("homeassistant/climate/kelon_ac/mode/state", mode_str.c_str(), mode_str.length(), 0, 0);

                // 发布风扇状态
                std::string fan_str;
                switch(app->kelon_ac_->getFan()){
                    case kKelonFanMin: fan_str = "low"; break;
                    case kKelonFanMedium: fan_str = "medium"; break;
                    case kKelonFanMax: fan_str = "high"; break;
                    case kKelonFanAuto: fan_str = "auto"; break;
                    default: fan_str = "auto";
                }
                app->mqttclient_->publish("homeassistant/climate/kelon_ac/fan/state", fan_str.c_str(), fan_str.length(), 0, 0);

                // 发布当前环境温湿度到AC控件
                char current_temp_str[16]; 
                char current_humi_str[16]; 

                snprintf(current_temp_str, sizeof(current_temp_str), "%.2f", app->sht3x_->Tem_val);
                snprintf(current_humi_str, sizeof(current_humi_str), "%.2f", app->sht3x_->Hum_val);

                app->mqttclient_->publish("homeassistant/climate/kelon_ac/current_temperature", current_temp_str, strlen(current_temp_str), 0, 0);
                app->mqttclient_->publish("homeassistant/climate/kelon_ac/current_humidity", current_humi_str, strlen(current_humi_str), 0, 0);
                

                if (app->kelon_ac_->getTogglePower()) { // 使用你的 getTogglePower() 获取当前开关状态
                    app->mqttclient_->publish("homeassistant/switch/kelon_ac_power/state", "ON", 2, 0, 0);
                } else {
                    app->mqttclient_->publish("homeassistant/switch/kelon_ac_power/state", "OFF", 3, 0, 0);
                }

            } else {
                // 如果 kelon_ac_, mqttclient_ 或 sht3x_ 中的任何一个为空，打印警告并继续循环
                // 否则会在此处崩溃
                ESP_LOGW(TAG, "Missing kelon_ac_, mqttclient_ or sht3x_ instance in HAStateUpdate task.");
            }
            vTaskDelay(5000 / portTICK_PERIOD_MS); // 延时5秒
        }
    }, "HAStateUpdate", 4096, this, 5, NULL);


    // // 主循环保持设备运行
    // while(true){
    //     vTaskDelay(1000 / portTICK_PERIOD_MS);
    // }

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