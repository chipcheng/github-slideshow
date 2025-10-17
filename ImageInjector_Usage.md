# ImageInjector 使用说明

## 功能概述

ImageInjector 是一个支持图片和视频注入的相机数据流处理系统，具有以下特性：

- **图片处理**：支持 JPEG 格式图片解码和注入
- **视频处理**：支持 MP4/AVI/MKV 视频文件解码
- **自动权限修复**：自动修复文件权限和 SELinux 上下文
- **文件管理**：智能保留最后一个文件，删除已处理的文件

## 文件命名规则

### 图片文件
- 前缀：`inject_`
- 格式：`.jpg` 或 `.jpeg`
- 示例：`/data/misc/cameraserver/inject_test.jpg`

### 视频文件
- 前缀：`video_`
- 格式：`.mp4`、`.avi`、`.mkv`
- 示例：`/data/misc/cameraserver/video_test.mp4`

## 自动权限修复

ImageInjector 会自动检测并修复文件权限问题：

### 修复内容
1. **文件权限**：设置为 `666` (rw-rw-rw-)
2. **文件所有者**：设置为 `cameraserver:cameraserver`
3. **SELinux 上下文**：设置为 `u:object_r:cameraserver_data_file:s0`

### 权限验证
- 自动检查文件是否可读
- 记录权限修复结果
- 跳过无法修复的文件

## 使用方法

### 1. 基本使用
```cpp
#include "ImageInjector.h"

// 创建 ImageInjector 实例
android::ImageInjector injector;

// 启动监控
injector.startMonitoring();

// 检查是否应该注入
if (injector.shouldInject()) {
    // 注入到相机缓冲区
    injector.injectToBuffer(bufferData, width, height, stride, format);
}

// 停止监控
injector.stopMonitoring();
```

### 2. 文件处理流程
1. 将图片/视频文件放入 `/data/misc/cameraserver/` 目录
2. ImageInjector 自动检测文件
3. 自动修复文件权限
4. 解码文件内容
5. 添加到缓存队列
6. 删除已处理的文件（保留最后一个）

### 3. 手动处理文件
```cpp
// 处理单个图片文件
if (injector.loadImageFile("/path/to/image.jpg")) {
    // 图片加载成功
}

// 处理视频文件
if (injector.loadVideoFile("/path/to/video.mp4")) {
    // 视频加载成功
    std::vector<uint8_t> yuvData;
    int width, height;
    injector.decodeVideoFrame("/path/to/video.mp4", 0, yuvData, width, height);
}
```

## 配置参数

### 缓存配置
```cpp
static constexpr int CACHE_SIZE = 10;           // 缓存帧数
static constexpr int MAX_CACHE_SIZE = 20;       // 最大缓存帧数
static constexpr int64_t FRAME_EXPIRY_MS = 30000; // 帧过期时间（30秒）
```

### 视频配置
```cpp
static constexpr int MAX_VIDEO_FRAMES = 100;  // 最大视频帧数
static constexpr int64_t VIDEO_FRAME_TIMEOUT_US = 1000000; // 1秒超时
```

### 图片配置
```cpp
static constexpr int MAX_INJECTION_WIDTH = 1920;
static constexpr int MAX_INJECTION_HEIGHT = 1080;
```

## 日志输出

### 文件发现
```
I/ImageInjector: Found image file: /data/misc/cameraserver/inject_test.jpg
I/ImageInjector: Found video file: /data/misc/cameraserver/video_test.mp4
```

### 权限修复
```
I/ImageInjector: Fixing file permissions for: /data/misc/cameraserver/inject_test.jpg
I/ImageInjector: File permissions after fix: mode=666, uid=1000, gid=1000
I/ImageInjector: File is now readable by cameraserver
```

### 处理结果
```
I/ImageInjector: Successfully loaded video file: /data/misc/cameraserver/video_test.mp4, extracted 60 frames
I/ImageInjector: Loaded 5 new frames to cache, total cache size: 8
```

## 错误处理

### 权限问题
```
W/ImageInjector: Skipping file due to permission issues: /data/misc/cameraserver/inject_test.jpg
```

### 文件格式问题
```
E/ImageInjector: Failed to decode image: /data/misc/cameraserver/invalid.jpg
E/ImageInjector: Failed to decode video: /data/misc/cameraserver/invalid.mp4
```

### 内存问题
```
W/ImageInjector: Cache full (10/10), stopping file loading
```

## 性能统计

停止监控时会输出性能统计：
```
I/ImageInjector: Performance Statistics:
I/ImageInjector:   Total Frames Processed: 150
I/ImageInjector:   Cache Hits: 120
I/ImageInjector:   Cache Misses: 30
I/ImageInjector:   Cache Hit Rate: 80.00%
```

## 注意事项

1. **文件权限**：确保 ImageInjector 有足够权限修复文件
2. **内存使用**：视频文件会消耗较多内存
3. **文件格式**：只支持 JPEG 图片和 H.264/H.265 视频
4. **线程安全**：所有操作都是线程安全的
5. **资源清理**：析构函数会自动清理所有资源

## 故障排除

### 1. 文件无法访问
- 检查文件权限和 SELinux 上下文
- 查看权限修复日志
- 确认 cameraserver 用户存在

### 2. 视频无法解码
- 检查视频格式是否支持
- 确认视频编码格式（H.264/H.265）
- 查看解码错误日志

### 3. 内存不足
- 减少缓存大小
- 降低视频分辨率
- 限制同时处理的文件数量

### 4. 性能问题
- 调整扫描间隔
- 优化文件处理策略
- 监控系统资源使用

这个 ImageInjector 系统提供了完整的图片和视频注入功能，同时自动处理权限问题，使用简单且功能强大。