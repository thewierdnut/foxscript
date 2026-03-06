#pragma once

#include "VideoCapture.hh"

#include <map>
#include <string>
#include <vector>
#include <cstdint>


class VideoCaptureV4L2: public VideoCapture
{
public:
   VideoCaptureV4L2();
   virtual ~VideoCaptureV4L2();

   virtual size_t DeviceCount() const override { return m_devices.size(); }

   virtual bool Open(size_t idx) override;
   virtual void Close() override;

   virtual int Width() const override { return m_devices[m_dev_idx].format.width; }
   virtual int Height() const override { return m_devices[m_dev_idx].format.height; }
   virtual int Pitch() const override { return m_pitch; }

   // Always YUYV (YUY2) format.
   virtual void* GetFrame() override;

private:
   struct CameraInfo
   {
      std::string name;
      std::string path;
      struct Format {
         uint32_t pixel_format;
         uint32_t width;
         uint32_t height;
         struct { uint32_t num; uint32_t den; } rate;
      };
      Format format;
   };

   std::vector<CameraInfo> m_devices;
   
   struct BufferInfo
   {
      void* p;
      size_t length;
   };
   std::vector<BufferInfo> m_buffers;
   // std::vector<uint8_t> m_rgb_buffer;
   int m_current_buffer = -1;

   int m_fd = -1;
   int m_dev_idx = -1;
   int m_pitch = -1;
   // int m_rgb_stride = -1;
};