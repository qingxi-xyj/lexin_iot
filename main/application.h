#pragma once

#include "board.h"
#include "display.h"
#include "audio/audio_service.h"
#include "assistant/voice_photo_assistant.h" // 引入新的助手
#include "services/xunfei_stt_service.h"      // 引入讯飞服务
#include "services/doubao_api_service.h"    // 引入豆包服务
#include <memory>
#include <string>
#include <functional>

// 定义设备状态
enum DeviceState {
    kStateBooting,
    kStateIdle,
    kStateListening,
    kStateThinking,
    kStateSpeaking,
    kStateCapturingPhoto,
    kStateAnalyzingPhoto,
    kStateGeneratingResponse,
    kStateError
};

class Application {
public:
    static Application& GetInstance();
    void Initialize();
    void Start();
    void SetState(DeviceState state);
    DeviceState GetState() const;

    // 公共服务访问接口
    AudioService& GetAudioService();
    Display& GetDisplay();

    // UI交互接口
    void Alert(const std::string& title, const std::string& message, const std::string& emotion);
    void SetChatMessage(const std::string& role, const std::string& content);

private:
    Application();
    ~Application();
    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    void HandleWakeWord();
    void HandleSpeechResult(const std::string& text);
    void HandleAssistantStateChange(VoicePhotoAssistant::State state);

    DeviceState current_state_;
    std::unique_ptr<AudioService> audio_service_;
    std::unique_ptr<Display> display_;
    
    // ==================== 新架构核心组件 ====================
    std::unique_ptr<XunfeiSttService> xunfei_service_;
    std::unique_ptr<DoubaoApiService> doubao_service_;
    std::unique_ptr<VoicePhotoAssistant> assistant_;
    // ========================================================
};