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
#include "VideoCaptureV4L2.hh"

#include <cassert>
#include <iostream>
#include <string>
#include <cstdint>

#include <fcntl.h>
#include <unistd.h>
#include <linux/videodev2.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#include <SDL2/SDL_log.h>


namespace {
   uint8_t Clamp(int v)
   {
      if (v < 0) return 0;
      if (v > 255) return 255;
      return v;
   }

   // void YUYV_to_RGB24(const void* yuyv, void* rgb, int stride, int height)
   // {
   //    // TODO: will stride ever be negative (indicating bottom up image)?
   //    const uint8_t* src = (const uint8_t*)yuyv;
   //    uint8_t* row = (uint8_t*)rgb;

   //    for (int h = 0; h < height; ++h)
   //    {
   //       // assert(src == ((uint8_t*)yuyv + h * 1920 * 4 / 2));
   //       uint8_t *p  = row;
   //       while (p + 6 <= row + stride)
   //       {
   //          uint8_t y1 = src[0];
   //          uint8_t u  = src[1];
   //          uint8_t y2 = src[2];
   //          uint8_t v  = src[3];

   //          // TODO: convert to integer arithmetic.
   //          p[0] = Clamp(y1 + 1.4075 * (v - 128));
   //          p[1] = Clamp(y1 - 0.3455 * (u - 128) - 0.7169 * (v - 128));
   //          p[2] = Clamp(y1 + 1.7790 * (u - 128));
   //          p[3] = Clamp(y2 + 1.4075 * (v - 128));
   //          p[4] = Clamp(y2 - 0.3455 * (u - 128) - 0.7169 * (v - 128));
   //          p[5] = Clamp(y2 + 1.7790 * (u - 128));

   //          p += 6;
   //          src += 4;
   //       }
         

   //       row += stride;
   //    }
   //    // assert(src == ((uint8_t*)yuyv + 1080 * 1920 * 4 / 2));
   // }

}


VideoCaptureV4L2::VideoCaptureV4L2()
{
   std::string path = "/dev/video";
   for (int i = 0; i < 64; ++i)
   {
      int fd = open((path + std::to_string(i)).c_str(), O_RDWR|O_NONBLOCK);
      if (fd < 0)
         continue;

      v4l2_capability cap;
      if (ioctl(fd, VIDIOC_QUERYCAP, &cap) < 0)
      {
         close(fd);
         continue;
      }

      if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE))
      {
         close(fd);
         continue;
      }
      std::cout << (char*)cap.card << " /dev/video" << i << '\n';


      CameraInfo info = {
         .name = (char*)cap.card,
         .path = path + std::to_string(i)
      };
      v4l2_fmtdesc fmt{
         .index = 0,
         .type = V4L2_BUF_TYPE_VIDEO_CAPTURE
      };
      while (ioctl(fd, VIDIOC_ENUM_FMT, &fmt) == 0)
      {
         union {uint32_t i; char c[4];} fourcc{fmt.pixelformat};
         SDL_Log("Device %d  %c%c%c%c",
            i,
            fourcc.c[0], fourcc.c[1], fourcc.c[2], fourcc.c[3]
         );
         ++fmt.index;
         // Only support planar uncompressed formats, since I'm too lazy to
         // pull in a jpeg decompressor, and this is mostly just for testing.
         if (fmt.pixelformat != V4L2_PIX_FMT_YUYV) // This is the one my camera outputs.
            continue;
         
         v4l2_frmsizeenum fs{
            .index = 0,
            .pixel_format = fmt.pixelformat
         };
         while (ioctl(fd, VIDIOC_ENUM_FRAMESIZES, &fs) == 0)
         {
            ++fs.index;
            if (fs.type == V4L2_FRMSIZE_TYPE_DISCRETE)
            {
               v4l2_frmivalenum frate{
                  .index = 0,
                  .pixel_format = fmt.pixelformat,
                  .width = fs.discrete.width,
                  .height = fs.discrete.height
               };
               while (ioctl(fd, VIDIOC_ENUM_FRAMEINTERVALS, &frate) == 0)
               {
                  ++frate.index;
                  if (frate.type == V4L2_FRMIVAL_TYPE_DISCRETE)
                  {
                     union {uint32_t i; char c[4];} fourcc{fmt.pixelformat};
                     std::cout << "   " << fourcc.c[0] << fourcc.c[1] << fourcc.c[2] << fourcc.c[3]
                               << ": "  << fmt.description
                               << ' '   << fs.discrete.width << 'x' << fs.discrete.height
                               << " @"  << (float)frate.discrete.denominator / frate.discrete.numerator << "hz" <<  '\n';

                     // We want the highest resolution, then the highest framerate.
                     // Don't consider capture formats higher than 1080p
                     if (fs.discrete.height > 1080)
                        continue;
                     if (fs.discrete.width < info.format.width)
                        continue;
                     if (fs.discrete.height < info.format.height)
                        continue;
                     if ((float)frate.discrete.denominator / frate.discrete.numerator <
                         (float)info.format.rate.den / info.format.rate.num)
                        continue;
                     
                     info.format.pixel_format = fmt.pixelformat;
                     info.format.width = fs.discrete.width;
                     info.format.height = fs.discrete.height;
                     info.format.rate.num = frate.discrete.numerator;
                     info.format.rate.den = frate.discrete.denominator;
                  }
               }
            }
         }
      }

      // Ignore devices that don't have a supported pixel format
      if (info.format.pixel_format == 0)
      {
         close(fd);
         continue;
      }

      union {uint32_t i; char c[4];} fourcc{info.format.pixel_format};
      SDL_Log("   selected %c%c%c%c %dx%d @%fhz",
         fourcc.c[0], fourcc.c[1], fourcc.c[2], fourcc.c[3],
         info.format.width,
         info.format.height,
         (float)info.format.rate.den / info.format.rate.num
      );

      m_devices.push_back(info);
      
      close(fd);
   }
}

