/*
 * Copyright (C) 2024 The Android Open Source Project
 */

#ifndef ANDROID_GUI_IMAGE_INJECTOR_H
#define ANDROID_GUI_IMAGE_INJECTOR_H

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include <queue>
#include <condition_variable>
#include <deque>

namespace android {

/**
 * ImageInjector - 在 Surface 层注入图片到相机数据流
 * 支持多帧缓存队列，优化大量图片帧处理
 * 修复版本：增强内存管理和错误处理
 */
    class ImageInjector {
    public:
        ImageInjector();
        ~ImageInjector();

        void startMonitoring();
        void stopMonitoring();
        bool shouldInject() const;
        bool injectToBuffer(void* bufferData, uint32_t width, uint32_t height,
                            uint32_t stride, uint32_t format);
        static bool isCameraBuffer(uint32_t width, uint32_t height, uint32_t format, uint64_t usage);

    private:
        void monitorThreadLoop();

        // 图片处理
        bool loadImageFile(const std::string& filePath);
        bool decodeImage(const uint8_t* data, size_t size,
                         std::vector<uint8_t>& rgbData,
                         int& width, int& height);

        // 缓存队列管理
        struct CachedFrame {
            std::vector<uint8_t> yuvData;
            int width;
            int height;
            std::string sourceFile;
            int64_t loadTime;
            int frameId;
            bool valid;
            size_t memorySize;  // 添加内存大小跟踪

            CachedFrame() : width(0), height(0), loadTime(0), frameId(-1), valid(false), memorySize(0) {}
        };

        bool addFrameToCache(const std::string& filePath, int frameId);
        bool getNextFrameForInjection(CachedFrame& frame);
        void cleanupOldFrames();
        void clearCache();
        int findEmptyCacheSlot();
        int findOldestCacheSlot();

        // 通用处理
        bool convertRGBToYUV(const std::vector<uint8_t>& rgbData,
                             int width, int height,
                             std::vector<uint8_t>& yuvData);
        bool scaleYUVImage(const std::vector<uint8_t>& srcYuv,
                           int srcWidth, int srcHeight,
                           std::vector<uint8_t>& dstYuv,
                           int dstWidth, int dstHeight);
        bool copyYUVToBuffer(const uint8_t* sourceData,
                             uint32_t width, uint32_t height,
                             uint32_t stride, void* bufferData);

        // 文件管理
        void cleanupSourceFile(const std::string& filePath);
        std::vector<std::string> getAllImageFiles();
        bool deleteFile(const std::string& filePath);

        // 内存管理
        bool checkMemoryLimit(size_t additionalSize);
        void updateMemoryUsage(size_t size);
        void releaseMemory(size_t size);

        // 配置常量
        static constexpr const char* MONITOR_PATH = "/data/misc/cameraserver/";
        static constexpr const char* IMAGE_PREFIX = "inject_";
        static constexpr int SCAN_INTERVAL_MS = 2000;  // 增加扫描间隔，减少CPU负载
        static constexpr int MAX_INJECTION_WIDTH = 1920;
        static constexpr int MAX_INJECTION_HEIGHT = 1080;

        // 缓存配置
        static constexpr int CACHE_SIZE = 5;            // 减少缓存大小，降低内存使用
        static constexpr int MAX_CACHE_SIZE = 10;       // 减少最大缓存帧数
        static constexpr int64_t FRAME_EXPIRY_MS = 30000; // 帧过期时间（30秒）

        // 内存管理配置
        static constexpr size_t MAX_MEMORY_USAGE = 50 * 1024 * 1024;  // 最大内存使用量（50MB）
        static constexpr size_t MAX_IMAGE_SIZE = 5 * 1024 * 1024;     // 单个图片最大大小（5MB）
        static constexpr size_t MAX_IMAGE_DIMENSION = 4096;           // 最大图片尺寸

        // 线程控制
        std::thread mMonitorThread;
        std::atomic<bool> mExitRequested;

        // 注入状态
        std::atomic<bool> mInjectionEnabled;
        std::atomic<int> mCurrentFrameId;
        std::atomic<int> mNextFrameId;

        // 缓存队列
        std::vector<CachedFrame> mFrameCache;
        std::mutex mCacheMutex;
        std::atomic<int> mCacheSize;
        std::atomic<int> mInjectionIndex;

        // 内存管理
        std::atomic<size_t> mCurrentMemoryUsage;  // 当前内存使用量（字节）

        // 性能统计
        std::atomic<int64_t> mTotalFramesProcessed;
        std::atomic<int64_t> mCacheHits;
        std::atomic<int64_t> mCacheMisses;
        std::atomic<int64_t> mMemoryErrors;

        // 最后一个图片帧管理
        std::string mLastImageFile;  // 最后一个图片文件路径
        std::mutex mLastImageMutex;  // 保护最后一个图片文件的互斥锁

        // 原有成员变量（为了兼容性保留）
        std::mutex mDataMutex;
        std::vector<uint8_t> mInjectionYuvData;
        int mInjectionWidth;
        int mInjectionHeight;
        std::string mCurrentFile;
        time_t mLastModTime;
    };

} // namespace android

#endif // ANDROID_GUI_IMAGE_INJECTOR_H