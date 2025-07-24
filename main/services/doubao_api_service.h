#pragma once

#include <string>

class DoubaoApiService {
public:
    DoubaoApiService(const std::string& api_key, const std::string& secret_key);

    /**
     * @brief 从用户语音文本中识别意图。
     * @param text 用户语音识别后的文本。
     * @return std::string "拍照" 或 "聊天"。
     */
    std::string DetectIntent(const std::string& text);

    /**
     * @brief 根据图像分析结果和原始查询，生成自然语言回复。
     * @param analysis_result 私有模型返回的JSON分析结果。
     * @param original_query 用户的原始语音查询。
     * @return std::string 生成的自然语言回复。
     */
    std::string GenerateResponseFromAnalysis(const std::string& analysis_result, const std::string& original_query);

private:
    std::string api_key_;
    std::string secret_key_;
};