VideoCaptureV4L2::~VideoCaptureV4L2()
{
   Close();
}

bool VideoCaptureV4L2::Open(size_t idx)
{
   assert(idx < m_devices.size());
   if (idx >= m_devices.size())
      return false;

   auto& d = m_devices[idx];
   m_dev_idx = idx;

   if ((m_fd = open(d.path.c_str(), O_RDWR|O_NONBLOCK)) < 0)
      return false;

   v4l2_format fmt = {
      .type = V4L2_BUF_TYPE_VIDEO_CAPTURE,
      .fmt = {
         .pix = {
            .width = d.format.width,
            .height = d.format.height,
            .pixelformat = d.format.pixel_format,
         }
      }
   };
   if (ioctl(m_fd, VIDIOC_S_FMT, &fmt) < 0)
   {
      std::cout << "Unable to set requested format\n";
      Close();
      return false;
   }

   m_pitch = fmt.fmt.pix.bytesperline;

   // m_rgb_stride = (fmt.fmt.pix.width * 3 + 7) / 8 * 8;
   // m_rgb_buffer.resize(m_rgb_stride * fmt.fmt.pix.height);

   // TODO: How to set rate? Do we care?

   v4l2_requestbuffers rb{
      .count = 3,
      .type = V4L2_BUF_TYPE_VIDEO_CAPTURE,
      .memory = V4L2_MEMORY_MMAP
   };
   if (ioctl(m_fd, VIDIOC_REQBUFS, &rb) < 0)
   {
      std::cout << "Unable to request buffers\n";
      Close();
      return false;
   }
   
   // rb.count could have been modifed by the driver.
   for (uint32_t i = 0; i < rb.count; ++i)
   {
      v4l2_buffer buf{
         .index = i,
         .type = rb.type
      };
      if (ioctl(m_fd, VIDIOC_QUERYBUF, &buf) < 0)
      {
         std::cout << "Unable to query requested buffer\n";
         Close();
         return false;
      }
      
      void* m = mmap(nullptr, buf.length, PROT_READ|PROT_WRITE, MAP_SHARED, m_fd, buf.m.offset);
      if (m == MAP_FAILED)
      {
         std::cout << "Unable to mmap shared buffer\n";
         Close();
         return false;
      }
      m_buffers.emplace_back(BufferInfo{m, buf.length});

      if (ioctl(m_fd, VIDIOC_QBUF, &buf) < 0)
      {
         std::cout << "Unable to enqueue buffer\n";
         Close();
         return false;
      }
   }

   int on = V4L2_BUF_TYPE_VIDEO_CAPTURE;
   if (ioctl(m_fd, VIDIOC_STREAMON, &on) < 0)
   {
      std::cout << "Unable to start streaming\n";
      Close();
      return false;
   }

   return true;
}

void VideoCaptureV4L2::Close()
{
   int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
   ioctl(m_fd, VIDIOC_STREAMOFF, &type);
   
   for (auto& b: m_buffers)
   {
      munmap(b.p, b.length);
   }
   m_buffers.clear();

   if (m_fd >= 0)
      close(m_fd);
   m_fd = -1;
}

void* VideoCaptureV4L2::GetFrame()
{
   assert(m_fd >= 0);
   if (m_fd < 0)
      return nullptr;



   fd_set fds;
   FD_ZERO(&fds);
   FD_SET(m_fd, &fds);
   struct timeval timeout { .tv_usec = 0 };
   if (1 != select(m_fd + 1, &fds, nullptr, nullptr, &timeout))
      return nullptr;

   // Request the new buffer, and return the old one.
   struct v4l2_buffer buffer{
      .type = V4L2_BUF_TYPE_VIDEO_CAPTURE,
      .memory = V4L2_MEMORY_MMAP
   };
   if (ioctl(m_fd, VIDIOC_DQBUF, &buffer) < 0)
      return nullptr;

   if (m_current_buffer >= 0)
   {
      struct v4l2_buffer old_buffer{
         .index = (uint32_t)m_current_buffer,
         .type = V4L2_BUF_TYPE_VIDEO_CAPTURE,
         .memory = V4L2_MEMORY_MMAP,
      };
      ioctl(m_fd, VIDIOC_QBUF, &buffer);
   }


   m_current_buffer = buffer.index;
   // TODO: Is it bad to assume that m_stride is Width() * 2 for YUYV data?
   assert(m_pitch == Width() * 2);
   // YUYV_to_RGB24(m_buffers[buffer.index].p, m_rgb_buffer.data(), m_rgb_stride, Height());

   // ioctl(m_fd, VIDIOC_QBUF, &buffer);
   // return m_rgb_buffer.data();

   return m_buffers[buffer.index].p;
}
