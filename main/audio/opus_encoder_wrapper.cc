#include "opus_encoder_wrapper.h"
#include <esp_log.h>

static const char* TAG = "OpusEncoder";

OpusEncoderWrapper::OpusEncoderWrapper(int sample_rate, int channels, int frame_duration_ms) {
    int error;
    
    // 创建Opus编码器
    encoder_ = opus_encoder_create(sample_rate, channels, OPUS_APPLICATION_VOIP, &error);
    if (error != OPUS_OK || encoder_ == nullptr) {
        ESP_LOGE(TAG, "Failed to create Opus encoder: %d", error);
        return;
    }
    
    // 配置编码器参数
    opus_encoder_ctl(encoder_, OPUS_SET_BITRATE(16000));
    opus_encoder_ctl(encoder_, OPUS_SET_COMPLEXITY(4));
    
    // 计算每帧样本数
    samples_per_frame_ = (sample_rate * frame_duration_ms) / 1000;
    
    // 最大包大小 (按照Opus规范)
    max_packet_size_ = 1275; // 最大Opus包大小
    
    // 初始化帧缓冲区
    frame_buffer_.resize(samples_per_frame_, 0);
    frame_buffer_pos_ = 0;
    
    ESP_LOGI(TAG, "Opus encoder initialized: sample_rate=%d, channels=%d, frame_duration=%dms", 
             sample_rate, channels, frame_duration_ms);
}

OpusEncoderWrapper::~OpusEncoderWrapper() {
    if (encoder_) {
        opus_encoder_destroy(encoder_);
        encoder_ = nullptr;
    }
}

bool OpusEncoderWrapper::Encode(std::vector<int16_t>&& pcm_data, std::vector<uint8_t>& opus_data) {
    if (!encoder_) {
        ESP_LOGE(TAG, "Encoder not initialized");
        return false;
    }
    
    // 安全检查 - 防止空数据
    if (pcm_data.empty()) {
        ESP_LOGE(TAG, "Empty PCM data");
        return false;
    }
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    // 使用帧累积策略，将PCM数据累积到完整帧
    try {
        // 计算当前帧中还需要多少样本
        size_t samples_needed = samples_per_frame_ - frame_buffer_pos_;
        
        // 如果输入数据不足一帧
        if (pcm_data.size() < samples_needed) {
            // 将所有数据复制到帧缓冲区
            std::copy(pcm_data.begin(), pcm_data.end(), frame_buffer_.begin() + frame_buffer_pos_);
            frame_buffer_pos_ += pcm_data.size();
            
            // 帧未填满，返回成功但不进行编码
            return true;
        }
        
        // 填充当前帧的剩余部分
        std::copy(pcm_data.begin(), pcm_data.begin() + samples_needed, frame_buffer_.begin() + frame_buffer_pos_);
        
        // 准备输出缓冲区
        opus_data.resize(max_packet_size_);
        
        // 编码完整帧
        opus_int32 encoded_bytes = opus_encode(
            encoder_,
            frame_buffer_.data(),
            samples_per_frame_,
            opus_data.data(),
            opus_data.size()
        );
        
        if (encoded_bytes < 0) {
            ESP_LOGE(TAG, "Opus encoding failed: %ld", (long)encoded_bytes);
            frame_buffer_pos_ = 0;  // 重置帧缓冲区
            return false;
        }
        
        // 调整输出大小为实际编码长度
        opus_data.resize(encoded_bytes);
        
        ESP_LOGD(TAG, "Encoded full frame of %d samples into %ld bytes", 
                 samples_per_frame_, (long)encoded_bytes);
        
        // 处理剩余数据
        if (pcm_data.size() > samples_needed) {
            // 将剩余数据移动到帧缓冲区开始位置
            size_t remaining = pcm_data.size() - samples_needed;
            std::copy(pcm_data.begin() + samples_needed, pcm_data.end(), frame_buffer_.begin());
            frame_buffer_pos_ = remaining;
            
            if (remaining >= samples_per_frame_) {
                ESP_LOGW(TAG, "Remaining data exceeds frame size, may cause delay in encoding");
            }
        } else {
            // 恰好用完所有数据
            frame_buffer_pos_ = 0;
        }
        
        return true;
    } 
    catch (const std::exception& e) {
        ESP_LOGE(TAG, "Exception during encoding: %s", e.what());
        frame_buffer_pos_ = 0;  // 重置帧缓冲区
        return false;
    }
    catch (...) {
        ESP_LOGE(TAG, "Unknown exception during encoding");
        frame_buffer_pos_ = 0;  // 重置帧缓冲区
        return false;
    }
}

void OpusEncoderWrapper::SetComplexity(int complexity) {
    if (!encoder_) {
        ESP_LOGE(TAG, "Encoder not initialized");
        return;
    }
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    // 确保复杂度在有效范围内 (0-10)
    if (complexity < 0) complexity = 0;
    if (complexity > 10) complexity = 10;
    
    int result = opus_encoder_ctl(encoder_, OPUS_SET_COMPLEXITY(complexity));
    if (result != OPUS_OK) {
        ESP_LOGE(TAG, "Failed to set complexity: %d", result);
    } else {
        ESP_LOGI(TAG, "Set Opus encoder complexity to %d", complexity);
    }
}


// 重置编码器状态和缓冲区
void OpusEncoderWrapper::Reset() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (encoder_) {
        opus_encoder_ctl(encoder_, OPUS_RESET_STATE);
        ESP_LOGI("OpusEncoder", "Opus encoder state reset");
    }
    
    // 重置帧缓冲区
    frame_buffer_pos_ = 0;
    std::fill(frame_buffer_.begin(), frame_buffer_.end(), 0);
}