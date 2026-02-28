#pragma once

#include <cstddef>
#include <memory>
class VideoCaptureDevice;

class VideoCaptureApi
{
public:
   virtual ~VideoCaptureApi() = default;

   virtual size_t DeviceCount() = 0;
   virtual std::shared_ptr<VideoCaptureDevice> Open(int idx) = 0;
};