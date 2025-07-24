#include "afe_wake_word.h"
#include "application.h"

#include <esp_log.h>
#include <model_path.h>
#include <arpa/inet.h>
#include <sstream>

#define DETECTION_RUNNING_EVENT 1

#define TAG "AfeWakeWord"

AfeWakeWord::AfeWakeWord()
    : afe_data_(nullptr),
      wake_word_pcm_(),
      wake_word_opus_() {

    event_group_ = xEventGroupCreate();
}

AfeWakeWord::~AfeWakeWord() {
    if (afe_data_ != nullptr) {
        afe_iface_->destroy(afe_data_);
    }

    if (wake_word_encode_task_stack_ != nullptr) {
        heap_caps_free(wake_word_encode_task_stack_);
    }

    vEventGroupDelete(event_group_);
}

bool AfeWakeWord::Initialize(AudioCodec* codec) {
    codec_ = codec;
    int ref_num = codec_->input_reference() ? 1 : 0;

    srmodel_list_t *models = esp_srmodel_init("model");
    if (models == nullptr || models->num == -1) {
        ESP_LOGE(TAG, "Failed to initialize wakenet model");
        return false;
    }
    for (int i = 0; i < models->num; i++) {
        ESP_LOGI(TAG, "Model %d: %s", i, models->model_name[i]);
        if (strstr(models->model_name[i], ESP_WN_PREFIX) != NULL) {
            wakenet_model_ = models->model_name[i];
            auto words = esp_srmodel_get_wake_words(models, wakenet_model_);
            // split by ";" to get all wake words
            std::stringstream ss(words);
            std::string word;
            while (std::getline(ss, word, ';')) {
                wake_words_.push_back(word);
            }
        }
    }

    std::string input_format;
    for (int i = 0; i < codec_->input_channels() - ref_num; i++) {
        input_format.push_back('M');
    }
    for (int i = 0; i < ref_num; i++) {
        input_format.push_back('R');
    }
    afe_config_t* afe_config = afe_config_init(input_format.c_str(), models, AFE_TYPE_SR, AFE_MODE_HIGH_PERF);
    afe_config->aec_init = codec_->input_reference();
    afe_config->aec_mode = AEC_MODE_SR_HIGH_PERF;
    afe_config->afe_perferred_core = 1;
    afe_config->afe_perferred_priority = 1;
    afe_config->memory_alloc_mode = AFE_MEMORY_ALLOC_MORE_PSRAM;
    
    afe_iface_ = esp_afe_handle_from_config(afe_config);
    afe_data_ = afe_iface_->create_from_config(afe_config);

    xTaskCreate([](void* arg) {
        auto this_ = (AfeWakeWord*)arg;
        this_->AudioDetectionTask();
        vTaskDelete(NULL);
    }, "audio_detection", 4096, this, 3, nullptr);

    return true;
}

// 无参数版本的 OnWakeWordDetected 函数
void AfeWakeWord::OnWakeWordDetected() {
    ESP_LOGI(TAG, "Wake word detected, starting to listen");
    
    // 获取语音助手实例并检查其状态
    auto& assistant = VoicePhotoAssistant::GetInstance();
    if (assistant.GetState() != kStateIdle && 
        assistant.GetState() != kStateSpeaking) {
        ESP_LOGI(TAG, "Wake word detected but assistant is already active, ignoring");
        return;
    }
    
    // 通知助手已检测到唤醒词
    assistant.OnWakeWord();
    
    // 编码唤醒词音频数据
    EncodeWakeWordData();
}

// 带回调参数版本的 OnWakeWordDetected 函数
void AfeWakeWord::OnWakeWordDetected(std::function<void(const std::string&)> callback) {
    ESP_LOGI(TAG, "Wake word detected with callback");
    
    // 获取语音助手实例并检查其状态
    auto& assistant = VoicePhotoAssistant::GetInstance();
    if (assistant.GetState() != kStateIdle && 
        assistant.GetState() != kStateSpeaking) {
        ESP_LOGI(TAG, "Wake word detected but assistant is already active, ignoring");
        return;
    }
    
    // 使用固定的唤醒词，因为 GetLastWakeWord() 不可用
    const std::string wake_word = "你好小智"; // 使用默认唤醒词
    
    // 调用回调函数
    if (callback) {
        callback(wake_word);
    }
    
    // 通知助手已检测到唤醒词
    assistant.OnWakeWord();
    
    // 编码唤醒词音频数据
    EncodeWakeWordData();
}

void AfeWakeWord::Start() {
    xEventGroupSetBits(event_group_, DETECTION_RUNNING_EVENT);
}



void AfeWakeWord::Feed(const std::vector<int16_t>& data) {
    if (afe_data_ == nullptr) {
        return;
    }
    afe_iface_->feed(afe_data_, data.data());
}

size_t AfeWakeWord::GetFeedSize() {
    if (afe_data_ == nullptr) {
        return 0;
    }
    return afe_iface_->get_feed_chunksize(afe_data_) * codec_->input_channels();
}

