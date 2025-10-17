/*
 * Copyright (C) 2024 The Android Open Source Project
 */

#define LOG_TAG "ImageInjector"

#include "ImageInjector.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <chrono>
#include <cstring>

#include <android/log.h>
#include <log/log.h>
#include <jpeglib.h>

namespace android {

    ImageInjector::ImageInjector()
            : mExitRequested(false),
              mInjectionEnabled(false),
              mCurrentFrameId(0),
              mNextFrameId(0),
              mCacheSize(0),
              mInjectionIndex(0),
              mCurrentMemoryUsage(0),
              mTotalFramesProcessed(0),
              mCacheHits(0),
              mCacheMisses(0),
              mMemoryErrors(0),
              mInjectionWidth(0),
              mInjectionHeight(0),
              mLastModTime(0) {

        // 初始化缓存
        mFrameCache.resize(MAX_CACHE_SIZE);

        ALOGI("ImageInjector created with enhanced safety features");
        ALOGI("Cache configuration: Size=%d, Max=%d, Expiry=%lldms",
              CACHE_SIZE, MAX_CACHE_SIZE, (long long)FRAME_EXPIRY_MS);
        ALOGI("Memory limits: Max=%zuMB, Per-image=%zuMB, Max-dimension=%zu",
              MAX_MEMORY_USAGE / (1024*1024), MAX_IMAGE_SIZE / (1024*1024), MAX_IMAGE_DIMENSION);
        ALOGI("Monitor path: %s", MONITOR_PATH);
    }

    ImageInjector::~ImageInjector() {
        stopMonitoring();
        clearCache();
        ALOGI("ImageInjector destroyed");
    }

    void ImageInjector::startMonitoring() {
        if (mMonitorThread.joinable()) {
            ALOGW("Monitoring already started");
            return;
        }

        mExitRequested = false;
        mMonitorThread = std::thread(&ImageInjector::monitorThreadLoop, this);
        ALOGI("Image monitoring started with enhanced safety");
    }

    void ImageInjector::stopMonitoring() {
        if (!mMonitorThread.joinable()) {
            return;
        }

        mExitRequested = true;
        if (mMonitorThread.joinable()) {
            mMonitorThread.join();
        }

        // 输出性能统计
        ALOGI("Performance Statistics:");
        ALOGI("  Total Frames Processed: %lld", (long long)mTotalFramesProcessed.load());
        ALOGI("  Cache Hits: %lld", (long long)mCacheHits.load());
        ALOGI("  Cache Misses: %lld", (long long)mCacheMisses.load());
        ALOGI("  Memory Errors: %lld", (long long)mMemoryErrors.load());
        ALOGI("  Current Memory Usage: %zuMB", mCurrentMemoryUsage.load() / (1024*1024));
        if (mTotalFramesProcessed > 0) {
            float hitRate = (float)mCacheHits / mTotalFramesProcessed * 100.0f;
            ALOGI("  Cache Hit Rate: %.2f%%", hitRate);
        }

        ALOGI("Image monitoring stopped");
    }

