#pragma once

#include "Button.hh"
#include "ZoomGesture.hh"

#include <SDL2/SDL.h>

struct SDL_Window;
struct SDL_Renderer;
struct SDL_Texture;

class Text;

class MainWindow final
{
public:
   MainWindow();
   ~MainWindow();

   bool Create(size_t frame_width, size_t frame_height);

   void UpdateTexture(void* p, int pitch);
   void Draw();
   void Resize(int new_width, int new_height);
   void Translate(int dx, int dy);

   bool Paused() const { return m_paused; }

   bool MouseDown(const SDL_Point& p, int button);
   bool MouseUp(const SDL_Point& p, int button);
   bool MouseMotion(const SDL_Point& p, int button);
   bool MouseWheel(const SDL_Point& p, int s);
   bool FingerDown(const SDL_Point& p, int fid);
   bool FingerUp(const SDL_Point& p, int fid);
   bool FingerMotion(const SDL_Point& p, const SDL_Point& dp, int fid);

   const SDL_Point& Size() const { return m_window_size; }
   

   void Stamp()
   {
      // m_debug_stamp = true;
      // Glyph g {
      //    .x = m_window_size.x / 4,
      //    .y = m_window_size.y / 2,
      //    .height = m_initial_height,
      //    .stride = m_initial_stride,
      // };
      // SampleGlyph(g);
      // m_debug_stamp = false;
   }

protected:
   struct Glyph
   {
      uint16_t g;
      int x;
      int y;

      int height;
      int stride;
   };

   // x and y are in screen coordinates
   void RenderGlyph(const Glyph& g, uint8_t cr = 0, uint8_t cg = 255, uint8_t cb = 0);

   int ScreenToImg(int x) { return x * m_scale.den / m_scale.num; }
   int ImgToScreen(int x) { return x * m_scale.num / m_scale.den; }

   // Caller fills out the glyph position and size, and then this function
   // sets the glyph. The return value is how many sample points matched the
   // glyph, so the higher the better, but zero indicates no match.
   size_t SampleGlyph(Glyph& g);

   // @param x, y in screen coordinates
   // @param has_space give the horizontal coordinate more leeway since its a
   //        new word
   // Try and find multiple glyphs in a row (ie a "word")
   size_t ScanForGlyphSequence(int x, int y, std::vector<Glyph>& retglyphs);

   // Vary the stride and the height to try and find the most number of line
   // segments possible
   void ScanForGlyphs();

   void Pause();
   void Play();
   void Zoom(const SDL_Point& pos, float f);

   uint8_t& Pixel(int x, int y)
   {
      return m_yuv_data[y * m_frame_pitch + x * 2];
   };

private:
   std::shared_ptr<SDL_Window> m_window;
   std::shared_ptr<SDL_Renderer> m_renderer;
   std::shared_ptr<SDL_Texture> m_texture;
   std::shared_ptr<SDL_Texture> m_circle_texture;

   SDL_Rect m_dst_rect{};
   SDL_Point m_window_size{};
   int m_initial_height = 64;
   int m_initial_stride = 38;
   int m_frame_width = 640;
   int m_frame_height = 480;
   int m_frame_pitch = 480;
   int m_estimated_height = 64;
   int m_estimated_stride = 38;

   float m_zoom = 1;
   struct {int num, den; } m_scale{1,1};     // rational integer version of m_zoom for critical path

   std::vector<Glyph> m_detected_glyphs;
   Glyph m_bad_glyph{};
   std::vector<SDL_Rect> m_word_rects;

   bool m_debug_stamp = false;

   struct Params
   {
      int dx;     /// x delta from target position.
      int dy;     /// y delta from target position.
      int ratio;  /// The fraction out of 128 the width is from the height.
      int scale;  /// The fraction out of 128 the height varies from the initial guess.
   };
   static constexpr int PARAM_DEN = 128; // Keep this at a power of two for fast math.
   std::vector<Params> m_guess_params;
   
   std::shared_ptr<Text> m_font;

   // cv::Mat m_data;
   uint8_t* m_yuv_data = nullptr;

   Button m_pause;
   Button m_play;
   bool m_paused = false;

   ZoomGesture zg;
};