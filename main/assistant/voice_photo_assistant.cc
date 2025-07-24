#include "voice_photo_assistant.h"
#include "board.h"
#include "application.h"
#include <esp_log.h>

#define TAG "VoicePhotoAssistant"

VoicePhotoAssistant& VoicePhotoAssistant::GetInstance() {
    static VoicePhotoAssistant instance;
    return instance;
}

VoicePhotoAssistant::VoicePhotoAssistant() {
    stt_service_ = std::make_unique<XunfeiSttService>();
    doubao_service_ = std::make_unique<DoubaoApiService>();
    vision_service_ = std::make_unique<PrivateVisionService>();
}

VoicePhotoAssistant::~VoicePhotoAssistant() {
}

bool VoicePhotoAssistant::Initialize() {
    ESP_LOGI(TAG, "Initializing VoicePhotoAssistant");
    
    // 初始化讯飞STT服务
    if (!stt_service_->Initialize("eea569cf", "79b32367bb028b5c6fc96b8a1b90f81d", "Y2MzOTNkYzQ0ZTlmZWEzZWUwMTEwZWRl")) {
        ESP_LOGE(TAG, "Failed to initialize STT service");
        return false;
    }
    
    // 初始化豆包API服务
    if (!doubao_service_->Initialize("sk-5b1c4856c819468cbc84120ef8810c22gnv3ztvn1m19yi04", "https://ai-gateway.vei.volces.com/v1")) {
        ESP_LOGE(TAG, "Failed to initialize Doubao API service");
        return false;
    }
    
    // 初始化私有视觉模型服务
    if (!vision_service_->Initialize("http://your-private-model-endpoint.com/analyze")) {
        ESP_LOGE(TAG, "Failed to initialize Private Vision service");
        return false;
    }
    
    // 设置STT结果回调
    stt_service_->SetResultCallback([this](const std::string& text, bool is_final) {
        if (is_final) {
            ProcessUserSpeech(text);
        }
    });
    
    return true;
}

void VoicePhotoAssistant::OnWakeWord() {
    ESP_LOGI(TAG, "Wake word detected, starting to listen");
    
    if (state_ != kStateIdle) {
        ESP_LOGW(TAG, "Already in active state: %d", state_);
        return;
    }
    
    // 更新状态
    state_ = kStateListening;
    if (state_change_callback_) {
        state_change_callback_(state_);
    }
    
    // 使用Application的方法正确处理唤醒和状态转换
    auto& application = Application::GetInstance();
    
    // 1. 显示提示并播放声音
    application.Alert("正在聆听", "请说出您的需求...", "neutral");
    
    // 2. 停止当前活动以重置缓冲区
    application.StopListening();
    
    // 3. 关键步骤：短暂延迟以确保缓冲区清空
    vTaskDelay(pdMS_TO_TICKS(50));
    
    // 4. 重置音频输入硬件
    auto& board = Board::GetInstance();
    auto* codec = board.GetAudioCodec();
    if (codec) {
        codec->EnableInput(false);
        vTaskDelay(pdMS_TO_TICKS(20));
        codec->EnableInput(true);
    }
    
    // 5. 启动新的监听会话
    application.StartListening();
    
    // 允许短暂延迟以确保状态完全转换
    vTaskDelay(pdMS_TO_TICKS(30));
    
    // 模拟收到"请拍照"命令
    std::vector<uint8_t> dummy_audio(1600, 0);  // 模拟音频数据
    std::string result = stt_service_->RecognizeAudio(dummy_audio);
    
    ESP_LOGI(TAG, "Simulated speech recognition result: %s", result.c_str());
}

