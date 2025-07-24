#include "esp32_camera.h"
#include "mcp_server.h"
#include "display.h"
#include "board.h"
#include "system_info.h"

#include <esp_log.h>
#include <esp_heap_caps.h>
#include <img_converters.h>
#include <cstring>
#include <esp_timer.h>

#define TAG "Esp32Camera"

// 检测相机型号的辅助函数
bool detect_and_configure_camera(sensor_t* s) {
    if (s == nullptr) {
        ESP_LOGE(TAG, "Failed to get camera sensor");
        return false;
    }
    
    // 记录相机信息以便调试
    ESP_LOGI(TAG, "Camera sensor info - PID: 0x%x, VER: 0x%x, MIDL: 0x%x, MIDH: 0x%x", 
             s->id.PID, s->id.VER, s->id.MIDL, s->id.MIDH);
    
    // 根据相机型号应用特定设置
    if (s->id.PID == GC0308_PID) {
        ESP_LOGI(TAG, "Detected GC0308 camera");
        s->set_hmirror(s, 0);  // 设置水平镜像
    } else if (s->id.PID == OV2640_PID) {
        ESP_LOGI(TAG, "Detected OV2640 camera");
    } else if (s->id.PID == OV3660_PID) {
        ESP_LOGI(TAG, "Detected OV3660 camera");
    } else {
        ESP_LOGW(TAG, "Unknown camera model (PID: 0x%x), applying generic settings", s->id.PID);
    }
    
    // 应用通用相机优化设置
    s->set_brightness(s, 1);     // 略微增加亮度
    s->set_contrast(s, 1);       // 略微增加对比度
    s->set_saturation(s, 0);     // 默认饱和度
    s->set_gainceiling(s, GAINCEILING_2X); // 增益上限
    s->set_quality(s, 10);       // 降低JPEG质量以提高帧率
    s->set_colorbar(s, 0);       // 禁用彩条测试
    s->set_whitebal(s, 1);       // 启用白平衡
    s->set_gain_ctrl(s, 1);      // 自动增益控制
    s->set_exposure_ctrl(s, 1);  // 自动曝光控制
    s->set_aec2(s, 1);           // 增强型自动曝光
    
    return true;
}

Esp32Camera::Esp32Camera(const camera_config_t& config) {
    // 尝试多种初始化策略
    bool camera_init_success = false;
    
    for (int attempt = 0; attempt < 3 && !camera_init_success; attempt++) {
        ESP_LOGI(TAG, "Camera initialization attempt %d/3", attempt + 1);
        
        // 根据尝试次数修改配置
        camera_config_t adjusted_config = config;
        
        if (attempt == 1) {
            // 第二次尝试：降低时钟频率
            adjusted_config.xclk_freq_hz = 10000000; // 降至10MHz
            ESP_LOGI(TAG, "Trying with reduced clock frequency: %d Hz", adjusted_config.xclk_freq_hz);
        } else if (attempt == 2) {
            // 第三次尝试：改变像素格式
            adjusted_config.pixel_format = PIXFORMAT_RGB565;
            ESP_LOGI(TAG, "Trying with RGB565 pixel format");
        }
        
        // 相机初始化
        esp_err_t err = esp_camera_init(&adjusted_config);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "Camera init failed on attempt %d with error 0x%x", attempt + 1, err);
            // 在重试前等待一小段时间
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }
        
        // 相机初始化成功，尝试获取和配置传感器
        sensor_t *s = esp_camera_sensor_get();
        if (detect_and_configure_camera(s)) {
            camera_init_success = true;
            ESP_LOGI(TAG, "Camera initialized successfully on attempt %d", attempt + 1);
        } else {
            // 传感器配置失败，释放资源并重试
            esp_camera_deinit();
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }
    
    if (!camera_init_success) {
        ESP_LOGE(TAG, "All camera initialization attempts failed");
        return;
    }
    
    // 初始化预览图片的内存
    memset(&preview_image_, 0, sizeof(preview_image_));
    preview_image_.header.magic = LV_IMAGE_HEADER_MAGIC;
    preview_image_.header.cf = LV_COLOR_FORMAT_RGB565;
    preview_image_.header.flags = LV_IMAGE_FLAGS_ALLOCATED | LV_IMAGE_FLAGS_MODIFIABLE;

    switch (config.frame_size) {
        case FRAMESIZE_SVGA:
            preview_image_.header.w = 800;
            preview_image_.header.h = 600;
            break;
        case FRAMESIZE_VGA:
            preview_image_.header.w = 640;
            preview_image_.header.h = 480;
            break;
        case FRAMESIZE_QVGA:
            preview_image_.header.w = 320;
            preview_image_.header.h = 240;
            break;
        case FRAMESIZE_128X128:
            preview_image_.header.w = 128;
            preview_image_.header.h = 128;
            break;
        case FRAMESIZE_240X240:
            preview_image_.header.w = 240;
            preview_image_.header.h = 240;
            break;
        default:
            ESP_LOGE(TAG, "Unsupported frame size: %d, image preview will not be shown", config.frame_size);
            preview_image_.data_size = 0;
            preview_image_.data = nullptr;
            return;
    }

    preview_image_.header.stride = preview_image_.header.w * 2;
    preview_image_.data_size = preview_image_.header.w * preview_image_.header.h * 2;
    preview_image_.data = (uint8_t*)heap_caps_malloc(preview_image_.data_size, MALLOC_CAP_SPIRAM);
    if (preview_image_.data == nullptr) {
        ESP_LOGE(TAG, "Failed to allocate memory for preview image");
        return;
    }
}

