/*
 * Copyright (C) 2024 The Android Open Source Project
 */

#ifndef ANDROID_GUI_VIDEO_DECODER_H
#define ANDROID_GUI_VIDEO_DECODER_H

#include <string>
#include <vector>
#include <memory>
#include <cstdint>

namespace android {

    class VideoDecoder {
    public:
        VideoDecoder();
        ~VideoDecoder();

        // 初始化视频解码器
        bool initialize(const std::string& filePath);

        // 解码视频帧
        bool decodeFrame(int frameIndex, std::vector<uint8_t>& yuvData, int& width, int& height);

        // 获取视频信息
        int getFrameCount() const;
        int getWidth() const;
        int getHeight() const;

        // 清理资源
        void cleanup();

    private:
        class Impl;
        std::unique_ptr<Impl> mImpl;
    };

} // namespace android

#endif // ANDROID_GUI_VIDEO_DECODER_H