    void ImageInjector::monitorThreadLoop() {
        ALOGI("Monitor thread started - Enhanced safety mode");
        ALOGI("Cache size: %d frames, Scan interval: %dms", CACHE_SIZE, SCAN_INTERVAL_MS);

        int consecutiveEmptyScans = 0;
        const int MAX_CONSECUTIVE_EMPTY_SCANS = 3;  // 减少连续空扫描次数
        int scanCount = 0;

        while (!mExitRequested) {
            scanCount++;
            ALOGV("Scan #%d starting...", scanCount);

            // 1. 清理过期帧
            cleanupOldFrames();

            // 2. 扫描新文件
            DIR* dir = opendir(MONITOR_PATH);
            if (!dir) {
                ALOGE("Failed to open directory %s: %s", MONITOR_PATH, strerror(errno));
                std::this_thread::sleep_for(std::chrono::milliseconds(SCAN_INTERVAL_MS));
                continue;
            }

            std::vector<std::string> imageFiles;
            struct dirent* entry;

            while ((entry = readdir(dir)) != nullptr) {
                std::string filename = entry->d_name;

                // 检查文件名前缀和扩展名
                if (filename.find(IMAGE_PREFIX) != 0) continue;
                if (filename.find(".jpg") == std::string::npos &&
                    filename.find(".jpeg") == std::string::npos) continue;

                std::string fullPath = std::string(MONITOR_PATH) + filename;
                imageFiles.push_back(fullPath);
                ALOGI("Found image file: %s", fullPath.c_str());
            }
            closedir(dir);

            ALOGI("Scan #%d complete - Found %zu image files", scanCount, imageFiles.size());

            if (!imageFiles.empty()) {
                consecutiveEmptyScans = 0;

                // 清理之前保留的最后一个文件（如果有新文件的话）
                {
                    std::lock_guard<std::mutex> lock(mLastImageMutex);
                    if (!mLastImageFile.empty()) {
                        ALOGV("New files found, cleaning up previous last image: %s", mLastImageFile.c_str());
                        cleanupSourceFile(mLastImageFile);
                        mLastImageFile.clear();
                    }
                }

                // 3. 处理找到的文件（限制处理数量）
                int loadedCount = 0;
                int maxFilesToProcess = std::min(5, (int)imageFiles.size());  // 限制每次最多处理5个文件
                
                for (int i = 0; i < maxFilesToProcess; i++) {
                    const auto& filePath = imageFiles[i];
                    bool isLastFile = (i == maxFilesToProcess - 1);
                    
                    if (mCacheSize.load() >= CACHE_SIZE) {
                        ALOGW("Cache full (%d/%d), stopping file loading",
                              mCacheSize.load(), CACHE_SIZE);
                        break;
                    }

                    // 为每个文件分配帧ID并添加到缓存
                    int frameId = mNextFrameId++;
                    if (addFrameToCache(filePath, frameId)) {
                        loadedCount++;
                        mTotalFramesProcessed++;

                        // 更新最后一个图片文件
                        {
                            std::lock_guard<std::mutex> lock(mLastImageMutex);
                            mLastImageFile = filePath;
                        }

                        // 如果不是最后一个文件，立即删除源文件
                        if (!isLastFile) {
                            cleanupSourceFile(filePath);
                        } else {
                            ALOGI("Keeping last image file: %s", filePath.c_str());
                        }
                    } else {
                        mMemoryErrors++;
                    }
                }

                if (loadedCount > 0) {
                    ALOGI("Loaded %d new frames to cache, total cache size: %d",
                          loadedCount, mCacheSize.load());
                    mInjectionEnabled = true;
                }

            } else {
                // 没有找到文件
                consecutiveEmptyScans++;
                ALOGV("No image files found (consecutive: %d)", consecutiveEmptyScans);

                // 检查是否还有最后一个文件需要保留
                {
                    std::lock_guard<std::mutex> lock(mLastImageMutex);
                    if (!mLastImageFile.empty()) {
                        ALOGV("No new files found, keeping last image: %s", mLastImageFile.c_str());
                    }
                }

                // 如果连续多次扫描都没有文件，可以降低扫描频率
                if (consecutiveEmptyScans > MAX_CONSECUTIVE_EMPTY_SCANS) {
                    ALOGV("Reducing scan frequency due to consecutive empty scans");
                    std::this_thread::sleep_for(std::chrono::milliseconds(SCAN_INTERVAL_MS * 2));
                    continue;
                }
            }

            // 4. 检查是否需要保持注入状态
            if (mCacheSize.load() == 0 && mInjectionEnabled.load()) {
                mInjectionEnabled = false;
                ALOGW("Cache empty, injection disabled");
            } else if (mCacheSize.load() > 0 && !mInjectionEnabled.load()) {
                mInjectionEnabled = true;
                ALOGI("Cache has frames, injection enabled");
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(SCAN_INTERVAL_MS));
        }

        ALOGI("Monitor thread exited");
    }

