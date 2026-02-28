#pragma once


#include "res/res.hh"
#include "picopng.h"

#include <functional>
#include <memory>
#include <stdexcept>
#include <string>

#include <SDL2/SDL.h>

class Button
{
public:
   Button(const SDL_Rect& pos, const std::string& icon, std::function<void()> cb):
      m_pos(pos),
      m_icon_name(icon),
      m_cb(cb)
   {}

   void Create(const std::shared_ptr<SDL_Renderer>& r);

   void Enable(bool e = true) { m_enabled = e; }
   void Disable() { Enable(false); }

   void SetPosition(const SDL_Point& p) { m_pos.x = p.x; m_pos.y = p.y; }
   void SetPosition(const SDL_Rect& p) { m_pos = p; }
   SDL_Rect Position() const { return m_pos; }

   bool MouseDown(const SDL_Point& e, int button)
   {
      if (!m_enabled)
         return false;
      if (button != SDL_BUTTON_LEFT)
         return false;
      if (m_pos.x <= e.x && e.x < m_pos.x + m_pos.w &&
          m_pos.y <= e.y && e.y < m_pos.y + m_pos.y)
      {
         m_mousedown = button;
         return true;
      }
      return false;
   }
   bool MouseUp(const SDL_Point& e, int button)
   {
      if (button == m_mousedown)
      {
         m_mousedown = 0;
         if (m_pos.x <= e.x && e.x < m_pos.x + m_pos.w &&
          m_pos.y <= e.y && e.y < m_pos.y + m_pos.y)
         {
            m_cb();
         }
         return true;
      }
      return false;
   }
   bool MouseMotion(const SDL_Point&, int)
   {
      return m_mousedown != 0;
   }


   void Draw(const std::shared_ptr<SDL_Renderer>& r)
   {
      if (!m_enabled)
         return;
      if (!m_icon)
      {
         auto data = GetResource(m_icon_name);
         std::vector<uint8_t> icon_data;
         unsigned long width, height;
         if (decodePNG(icon_data, width, height, data.first, data.second))
         {
            throw std::runtime_error("Unable to load " + m_icon_name);
         }

         m_icon.reset(
            SDL_CreateTexture(
               r.get(),
               SDL_PIXELFORMAT_RGBA32,
               SDL_TEXTUREACCESS_STATIC,
               width,
               height
            ),
            SDL_DestroyTexture
         );

         SDL_SetTextureBlendMode(m_icon.get(), SDL_BLENDMODE_BLEND);
         SDL_UpdateTexture(m_icon.get(), NULL, icon_data.data(), width * sizeof(uint32_t));
      }

      SDL_RenderCopy(r.get(), m_icon.get(), NULL, &m_pos);
   }

private:
   SDL_Rect m_pos{};
   const std::string m_icon_name;
   std::shared_ptr<SDL_Texture> m_icon;

   bool m_enabled = true;
   int m_mousedown = 0;

   std::function<void()> m_cb;
};