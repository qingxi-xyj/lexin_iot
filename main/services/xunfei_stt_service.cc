#include "xunfei_stt_service.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char* TAG = "XunfeiSttService";

XunfeiSttService::XunfeiSttService(const std::string& app_id, const std::string& api_key, const std::string& api_secret)
    : app_id_(app_id), api_key_(api_key), api_secret_(api_secret) {
    ESP_LOGI(TAG, "Xunfei STT Service initialized.");
}

void XunfeiSttService::Recognize(std::function<void(const std::string& text)> on_result) {
    ESP_LOGI(TAG, "Starting speech recognition...");
    
    // ================== 模拟实现 ==================
    // 在真实场景中，这里会启动WebSocket连接，
    // 从AudioService获取音频流并发送，然后异步等待结果。
    // 为了快速调试，我们在此模拟一个2秒后返回固定结果的场景。
    
    ESP_LOGW(TAG, "MOCK: Simulating a 2-second speech recognition task.");
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    std::string mock_result = "请拍照";
    ESP_LOGW(TAG, "MOCK: Simulated recognition result: %s", mock_result.c_str());
    
    if (on_result) {
        on_result(mock_result);
    }
    // ===============================================
}