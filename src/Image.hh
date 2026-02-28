#pragma once

#include <string>
#include <vector>
#include <cstdint>

// Mostly just for debug purposes, load an image from resources as yuv
class Image final
{
public:
   Image(int width, int height);
   
   bool Load(const std::string& path);

   int Width() const { return m_width; }
   int Height() const { return m_height; }
   int Pitch() const { return m_width * 2; }

   // Always yuyv
   uint8_t* Data() { return m_yuv_data.data(); }

private:
   std::vector<uint8_t> m_yuv_data;

   const int m_width = 0;
   const int m_height = 0;
};