#include "Image.hh"

#include "res/res.hh"
#include "picopng.h"

#include <SDL2/SDL.h>

Image::Image(int width, int height):
   m_yuv_data(width * height * 2, 0x80),
   m_width(width),
   m_height(height)
{
}


bool Image::Load(const std::string& path)
{
   auto res = GetResource(path);
   if (!res.first)
   {
      SDL_Log("Unable to find %s", path.c_str());
      return false;
   }

   std::vector<uint8_t> rgba_data;
   unsigned long width = 0;
   unsigned long height = 0;
   if (decodePNG(rgba_data, width, height, res.first, res.second) != 0)
   {
      SDL_Log("Unable to decode %s", path.c_str());
      return false;
   }

   auto target_pixel = [&](int x, int y) -> uint8_t& {
      return m_yuv_data[y * m_width * 2 + x * 2];
   };

   auto src_pixel = [&](int x, int y) -> uint8_t {
      uint8_t* p = &rgba_data[y * width * 4 + x * 4];
      // Not an accurate intensity transformation. Don't care.
      return ((unsigned)p[0] + p[1] + p[2]) / 3u;
   };

   // Chop off the last column if the width is not divisible by two.
   unsigned long pitch = (m_width & ~1ul) * 2;

   // Convert RGB to yuyv grayscale, and scale the image to the expected size
   // using nearest-neighbor.
   for (int y = 0; y < m_height; ++y)
   {
      
      int rgby = y * height / m_height;
      for (int x = 0; x < m_width; ++x)
      {
         int rgbx = x * width / m_width;
         target_pixel(x, y) = src_pixel(rgbx, rgby);
      }
   }
   return true;
}