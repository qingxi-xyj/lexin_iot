#include "doubao_api_service.h"
#include <esp_log.h>
#include <esp_http_client.h>
#include <cJSON.h>
#include "system_info.h"

#define TAG "DoubaoApiService"

DoubaoApiService::DoubaoApiService() {
}

DoubaoApiService::~DoubaoApiService() {
}

bool DoubaoApiService::Initialize(const std::string& api_key, const std::string& base_url) {
    ESP_LOGI(TAG, "Initializing DoubaoApiService");
    api_key_ = api_key;
    base_url_ = base_url;
    return true;
}

std::string DoubaoApiService::DetectIntent(const std::string& user_query) {
    ESP_LOGI(TAG, "Detecting intent for: %s", user_query.c_str());
    
    // 构造意图检测的prompt
    const char* intent_prompt = R"(
        你是一个智能语音助手的意图识别组件。你的任务是确定用户输入的意图是"拍照"还是"聊天"。
        如果用户的输入包含拍照、拍张照、帮我拍照、照相、拍个照等相关词汇，则意图为"拍照"。
        否则意图为"聊天"。
        只返回"拍照"或"聊天"这两个词中的一个，不要有任何额外文字。
    )";
    
    // 发送请求到豆包API
    std::string response = SendRequest(intent_prompt, user_query, true);
    
    // 清理响应文本（移除空格、引号等）
    response.erase(std::remove_if(response.begin(), response.end(), 
                                  [](unsigned char c) { return std::isspace(c) || c == '\"'; }), 
                   response.end());
    
    // 如果响应中包含"拍照"，则返回"拍照"，否则返回"聊天"
    if (response.find("拍照") != std::string::npos) {
        return "拍照";
    } else {
        return "聊天";
    }
}

std::string DoubaoApiService::GenerateResponseFromAnalysis(const std::string& analysis_json, 
                                                         const std::string& user_query) {
    ESP_LOGI(TAG, "Generating response from analysis");
    
    // 构造结合图像分析的prompt
    std::string prompt = R"(
        你是一个智能语音助手，具有图像分析能力。用户请求你分析一张图片，我已经通过计算机视觉模型分析了这张图片，
        并将分析结果以JSON格式提供给你。请基于这个分析结果，用自然、友好的语言回答用户的问题。
        
        用户的问题是: )" + user_query + R"(
        
        图像分析结果: )" + analysis_json;
    
    // 发送请求到豆包API
    return SendRequest(prompt, "", false);
}

std::string DoubaoApiService::GenerateResponse(const std::string& user_query) {
    ESP_LOGI(TAG, "Generating chat response for: %s", user_query.c_str());
    
    // 构造普通对话的prompt
    std::string prompt = R"(
        你是一个智能语音助手，名叫"小乐"。请用自然、友好的语言回答用户的问题。
        保持回答简洁明了，因为用户是通过语音与你交流的。
    )";
    
    // 发送请求到豆包API
    return SendRequest(prompt, user_query, false);
}

std::string DoubaoApiService::SendRequest(const std::string& prompt, 
                                        const std::string& user_message,
                                        bool is_intent_detection) {
    ESP_LOGI(TAG, "Sending request to Doubao API");
    
    // 在实际项目中，这里应该构造HTTP请求并发送到豆包API
    // 出于演示目的，我们先返回模拟响应
    
    if (is_intent_detection) {
        // 简单的关键词匹配用于演示
        if (user_message.find("拍照") != std::string::npos || 
            user_message.find("照相") != std::string::npos ||
            user_message.find("拍张") != std::string::npos) {
            return "拍照";
        } else {
            return "聊天";
        }
    } else if (prompt.find("图像分析结果") != std::string::npos) {
        // 图像分析响应示例
        return "我看到了图像中的内容，这是一张很漂亮的照片。";
    } else {
        // 普通对话响应示例
        return "我是小乐，很高兴为您服务！";
    }
}