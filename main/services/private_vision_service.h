#ifndef PRIVATE_VISION_SERVICE_H
#define PRIVATE_VISION_SERVICE_H

#include <string>
#include <vector>
#include <esp_http_client.h>

// 私有视觉模型服务
class PrivateVisionService {
public:
    PrivateVisionService();
    ~PrivateVisionService();

    // 初始化服务
    bool Initialize(const std::string& api_url, const std::string& api_key = "");
    
    // 分析图像
    std::string AnalyzeImage(const std::vector<uint8_t>& image_data, const std::string& query);

private:
    std::string api_url_;
    std::string api_key_;
};

#endif // PRIVATE_VISION_SERVICE_H