    bool ImageInjector::addFrameToCache(const std::string& filePath, int frameId) {
        ALOGI("Adding frame to cache: %s (ID: %d)", filePath.c_str(), frameId);

        // 1. 检查内存使用量
        if (!checkMemoryLimit(MAX_IMAGE_SIZE)) {
            ALOGW("Memory usage too high (%zu bytes), skipping file: %s", 
                  mCurrentMemoryUsage.load(), filePath.c_str());
            return false;
        }

        // 2. 检查缓存大小限制
        if (mCacheSize.load() >= CACHE_SIZE) {
            ALOGW("Cache size limit reached (%d), skipping file: %s", 
                  mCacheSize.load(), filePath.c_str());
            return false;
        }

        // 3. 找到可用的缓存槽
        int cacheIndex = findEmptyCacheSlot();
        if (cacheIndex == -1) {
            cacheIndex = findOldestCacheSlot();
            ALOGW("Cache full, replacing oldest frame at slot %d", cacheIndex);
        }

        // 4. 安全地读取文件
        int fd = open(filePath.c_str(), O_RDONLY);
        if (fd < 0) {
            ALOGE("Failed to open file: %s, error: %s", filePath.c_str(), strerror(errno));
            return false;
        }

        // 获取文件大小并检查限制
        off_t fileSize = lseek(fd, 0, SEEK_END);
        if (fileSize < 0) {
            ALOGE("Failed to get file size: %s", strerror(errno));
            close(fd);
            return false;
        }

        if (fileSize > MAX_IMAGE_SIZE) {
            ALOGW("File too large (%ld bytes), skipping: %s", fileSize, filePath.c_str());
            close(fd);
            return false;
        }

        lseek(fd, 0, SEEK_SET);

        // 使用异常安全的文件读取
        std::vector<uint8_t> fileData;
        try {
            fileData.resize(fileSize);
            ssize_t bytesRead = read(fd, fileData.data(), fileSize);
            close(fd);
            
            if (bytesRead != fileSize) {
                ALOGE("Failed to read complete file: expected %ld, got %zd", fileSize, bytesRead);
                return false;
            }
        } catch (const std::exception& e) {
            ALOGE("Exception during file read: %s", e.what());
            close(fd);
            return false;
        }

        // 5. 安全地解码图片
        std::vector<uint8_t> rgbData;
        int width, height;
        
        try {
            if (!decodeImage(fileData.data(), fileSize, rgbData, width, height)) {
                ALOGE("Failed to decode image: %s", filePath.c_str());
                return false;
            }
        } catch (const std::exception& e) {
            ALOGE("Exception during image decode: %s", e.what());
            return false;
        }

        ALOGI("Decoded image: %dx%d", width, height);

        // 6. 检查图片尺寸合理性
        if (width <= 0 || height <= 0 || width > MAX_IMAGE_DIMENSION || height > MAX_IMAGE_DIMENSION) {
            ALOGW("Invalid image dimensions: %dx%d, skipping file: %s", width, height, filePath.c_str());
            return false;
        }

        // 7. 确定目标分辨率
        int targetWidth = width;
        int targetHeight = height;

        if (width > MAX_INJECTION_WIDTH || height > MAX_INJECTION_HEIGHT) {
            float scale = std::min(
                    (float)MAX_INJECTION_WIDTH / width,
                    (float)MAX_INJECTION_HEIGHT / height
            );
            targetWidth = std::max(1, (int)(width * scale));
            targetHeight = std::max(1, (int)(height * scale));
            ALOGI("Scaling image from %dx%d to %dx%d", width, height, targetWidth, targetHeight);
        }

        // 8. 安全地转换为 YUV
        std::vector<uint8_t> yuvData;
        try {
            if (!convertRGBToYUV(rgbData, width, height, yuvData)) {
                ALOGE("Failed to convert to YUV: %s", filePath.c_str());
                return false;
            }
        } catch (const std::exception& e) {
            ALOGE("Exception during RGB to YUV conversion: %s", e.what());
            return false;
        }

        // 9. 安全地缩放图片
        std::vector<uint8_t> finalYuvData;
        try {
            if (targetWidth != width || targetHeight != height) {
                if (!scaleYUVImage(yuvData, width, height, finalYuvData, targetWidth, targetHeight)) {
                    ALOGE("Failed to scale image: %s", filePath.c_str());
                    return false;
                }
            } else {
                finalYuvData = std::move(yuvData);
            }
        } catch (const std::exception& e) {
            ALOGE("Exception during image scaling: %s", e.what());
            return false;
        }

        // 10. 将数据复制到缓存槽
        std::lock_guard<std::mutex> lock(mCacheMutex);

        CachedFrame& frame = mFrameCache[cacheIndex];

        // 如果替换现有帧，先释放内存
        if (frame.valid) {
            releaseMemory(frame.memorySize);
            mCacheSize--;
        }

        frame.yuvData = std::move(finalYuvData);
        frame.width = targetWidth;
        frame.height = targetHeight;
        frame.frameId = frameId;
        frame.sourceFile = filePath;
        frame.loadTime = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
        frame.valid = true;
        frame.memorySize = frame.yuvData.size();

        // 更新内存使用量
        updateMemoryUsage(frame.memorySize);

        // 增加计数
        mCacheSize++;

        ALOGI("Frame added to cache at slot %d, ID: %d, size: %dx%d, memory: %zu bytes, cache size: %d",
              cacheIndex, frameId, targetWidth, targetHeight, frame.memorySize, mCacheSize.load());

        return true;
    }

