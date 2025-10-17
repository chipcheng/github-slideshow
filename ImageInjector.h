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

// 视频解码相关头文件
#include <media/NdkMediaCodec.h>
#include <media/NdkMediaExtractor.h>
#include <media/NdkMediaFormat.h>
#include <media/NdkMediaMuxer.h>
#include <android/native_window.h>
#include <android/native_window_jni.h>

namespace android {

/**
 * ImageInjector - 在 Surface 层注入图片到相机数据流
 * 支持多帧缓存队列，优化大量图片帧处理
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
        
        // 视频处理
        bool loadVideoFile(const std::string& filePath);
        bool decodeVideoFrame(const std::string& filePath, int frameIndex,
                              std::vector<uint8_t>& yuvData, int& width, int& height);
        bool initializeVideoDecoder(const std::string& filePath);
        void cleanupVideoDecoder();
        bool extractVideoFrames(const std::string& filePath, std::vector<std::vector<uint8_t>>& frames);

        // 缓存队列管理
        struct CachedFrame {
            std::vector<uint8_t> yuvData;
            int width;
            int height;
            std::string sourceFile;
            int64_t loadTime;
            int frameId;
            bool valid;

            CachedFrame() : width(0), height(0), loadTime(0), frameId(-1), valid(false) {}
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

        // 配置常量
        static constexpr const char* MONITOR_PATH = "/data/misc/cameraserver/";
        static constexpr const char* IMAGE_PREFIX = "inject_";
        static constexpr const char* VIDEO_PREFIX = "video_";
        static constexpr int SCAN_INTERVAL_MS = 1000;  // 更频繁的扫描，应对高帧率
        static constexpr int MAX_INJECTION_WIDTH = 1920;
        static constexpr int MAX_INJECTION_HEIGHT = 1080;
        
        // 视频解码配置
        static constexpr int MAX_VIDEO_FRAMES = 100;  // 最大视频帧数
        static constexpr int64_t VIDEO_FRAME_TIMEOUT_US = 1000000; // 1秒超时

        // 缓存配置
        static constexpr int CACHE_SIZE = 10;           // 缓存帧数
        static constexpr int MAX_CACHE_SIZE = 20;       // 最大缓存帧数（防止内存溢出）
        static constexpr int64_t FRAME_EXPIRY_MS = 30000; // 帧过期时间（30秒）

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

        // 性能统计
        std::atomic<int64_t> mTotalFramesProcessed;
        std::atomic<int64_t> mCacheHits;
        std::atomic<int64_t> mCacheMisses;

        // 最后一个图片帧管理
        std::string mLastImageFile;  // 最后一个图片文件路径
        std::mutex mLastImageMutex;  // 保护最后一个图片文件的互斥锁
        
        // 视频解码相关
        AMediaCodec* mVideoDecoder;  // 视频解码器
        AMediaExtractor* mMediaExtractor;  // 媒体提取器
        AMediaFormat* mVideoFormat;  // 视频格式
        std::mutex mVideoDecoderMutex;  // 视频解码器互斥锁
        std::atomic<bool> mVideoDecoderInitialized;  // 视频解码器是否已初始化
        std::vector<std::vector<uint8_t>> mVideoFrames;  // 视频帧缓存
        std::mutex mVideoFramesMutex;  // 视频帧缓存互斥锁
        int mCurrentVideoFrameIndex;  // 当前视频帧索引

        // 内存管理
        std::atomic<size_t> mCurrentMemoryUsage;  // 当前内存使用量（字节）
        static constexpr size_t MAX_MEMORY_USAGE = 100 * 1024 * 1024;  // 最大内存使用量（100MB）
        static constexpr size_t MAX_IMAGE_SIZE = 10 * 1024 * 1024;     // 单个图片最大大小（10MB）

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