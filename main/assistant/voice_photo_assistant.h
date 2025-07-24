#pragma once

#include "services/xunfei_stt_service.h"
#include "services/doubao_api_service.h"
#include <string>
#include <functional>

class VoicePhotoAssistant {
public:
    enum class State {
        kIdle,
        kListening,
        kThinking,
        kCapturingPhoto,
        kAnalyzingPhoto,
        kGeneratingResponse,
        kSpeaking
    };

    VoicePhotoAssistant(
        XunfeiSttService& stt_service,
        DoubaoApiService& doubao_service,
        std::function<void(State)> on_state_change
    );

    /**
     * @brief 当检测到唤醒词时由Application调用。
     */
    void OnWakeWord();

private:
    void SetState(State new_state);
    void StartListening();
    void ProcessUserSpeech(const std::string& text);
    void HandlePhotoIntent(const std::string& query);
    void HandleChatIntent(const std::string& query);
    void SavePhotoContext(const std::string& analysis_result, const std::string& query);

    State current_state_;
    XunfeiSttService& stt_service_;
    DoubaoApiService& doubao_service_;
    std::function<void(State)> state_change_callback_;
};