    bool ImageInjector::checkMemoryLimit(size_t additionalSize) {
        return (mCurrentMemoryUsage.load() + additionalSize) <= MAX_MEMORY_USAGE;
    }

    void ImageInjector::updateMemoryUsage(size_t size) {
        mCurrentMemoryUsage += size;
    }

    void ImageInjector::releaseMemory(size_t size) {
        if (mCurrentMemoryUsage.load() >= size) {
            mCurrentMemoryUsage -= size;
        } else {
            mCurrentMemoryUsage.store(0);
        }
    }

    bool ImageInjector::getNextFrameForInjection(CachedFrame& frame) {
        if (mCacheSize.load() == 0) {
            ALOGW("No frames in cache for injection");
            mCacheMisses++;
            return false;
        }

        std::lock_guard<std::mutex> lock(mCacheMutex);

        // 简单的轮询策略：按顺序使用缓存帧
        int startIndex = mInjectionIndex.load();
        int currentIndex = startIndex;
        int checkedCount = 0;

        do {
            if (mFrameCache[currentIndex].valid) {
                frame = mFrameCache[currentIndex];
                mInjectionIndex.store((currentIndex + 1) % MAX_CACHE_SIZE);
                mCacheHits++;

                ALOGV("Injection frame found at slot %d, ID: %d", currentIndex, frame.frameId);
                return true;
            }

            currentIndex = (currentIndex + 1) % MAX_CACHE_SIZE;
            checkedCount++;
        } while (currentIndex != startIndex && checkedCount < MAX_CACHE_SIZE);

        ALOGW("No valid frames found in cache despite cache size > 0");
        mCacheMisses++;
        return false;
    }

    void ImageInjector::cleanupOldFrames() {
        std::lock_guard<std::mutex> lock(mCacheMutex);

        int64_t currentTime = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();

        int cleanedCount = 0;
        for (auto& frame : mFrameCache) {
            if (frame.valid && (currentTime - frame.loadTime) > FRAME_EXPIRY_MS) {
                ALOGV("Cleaning up expired frame ID: %d, age: %lldms",
                      frame.frameId, (long long)(currentTime - frame.loadTime));

                // 释放内存
                releaseMemory(frame.memorySize);
                std::vector<uint8_t> empty;
                frame.yuvData.swap(empty);
                frame.yuvData.shrink_to_fit();

                frame.valid = false;
                frame.memorySize = 0;
                cleanedCount++;
            }
        }

        if (cleanedCount > 0) {
            mCacheSize -= cleanedCount;
            ALOGI("Cleaned %d expired frames, current cache size: %d, memory usage: %zuMB",
                  cleanedCount, mCacheSize.load(), mCurrentMemoryUsage.load() / (1024*1024));
        }
    }

    void ImageInjector::clearCache() {
        std::lock_guard<std::mutex> lock(mCacheMutex);

        for (auto& frame : mFrameCache) {
            if (frame.valid) {
                releaseMemory(frame.memorySize);
                std::vector<uint8_t> empty;
                frame.yuvData.swap(empty);
                frame.yuvData.shrink_to_fit();
                frame.valid = false;
                frame.memorySize = 0;
            }
        }

        mCacheSize.store(0);
        mInjectionIndex.store(0);
        mCurrentMemoryUsage.store(0);
        ALOGI("Cache cleared");
    }

    int ImageInjector::findEmptyCacheSlot() {
        std::lock_guard<std::mutex> lock(mCacheMutex);

        for (int i = 0; i < MAX_CACHE_SIZE; i++) {
            if (!mFrameCache[i].valid) {
                return i;
            }
        }

        return -1; // 没有空槽
    }

    int ImageInjector::findOldestCacheSlot() {
        std::lock_guard<std::mutex> lock(mCacheMutex);

        int oldestIndex = -1;
        int64_t oldestTime = INT64_MAX;

        for (int i = 0; i < MAX_CACHE_SIZE; i++) {
            if (mFrameCache[i].valid && mFrameCache[i].loadTime < oldestTime) {
                oldestTime = mFrameCache[i].loadTime;
                oldestIndex = i;
            }
        }

        return oldestIndex;
    }

