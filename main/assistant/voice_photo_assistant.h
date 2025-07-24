#ifndef VOICE_PHOTO_ASSISTANT_H
#define VOICE_PHOTO_ASSISTANT_H

#include <string>
#include <memory>
#include <map> // 添加这个头文件
#include "services/xunfei_stt_service.h"
#include "services/doubao_api_service.h"
#include "services/private_vision_service.h"

// 定义助手状态
enum AssistantState {
    kStateIdle,
    kStateListening,
    kStateProcessingSpeech,
    kStateCapturingPhoto,
    kStateAnalyzingPhoto,
    kStateGeneratingResponse,
    kStateSpeaking
};

// 语音拍照助手
class VoicePhotoAssistant {
public:
    static VoicePhotoAssistant& GetInstance();
    
    // 删除拷贝构造函数和赋值运算符
    VoicePhotoAssistant(const VoicePhotoAssistant&) = delete;
    VoicePhotoAssistant& operator=(const VoicePhotoAssistant&) = delete;
    
    // 初始化助手
    bool Initialize();
    
    // 处理唤醒事件
    void OnWakeWord();
    
    // 处理用户语音输入
    void ProcessUserSpeech(const std::string& speech_text);
    
    // 获取当前状态
    AssistantState GetState() const { return state_; }
    
    // 设置状态改变回调
    void SetStateChangeCallback(std::function<void(AssistantState)> callback);

private:
    VoicePhotoAssistant();
    ~VoicePhotoAssistant();
    
    // 处理拍照意图
    void HandlePhotoIntent(const std::string& query);
    
    // 处理普通聊天意图
    void HandleChatIntent(const std::string& query);
    
    // 添加保存照片上下文的方法声明
    void SavePhotoContext(const std::string& analysis_result, const std::string& query);
    
    AssistantState state_ = kStateIdle;
    std::function<void(AssistantState)> state_change_callback_;
    
    // 添加对话上下文
    std::map<std::string, std::string> conversation_context_;
    
    std::unique_ptr<XunfeiSttService> stt_service_;
    std::unique_ptr<DoubaoApiService> doubao_service_;
    std::unique_ptr<PrivateVisionService> vision_service_;
};

#endif // VOICE_PHOTO_ASSISTANT_H