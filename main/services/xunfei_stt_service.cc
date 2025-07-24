#include "xunfei_stt_service.h"
#include <esp_log.h>
#include <mbedtls/base64.h>
#include <mbedtls/md.h>
#include <cJSON.h>
#include <time.h>
#include <sys/time.h>

#define TAG "XunfeiSttService"

XunfeiSttService::XunfeiSttService() {
}

XunfeiSttService::~XunfeiSttService() {
}

bool XunfeiSttService::Initialize(const std::string& appid, const std::string& apikey, const std::string& apisecret) {
    ESP_LOGI(TAG, "Initializing XunfeiSttService");
    appid_ = appid;
    apikey_ = apikey;
    apisecret_ = apisecret;
    return true;
}

std::string XunfeiSttService::Base64Encode(const std::string& input) {
    size_t output_len = 0;
    mbedtls_base64_encode(nullptr, 0, &output_len, 
                         (const unsigned char*)input.c_str(), input.length());
    
    unsigned char* output_buffer = (unsigned char*)malloc(output_len);
    if (!output_buffer) {
        ESP_LOGE(TAG, "Failed to allocate memory for base64 encoding");
        return "";
    }
    
    mbedtls_base64_encode(output_buffer, output_len, &output_len, 
                         (const unsigned char*)input.c_str(), input.length());
    
    std::string result((char*)output_buffer, output_len);
    free(output_buffer);
    return result;
}

std::string XunfeiSttService::HmacSha256(const std::string& key, const std::string& data) {
    unsigned char hmac[32];
    mbedtls_md_context_t ctx;
    mbedtls_md_init(&ctx);
    mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), 1);
    mbedtls_md_hmac_starts(&ctx, (const unsigned char*)key.c_str(), key.length());
    mbedtls_md_hmac_update(&ctx, (const unsigned char*)data.c_str(), data.length());
    mbedtls_md_hmac_finish(&ctx, hmac);
    mbedtls_md_free(&ctx);
    
    std::string result((char*)hmac, 32);
    return result;
}

std::string XunfeiSttService::FormatRFC1123Date() {
    char date_str[100];
    time_t now;
    struct tm timeinfo;
    time(&now);
    gmtime_r(&now, &timeinfo);
    strftime(date_str, sizeof(date_str), "%a, %d %b %Y %H:%M:%S GMT", &timeinfo);
    return std::string(date_str);
}

// 简化的语音识别实现 - 使用批处理模式而非实时流
std::string XunfeiSttService::RecognizeAudio(const std::vector<uint8_t>& audio_data) {
    ESP_LOGI(TAG, "Recognizing audio data (%zu bytes)", audio_data.size());
    
    // 在这里，我们简化实现，仅返回模拟结果
    // 实际项目中，应该发送HTTP请求到讯飞API
    
    // 模拟成功识别
    std::string result = "请拍照";
    
    // 调用回调函数
    if (result_callback_) {
        result_callback_(result, true);
    }
    
    return result;
}

void XunfeiSttService::SetResultCallback(std::function<void(const std::string& text, bool is_final)> callback) {
    result_callback_ = callback;
}