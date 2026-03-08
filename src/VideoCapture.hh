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
