# ImageInjector 视频解码功能使用指南

## 功能概述

ImageInjector 现在支持视频文件解码，可以将视频流解码为 YUV 格式并注入到相机数据流中。

## 支持的视频格式

- **MP4** (H.264/H.265)
- **AVI** (H.264)
- **MKV** (H.264/H.265)

## 文件命名规则

视频文件需要以 `video_` 前缀命名，例如：
```
/data/misc/cameraserver/video_test.mp4
/data/misc/cameraserver/video_sample.avi
/data/misc/cameraserver/video_demo.mkv
```

## 技术实现

### 1. 使用 MediaCodec API
- 利用 Android 原生 MediaCodec 进行硬件加速解码
- 支持 H.264/H.265 编码格式
- 自动检测视频轨道和格式

### 2. 视频帧提取
- 自动提取视频中的所有帧
- 最大支持 100 帧（可配置）
- 每帧转换为 YUV420 格式

### 3. 内存管理
- 视频帧缓存使用独立的内存管理
- 支持大视频文件的分块处理
- 自动清理过期帧

## 核心函数说明

### `loadVideoFile(const std::string& filePath)`
- 加载视频文件并初始化解码器
- 提取所有视频帧到内存缓存
- 返回是否成功

### `decodeVideoFrame(const std::string& filePath, int frameIndex, std::vector<uint8_t>& yuvData, int& width, int& height)`
- 解码指定索引的视频帧
- 输出 YUV 数据和尺寸信息
- 用于单帧访问

### `initializeVideoDecoder(const std::string& filePath)`
- 初始化 MediaCodec 解码器
- 配置视频格式和参数
- 启动解码器

### `extractVideoFrames(const std::string& filePath, std::vector<std::vector<uint8_t>>& frames)`
- 提取视频中的所有帧
- 存储为 YUV 格式数据
- 限制最大帧数防止内存溢出

## 使用示例

### 1. 基本使用
```cpp
ImageInjector injector;
injector.startMonitoring();

// 将视频文件放入监控目录
// /data/misc/cameraserver/video_test.mp4

// 系统会自动检测并处理视频文件
// 视频帧会被添加到缓存队列中
```

### 2. 手动处理视频文件
```cpp
ImageInjector injector;

// 加载视频文件
if (injector.loadVideoFile("/path/to/video.mp4")) {
    // 获取视频帧
    std::vector<uint8_t> yuvData;
    int width, height;
    
    if (injector.decodeVideoFrame("/path/to/video.mp4", 0, yuvData, width, height)) {
        ALOGI("Decoded frame 0: %dx%d, data size: %zu", width, height, yuvData.size());
    }
}
```

## 配置参数

### 视频解码配置
```cpp
static constexpr int MAX_VIDEO_FRAMES = 100;  // 最大视频帧数
static constexpr int64_t VIDEO_FRAME_TIMEOUT_US = 1000000; // 1秒超时
```

### 缓存配置
```cpp
static constexpr int CACHE_SIZE = 10;           // 缓存帧数
static constexpr int MAX_CACHE_SIZE = 20;       // 最大缓存帧数
static constexpr int64_t FRAME_EXPIRY_MS = 30000; // 帧过期时间（30秒）
```

## 性能优化建议

### 1. 视频文件大小
- 建议视频文件不超过 100MB
- 分辨率建议不超过 1920x1080
- 帧率建议不超过 30fps

### 2. 内存使用
- 每个视频帧约占用 3-6MB 内存（1080p）
- 建议同时处理的视频文件不超过 2-3 个
- 定期清理过期帧

### 3. 处理策略
- 视频文件会按顺序处理
- 每次最多处理 5 个文件
- 最后一个视频文件会被保留

## 错误处理

### 常见错误
1. **解码器初始化失败**
   - 检查视频文件格式是否支持
   - 确认 MediaCodec 可用

2. **内存不足**
   - 减少 MAX_VIDEO_FRAMES 参数
   - 降低视频分辨率

3. **文件读取失败**
   - 检查文件路径和权限
   - 确认文件完整性

### 日志输出
```
I/ImageInjector: Found video file: /data/misc/cameraserver/video_test.mp4
I/ImageInjector: Video decoder initialized successfully
I/ImageInjector: Extracted 60 video frames
I/ImageInjector: Successfully loaded video file: /data/misc/cameraserver/video_test.mp4, extracted 60 frames
```

## 注意事项

1. **线程安全**：所有视频解码操作都是线程安全的
2. **资源清理**：析构函数会自动清理所有视频解码资源
3. **格式支持**：主要支持 H.264/H.265 编码的 MP4/AVI/MKV 文件
4. **性能影响**：视频解码会消耗较多 CPU 和内存资源

## 故障排除

### 1. 视频无法解码
- 检查文件格式是否支持
- 确认视频编码格式（H.264/H.265）
- 查看日志中的错误信息

### 2. 内存不足
- 减少视频帧数限制
- 降低视频分辨率
- 增加系统可用内存

### 3. 性能问题
- 减少同时处理的视频文件数量
- 降低视频帧率
- 使用硬件加速解码

这个视频解码功能为 ImageInjector 提供了强大的视频处理能力，可以无缝地将视频内容注入到相机数据流中。