Esp32Camera::~Esp32Camera() {
    if (fb_) {
        esp_camera_fb_return(fb_);
        fb_ = nullptr;
    }
    if (preview_image_.data) {
        heap_caps_free((void*)preview_image_.data);
        preview_image_.data = nullptr;
    }
    esp_camera_deinit();
}

void Esp32Camera::SetExplainUrl(const std::string& url, const std::string& token) {
    explain_url_ = url;
    explain_token_ = token;
}

bool Esp32Camera::Capture() {
    // 等待上一个编码线程完成
    if (encoder_thread_.joinable()) {
        encoder_thread_.join();
    }
    
    // 检查相机传感器可用性
    sensor_t *s = esp_camera_sensor_get();
    if (s == nullptr) {
        ESP_LOGE(TAG, "Camera sensor not available");
        return false;
    }
    
    // 检查并释放之前的帧缓冲区
    if (fb_ != nullptr) {
        esp_camera_fb_return(fb_);
        fb_ = nullptr;
    }
    
    // 添加捕获延迟控制
    static int64_t last_capture_time = 0;
    int64_t current_time = esp_timer_get_time() / 1000;
    if (current_time - last_capture_time < 100) { // 至少间隔100ms
        vTaskDelay(pdMS_TO_TICKS(100 - (current_time - last_capture_time)));
    }
    
    // 尝试多次捕获，提高成功率
    for (int retry = 0; retry < 3; retry++) {
        // 捕获多帧以稳定图像
        for (int i = 0; i < 2; i++) {
            if (fb_ != nullptr) {
                esp_camera_fb_return(fb_);
                fb_ = nullptr;
            }
            
            fb_ = esp_camera_fb_get();
            if (fb_ == nullptr) {
                ESP_LOGW(TAG, "Camera capture failed, retry %d/3", retry + 1);
                vTaskDelay(pdMS_TO_TICKS(20)); // 短暂延迟
                continue;
            }
            
            // 捕获成功，等待一小段时间使图像稳定
            vTaskDelay(pdMS_TO_TICKS(10));
        }
        
        if (fb_ != nullptr) {
            // 捕获成功
            last_capture_time = esp_timer_get_time() / 1000;
            ESP_LOGI(TAG, "Camera capture successful: %dx%d, format: %d, len: %d", 
                     fb_->width, fb_->height, fb_->format, fb_->len);
            break;
        }
        
        // 如果是最后一次尝试前，重置相机设置
        if (retry == 1) {
            ESP_LOGW(TAG, "Multiple capture failures, resetting camera settings");
            s->set_hmirror(s, 0);  // 重置镜像设置
            s->set_vflip(s, 0);    // 重置翻转设置
            s->set_brightness(s, 0); // 重置亮度
            vTaskDelay(pdMS_TO_TICKS(100));
            s->set_brightness(s, 1); // 恢复亮度
        }
        
        vTaskDelay(pdMS_TO_TICKS(100)); // 重试前等待
    }
    
    // 检查最终结果
    if (fb_ == nullptr) {
        ESP_LOGE(TAG, "Camera capture failed after multiple attempts");
        return false;
    }
    
    // 处理预览图像
    if (preview_image_.data_size > 0 && preview_image_.data != nullptr) {
        auto display = Board::GetInstance().GetDisplay();
        if (display != nullptr) {
            auto src = (uint16_t*)fb_->buf;
            auto dst = (uint16_t*)preview_image_.data;
            size_t pixel_count = fb_->len / 2;
            for (size_t i = 0; i < pixel_count; i++) {
                // 交换每个16位字内的字节
                dst[i] = __builtin_bswap16(src[i]);
            }
            display->SetPreviewImage(&preview_image_);
        }
    } else {
        ESP_LOGW(TAG, "Skip preview because of unsupported frame size or uninitialized data");
    }
    
    return true;
}

