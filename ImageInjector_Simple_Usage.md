# ImageInjector 简化使用说明

## 功能概述

ImageInjector 是一个支持图片注入的相机数据流处理系统，具有以下特性：

- **图片处理**：支持 JPEG 格式图片解码和注入
- **自动权限修复**：自动修复文件权限和 SELinux 上下文
- **文件管理**：智能保留最后一个文件，删除已处理的文件

## 文件命名规则

### 图片文件
- 前缀：`inject_`
- 格式：`.jpg` 或 `.jpeg`
- 示例：`/data/misc/cameraserver/inject_test.jpg`

## 自动权限修复

ImageInjector 会自动检测并修复文件权限问题：

### 修复内容
1. **文件权限**：设置为 `666` (rw-rw-rw-)
2. **文件所有者**：设置为 `cameraserver:cameraserver`
3. **SELinux 上下文**：设置为 `u:object_r:cameraserver_data_file:s0`

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
1. 将图片文件放入 `/data/misc/cameraserver/` 目录
2. ImageInjector 自动检测文件
3. 自动修复文件权限
4. 解码文件内容
5. 添加到缓存队列
6. 删除已处理的文件（保留最后一个）

## 配置参数

### 缓存配置
```cpp
static constexpr int CACHE_SIZE = 10;           // 缓存帧数
static constexpr int MAX_CACHE_SIZE = 20;       // 最大缓存帧数
static constexpr int64_t FRAME_EXPIRY_MS = 30000; // 帧过期时间（30秒）
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
```

### 权限修复
```
I/ImageInjector: Fixing file permissions for: /data/misc/cameraserver/inject_test.jpg
I/ImageInjector: File permissions after fix: mode=666, uid=1000, gid=1000
I/ImageInjector: File is now readable by cameraserver
```

### 处理结果
```
I/ImageInjector: Loaded 5 new frames to cache, total cache size: 8
```

## 编译说明

### Android.mk 配置
```makefile
LOCAL_MODULE := libimageinjector
LOCAL_SRC_FILES := ImageInjector.cpp
LOCAL_SHARED_LIBRARIES := liblog libjpeg
LOCAL_C_INCLUDES := $(LOCAL_PATH)
LOCAL_CFLAGS := -Wall -Wextra -O2
```

### 依赖库
- `liblog` - Android 日志库
- `libjpeg` - JPEG 图片解码库
- `libselinux` - SELinux 支持（可选）

## 注意事项

1. **文件权限**：确保 ImageInjector 有足够权限修复文件
2. **文件格式**：只支持 JPEG 图片格式
3. **线程安全**：所有操作都是线程安全的
4. **资源清理**：析构函数会自动清理所有资源

## 故障排除

### 1. 文件无法访问
- 检查文件权限和 SELinux 上下文
- 查看权限修复日志
- 确认 cameraserver 用户存在

### 2. 图片无法解码
- 检查图片格式是否为 JPEG
- 查看解码错误日志
- 确认图片文件完整性

### 3. 编译错误
- 确保包含正确的头文件路径
- 检查依赖库是否正确链接
- 验证 Android.mk 配置

这个简化版本的 ImageInjector 专注于图片处理功能，避免了复杂的视频解码依赖问题。