    bool ImageInjector::shouldInject() const {
        bool enabled = mInjectionEnabled.load();
        int cacheSize = mCacheSize.load();
        bool should = enabled && (cacheSize > 0);

        ALOGV("shouldInject: %d (enabled: %d, cache size: %d)",
              should, enabled, cacheSize);

        if (!enabled) {
            ALOGV("Injection disabled - no files loaded");
        }
        if (cacheSize == 0) {
            ALOGV("Cache empty - no frames available");
        }

        return should;
    }

    bool ImageInjector::injectToBuffer(void* bufferData, uint32_t width, uint32_t height,
                                       uint32_t stride, uint32_t format) {
        ALOGV("injectToBuffer called: %dx%d, format=%d, stride=%d",
              width, height, format, stride);

        if (!shouldInject()) {
            ALOGV("shouldInject returned false, skipping injection");
            return false;
        }

        // 1. 从缓存获取下一帧
        CachedFrame frame;
        if (!getNextFrameForInjection(frame)) {
            ALOGW("No frame available for injection");
            return false;
        }

        if (!frame.valid) {
            ALOGW("Retrieved invalid frame from cache");
            return false;
        }

        ALOGV("Injecting frame ID: %d, size: %dx%d",
              frame.frameId, frame.width, frame.height);

        // 2. 检查格式
        if (format != 34 && format != 35) {
            ALOGW("Unsupported buffer format: %d (expected 34 or 35)", format);
            return false;
        }

        // 3. 缩放帧到目标尺寸（如果需要）
        std::vector<uint8_t> scaledData;
        const uint8_t* sourceData = nullptr;

        if (frame.width == (int)width && frame.height == (int)height) {
            sourceData = frame.yuvData.data();
            ALOGV("Frame size matches target, no scaling needed");
        } else {
            ALOGV("Scaling frame from %dx%d to %dx%d",
                  frame.width, frame.height, width, height);
            try {
                if (!scaleYUVImage(frame.yuvData, frame.width, frame.height,
                                   scaledData, width, height)) {
                    ALOGE("Failed to scale frame");
                    return false;
                }
                sourceData = scaledData.data();
            } catch (const std::exception& e) {
                ALOGE("Exception during frame scaling: %s", e.what());
                return false;
            }
        }

        // 4. 复制到buffer
        bool success = false;
        try {
            success = copyYUVToBuffer(sourceData, width, height, stride, bufferData);
        } catch (const std::exception& e) {
            ALOGE("Exception during buffer copy: %s", e.what());
            return false;
        }

        if (success) {
            ALOGV("Successfully injected frame ID: %d", frame.frameId);
        } else {
            ALOGW("Failed to inject frame ID: %d", frame.frameId);
        }

        return success;
    }

    void ImageInjector::cleanupSourceFile(const std::string& filePath) {
        if (deleteFile(filePath)) {
            ALOGV("Successfully cleaned up source file: %s", filePath.c_str());
        } else {
            ALOGW("Failed to clean up source file: %s", filePath.c_str());
        }
    }

    std::vector<std::string> ImageInjector::getAllImageFiles() {
        std::vector<std::string> imageFiles;

        DIR* dir = opendir(MONITOR_PATH);
        if (!dir) {
            ALOGE("Failed to open directory for file listing: %s", strerror(errno));
            return imageFiles;
        }

        struct dirent* entry;
        while ((entry = readdir(dir)) != nullptr) {
            std::string filename = entry->d_name;

            // 检查文件名前缀和扩展名
            if (filename.find(IMAGE_PREFIX) != 0) continue;
            if (filename.find(".jpg") == std::string::npos &&
                filename.find(".jpeg") == std::string::npos) continue;

            std::string fullPath = std::string(MONITOR_PATH) + filename;
            imageFiles.push_back(fullPath);
        }

        closedir(dir);
        return imageFiles;
    }

    bool ImageInjector::deleteFile(const std::string& filePath) {
        if (unlink(filePath.c_str()) == 0) {
            return true;
        } else {
            ALOGE("Failed to delete file %s: %s", filePath.c_str(), strerror(errno));
            return false;
        }
    }

