#ifndef DOUBAO_API_SERVICE_H
#define DOUBAO_API_SERVICE_H

#include <string>
#include <memory>
#include <esp_http_client.h>
#include <algorithm> // 添加此行以支持std::remove_if

// 豆包API服务
class DoubaoApiService {
public:
    DoubaoApiService();
    ~DoubaoApiService();

    // 初始化服务
    bool Initialize(const std::string& api_key, const std::string& base_url);
    
    // 判断用户意图（拍照/聊天）
    std::string DetectIntent(const std::string& user_query);
    
    // 处理图像分析结果，生成自然语言回复
    std::string GenerateResponseFromAnalysis(const std::string& analysis_json, const std::string& user_query);
    
    // 直接进行普通对话
    std::string GenerateResponse(const std::string& user_query);

private:
    // 发送请求到豆包API - 修改函数签名以匹配实现
    std::string SendRequest(const std::string& prompt, 
                           const std::string& user_message,
                           bool is_intent_detection = false);
    
    std::string api_key_;
    std::string base_url_;
};

#endif // DOUBAO_API_SERVICE_H