#include "VideoCaptureAndroid.hh"
#include <algorithm>

#include <SDL2/SDL_log.h>

namespace
{
    void processYuv420ToYuyv(AImage* image, int width, int height, uint8_t* buffer)
    {
        int32_t yStride;
        uint8_t *yP;
        int32_t yL;

        if (AMEDIA_OK != AImage_getPlaneData(image, 0, &yP, &yL))
            return;
        AImage_getPlaneRowStride(image, 0, &yStride);

        // We write to backBuffer then swap
        for (int y = 0; y < height; y++)
        {
            for (int x = 0; x < width; x++)
            {
                int outIdx = (y * width + x) * 2;
                buffer[outIdx] = yP[y * yStride + x];
                buffer[outIdx + 1] = 0x80;
            }
        }
    }
}


VideoCaptureAndroid::VideoCaptureAndroid()
{
    cameraManager = ACameraManager_create();
}

VideoCaptureAndroid::~VideoCaptureAndroid()
{
    Close();
    if (cameraManager) ACameraManager_delete(cameraManager);
}

size_t VideoCaptureAndroid::DeviceCount() const
{
    ACameraIdList* idList = nullptr;
    ACameraManager_getCameraIdList(cameraManager, &idList);
    int count = idList ? idList->numCameras : 0;
    ACameraManager_deleteCameraIdList(idList);
    return static_cast<size_t>(count);
}