    // 保留原有loadImageFile函数用于单帧模式（向后兼容）
    bool ImageInjector::loadImageFile(const std::string& filePath) {
        ALOGI("Loading image: %s", filePath.c_str());

        // 读取文件
        int fd = open(filePath.c_str(), O_RDONLY);
        if (fd < 0) {
            ALOGE("Failed to open file: %s", strerror(errno));
            return false;
        }

        off_t fileSize = lseek(fd, 0, SEEK_END);
        lseek(fd, 0, SEEK_SET);

        std::vector<uint8_t> fileData(fileSize);
        if (read(fd, fileData.data(), fileSize) != fileSize) {
            ALOGE("Failed to read file");
            close(fd);
            return false;
        }
        close(fd);

        // 解码图片
        std::vector<uint8_t> rgbData;
        int width, height;
        if (!decodeImage(fileData.data(), fileSize, rgbData, width, height)) {
            ALOGE("Failed to decode image");
            return false;
        }

        ALOGI("Decoded image: %dx%d", width, height);

        // 确定目标分辨率
        int targetWidth = width;
        int targetHeight = height;

        if (width > MAX_INJECTION_WIDTH || height > MAX_INJECTION_HEIGHT) {
            float scale = std::min(
                    (float)MAX_INJECTION_WIDTH / width,
                    (float)MAX_INJECTION_HEIGHT / height
            );
            targetWidth = (int)(width * scale);
            targetHeight = (int)(height * scale);
            ALOGI("Scaling image from %dx%d to %dx%d", width, height, targetWidth, targetHeight);
        }

        // 转换为 YUV
        std::vector<uint8_t> yuvData;
        if (!convertRGBToYUV(rgbData, width, height, yuvData)) {
            ALOGE("Failed to convert to YUV");
            return false;
        }

        // 如果需要缩放
        std::vector<uint8_t> finalYuvData;
        if (targetWidth != width || targetHeight != height) {
            if (!scaleYUVImage(yuvData, width, height, finalYuvData, targetWidth, targetHeight)) {
                ALOGE("Failed to scale image");
                return false;
            }
        } else {
            finalYuvData = std::move(yuvData);
        }

        // 保存到成员变量（单帧模式）
        {
            std::lock_guard<std::mutex> lock(mDataMutex);
            mInjectionYuvData = std::move(finalYuvData);
            mInjectionWidth = targetWidth;
            mInjectionHeight = targetHeight;
        }

        ALOGI("Image loaded successfully: %dx%d", targetWidth, targetHeight);
        return true;
    }

    // 以下函数保持原有实现不变
    bool ImageInjector::decodeImage(const uint8_t* data, size_t size,
                                    std::vector<uint8_t>& rgbData,
                                    int& width, int& height) {
        // 检测文件类型
        bool isJpeg = (size >= 2 && data[0] == 0xFF && data[1] == 0xD8);

        if (isJpeg) {
            // JPEG 解码
            struct jpeg_decompress_struct cinfo;
            struct jpeg_error_mgr jerr;

            cinfo.err = jpeg_std_error(&jerr);
            jpeg_create_decompress(&cinfo);
            jpeg_mem_src(&cinfo, data, size);
            jpeg_read_header(&cinfo, TRUE);
            jpeg_start_decompress(&cinfo);

            width = cinfo.output_width;
            height = cinfo.output_height;
            int channels = cinfo.output_components;

            rgbData.resize(width * height * 3);
            std::vector<uint8_t*> rowPointers(height);
            for (int i = 0; i < height; i++) {
                rowPointers[i] = rgbData.data() + i * width * 3;
            }

            while (cinfo.output_scanline < cinfo.output_height) {
                jpeg_read_scanlines(&cinfo, &rowPointers[cinfo.output_scanline], 1);
            }

            jpeg_finish_decompress(&cinfo);
            jpeg_destroy_decompress(&cinfo);

            return true;
        }

        ALOGE("Only JPEG format is currently supported");
        return false;
    }

