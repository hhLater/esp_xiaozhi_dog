#ifndef _APPLICATION_H_
#define _APPLICATION_H_

#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/task.h>

#include <string>
#include <mutex>
#include <list>

#include <opus_encoder.h>
#include <opus_decoder.h>
#include <opus_resampler.h>

#include "protocol.h"
#include "ota.h"
#include "background_task.h"

#if CONFIG_IDF_TARGET_ESP32S3
#include "wake_word_detect.h"
#include "audio_processor.h"
#include "pet_dog.h"
#endif

#define SCHEDULE_EVENT (1 << 0)
#define AUDIO_INPUT_READY_EVENT (1 << 1)
#define AUDIO_OUTPUT_READY_EVENT (1 << 2)

#define ACTION_TASK_EVENT (1 << 0)
#define ACTION_TASK_EVENT_STOP (1 << 1)

enum DeviceState {
    kDeviceStateUnknown,
    kDeviceStateStarting,
    kDeviceStateWifiConfiguring,
    kDeviceStateIdle,
    kDeviceStateConnecting,
    kDeviceStateListening,
    kDeviceStateSpeaking,
    kDeviceStateUpgrading,
    kDeviceStateFatalError
};

enum ActionState {
    kActionStateWalk,
    kActionStateSleep,
    kActionStateStand,
    kActionStateSitdown,
    kActionStateWalkBack,
    kActionStateTurnLeft,
    kActionStateTurnRight,
    kActionStateWave,
    kActionStateStop
};

#define OPUS_FRAME_DURATION_MS 60

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
    DeviceState GetDeviceState() const { return device_state_; }
    ActionState GetActionState() const { return action_state_; }
    void SetActionState(ActionState newState);
    void SetDeviceState(DeviceState state);
    bool IsVoiceDetected() const { return voice_detected_; }
    void Schedule(std::function<void()> callback);
    void Alert(const std::string& title, const std::string& message);
    void AbortSpeaking(AbortReason reason);
    void ToggleChatState();
    void StartListening();
    void StopListening();
    void UpdateIotStates();

    EventGroupHandle_t action_event_group_;

private:
    Application();
    ~Application();

#if CONFIG_IDF_TARGET_ESP32S3
    WakeWordDetect wake_word_detect_;
    AudioProcessor audio_processor_;
    PetDog dog;
#endif
    Ota ota_;
    std::mutex mutex_;
    std::list<std::function<void()>> main_tasks_;
    std::unique_ptr<Protocol> protocol_;
    EventGroupHandle_t event_group_;
    volatile DeviceState device_state_ = kDeviceStateIdle;
    volatile ActionState action_state_ = kActionStateSleep;
    bool keep_listening_ = false;
    bool aborted_ = false;
    bool voice_detected_ = false;
    std::string last_iot_states_;

    // Audio encode / decode
    BackgroundTask background_task_;
    std::chrono::steady_clock::time_point last_output_time_;
    std::list<std::vector<uint8_t>> audio_decode_queue_;

    std::unique_ptr<OpusEncoderWrapper> opus_encoder_;
    std::unique_ptr<OpusDecoderWrapper> opus_decoder_;

    int opus_decode_sample_rate_ = -1;
    OpusResampler input_resampler_;
    OpusResampler reference_resampler_;
    OpusResampler output_resampler_;

    void MainLoop();
    void InputAudio();
    void OutputAudio();
    void ResetDecoder();
    void SetDecodeSampleRate(int sample_rate);
    void CheckNewVersion();

    void PlayLocalFile(const char* data, size_t size);
};

#endif // _APPLICATION_H_
