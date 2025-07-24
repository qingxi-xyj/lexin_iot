#include "voice_photo_assistant.h"
#include "application.h" // 引入Application头文件以访问全局服务
#include "board.h"
#include "esp_log.h"

static const char* TAG = "VoicePhotoAssistant";

VoicePhotoAssistant::VoicePhotoAssistant(
    XunfeiSttService& stt_service,
    DoubaoApiService& doubao_service,
    std::function<void(State)> on_state_change)
    : current_state_(State::kIdle),
      stt_service_(stt_service),
      doubao_service_(doubao_service),
      state_change_callback_(on_state_change) {
}

void VoicePhotoAssistant::SetState(State new_state) {
    if (current_state_ != new_state) {
        current_state_ = new_state;
        if (state_change_callback_) {
            state_change_callback_(current_state_);
        }
    }
}

void VoicePhotoAssistant::OnWakeWord() {
    if (current_state_ == State::kIdle) {
        StartListening();
    }
}

void VoicePhotoAssistant::StartListening() {
    ESP_LOGI(TAG, "Starting to listen for user command...");
    SetState(State::kListening);
    
    auto& app = Application::GetInstance();
    // 开启语音处理，关闭唤醒词检测
    app.GetAudioService().EnableVoiceProcessing(true);
    app.GetAudioService().EnableWakeWordDetection(false);

    // 调用STT服务进行识别
    stt_service_.Recognize([this](const std::string& text) {
        // STT回调在新任务中执行，避免阻塞
        xTaskCreate([](void* param) {
            auto* assistant = static_cast<VoicePhotoAssistant*>(param);
            std::string* speech_text = static_cast<std::string*>(assistant->user_speech_text_param);
            assistant->ProcessUserSpeech(*speech_text);
            delete speech_text;
            vTaskDelete(NULL);
        }, "ProcessSpeechTask", 4096, new std::pair<VoicePhotoAssistant*, new std::string(text)>{this, new std::string(text)}, 5, NULL);
    });
}


void VoicePhotoAssistant::ProcessUserSpeech(const std::string& text) {
    SetState(State::kThinking);
    
    std::string intent = doubao_service_.DetectIntent(text);

    if (intent == "拍照") {
        HandlePhotoIntent(text);
    } else {
        HandleChatIntent(text);
    }
}

void VoicePhotoAssistant::HandlePhotoIntent(const std::string& query) {
    ESP_LOGI(TAG, "Handling photo intent: %s", query.c_str());
    auto& app = Application::GetInstance();

    // 暂停所有音频输入处理，防止AFE溢出
    ESP_LOGI(TAG, "Pausing audio service for photo capture...");
    app.GetAudioService().EnableVoiceProcessing(false);
    app.GetAudioService().EnableWakeWordDetection(false);

    SetState(State::kCapturingPhoto);
    app.Alert("正在拍照", "请保持稳定...", "happy");
    
    auto camera = Board::GetInstance().GetCamera();
    if (!camera || !camera->Capture()) {
        app.Alert("拍照失败", "无法拍照，请重试", "sad");
        SetState(State::kIdle);
        // 恢复待机状态（只开启唤醒词）
        app.GetAudioService().EnableWakeWordDetection(true);
        return;
    }
    
    SetState(State::kAnalyzingPhoto);
    app.Alert("正在分析", "请稍候...", "neutral");
    
    // ================== 私有模型调用（模拟） ==================
    ESP_LOGW(TAG, "MOCK: Calling private vision model...");
    // 真实实现：camera->Explain(query) 会将图像上传到私有服务器
    // std::string analysis_result = camera->Explain(query); 
    vTaskDelay(pdMS_TO_TICKS(3000)); // 模拟网络和分析耗时
    std::string analysis_result = "{\"description\":\"a cute cat is sitting on the table\"}";
    ESP_LOGW(TAG, "MOCK: Private model result: %s", analysis_result.c_str());
    // ==========================================================

    SetState(State::kGeneratingResponse);
    std::string response = doubao_service_.GenerateResponseFromAnalysis(analysis_result, query);
    
    SetState(State::kSpeaking);
    app.Alert("分析结果", response.c_str(), "happy");
    
    // 流程结束，回到待机状态
    SetState(State::kIdle);
    ESP_LOGI(TAG, "Photo intent finished. Resuming to idle state.");
    app.GetAudioService().EnableWakeWordDetection(true);

    SavePhotoContext(analysis_result, query);
}

void VoicePhotoAssistant::HandleChatIntent(const std::string& query) {
    // 聊天意图的处理流程（待实现）
    ESP_LOGI(TAG, "Handling chat intent: %s", query.c_str());
    SetState(State::kIdle);
    Application::GetInstance().GetAudioService().EnableWakeWordDetection(true);
}

void VoicePhotoAssistant::SavePhotoContext(const std::string& analysis_result, const std::string& query) {
    // 上下文保存逻辑（待实现）
    ESP_LOGI(TAG, "Saving photo context...");
}