bool Esp32Camera::SetHMirror(bool enabled) {
    sensor_t *s = esp_camera_sensor_get();
    if (s == nullptr) {
        ESP_LOGE(TAG, "Failed to get camera sensor");
        return false;
    }
    
    esp_err_t err = s->set_hmirror(s, enabled);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set horizontal mirror: %d", err);
        return false;
    }
    
    ESP_LOGI(TAG, "Camera horizontal mirror set to: %s", enabled ? "enabled" : "disabled");
    return true;
}

bool Esp32Camera::SetVFlip(bool enabled) {
    sensor_t *s = esp_camera_sensor_get();
    if (s == nullptr) {
        ESP_LOGE(TAG, "Failed to get camera sensor");
        return false;
    }
    
    esp_err_t err = s->set_vflip(s, enabled);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set vertical flip: %d", err);
        return false;
    }
    
    ESP_LOGI(TAG, "Camera vertical flip set to: %s", enabled ? "enabled" : "disabled");
    return true;
}

std::string Esp32Camera::Explain(const std::string& question) {
    // 检查相机状态并尝试捕获
    if (fb_ == nullptr) {
        ESP_LOGW(TAG, "No camera frame available, attempting to capture");
        if (!Capture()) {
            return "{\"success\": false, \"message\": \"Failed to capture image from camera\"}";
        }
    }
    
    if (explain_url_.empty()) {
        return "{\"success\": false, \"message\": \"Image explain URL or token is not set\"}";
    }

    // 创建局部的 JPEG 队列
    QueueHandle_t jpeg_queue = xQueueCreate(40, sizeof(JpegChunk));
    if (jpeg_queue == nullptr) {
        ESP_LOGE(TAG, "Failed to create JPEG queue");
        return "{\"success\": false, \"message\": \"Failed to create JPEG queue\"}";
    }

    // 在独立线程中编码图像为JPEG
    encoder_thread_ = std::thread([this, jpeg_queue]() {
        // 确保fb_有效
        if (fb_ == nullptr) {
            ESP_LOGE(TAG, "Frame buffer is null in encoder thread");
            JpegChunk end_marker = {.data = nullptr, .len = 0};
            xQueueSend(jpeg_queue, &end_marker, portMAX_DELAY);
            return;
        }
        
        // 将帧缓冲区转换为JPEG
        frame2jpg_cb(fb_, 80, [](void* arg, size_t index, const void* data, size_t len) -> unsigned int {
            auto jpeg_queue = (QueueHandle_t)arg;
            if (data == nullptr || len == 0) {
                return 0;
            }
            
            // 分配内存存储JPEG块
            JpegChunk chunk;
            chunk.data = (uint8_t*)heap_caps_aligned_alloc(16, len, MALLOC_CAP_SPIRAM);
            if (chunk.data == nullptr) {
                ESP_LOGE(TAG, "Failed to allocate memory for JPEG chunk");
                return 0;
            }
            
            chunk.len = len;
            memcpy(chunk.data, data, len);
            xQueueSend(jpeg_queue, &chunk, portMAX_DELAY);
            return len;
        }, jpeg_queue);
        
        // 发送结束标记
        JpegChunk end_marker = {.data = nullptr, .len = 0};
        xQueueSend(jpeg_queue, &end_marker, portMAX_DELAY);
    });

    auto network = Board::GetInstance().GetNetwork();
    auto http = network->CreateHttp(3);
    // 构造multipart/form-data请求体
    std::string boundary = "----ESP32_CAMERA_BOUNDARY";

    // 配置HTTP客户端，使用分块传输编码
    http->SetHeader("Device-Id", SystemInfo::GetMacAddress().c_str());
    http->SetHeader("Client-Id", Board::GetInstance().GetUuid().c_str());
    if (!explain_token_.empty()) {
        http->SetHeader("Authorization", "Bearer " + explain_token_);
    }
    http->SetHeader("Content-Type", "multipart/form-data; boundary=" + boundary);
    http->SetHeader("Transfer-Encoding", "chunked");
    if (!http->Open("POST", explain_url_)) {
        ESP_LOGE(TAG, "Failed to connect to explain URL");
        // 清理队列
        encoder_thread_.join();
        JpegChunk chunk;
        while (xQueueReceive(jpeg_queue, &chunk, portMAX_DELAY) == pdPASS) {
            if (chunk.data != nullptr) {
                heap_caps_free(chunk.data);
            } else {
                break;
            }
        }
        vQueueDelete(jpeg_queue);
        return "{\"success\": false, \"message\": \"Failed to connect to explain URL\"}";
    }
    
    {
        // 第一块：question字段
        std::string question_field;
        question_field += "--" + boundary + "\r\n";
        question_field += "Content-Disposition: form-data; name=\"question\"\r\n";
        question_field += "\r\n";
        question_field += question + "\r\n";
        http->Write(question_field.c_str(), question_field.size());
    }
    {
        // 第二块：文件字段头部
        std::string file_header;
        file_header += "--" + boundary + "\r\n";
        file_header += "Content-Disposition: form-data; name=\"file\"; filename=\"camera.jpg\"\r\n";
        file_header += "Content-Type: image/jpeg\r\n";
        file_header += "\r\n";
        http->Write(file_header.c_str(), file_header.size());
    }

    // 第三块：JPEG数据
    size_t total_sent = 0;
    while (true) {
        JpegChunk chunk;
        if (xQueueReceive(jpeg_queue, &chunk, portMAX_DELAY) != pdPASS) {
            ESP_LOGE(TAG, "Failed to receive JPEG chunk");
            break;
        }
        if (chunk.data == nullptr) {
            break; // The last chunk
        }
        http->Write((const char*)chunk.data, chunk.len);
        total_sent += chunk.len;
        heap_caps_free(chunk.data);
    }
    // 等待编码线程完成
    encoder_thread_.join();
    // 清理队列
    vQueueDelete(jpeg_queue);

    {
        // 第四块：multipart尾部
        std::string multipart_footer;
        multipart_footer += "\r\n--" + boundary + "--\r\n";
        http->Write(multipart_footer.c_str(), multipart_footer.size());
    }
    // 结束块
    http->Write("", 0);

    if (http->GetStatusCode() != 200) {
        ESP_LOGE(TAG, "Failed to upload photo, status code: %d", http->GetStatusCode());
        return "{\"success\": false, \"message\": \"Failed to upload photo\"}";
    }

    std::string result = http->ReadAll();
    http->Close();

    // 获取剩余任务栈大小
    size_t remain_stack_size = uxTaskGetStackHighWaterMark(nullptr);
    ESP_LOGI(TAG, "Explain image size=%dx%d, compressed size=%d, remain stack size=%d, question=%s\n%s",
        fb_->width, fb_->height, total_sent, remain_stack_size, question.c_str(), result.c_str());
    return result;
}