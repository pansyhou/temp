#ifndef _APPLICATION_H_
#define _APPLICATION_H_

#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/task.h>
#include <esp_timer.h>
#include <bsp/bsp_sht3x.h>

#include <string>
#include <mutex>
#include <list>
#include <vector>
#include <condition_variable>
#include <memory>

#include <mqtt_protocol.h>
#include <ir_kelon.h>

class Application {
public:
    static Application& GetInstance() {
        static Application instance;
        return instance;
    }
    // 删除拷贝构造函数和赋值运算符
    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    void Start();
    

private:
    Application();
    ~Application();
    void MainLoop();
    void wifi_init();
    // 新的 MQTT 事件处理成员函数
    void mqtt_reg();
    sht3x *sht3x_;
    MqttClient *mqttclient_;
    IRKelonAc *kelon_ac_;
    // std::unique_ptr<WakeWord> wake_word_;
    // std::unique_ptr<AudioProcessor> audio_processor_;
    // Ota ota_;
    // std::mutex mutex_;
    // std::list<std::function<void()>> main_tasks_;
    // std::unique_ptr<Protocol> protocol_;
    // EventGroupHandle_t event_group_ = nullptr;
    // esp_timer_handle_t clock_timer_handle_ = nullptr;
    // volatile DeviceState device_state_ = kDeviceStateUnknown;
    // ListeningMode listening_mode_ = kListeningModeAutoStop;
    // AecMode aec_mode_ = kAecOff;

    // bool aborted_ = false;
    // bool voice_detected_ = false;
    // bool busy_decoding_audio_ = false;
    // int clock_ticks_ = 0;
    // TaskHandle_t check_new_version_task_handle_ = nullptr;

    // // Audio encode / decode
    // TaskHandle_t audio_loop_task_handle_ = nullptr;
    // BackgroundTask* background_task_ = nullptr;
    // std::chrono::steady_clock::time_point last_output_time_;
    // std::list<AudioStreamPacket> audio_send_queue_;
    // std::list<AudioStreamPacket> audio_decode_queue_;
    // std::condition_variable audio_decode_cv_;

    // // 新增：用于维护音频包的timestamp队列
    // std::list<uint32_t> timestamp_queue_;
    // std::mutex timestamp_mutex_;

    // std::unique_ptr<OpusEncoderWrapper> opus_encoder_;
    // std::unique_ptr<OpusDecoderWrapper> opus_decoder_;

    // OpusResampler input_resampler_;
    // OpusResampler reference_resampler_;
    // OpusResampler output_resampler_;

    // void MainEventLoop();
    // void OnAudioInput();
    // void OnAudioOutput();
    // bool ReadAudio(std::vector<int16_t>& data, int sample_rate, int samples);
    // void ResetDecoder();
    // void SetDecodeSampleRate(int sample_rate, int frame_duration);
    // void CheckNewVersion();
    // void ShowActivationCode();
    // void OnClockTimer();
    // void SetListeningMode(ListeningMode mode);
    // void AudioLoop();
};

#endif // _APPLICATION_H_




