#ifndef XUNFEI_STT_SERVICE_H
#define XUNFEI_STT_SERVICE_H

#include <string>
#include <vector>
#include <functional>
#include <esp_http_client.h>

// 讯飞语音识别服务
class XunfeiSttService {
public:
    XunfeiSttService();
    ~XunfeiSttService();

    // 初始化服务
    bool Initialize(const std::string& appid, const std::string& apikey, const std::string& apisecret);
    
    // 识别音频数据（批处理模式）
    std::string RecognizeAudio(const std::vector<uint8_t>& audio_data);
    
    // 设置识别结果回调
    void SetResultCallback(std::function<void(const std::string& text, bool is_final)> callback);

private:
    // 创建认证URL
    std::string CreateAuthUrl();
    
    // 辅助函数
    std::string Base64Encode(const std::string& input);
    std::string HmacSha256(const std::string& key, const std::string& data);
    std::string FormatRFC1123Date();
    
    std::string appid_;
    std::string apikey_;
    std::string apisecret_;
    
    std::function<void(const std::string& text, bool is_final)> result_callback_;
};

#endif // XUNFEI_STT_SERVICE_H