// 优化 AudioDetectionTask 方法
// 优化 AudioDetectionTask 方法
void AfeWakeWord::AudioDetectionTask() {
    auto fetch_size = afe_iface_->get_fetch_chunksize(afe_data_);
    auto feed_size = afe_iface_->get_feed_chunksize(afe_data_);
    ESP_LOGI(TAG, "Audio detection task started, feed size: %d fetch size: %d",
        feed_size, fetch_size);

    // 缓冲区管理变量
    int consecutive_errors = 0;
    const int MAX_CONSECUTIVE_ERRORS = 5;
    TickType_t last_reset_time = xTaskGetTickCount();
    const TickType_t RESET_INTERVAL = pdMS_TO_TICKS(500); // 每500ms检查一次

    while (true) {
        // 短超时检查状态
        EventBits_t bits = xEventGroupWaitBits(
            event_group_, 
            DETECTION_RUNNING_EVENT, 
            pdFALSE, 
            pdTRUE, 
            pdMS_TO_TICKS(10)
        );
        
        if ((bits & DETECTION_RUNNING_EVENT) == 0) {
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }

        // 获取数据
        auto res = afe_iface_->fetch(afe_data_);
        
        // 错误处理
        if (res == nullptr || res->ret_value == ESP_FAIL) {
            consecutive_errors++;
            if (consecutive_errors > MAX_CONSECUTIVE_ERRORS) {
                ESP_LOGW(TAG, "Multiple fetch errors (%d), resetting AFE buffer", 
                         consecutive_errors);
                afe_iface_->reset_buffer(afe_data_);
                consecutive_errors = 0;
            }
            vTaskDelay(pdMS_TO_TICKS(5));
            continue;
        }
        consecutive_errors = 0;

        // 周期性缓冲区维护 - 更频繁重置缓冲区
        TickType_t current_time = xTaskGetTickCount();
        if (current_time - last_reset_time > RESET_INTERVAL) {
            ESP_LOGD(TAG, "Performing routine buffer reset");
            afe_iface_->reset_buffer(afe_data_);
            last_reset_time = current_time;
        }

        // 存储唤醒词数据 - 限制存储量
        StoreWakeWordData(res->data, res->data_size / sizeof(int16_t));

        // 唤醒词检测
        if (res->wakeup_state == WAKENET_DETECTED) {
            ESP_LOGI(TAG, "Wake word detected!");
            
            // 立即重置缓冲区
            afe_iface_->reset_buffer(afe_data_);
            
            // 停止检测
            Stop();
            
            // 设置检测到的唤醒词
            last_detected_wake_word_ = wake_words_[res->wakenet_model_index - 1];

            // 执行回调
            if (wake_word_detected_callback_) {
                wake_word_detected_callback_(last_detected_wake_word_);
            }
        }
    }
}

// 优化 StoreWakeWordData 方法
void AfeWakeWord::StoreWakeWordData(const int16_t* data, size_t samples) {
    static const size_t MAX_BUFFER_SIZE = 1500 / 30;  // 约1.5秒的数据
    
    // 限制队列大小
    if (wake_word_pcm_.size() >= MAX_BUFFER_SIZE) {
        wake_word_pcm_.pop_front();
    }
    
    // 添加新数据
    wake_word_pcm_.emplace_back(std::vector<int16_t>(data, data + samples));
}

// 优化 Stop 方法
void AfeWakeWord::Stop() {
    ESP_LOGI(TAG, "Stopping wake word detection");
    xEventGroupClearBits(event_group_, DETECTION_RUNNING_EVENT);
    
    // 重置缓冲区
    if (afe_data_ != nullptr) {
        ESP_LOGI(TAG, "Fully resetting AFE buffer on stop");
        afe_iface_->reset_buffer(afe_data_);
    }
}

void AfeWakeWord::EncodeWakeWordData() {
    wake_word_opus_.clear();
    if (wake_word_encode_task_stack_ == nullptr) {
        wake_word_encode_task_stack_ = (StackType_t*)heap_caps_malloc(4096 * 8, MALLOC_CAP_SPIRAM);
    }
    wake_word_encode_task_ = xTaskCreateStatic([](void* arg) {
        auto this_ = (AfeWakeWord*)arg;
        {
            auto start_time = esp_timer_get_time();
            auto encoder = std::make_unique<OpusEncoderWrapper>(16000, 1, OPUS_FRAME_DURATION_MS);
            encoder->SetComplexity(0); // 0 is the fastest

            int packets = 0;
            for (auto& pcm: this_->wake_word_pcm_) {
                std::vector<uint8_t> opus_data;
                // 确保PCM数据有效且不为空
                if (!pcm.empty()) {
                    try {
                        if (encoder->Encode(std::move(pcm), opus_data)) {
                            if (!opus_data.empty()) {
                                std::lock_guard<std::mutex> lock(this_->wake_word_mutex_);
                                this_->wake_word_opus_.emplace_back(std::move(opus_data));
                                this_->wake_word_cv_.notify_all();
                            }
                        }
                    } catch (const std::exception& e) {
                        ESP_LOGE(TAG, "Exception during encoding: %s", e.what());
                    } catch (...) {
                        ESP_LOGE(TAG, "Unknown exception during encoding");
                    }
                }
                packets++;
            }
            this_->wake_word_pcm_.clear();

            auto end_time = esp_timer_get_time();
            ESP_LOGI(TAG, "Encode wake word opus %d packets in %ld ms", packets, (long)((end_time - start_time) / 1000));

            std::lock_guard<std::mutex> lock(this_->wake_word_mutex_);
            this_->wake_word_opus_.push_back(std::vector<uint8_t>());
            this_->wake_word_cv_.notify_all();
        }
        vTaskDelete(NULL);
    }, "encode_detect_packets", 4096 * 8, this, 2, wake_word_encode_task_stack_, &wake_word_encode_task_buffer_);
}

bool AfeWakeWord::GetWakeWordOpus(std::vector<uint8_t>& opus) {
    std::unique_lock<std::mutex> lock(wake_word_mutex_);
    wake_word_cv_.wait(lock, [this]() {
        return !wake_word_opus_.empty();
    });
    opus.swap(wake_word_opus_.front());
    wake_word_opus_.pop_front();
    return !opus.empty();
}