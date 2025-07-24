#ifndef OPUS_ENCODER_WRAPPER_H
#define OPUS_ENCODER_WRAPPER_H

#include <memory>
#include <vector>
#include <mutex>
#include "opus.h"

class OpusEncoderWrapper {
public:
    OpusEncoderWrapper(int sample_rate, int channels, int frame_duration_ms);
    ~OpusEncoderWrapper();

    // 编码PCM数据
    bool Encode(std::vector<int16_t>&& pcm_data, std::vector<uint8_t>& opus_data);
    
    // 设置复杂度 (0-10) - 用于控制CPU使用率，值越高音质越好但CPU使用越多
    void SetComplexity(int complexity);
    
    // 将Reset方法移到public部分
    void Reset();  // 重置编码器状态和缓冲区

private:
    OpusEncoder* encoder_;
    std::mutex mutex_;
    int max_packet_size_;
    int samples_per_frame_;
    
    // 帧缓冲区成员变量
    std::vector<int16_t> frame_buffer_;  // 用于累积音频帧
    size_t frame_buffer_pos_ = 0;        // 当前帧缓冲区位置
};

#endif // OPUS_ENCODER_WRAPPER_H