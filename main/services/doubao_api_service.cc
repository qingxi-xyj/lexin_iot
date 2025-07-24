#include "doubao_api_service.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char* TAG = "DoubaoApiService";

DoubaoApiService::DoubaoApiService(const std::string& api_key, const std::string& secret_key)
    : api_key_(api_key), secret_key_(secret_key) {
    ESP_LOGI(TAG, "Doubao API Service initialized.");
}

std::string DoubaoApiService::DetectIntent(const std::string& text) {
    ESP_LOGI(TAG, "Detecting intent for text: '%s'", text.c_str());
    
    // ================== 模拟实现 ==================
    // 真实实现会调用HTTP客户端向豆包API发送请求。
    // 这里我们简单地判断文本是否包含“拍照”。
    if (text.find("拍照") != std::string::npos) {
        ESP_LOGW(TAG, "MOCK: Intent detected as '拍照'");
        return "拍照";
    }
    ESP_LOGW(TAG, "MOCK: Intent detected as '聊天'");
    return "聊天";
    // ===============================================
}

std::string DoubaoApiService::GenerateResponseFromAnalysis(const std::string& analysis_result, const std::string& original_query) {
    ESP_LOGI(TAG, "Generating response from analysis: %s", analysis_result.c_str());
    
    // ================== 模拟实现 ==================
    // 真实实现会调用HTTP客户端，将分析结果和原始问题发给豆包API。
    ESP_LOGW(TAG, "MOCK: Simulating a 1-second LLM response generation.");
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    std::string mock_response = "好的，照片已经分析完毕。我看到照片里有一只可爱的猫咪。";
    ESP_LOGW(TAG, "MOCK: Simulated response: %s", mock_response.c_str());
    return mock_response;
    // ===============================================
}