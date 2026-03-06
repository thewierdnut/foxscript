#pragma once

#include <cstddef>
#include <memory>

class VideoCapture
{
public:
   virtual ~VideoCapture() = default;

   virtual size_t DeviceCount() const = 0;

   virtual bool Open(size_t idx) = 0;
   virtual void Close() = 0;

   virtual int Width() const = 0;
   virtual int Height() const = 0;
   virtual int Pitch() const = 0;

   // Always YUYV (YUY2) format.
   virtual void* GetFrame() = 0;
};