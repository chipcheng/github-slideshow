# ImageInjector 修复方案

## 问题分析

根据日志分析，相机启动失败的主要原因是：

1. **内存访问错误（SIGSEGV）**：在 `PreviewSpacer-1` 线程中发生段错误
2. **ImageInjector 处理大量图片时崩溃**：发现221个图片文件，处理过程中出现内存问题
3. **图片尺寸过大**：2160x3840 的图片处理时可能导致内存不足

## 修复方案

### 1. 内存管理增强

#### 新增内存限制
- **最大内存使用量**：50MB（从无限制改为有限制）
- **单图片最大大小**：5MB（防止处理过大文件）
- **最大图片尺寸**：4096x4096（防止处理超大图片）
- **缓存大小**：减少到5帧（从10帧减少）

#### 内存跟踪
```cpp
// 新增成员变量
std::atomic<size_t> mCurrentMemoryUsage;  // 当前内存使用量
static constexpr size_t MAX_MEMORY_USAGE = 50 * 1024 * 1024;  // 50MB
static constexpr size_t MAX_IMAGE_SIZE = 5 * 1024 * 1024;     // 5MB
static constexpr size_t MAX_IMAGE_DIMENSION = 4096;           // 4096x4096
```

### 2. 错误处理增强

#### 异常安全
- 所有图片处理操作都包装在 `try-catch` 块中
- 文件读取使用异常安全的操作
- 内存分配失败时的优雅降级

#### 边界检查
```cpp
// 图片尺寸检查
if (width <= 0 || height <= 0 || width > MAX_IMAGE_DIMENSION || height > MAX_IMAGE_DIMENSION) {
    ALOGW("Invalid image dimensions: %dx%d, skipping file: %s", width, height, filePath.c_str());
    return false;
}

// 文件大小检查
if (fileSize > MAX_IMAGE_SIZE) {
    ALOGW("File too large (%ld bytes), skipping: %s", fileSize, filePath.c_str());
    return false;
}
```

### 3. 性能优化

#### 减少处理负载
- **扫描间隔**：从1秒增加到2秒
- **每次处理文件数**：限制为5个（从无限制改为有限制）
- **连续空扫描**：减少到3次（从5次减少）

#### 内存优化
```cpp
// 限制每次处理的文件数量
int maxFilesToProcess = std::min(5, (int)imageFiles.size());

// 减少缓存大小
static constexpr int CACHE_SIZE = 5;            // 从10减少到5
static constexpr int MAX_CACHE_SIZE = 10;       // 从20减少到10
```

### 4. 线程安全改进

#### 锁保护
- 所有缓存操作都有适当的互斥锁保护
- 内存使用量更新使用原子操作
- 避免在图片处理过程中修改共享状态

#### 内存释放
```cpp
// 安全的内存释放
void ImageInjector::releaseMemory(size_t size) {
    if (mCurrentMemoryUsage.load() >= size) {
        mCurrentMemoryUsage -= size;
    } else {
        mCurrentMemoryUsage.store(0);
    }
}
```

## 使用方法

### 1. 替换文件
```bash
# 备份原文件
cp ImageInjector.h ImageInjector.h.backup
cp ImageInjector.cpp ImageInjector.cpp.backup

# 使用修复版本
cp ImageInjector_fixed.h ImageInjector.h
cp ImageInjector_fixed.cpp ImageInjector.cpp
```

### 2. 重新编译
```bash
# 清理并重新编译
make clean
make
```

### 3. 部署测试
```bash
# 推送新版本到设备
adb push ImageInjector /system/lib64/
adb reboot
```

## 预期效果

### 1. 稳定性提升
- 消除内存访问错误
- 防止处理过大图片导致的崩溃
- 减少内存使用量

### 2. 性能优化
- 降低CPU使用率（减少扫描频率）
- 减少内存占用（限制缓存大小）
- 提高处理效率（限制单次处理文件数）

### 3. 错误恢复
- 单个文件处理失败不影响整体功能
- 内存不足时优雅降级
- 详细的错误日志便于调试

## 监控指标

### 1. 内存使用
```cpp
ALOGI("Current Memory Usage: %zuMB", mCurrentMemoryUsage.load() / (1024*1024));
```

### 2. 性能统计
```cpp
ALOGI("Performance Statistics:");
ALOGI("  Total Frames Processed: %lld", (long long)mTotalFramesProcessed.load());
ALOGI("  Cache Hits: %lld", (long long)mCacheHits.load());
ALOGI("  Cache Misses: %lld", (long long)mCacheMisses.load());
ALOGI("  Memory Errors: %lld", (long long)mMemoryErrors.load());
```

### 3. 错误处理
- 文件读取失败
- 图片解码失败
- 内存分配失败
- 图片缩放失败

## 注意事项

1. **向后兼容**：保留了原有的 `loadImageFile` 函数用于单帧模式
2. **配置调整**：可以根据实际需求调整内存限制和缓存大小
3. **日志级别**：大部分日志改为 `ALOGV` 级别，减少日志输出
4. **异常处理**：所有关键操作都有异常保护

这个修复方案应该能够解决相机启动失败的问题，同时提高系统的稳定性和性能。