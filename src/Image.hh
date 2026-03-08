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
