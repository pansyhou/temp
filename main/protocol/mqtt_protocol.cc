#include <mqtt_protocol.h>

#include "esp_log.h"
#include <string.h> // For strlen

static const char *TAG = "MqttClient";

// 静态成员变量不能在头文件中直接初始化
// 这里需要在 .cpp 文件中定义，但通常对于句柄和配置，我们会在构造函数中处理
// 所以这里不需要特殊的静态成员变量初始化

MqttClient::MqttClient(const char* broker_uri, const char* username, const char* password)
    : _client(nullptr), _mqttEventHandler(nullptr) { // 初始化成员变量
    
    // 清零配置结构体
    memset(&_mqtt_cfg, 0, sizeof(esp_mqtt_client_config_t));
    
    // 设置 Broker URI
    _mqtt_cfg.broker.address.uri = broker_uri;

    // 设置用户名和密码 (如果有)
    if (username) {
        _mqtt_cfg.credentials.username = username;
    }
    if (password) {
        _mqtt_cfg.credentials.authentication.password = password;
    }

    // 初始化 MQTT 客户端
    _client = esp_mqtt_client_init(&_mqtt_cfg);
    if (_client == nullptr) {
        ESP_LOGE(TAG, "Failed to initialize MQTT client!");
        // 可以在这里抛出异常或处理错误
    } else {
        // 注册事件处理函数，将当前 MyMqttClient 实例作为 user_context 传递
        esp_mqtt_client_register_event(_client, MQTT_EVENT_ANY, mqtt_event_static_handler, this);
    }
}

MqttClient::~MqttClient() {
    if (_client) {
        esp_mqtt_client_destroy(_client); // 销毁客户端资源
        _client = nullptr;
    }
}

esp_err_t MqttClient::start() {
    if (!_client) {
        ESP_LOGE(TAG, "MQTT client not initialized, cannot start.");
        return ESP_FAIL;
    }
    return esp_mqtt_client_start(_client);
}

esp_err_t MqttClient::stop() {
    if (!_client) {
        ESP_LOGE(TAG, "MQTT client not initialized, cannot stop.");
        return ESP_FAIL;
    }
    return esp_mqtt_client_stop(_client);
}

int MqttClient::publish(const char* topic, const char* data, int len, int qos, int retain) {
    if (!_client) {
        ESP_LOGE(TAG, "MQTT client not initialized, cannot publish.");
        return -1;
    }
    return esp_mqtt_client_publish(_client, topic, data, len, qos, retain);
}

int MqttClient::subscribe(const char* topic, int qos) {
    if (!_client) {
        ESP_LOGE(TAG, "MQTT client not initialized, cannot subscribe.");
        return -1;
    }
    return esp_mqtt_client_subscribe(_client, topic, qos);
}

int MqttClient::unsubscribe(const char* topic) {
    if (!_client) {
        ESP_LOGE(TAG, "MQTT client not initialized, cannot unsubscribe.");
        return -1;
    }
    return esp_mqtt_client_unsubscribe(_client, topic);
}


// 新增：注册回调的方法实现
void MqttClient::onMqttEvent(MqttEventHandler callback) {
    _mqttEventHandler = callback;
}

// 静态事件处理函数，负责将事件转发给 _mqttEventHandler
void MqttClient::mqtt_event_static_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
    MqttClient* self = static_cast<MqttClient*>(handler_args); // 获取 MyMqttClient 实例
    esp_mqtt_event_handle_t event = static_cast<esp_mqtt_event_handle_t>(event_data);

    ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%" PRIi32 "", base, event_id);

    // 如果外部注册了回调，则调用它
    if (self && self->_mqttEventHandler) {
        self->_mqttEventHandler(event); // 调用外部注册的 std::function 回调
    } 
    else {
        // 默认的事件日志 (如果没有设置外部处理函数)
        switch ((esp_mqtt_event_id_t)event_id) {
            case MQTT_EVENT_CONNECTED: ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED (default)"); break;
            case MQTT_EVENT_DISCONNECTED: ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED (default)"); break;
            case MQTT_EVENT_PUBLISHED: ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED (default), msg_id=%d", event->msg_id); break;
            case MQTT_EVENT_DATA:
                ESP_LOGI(TAG, "MQTT_EVENT_DATA (default)");
                printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
                printf("DATA=%.*s\r\n", event->data_len, event->data);
                break;
            case MQTT_EVENT_ERROR: ESP_LOGI(TAG, "MQTT_EVENT_ERROR (default)"); break;
            default: ESP_LOGI(TAG, "Other event id (default):%d", event->event_id); break;
        }
    }
}