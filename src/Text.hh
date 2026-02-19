#pragma once

#include <memory>

struct SDL_Renderer;
struct SDL_Texture;
struct SDL_Point;

class Text final
{
public:
   Text(const std::shared_ptr<SDL_Renderer>& r, const std::string& name);
   ~Text();

   int Render(const std::shared_ptr<SDL_Renderer>& r, const SDL_Point& pos, const std::string& s);

   int Height() const { return m_height; }
   int Stride() const { return m_stride; }

private:
   std::shared_ptr<SDL_Texture> m_font_texture;

   int m_stride = 0;
   int m_height = 0;
};