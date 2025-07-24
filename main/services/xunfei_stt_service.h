#pragma once

#include <string>
#include <functional>

class XunfeiSttService {
public:
    XunfeiSttService(const std::string& app_id, const std::string& api_key, const std::string& api_secret);
    
    /**
     * @brief 开始异步语音识别。
     * 
     * 在真实实现中，此函数将启动WebSocket连接并开始发送音频流。
     * 完成识别后，将通过 on_result 回调返回结果。
     * @param on_result 识别完成后的回调函数，参数为识别出的文本。
     */
    void Recognize(std::function<void(const std::string& text)> on_result);

private:
    std::string app_id_;
    std::string api_key_;
    std::string api_secret_;
};