bool VideoCaptureAndroid::Open(size_t idx)
{
    if (!isClosed) Close();

    ACameraIdList* idList = nullptr;
    ACameraManager_getCameraIdList(cameraManager, &idList);
    if (!idList || idx >= idList->numCameras)
    {
        SDL_Log("Invalid camera index");
        return false;
    }
    for (int i = 0; i < idList->numCameras; ++i)
    {
        SDL_Log("Camera %d is %s", i, idList->cameraIds[i]);
    }
    
    const char* cameraId = idList->cameraIds[idx];

    // 1. Find optimal resolution (Max 1080p)
    ACameraMetadata* metadata = nullptr;
    ACameraManager_getCameraCharacteristics(cameraManager, cameraId, &metadata);
    ACameraMetadata_const_entry entry;
    ACameraMetadata_getConstEntry(metadata, ACAMERA_SCALER_AVAILABLE_STREAM_CONFIGURATIONS, &entry);

    int32_t bestW = 0, bestH = 0;
    for (uint32_t i = 0; i < entry.count; i += 4)
    {
        int32_t format = entry.data.i32[i];
        int32_t w = entry.data.i32[i+1];
        int32_t h = entry.data.i32[i+2];
        SDL_Log("Camera: %d %dx%d", format, w, h);
        if (format == AIMAGE_FORMAT_YUV_420_888 && entry.data.i32[i+3] == 0)
        {
            if (w <= 1920 && h <= 1080 && (w * h > bestW * bestH))
            {
                bestW = w; bestH = h;
            }
        }
    }
    ACameraMetadata_free(metadata);
    if (bestW == 0) return false;

    this->width = bestW;
    this->height = bestH;
    SDL_Log("Selected %s %dx%d image", cameraId, bestW, bestH);

    // 2. Initialize Double Buffers
    size_t frameSize = width * height * 2;
    bufferA.assign(frameSize, 0);
    bufferB.assign(frameSize, 0);
    frontBuffer = bufferA.data();
    backBuffer = bufferB.data();

    ACameraDevice_StateCallbacks cdsc{
        .context = this,
        //.onClientSharedAccessPriorityChanged = [](void *, ACameraDevice *, bool){},
        .onDisconnected = [](void *, ACameraDevice *){},
        .onError = [](void*, ACameraDevice*, int){}
    };


    // 3. Setup Camera Pipeline
    auto camera_status = ACameraManager_openCamera(cameraManager, cameraId, &cdsc, &cameraDevice);
    if (camera_status != ACAMERA_OK)
    {
        SDL_Log("ACameraManager_openCamera(%p, \"%s\", nullptr, &cameraDevice) returned %d", cameraManager, cameraId, (int)camera_status);
    }
    auto media_status = AImageReader_new(width, height, AIMAGE_FORMAT_YUV_420_888, 2, &imageReader);
    if (media_status != AMEDIA_OK)
    {
        SDL_Log("AImageReader_new() returned %d", (int)media_status);
    }
    
    AImageReader_ImageListener listener{.context = this, .onImageAvailable = onImageAvailable};
    media_status = AImageReader_setImageListener(imageReader, &listener);
    if (media_status != AMEDIA_OK)
    {
        SDL_Log("AImageReader_setImageListener() returned %d", (int)media_status);
    }
    media_status = AImageReader_getWindow(imageReader, &imageWindow);
    if (media_status != AMEDIA_OK)
    {
        SDL_Log("AImageReader_getWindow() returned %d", (int)media_status);
    }
    camera_status = ACameraOutputTarget_create(imageWindow, &outputTarget);
    if (camera_status != ACAMERA_OK)
    {
        SDL_Log("ACameraOutputTarget_create() returned %d", (int)camera_status);
    }
    camera_status = ACaptureSessionOutput_create(imageWindow, &sessionOutput);
    if (camera_status != ACAMERA_OK)
    {
        SDL_Log("ACaptureSessionOutput_create() returned %d", (int)camera_status);
    }
    camera_status = ACaptureSessionOutputContainer_create(&outputContainer);
    if (camera_status != ACAMERA_OK)
    {
        SDL_Log("ACaptureSessionOutputContainer_create() returned %d", (int)camera_status);
    }
    camera_status = ACaptureSessionOutputContainer_add(outputContainer, sessionOutput);
    if (camera_status != ACAMERA_OK)
    {
        SDL_Log("ACaptureSessionOutputContainer_add() returned %d", (int)camera_status);
    }
    ACameraCaptureSession_stateCallbacks cssc {
        .context = this,
    };
    cssc.onClosed = [](void *, ACameraCaptureSession *){};
    cssc.onActive = [](void *, ACameraCaptureSession *){};
    cssc.onReady = [](void *, ACameraCaptureSession *){};
    
    camera_status = ACameraDevice_createCaptureSession(cameraDevice, outputContainer, &cssc, &captureSession);
    if (camera_status != ACAMERA_OK)
    {
        SDL_Log("ACameraDevice_createCaptureSession() returned %d", (int)camera_status);
    }
    camera_status = ACameraDevice_createCaptureRequest(cameraDevice,  TEMPLATE_STILL_CAPTURE , &captureRequest);
    if (camera_status != ACAMERA_OK)
    {
        SDL_Log("ACameraDevice_createCaptureRequest() returned %d", (int)camera_status);
    }
    uint8_t macro = ACAMERA_CONTROL_AF_MODE_MACRO;
    camera_status = ACaptureRequest_setEntry_u8(captureRequest, ACAMERA_CONTROL_AF_MODE, 1, &macro);
    if (camera_status != ACAMERA_OK)
    {
        SDL_Log("ACaptureRequest_setEntry_u8(ACAMERA_CONTROL_AF_MODE, ACAMERA_CONTROL_AF_MODE_MACRO) returned %d", (int)camera_status);
    }
    camera_status = ACaptureRequest_addTarget(captureRequest, outputTarget);
    if (camera_status != ACAMERA_OK)
    {
        SDL_Log("ACaptureRequest_addTarget() returned %d", (int)camera_status);
    }
    camera_status = ACameraCaptureSession_setRepeatingRequest(captureSession, nullptr, 1, &captureRequest, nullptr);
    if (camera_status != ACAMERA_OK)
    {
        SDL_Log("ACaptureRequest_addTarget() returned %d", (int)camera_status);
    }

    ACameraManager_deleteCameraIdList(idList);
    isClosed = false;
    return true;
}

void VideoCaptureAndroid::onImageAvailable(void* context, AImageReader* reader)
{
    auto* self = static_cast<VideoCaptureAndroid*>(context);
    AImage* image = nullptr;
    if (AImageReader_acquireLatestImage(reader, &image) == AMEDIA_OK)
    {
        if (self->frames_written == self->frames_read)
        {
            processYuv420ToYuyv(image, self->width, self->height, self->backBuffer);
            std::lock_guard<std::mutex> lock(self->swapMutex);
            std::swap(self->frontBuffer, self->backBuffer);
            ++self->frames_written;
        }
        AImage_delete(image);
    }
}



void* VideoCaptureAndroid::GetFrame()
{
    std::lock_guard<std::mutex> lock(swapMutex);
    if (frames_written != frames_read)
    {
        frames_read = frames_written;
        return static_cast<void*>(frontBuffer);
    }
    return nullptr;
}

void VideoCaptureAndroid::Close()
{
    if (isClosed) return;
    if (captureSession)
    {
        ACameraCaptureSession_stopRepeating(captureSession);
        ACameraCaptureSession_close(captureSession);
    }
    // Cleanup remaining handles...
    if (imageReader) AImageReader_delete(imageReader);
    if (cameraDevice) ACameraDevice_close(cameraDevice);
    isClosed = true;
}