void VoicePhotoAssistant::ProcessUserSpeech(const std::string& speech_text) {
    ESP_LOGI(TAG, "Processing user speech: %s", speech_text.c_str());
    
    state_ = kStateProcessingSpeech;
    if (state_change_callback_) {
        state_change_callback_(state_);
    }
    
    // 使用Application类显示用户消息
    auto& application = Application::GetInstance();
    application.SetChatMessage("user", speech_text.c_str());
    
    // 识别意图
    std::string intent = doubao_service_->DetectIntent(speech_text);
    ESP_LOGI(TAG, "Detected intent: %s", intent.c_str());
    
    // 根据意图分支处理
    if (intent == "拍照") {
        HandlePhotoIntent(speech_text);
    } else {
        HandleChatIntent(speech_text);
    }
}

void VoicePhotoAssistant::HandlePhotoIntent(const std::string& query) {
    ESP_LOGI(TAG, "Handling photo intent: %s", query.c_str());
    
    // 更新状态
    state_ = kStateCapturingPhoto;
    if (state_change_callback_) {
        state_change_callback_(state_);
    }
    
    // 显示正在拍照提示
    auto& application = Application::GetInstance();
    application.Alert("正在拍照", "请保持稳定...", "happy");
    
    // 拍照
    auto camera = Board::GetInstance().GetCamera();
    if (!camera || !camera->Capture()) {
        application.Alert("拍照失败", "无法拍照，请重试", "sad");
        state_ = kStateIdle;
        if (state_change_callback_) {
            state_change_callback_(state_);
        }
        return;
    }
    
    // 更新状态
    state_ = kStateAnalyzingPhoto;
    if (state_change_callback_) {
        state_change_callback_(state_);
    }
    
    // 显示正在分析提示
    application.Alert("正在分析", "正在分析照片...", "neutral");
    
    // 发送到图像分析服务
    std::string analysis_result = camera->Explain(query);
    
    // 更新状态
    state_ = kStateGeneratingResponse;
    if (state_change_callback_) {
        state_change_callback_(state_);
    }
    
    // 将分析结果发送给豆包API
    std::string response = doubao_service_->GenerateResponseFromAnalysis(analysis_result, query);
    
    // 使用Application类显示助手消息
    application.SetChatMessage("assistant", response.c_str());
    
    // 播放回复
    state_ = kStateSpeaking;
    if (state_change_callback_) {
        state_change_callback_(state_);
    }
    
    // 使用TTS播放响应
    application.Alert("分析结果", response.c_str(), "happy");
    
    // 修改：不要回到空闲状态，而是进入聆听状态
    state_ = kStateListening;
    if (state_change_callback_) {
        state_change_callback_(state_);
    }
    
    // 显示继续对话的提示
    application.Alert("继续对话", "还有什么我可以帮您的吗？", "neutral");
    
    // 保存照片分析上下文，如果有上下文管理机制的话
     SavePhotoContext(analysis_result, query);
}

// 修改保存照片上下文的方法，添加第二个参数
void VoicePhotoAssistant::SavePhotoContext(const std::string& analysis_result, const std::string& query) {
    conversation_context_["last_photo_analysis"] = analysis_result;
    conversation_context_["last_photo_query"] = query;
    conversation_context_["has_recent_photo"] = "true";
}

void VoicePhotoAssistant::HandleChatIntent(const std::string& query) {
    ESP_LOGI(TAG, "Handling chat intent: %s", query.c_str());
    
    // 更新状态
    state_ = kStateGeneratingResponse;
    if (state_change_callback_) {
        state_change_callback_(state_);
    }
    
    // 获取回复
    std::string response = doubao_service_->GenerateResponse(query);
    
    // 使用Application类显示助手消息
    auto& application = Application::GetInstance();
    application.SetChatMessage("assistant", response.c_str());
    
    // 播放回复
    state_ = kStateSpeaking;
    if (state_change_callback_) {
        state_change_callback_(state_);
    }
    
    // 使用TTS播放响应
    application.Alert("回复", response.c_str(), "happy");
    
    // 恢复空闲状态
    state_ = kStateIdle;
    if (state_change_callback_) {
        state_change_callback_(state_);
    }
}

void VoicePhotoAssistant::SetStateChangeCallback(std::function<void(AssistantState)> callback) {
    state_change_callback_ = callback;
}