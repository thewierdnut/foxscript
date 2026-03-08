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
#include "Text.hh"

#include "res/res.hh"
#include "picopng.h"

#include <SDL2/SDL.h>
#include <stdexcept>


Text::Text(const std::shared_ptr<SDL_Renderer>& r, const std::string& name)
{
   auto res = GetResource(name);
   unsigned long width;
   unsigned long height;
   std::vector<unsigned char> image;
   if (decodePNG(image, width, height, res.first, res.second))
   {
      SDL_Log("Unable to load font texture");
      throw std::runtime_error("Unable to load font texture");
   }
   m_font_texture.reset(
      SDL_CreateTexture(
         r.get(),
         SDL_PIXELFORMAT_RGBA32,
         SDL_TEXTUREACCESS_STATIC,
         width,
         height
      ),
      SDL_DestroyTexture
   );

   SDL_SetTextureBlendMode(m_font_texture.get(), SDL_BLENDMODE_BLEND);
   SDL_UpdateTexture(m_font_texture.get(), NULL, image.data(), width * sizeof(uint32_t));

   // 95 printable characters (from ' ' to '~')
   m_stride = width / (95);
   m_height = height;
}

Text::~Text()
{
   // Stub. Leave here so shared_ptr destructors get compiled here.
}

int Text::Render(const std::shared_ptr<SDL_Renderer>& r, const SDL_Point& pos, const std::string& s)
{
   SDL_Rect src{
      .x = 0,
      .y = 0,
      .w = m_stride,
      .h = m_height
   };
   SDL_Rect dst{
      .x = pos.x,
      .y = pos.y,
      .w = m_stride,
      .h = m_height
   };
   
   for (char c: s)
   {
      if (c > 32 && c < 127)
      {
         src.x = (c - 32u) * m_stride;
         SDL_RenderCopy(r.get(), m_font_texture.get(), &src, &dst);
      }
      dst.x += m_stride;
   }
   return m_stride * s.size();
}
