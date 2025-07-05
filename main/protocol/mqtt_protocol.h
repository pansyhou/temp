#ifndef MQTT_H
#define MQTT_H

#include "mqtt_client.h"
#include "esp_event.h" // 确保包含 esp_event.h

#include <functional> // 引入 std::function

// 定义 MQTT 事件回调函数类型
// 注意：这里我们让它更通用，直接传递 esp_mqtt_event_handle_t
// 外部调用者可以根据 event->event_id 来判断是哪种事件
using MqttEventHandler = std::function<void(esp_mqtt_event_handle_t event)>;

class MqttClient {
public:
    // 构造函数：初始化 MQTT 客户端配置
    MqttClient(const char* broker_uri, const char* username = nullptr, const char* password = nullptr);

    // 析构函数：清理 MQTT 客户端资源
    ~MqttClient();

    // 启动 MQTT 客户端连接
    esp_err_t start();

    // 停止 MQTT 客户端连接
    esp_err_t stop();

    // 发布消息
    int publish(const char* topic, const char* data, int len, int qos, int retain);

    // 订阅主题
    int subscribe(const char* topic, int qos);

    // 取消订阅主题
    int unsubscribe(const char* topic);

    // 设置自定义事件处理函数
    void onMqttEvent(MqttEventHandler callback);
private:
    esp_mqtt_client_config_t _mqtt_cfg;
    esp_mqtt_client_handle_t _client;

    MqttEventHandler _mqttEventHandler; // 存储外部注册的回调函数

    // 静态的 MQTT 事件回调函数，ESP-IDF 内部会调用这个函数
    // 它的 `handler_args` 参数会是 `MyMqttClient*`
    static void mqtt_event_static_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data);

    
};



#endif