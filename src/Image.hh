#pragma once

#include <memory>
#include <string>
#include <vector>
#include <cstdint>

class VideoCapture;

// Mostly just for debug purposes, load an image from resources as yuv
class Image final
{
public:
   Image();
   
   bool Load(const std::string& path);
   bool Load(std::vector<uint8_t>& d, int width, int height);

   int Width() const { return m_width; }
   int Height() const { return m_height; }
   int Pitch() const { return m_width * 2; }

   // Always yuyv
   uint8_t* Data() { return m_yuv_data.data(); }

private:
   std::vector<uint8_t> m_yuv_data;

   int m_width = 0;
   int m_height = 0;
};