    bool ImageInjector::convertRGBToYUV(const std::vector<uint8_t>& rgbData,
                                        int width, int height,
                                        std::vector<uint8_t>& yuvData) {
        size_t ySize = width * height;
        size_t uvSize = (width / 2) * (height / 2) * 2;
        yuvData.resize(ySize + uvSize);

        uint8_t* yPlane = yuvData.data();
        uint8_t* uvPlane = yuvData.data() + ySize;

        // 转换 RGB 到 YUV
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                int rgbIdx = (y * width + x) * 3;
                int r = rgbData[rgbIdx];
                int g = rgbData[rgbIdx + 1];
                int b = rgbData[rgbIdx + 2];

                // Y
                int yVal = ((66 * r + 129 * g + 25 * b + 128) >> 8) + 16;
                yPlane[y * width + x] = std::clamp(yVal, 0, 255);

                // UV (每 2x2 采样一次)
                if (y % 2 == 0 && x % 2 == 0) {
                    int uVal = ((-38 * r - 74 * g + 112 * b + 128) >> 8) + 128;
                    int vVal = ((112 * r - 94 * g - 18 * b + 128) >> 8) + 128;

                    int uvIdx = (y / 2) * width + x;
                    uvPlane[uvIdx] = std::clamp(vVal, 0, 255);     // V
                    uvPlane[uvIdx + 1] = std::clamp(uVal, 0, 255); // U
                }
            }
        }

        return true;
    }

    bool ImageInjector::scaleYUVImage(const std::vector<uint8_t>& srcYuv,
                                      int srcWidth, int srcHeight,
                                      std::vector<uint8_t>& dstYuv,
                                      int dstWidth, int dstHeight) {
        // 简单的最近邻插值缩放
        size_t dstYSize = dstWidth * dstHeight;
        size_t dstUVSize = (dstWidth / 2) * (dstHeight / 2) * 2;
        dstYuv.resize(dstYSize + dstUVSize);

        const uint8_t* srcY = srcYuv.data();
        const uint8_t* srcUV = srcYuv.data() + srcWidth * srcHeight;
        uint8_t* dstY = dstYuv.data();
        uint8_t* dstUV = dstYuv.data() + dstYSize;

        float xRatio = (float)srcWidth / dstWidth;
        float yRatio = (float)srcHeight / dstHeight;

        // 缩放 Y 平面
        for (int y = 0; y < dstHeight; y++) {
            for (int x = 0; x < dstWidth; x++) {
                int srcXIdx = (int)(x * xRatio);
                int srcYIdx = (int)(y * yRatio);
                dstY[y * dstWidth + x] = srcY[srcYIdx * srcWidth + srcXIdx];
            }
        }

        // 缩放 UV 平面
        for (int y = 0; y < dstHeight / 2; y++) {
            for (int x = 0; x < dstWidth / 2; x++) {
                int srcXIdx = (int)(x * xRatio);
                int srcYIdx = (int)(y * yRatio);
                int srcIdx = srcYIdx * srcWidth + srcXIdx * 2;
                int dstIdx = y * dstWidth + x * 2;
                dstUV[dstIdx] = srcUV[srcIdx];
                dstUV[dstIdx + 1] = srcUV[srcIdx + 1];
            }
        }

        return true;
    }

    bool ImageInjector::copyYUVToBuffer(const uint8_t* sourceData,
                                        uint32_t width, uint32_t height,
                                        uint32_t stride, void* bufferData) {
        if (!sourceData || !bufferData) {
            ALOGE("Invalid parameters for copyYUVToBuffer");
            return false;
        }

        uint8_t* dst = static_cast<uint8_t*>(bufferData);
        size_t ySize = width * height;

        // 复制 Y 平面（考虑 stride）
        for (uint32_t y = 0; y < height; y++) {
            memcpy(dst + y * stride, sourceData + y * width, width);
        }

        // 复制 UV 平面
        uint8_t* dstUV = dst + stride * height;
        const uint8_t* srcUV = sourceData + ySize;
        for (uint32_t y = 0; y < height / 2; y++) {
            memcpy(dstUV + y * stride, srcUV + y * width, width);
        }

        return true;
    }

    bool ImageInjector::isCameraBuffer(uint32_t width, uint32_t height,
                                       uint32_t format, uint64_t usage) {
        if (format != 34 && format != 35) {
            return false;
        }

        // 常见相机分辨率范围
        if (width < 640 || width > 4096 || height < 480 || height > 3072) {
            return false;
        }

        // 检查 usage 标志
        const uint64_t CAMERA_USAGE_MASK = (1ULL << 8) | (1ULL << 16) | (1ULL << 0);
        if ((usage & CAMERA_USAGE_MASK) == 0) {
            return false;
        }

        return true;
    }

} // namespace android