#include "application.h"
#include "esp_log.h"
#include "board.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lang.h"
#include "system_info.h"

static const char* TAG = "Application";

Application& Application::GetInstance() {
    static Application instance;
    return instance;
}

Application::Application() : current_state_(kStateBooting) {}

Application::~Application() {}

void Application::Initialize() {
    ESP_LOGI(TAG, "Initializing application...");

    // 初始化硬件
    Board::GetInstance().Initialize();

    // 初始化显示服务
    display_ = std::make_unique<Display>();
    display_->Initialize();
    display_->SetStatus(Lang::Strings::BOOTING);

    // 初始化音频服务
    audio_service_ = std::make_unique<AudioService>();
    audio_service_->Initialize();

    // ==================== 新架构核心组件初始化 ====================
    // 初始化讯飞和豆包服务 (URL和KEY暂时为空，后续填充)
    xunfei_service_ = std::make_unique<XunfeiSttService>("", "", ""); 
    doubao_service_ = std::make_unique<DoubaoApiService>("", "");

    // 初始化语音拍照助手
    assistant_ = std::make_unique<VoicePhotoAssistant>(
        *xunfei_service_, 
        *doubao_service_,
        std::bind(&Application::HandleAssistantStateChange, this, std::placeholders::_1)
    );
    // ==========================================================

    // 设置唤醒词回调
    audio_service_->SetWakeWordCallback(std::bind(&Application::HandleWakeWord, this));
    
    ESP_LOGI(TAG, "Application initialized.");
}

void Application::Start() {
    ESP_LOGI(TAG, "Starting application...");
    SetState(kStateIdle);
    display_->SetStatus(Lang::Strings::NETWORK_CONNECTING);

    // 启动网络连接
    Board::GetInstance().StartNetwork();

    // 更新状态栏显示网络状态
    display_->UpdateStatusBar(true);

    // 网络就绪后，进入待机状态
    display_->SetStatus(Lang::Strings::IDLE);
    Alert("系统就绪", "你好，我是小乐，随时可以拍照", "happy");

    // 启动音频服务的待机模式（只检测唤醒词）
    audio_service_->EnableWakeWordDetection(true);
    
    ESP_LOGI(TAG, "Application started. Waiting for wake word.");
}

void Application::SetState(DeviceState state) {
    if (current_state_ != state) {
        current_state_ = state;
        ESP_LOGI(TAG, "Device state changed to: %d", static_cast<int>(state));
    }
}

DeviceState Application::GetState() const {
    return current_state_;
}

AudioService& Application::GetAudioService() {
    return *audio_service_;
}

Display& Application::GetDisplay() {
    return *display_;
}

void Application::Alert(const std::string& title, const std::string& message, const std::string& emotion) {
    ESP_LOGI(TAG, "Alert: %s - %s [%s]", title.c_str(), message.c_str(), emotion.c_str());
    display_->ShowAlert(title, message, emotion);
    // 注意：这里的语音播报逻辑需要与新的TTS服务集成，暂时留空或使用占位符
    // audio_service_->PlayTTS(message); 
}

void Application::SetChatMessage(const std::string& role, const std::string& content) {
    display_->SetChatMessage(role, content);
}

void Application::HandleWakeWord() {
    ESP_LOGI(TAG, "Wake word detected by Application.");
    if (current_state_ == kStateIdle) {
        assistant_->OnWakeWord();
    }
}

void Application::HandleSpeechResult(const std::string& text) {
    // 这个函数将在STT服务返回结果后被调用
    // 目前由VoicePhotoAssistant内部处理，这里暂时保留
}

void Application::HandleAssistantStateChange(VoicePhotoAssistant::State state) {
    // 将助手的内部状态映射到全局设备状态
    switch (state) {
        case VoicePhotoAssistant::State::kIdle:
            SetState(kStateIdle);
            display_->SetStatus(Lang::Strings::IDLE);
            break;
        case VoicePhotoAssistant::State::kListening:
            SetState(kStateListening);
            display_->SetStatus(Lang::Strings::LISTENING);
            Alert("正在聆听", "请说出您的需求...", "neutral");
            break;
        case VoicePhotoAssistant::State::kThinking:
            SetState(kStateThinking);
            display_->SetStatus(Lang::Strings::THINKING);
            break;
        case VoicePhotoAssistant::State::kCapturingPhoto:
            SetState(kStateCapturingPhoto);
            break;
        case VoicePhotoAssistant::State::kAnalyzingPhoto:
            SetState(kStateAnalyzingPhoto);
            break;
        case VoicePhotoAssistant::State::kGeneratingResponse:
            SetState(kStateGeneratingResponse);
            break;
        case VoicePhotoAssistant::State::kSpeaking:
            SetState(kStateSpeaking);
            break;
    }
}