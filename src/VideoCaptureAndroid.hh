// foxscript
// Copyright (C) 2026 thewierdnut
// 
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
#pragma once

#include <camera/NdkCameraManager.h>
#include <camera/NdkCameraDevice.h>
#include <media/NdkImageReader.h>
#include "VideoCapture.hh"
#include <vector>
#include <mutex>
#include <atomic>

class VideoCaptureAndroid: public VideoCapture
{
public:
    VideoCaptureAndroid();
    virtual ~VideoCaptureAndroid();

    // VideoCapture Interface
    virtual size_t DeviceCount() const override;
    virtual bool Open(size_t idx) override;
    virtual void Close() override;
    virtual int Width() const override { return width;  }
    virtual int Height() const override { return height; }
    virtual int Pitch() const override { return width * 2; }
    virtual void* GetFrame() override;

private:
    // Internal Helpers
    static void onImageAvailable(void* context, AImageReader* reader);

    // NDK Camera Handles
    ACameraManager* cameraManager = nullptr;
    ACameraDevice* cameraDevice = nullptr;
    AImageReader* imageReader = nullptr;
    ANativeWindow* imageWindow = nullptr;
    ACameraOutputTarget* outputTarget = nullptr;
    ACaptureSessionOutput* sessionOutput = nullptr;
    ACaptureSessionOutputContainer* outputContainer = nullptr;
    ACameraCaptureSession* captureSession = nullptr;
    ACaptureRequest* captureRequest = nullptr;

    // State and Buffering
    int width = 0;
    int height = 0;
    std::vector<uint8_t> bufferA;
    std::vector<uint8_t> bufferB;
    uint8_t* frontBuffer = nullptr;
    uint8_t* backBuffer = nullptr;

    std::mutex swapMutex;
    std::atomic<bool> isClosed{true};
    uint32_t frames_read = 0;
    uint32_t frames_written = 0;
};
