#include <SDL2/SDL.h>
#include <opencv2/opencv.hpp>
#include <iostream>

#include "TscSampler.hh"
#include "src/Text.hh"

constexpr size_t WIDTH = 640;
constexpr size_t HEIGHT = 480;
constexpr int LINE_THICKNESS = 5;

// Initial guess at the size of the runes.
constexpr int INITIAL_STRIDE = 38;
constexpr int INITIAL_HEIGHT = 64;

// How much to vary the size and height to find them.
constexpr int STRIDE_VARIANCE = 12;
constexpr int HEIGHT_VARIANCE = 20;

// Special glyph value representing a space
constexpr uint16_t GLYPH_SPACE = 0xffff;

struct SegmentInfo
{
   struct { int x, y; } p1;
   struct { int x, y; } p2;
};
// A description of each line segment of a rune. There are six along the
// outside representing vowel sounds, and six on the inside representing
// consonant sounds. These are given in arbitrary units where 10 represents
// the maximum height or width of the glyph, with (0, 0) being the center.
constexpr int GLYPH_SCALE = 100;
constexpr SegmentInfo SEGMENTS[12] = {
   {{ 50,-30}, { 00,-45}}, // Vowels, Outside edge, counterclockwise from top right.
   {{ 00,-45}, {-50,-30}},
   {{-50,-30}, {-50,-10}},
   {{-50, 20}, {-50, 40}},
   {{-50, 40}, { 00, 55}},
   {{ 00, 55}, { 50, 40}},
   {{ 00,-15}, { 50,-30}}, // Consonants, inside, counterclockwise from top right.
   {{ 00,-15}, { 00,-45}},
   {{ 00,-15}, {-50,-30}},
   {{ 00, 25}, {-50, 40}},
   {{ 00, 25}, { 00, 55}},
   {{ 00, 25}, { 50, 40}},
};

const char* VOWELS[64] = { // 6 bits, but only 18 values
//       000   001   010   011   100   101   110   111
/*000*/   "", "ie", "ay", "uh",   "",   "",   "",   "",
/*001*/   "",   "",   "",   "",   "",   "", "ah",  "a",
/*010*/ "oi",   "",   "",   "",   "",   "",   "",   "",
/*011*/   "",   "",   "",   "", "ou",   "",   "", "oo",
/*100*/ "ow",   "",   "",   "",   "",   "",   "",   "",
/*101*/   "",   "",   "",   "","ere",   "","eer", "ore",
/*110*/  "i",   "",   "", "ar",   "",   "",   "",   "",
/*111*/   "",   "",   "",   "", "eh", "ir",  "ee","oa",
// Minor irritations:
//    011100 is a ʊ, as in foot or good. There isn't a great way to spell
//           that reliably in english
//    000011 is a ə, as in the or mud, which is represented by any vowel in
//           english.
//    111111 is oʊ, as in no, or toe. Also not a great way to reliably repesent
//           in english
};

const char* CONSONANTS[64] = { // 6 bits, 24 values
//       000   001   010   011   100   101   110   111
/*000*/   "",   "",   "",   "",   "",  "w",   "",   "",
/*001*/   "",   "",  "j",   "",   "",   "",   "",   "",
/*010*/   "",  "p",  "l",  "r", "ch",  "t",  "y", "th",
/*011*/   "",  "f",   "",  "s",   "",   "",   "",   "",
/*100*/   "",   "",  "b",  "k",   "",   "",  "v",   "",
/*101*/  "m",   "",  "d",   "",  "n",   "",   "", "zh",
/*110*/   "",  "g",  "h",   "",   "",   "",  "z",   "",
/*111*/   "",   "", "th",   "",   "", "sh",   "", "ng",
// Minor irritations:
//    111111 could be nk or ng. its the n sound made by the back of the tongue
//    010111 and 111010 are th in thick and this, respectively.
};

// TODO: dictionary to clean up common words?

// A circle at the bottom is present if the vowel comes first.
constexpr struct {int x, y;} VOWEL_FIRST = {0, -6};

class SdlWindow
{
public:
   SdlWindow()
   {
      // Prepopulate the parameters we vary to find the glyphs, since they
      // should be sorted by how "abnormal" they are.
      for (int dx = -6; dx <= 6; dx += 2)
      {
         for (int dy = -6; dy <= 6; dy += 2)
         {
            for (int stride = INITIAL_STRIDE - STRIDE_VARIANCE; stride < INITIAL_STRIDE + STRIDE_VARIANCE; stride += 2)
            {
               for (int height = INITIAL_HEIGHT - HEIGHT_VARIANCE; height < INITIAL_HEIGHT + HEIGHT_VARIANCE; height += 2)
               {
                  m_guess_params.emplace_back(dx, dy, stride, height);
               }
            }
         }
      }
      std::sort(m_guess_params.begin(), m_guess_params.end(),
         [](const cv::Vec4i& a, const cv::Vec4i& b) {
            return abs(a[0]) + abs(a[1]) + abs(a[2] - INITIAL_STRIDE) + abs(a[3] - INITIAL_HEIGHT)
                 < abs(b[0]) + abs(b[1]) + abs(b[2] - INITIAL_STRIDE) + abs(b[3] - INITIAL_HEIGHT);
         }
      );
   }
   bool Create(size_t frame_width, size_t frame_height)
   {
      m_frame_width = frame_width;
      m_frame_height = frame_height;
      Resize(WIDTH, HEIGHT);


      // Initialize SDL
      if (SDL_Init(SDL_INIT_VIDEO) < 0) {
         std::cerr << "SDL could not initialize! SDL_Error: " << SDL_GetError() << std::endl;
         return false;
      }

      m_window.reset(SDL_CreateWindow("OpenCV + SDL2 Stream",
                                  SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                  WIDTH, HEIGHT, SDL_WINDOW_SHOWN|SDL_WINDOW_RESIZABLE),
                     SDL_DestroyWindow);
      if (!m_window)
         return false;

      m_renderer.reset(SDL_CreateRenderer(m_window.get(), -1, SDL_RENDERER_ACCELERATED),
                       SDL_DestroyRenderer);
      if (!m_renderer)
         return false;

      SDL_SetRenderDrawBlendMode(m_renderer.get(), SDL_BLENDMODE_BLEND);

      // Create a placeholder texture for the camera frames
      m_texture.reset(SDL_CreateTexture(m_renderer.get(), SDL_PIXELFORMAT_BGR24,
                                    SDL_TEXTUREACCESS_STREAMING, frame_width, frame_height),
                      SDL_DestroyTexture);

      m_font.reset(new Text(m_renderer, "text_24.png"));

      return true;
   }
   ~SdlWindow()
   {
      SDL_Quit();
   }

   operator SDL_Window*() { return m_window.get(); }

   void Updatetexture(cv::Mat& frame)
   {
      m_data = frame;

      // Update texture with OpenCV frame data
      // TODO: gemini lied to me. We should use SDL_LockTexture here.
      SDL_UpdateTexture(m_texture.get(), NULL, frame.data, frame.step);

      cv::cvtColor(frame, m_data, cv::COLOR_BGR2GRAY);
      cv::medianBlur(m_data, m_data, 5);
      cv::threshold(m_data, m_data, 64, 255, cv::THRESH_BINARY_INV);

      // Words are joined together, usually with the horizontal centerline,
      // but not always. Try to collect and save off rects representing each
      // word, so that we know to insert a space if we leave it.
      std::vector<std::vector<cv::Point>> contours;
      m_word_rects.clear();
      cv::findContours(m_data, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_NONE);
      for (auto& c: contours)
      {
         int x1 = 100000;
         int y1 = 100000;
         int x2 = -1;
         int y2 = -1;
         for (auto& p: c)
         {
            if (p.x < x1) x1 = p.x;
            if (p.y < y1) y1 = p.y;
            if (p.x > x2) x2 = p.x;
            if (p.y > y2) y2 = p.y;
         }
         m_word_rects.push_back(SDL_Rect{x1 - 2, y1 - 2, x2 - x1 + 4, y2 - y1 + 4});
      }

      ScanForGlyphs();
   }

   void Draw()
   {
      // Render
      SDL_SetRenderDrawColor(m_renderer.get(), 0, 0, 0, 255);
      SDL_RenderClear(m_renderer.get());

      auto r = m_dst_rect;
      r.x += m_dx;
      r.y += m_dy;

      // cv::Mat color;
      // cv::cvtColor(m_data, color, cv::COLOR_GRAY2BGR);
      // SDL_UpdateTexture(m_texture.get(), NULL, color.data, color.step);

      SDL_RenderCopy(m_renderer.get(), m_texture.get(), NULL, &r);

      // // Debug, drawing shape rects
      // SDL_SetRenderDrawColor(m_renderer.get(), 0, 0, 255, 128);
      // for (const SDL_Rect& r: m_word_rects)
      // {
      //    // These are in image coordinates.
      //    SDL_Rect rs{
      //       .x = ImgToScreen(r.x) + m_dx,
      //       .y = ImgToScreen(r.y) + m_dy,
      //       .w = ImgToScreen(r.w),
      //       .h = ImgToScreen(r.h)
      //    };
      //    SDL_RenderFillRect(m_renderer.get(), &rs);
      // }

      // Horizontal baseline;
      SDL_SetRenderDrawColor(m_renderer.get(), 255, 0, 0, 64);
      SDL_Rect horzline = m_dst_rect;
      horzline.y = horzline.h / 2 - LINE_THICKNESS/2;
      horzline.h = LINE_THICKNESS;
      SDL_RenderFillRect(m_renderer.get(), &horzline);

      // Vertial alignment marker
      SDL_Rect vertline{
         .x = horzline.w / 4 - LINE_THICKNESS/2,
         .y = horzline.y - m_estimated_height / 2,
         .w = LINE_THICKNESS,
         .h = m_estimated_height + LINE_THICKNESS
      };
      SDL_RenderFillRect(m_renderer.get(), &vertline);

      // Glyph overlay and translated text
      if (!m_detected_glyphs.empty())
      {
         SDL_Point text_pos{
            .y = horzline.y + INITIAL_HEIGHT
         };

         SDL_Rect text_background{
            .x = 0,
            .y = text_pos.y - m_font->Height() / 2,
            .w = horzline.w,
            .h = m_font->Height() * 2
         };
         SDL_SetRenderDrawColor(m_renderer.get(), 0, 0, 0, 64);
         SDL_RenderFillRect(m_renderer.get(), &text_background);

         
         bool space = true;
      
         for (auto& g: m_detected_glyphs)
         {
            RenderGlyph(g);

            if (space)
            {
               if (text_pos.x + m_font->Stride() > g.x - INITIAL_STRIDE/2)
               {
                  text_pos.x += m_font->Stride();
               }
               else
               {
                  text_pos.x = g.x - INITIAL_STRIDE/2;
               }
               space = false;
            }
            
            const char* vowel = VOWELS[g.g & 63];
            const char* consonant = CONSONANTS[(g.g >> 6) & 63];
            bool swapped = (g.g & (1 << 12)) != 0;
            if (g.g == GLYPH_SPACE)
            {
               space = true;
            }
            else if (swapped)
            {
               text_pos.x += m_font->Render(m_renderer, text_pos, vowel);
               text_pos.x += m_font->Render(m_renderer, text_pos, consonant);
            }
            else
            {
               text_pos.x += m_font->Render(m_renderer, text_pos, consonant);
               text_pos.x += m_font->Render(m_renderer, text_pos, vowel);
            }
         }
      }

      // If we saw a glyph that was invalid, chances are we detected part of a
      // good one. Show it so that the user knows they are close to matching.
      if (m_bad_glyph.g)
      {
         RenderGlyph(m_bad_glyph, 255, 0, 0);
      }

      // m_font->Render(m_renderer, {50, 50}, "!\"#{|}~");

      SDL_RenderPresent(m_renderer.get());
   }

   void Resize(int new_width, int new_height)
   {
      // Scale the target rect for the video so that it always fits and stays
      // the right aspect ratio.
      m_dst_rect.w = new_width;
      m_dst_rect.h = new_width * m_frame_height / m_frame_width;
      scale.num = new_width;
      scale.den = m_frame_width;
      if (m_dst_rect.h > new_height)
      {
         m_dst_rect.h = new_height;
         m_dst_rect.w = new_height * m_frame_width / m_frame_height;
         scale.num = new_height;
         scale.den = m_frame_height;
      }

      ScanForGlyphs();
   }

   void Translate(int dx, int dy)
   {
      m_dx += dx;
      m_dy += dy;
      ScanForGlyphs();
   }

   void Stamp()
   {
      m_debug_stamp = true;
      for (auto& g: m_detected_glyphs)
      {
         if (g.g != GLYPH_SPACE)
            SampleGlyph(g.x, g.y, g.stride, g.height);
      }

      m_debug_stamp = false;
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
   void RenderGlyph(const Glyph& g, uint8_t cr = 0, uint8_t cg = 255, uint8_t cb = 0)
   {
      if (g.g == GLYPH_SPACE)
      {
         // SDL_SetRenderDrawColor(m_renderer.get(), 0, 0, 255, 128);
         // SDL_Rect r{
         //    .x = x, .y = y - LINE_THICKNESS / 2,
         //    .w = m_estimated_stride * 6/10, .h = LINE_THICKNESS
         // };
         // SDL_RenderFillRect(m_renderer.get(), &r);

         SDL_SetRenderDrawColor(m_renderer.get(), 255, 0, 0, 64);
         SDL_Rect r2{
            .x = g.x - LINE_THICKNESS/2,
            .y = g.y - m_estimated_height / 2,
            .w = LINE_THICKNESS,
            .h = m_estimated_height
         };
         SDL_RenderFillRect(m_renderer.get(), &r2);

         return;
      }
      SDL_SetRenderDrawColor(m_renderer.get(), cr, cg, cb, 255);

      // bits 0 through 5 are the outside vowel
      // bits 6 through 11 are the inside consonant
      // bit 12 is the vowel order indicator
      for (uint16_t bit = 0; bit < 12; ++bit)
      {
         if (g.g & (1 << bit))
         {
            auto s = SEGMENTS[bit];
            // Scale to the estimated glyph size and translate to the coords.
            s.p1.x = g.x + s.p1.x * m_estimated_stride / GLYPH_SCALE;
            s.p1.y = g.y + s.p1.y * m_estimated_height / GLYPH_SCALE;
            s.p2.x = g.x + s.p2.x * m_estimated_stride / GLYPH_SCALE;
            s.p2.y = g.y + s.p2.y * m_estimated_height / GLYPH_SCALE;
            // Don't have a line thickness control, so just draw it multiple
            // times to fake it.
            for (int i = 0; i < LINE_THICKNESS; ++i)
            {
               SDL_RenderDrawLine(m_renderer.get(), s.p1.x + i - LINE_THICKNESS / 2, s.p1.y, s.p2.x + i - LINE_THICKNESS / 2, s.p2.y);
            }
         }
      }
      if (g.g & (1 << 12))
      {
         // This is supposed to be a circle, but SDL has no primitive to do
         // that. Probably need to manually fill a texture, then blit it here.
         SDL_Rect rect{
            .x = g.x + VOWEL_FIRST.x * m_estimated_stride / GLYPH_SCALE,
            .y = g.y + VOWEL_FIRST.y * m_estimated_height / GLYPH_SCALE,
            .w = LINE_THICKNESS,
            .h = LINE_THICKNESS,
         };

         SDL_RenderFillRect(m_renderer.get(), &rect);
      }

   }

   int ScreenToImg(int x)
   {
      return x * scale.den / scale.num;
   }

   int ImgToScreen(int x)
   {
      return x * scale.num / scale.den;
   }

   // x, y, stride, height given in screen units.
   // @returns glyph mask, number of sampled black pixels.
   std::pair<uint16_t, size_t> SampleGlyph(int x, int y, int stride, int height)
   {
      //TscSampler ts("sg");
      //uint16_t accuracy = 0;
      uint16_t glyph = 0;
      int px = ScreenToImg(x - m_dx);
      int py = ScreenToImg(y - m_dy);

      int p_stride = ScreenToImg(stride);
      int p_height = ScreenToImg(height);

      // Don't read off the edge of the picture.
      if (p_stride * 2 > x || x > m_data.cols - p_stride * 2 ||
          p_height * 2 > y || y > m_data.rows - p_height * 2)
         return {0, 0};
      
      // For each segment position, sample three pixels and 30, 50, and 70%
      // of the line segment. If they are all dark, then assume the line
      // segment is drawn.
      // TODO: white on black vs black on white?
      size_t black_pixels = 0;
      for (uint16_t i = 0; i < 12; ++i)
      {
         auto s = SEGMENTS[i];
         s.p1.x = px + s.p1.x * p_stride / GLYPH_SCALE;
         s.p1.y = py + s.p1.y * p_height / GLYPH_SCALE;
         s.p2.x = px + s.p2.x * p_stride / GLYPH_SCALE;
         s.p2.y = py + s.p2.y * p_height / GLYPH_SCALE;
         
         // int s1x = (s.p1.x * 2 + s.p2.x * 8) / 10; // 20%
         // int s1y = (s.p1.y * 2 + s.p2.y * 8) / 10;
         // int s2x = (s.p1.x + s.p2.x) / 2;          // 50%
         // int s2y = (s.p1.y + s.p2.y) / 2;
         // int s3x = (s.p1.x * 8 + s.p2.x * 2) / 10; // 80%
         // int s3y = (s.p1.y * 8 + s.p2.y * 2) / 10;
         // Line iterator, fairly accurate, 2116
         // cv::LineIterator it(m_data, {s1x, s1y}, {s3x, s3y});
         // uint32_t l = 0;
         // uint32_t sample_points = it.count;
         // for (int i = 0; i < it.count; ++i, ++it)
         // {
         //    // cv::Vec3b& pixel = m_data.at<cv::Vec3b>(it.pos());
         //    // if ((int)pixel.val[0] + pixel.val[1] + pixel.val[2] < 96)
         //    //    ++l;
         //    // if (m_debug_stamp)
         //    // {
         //    //    pixel.val[0] = 255;
         //    //    pixel.val[1] = 255;
         //    //    pixel.val[2] = 255;
         //    // }
         //    if (m_data.at<uint8_t>(it.pos()) == 255)
         //       ++l;
         //
         //    if (m_debug_stamp)
         //       m_data.at<uint8_t>(it.pos()) = 100;
         // }

         // Three 3x3 spots, 27 pixels total. Not accurate, 601
         // int s1x = (s.p1.x * 2 + s.p2.x * 8) / 10; // 20%
         // int s1y = (s.p1.y * 2 + s.p2.y * 8) / 10;
         // int s2x = (s.p1.x + s.p2.x) / 2;          // 50%
         // int s2y = (s.p1.y + s.p2.y) / 2;
         // int s3x = (s.p1.x * 8 + s.p2.x * 2) / 10; // 80%
         // int s3y = (s.p1.y * 8 + s.p2.y * 2) / 10;
         // uint32_t l = 0;
         // uint32_t sample_points = 0;
         // for (int dx = -1; dx <= 1; ++dx)
         // {
         //    for (int dy = -1; dy <= 1; ++dy)
         //    {
         //       l += m_data.at<uint8_t>(s1y + dy, s1x + dx) == 255;
         //       ++sample_points;
         //    }
         // }
         // for (int dx = -1; dx <= 1; ++dx)
         // {
         //    for (int dy = -1; dy <= 1; ++dy)
         //    {
         //       l += m_data.at<uint8_t>(s1y + dy, s1x + dx) == 255;
         //       ++sample_points;
         //    }
         // }
         // for (int dx = -1; dx <= 1; ++dx)
         // {
         //    for (int dy = -1; dy <= 1; ++dy)
         //    {
         //       l += m_data.at<uint8_t>(s1y + dy, s1x + dx) == 255;
         //       ++sample_points;
         //    }
         // }

         // sample 7 points in a line, better than 3x3, worse than line, 555
         // sample 15 points in a line, 763
         // sample 7 points out of 15, 686
         // sample 7 out of 7, 686
         // no stamp check, 636
         // sample 3, 386. Seems accurate enough, kind of surprising?
         // sample 3 out of 15,  369. Lets try this for now.
         uint32_t l = 0;
         // const cv::Point sp[] = {
         //    // {(s.p1.x * 1 + s.p2.x *15) / 16, (s.p1.y * 1 + s.p2.y *15) / 16},
         //    {(s.p1.x * 2 + s.p2.x *14) / 16, (s.p1.y * 2 + s.p2.y *14) / 16},
         //    // {(s.p1.x * 3 + s.p2.x *13) / 16, (s.p1.y * 3 + s.p2.y *13) / 16},
         //    {(s.p1.x * 4 + s.p2.x *12) / 16, (s.p1.y * 4 + s.p2.y *12) / 16},
         //    // {(s.p1.x * 5 + s.p2.x *11) / 16, (s.p1.y * 5 + s.p2.y *11) / 16},
         //    {(s.p1.x * 6 + s.p2.x *10) / 16, (s.p1.y * 6 + s.p2.y *10) / 16},
         //    // {(s.p1.x * 7 + s.p2.x * 9) / 16, (s.p1.y * 7 + s.p2.y * 9) / 16},
         //    {(s.p1.x * 8 + s.p2.x * 8) / 16, (s.p1.y * 8 + s.p2.y * 8) / 16},
         //    // {(s.p1.x * 9 + s.p2.x * 7) / 16, (s.p1.y * 9 + s.p2.y * 7) / 16},
         //    {(s.p1.x *10 + s.p2.x * 6) / 16, (s.p1.y *10 + s.p2.y * 6) / 16},
         //    // {(s.p1.x *11 + s.p2.x * 5) / 16, (s.p1.y *11 + s.p2.y * 5) / 16},
         //    {(s.p1.x *12 + s.p2.x * 4) / 16, (s.p1.y *12 + s.p2.y * 4) / 16},
         //    // {(s.p1.x *13 + s.p2.x * 3) / 16, (s.p1.y *13 + s.p2.y * 3) / 16},
         //    {(s.p1.x *14 + s.p2.x * 2) / 16, (s.p1.y *14 + s.p2.y * 2) / 16},
         //    // {(s.p1.x *15 + s.p2.x * 1) / 16, (s.p1.y *15 + s.p2.y * 1) / 16},
         // };
         // const cv::Point sp[] = {
         //    {(s.p1.x * 1 + s.p2.x * 7) / 8, (s.p1.y * 1 + s.p2.y * 7) / 8},
         //    {(s.p1.x * 2 + s.p2.x * 6) / 8, (s.p1.y * 2 + s.p2.y * 6) / 8},
         //    {(s.p1.x * 3 + s.p2.x * 5) / 8, (s.p1.y * 3 + s.p2.y * 5) / 8},
         //    {(s.p1.x * 4 + s.p2.x * 4) / 8, (s.p1.y * 4 + s.p2.y * 4) / 8},
         //    {(s.p1.x * 5 + s.p2.x * 3) / 8, (s.p1.y * 5 + s.p2.y * 3) / 8},
         //    {(s.p1.x * 6 + s.p2.x * 2) / 8, (s.p1.y * 6 + s.p2.y * 2) / 8},
         //    {(s.p1.x * 7 + s.p2.x * 1) / 8, (s.p1.y * 7 + s.p2.y * 1) / 8},
         // };
         // const cv::Point sp[] = {
         //    {(s.p1.x * 1 + s.p2.x * 3) / 4, (s.p1.y * 1 + s.p2.y * 3) / 4},
         //    {(s.p1.x * 2 + s.p2.x * 2) / 4, (s.p1.y * 2 + s.p2.y * 2) / 4},
         //    {(s.p1.x * 3 + s.p2.x * 1) / 4, (s.p1.y * 3 + s.p2.y * 1) / 4},
         // };
         const cv::Point sp[] = {
            {(s.p1.x * 1 + s.p2.x *15) / 16, (s.p1.y * 1 + s.p2.y *15) / 16},
            {(s.p1.x * 8 + s.p2.x * 8) / 16, (s.p1.y * 8 + s.p2.y * 8) / 16},
            {(s.p1.x *15 + s.p2.x * 1) / 16, (s.p1.y *15 + s.p2.y * 1) / 16},
         };
         const uint32_t sample_points = sizeof(sp)/sizeof(*sp);
         for (auto& p: sp)
         {
            l += m_data.at<uint8_t>(p.y, p.x) == 255;
            // if (m_debug_stamp)
            //    m_data.at<uint8_t>(p.y, p.x) = 100;
         }

         // uint8_t l1 = luminance(s1x, s1y);
         // uint8_t l2 = luminance(s2x, s2y);
         // uint8_t l3 = luminance(s3x, s3y);
         // cv::line(m_data, {s.p1.x, s.p1.y}, {s.p2.x, s.p2.y}, {31, 0, 31}, 3);

         //if (l1 < 32 && l2 < 32 && l3 < 32)
         if (l == sample_points) //* 3 / 4)
         {
            black_pixels += l;
            glyph |= (1 << i);
         }
      }

      return {glyph, black_pixels};
   }

   // @param x, y in screen coordinates
   // @param has_space give the horizontal coordinate more leeway since its a
   //        new word
   // Try and find multiple glyphs in a row (ie a "word")
   size_t ScanForGlyphSequence(int x, int y, std::vector<Glyph>& retglyphs, bool has_space = false)
   {
      // Limit our guessing to the rect containing our word
      const int ix = ScreenToImg(x - m_dx);
      const int iy = ScreenToImg(y - m_dy);
      const int iheight = ScreenToImg(INITIAL_HEIGHT);
      const int iwidth = ScreenToImg(INITIAL_STRIDE);

      int word_left = -100000;
      int word_right = -100000;
      for (auto& r: m_word_rects)
      {
         // Extend the left edge slightly, and use a minimum height to make
         // sure that words like "I" (glyph 1) that are floating and
         // disconnected still can find the glyph center in the rect.
         if (ix > r.x - (iwidth / 16) && iy - (iheight / 16) > r.y &&
             ix < r.x + r.w + iwidth / 8 && iy < r.y + iheight + iheight / 8)
         {
            word_left = ImgToScreen(r.x - (iwidth / 16)) + m_dx;
            word_right = ImgToScreen(r.x + r.w + iwidth / 8) + m_dx;
            break;
         }
      }
      if (word_left == -100000)
         return 0; // Not inside any word rect.

      // baseline     19578586
      // stride inc 2 10093313
      // both inc 3    4875075  match more finicky
      // st 2 he 3     7253668  better, but not as good as 1
      // all 1       128061247  Very accurate and stable. And slow.
      // all 2         9835266  Usable on desktop.
      // TscSampler ts("sg");

      // for (int i = 0; i < 30; ++i)
      // {
      //    m_data.at<uint8_t>(ScreenToImg(y - m_dy) + i, ScreenToImg(word_left - m_dx)) = 200;
      //    m_data.at<uint8_t>(ScreenToImg(y - m_dy) + i, ScreenToImg(word_right - m_dx)) = 200;
      // }

      const int ylim = has_space ? 1 : 6;
      const int xlim = has_space ? 12 : 6;

      int gx=0, gy=0, gs=0, gh=0;

      size_t best_guess = 0;
      retglyphs.clear();
      // Some shapes, especially symmetrical or simple ones tend to match
      // extreme settings. We need to instead try all of these combinations
      // sorted by how close to the center they are.
      // for (int dx = -xlim; dx <= xlim; dx += 2)
      // {
      //    for (int dy = -ylim; dy <= ylim; ++dy)
      //    {
      //       for (int stride = INITIAL_STRIDE - STRIDE_VARIANCE; stride < INITIAL_STRIDE + STRIDE_VARIANCE; stride += 1)
      //       {
      //          for (int height = INITIAL_HEIGHT - HEIGHT_VARIANCE; height < INITIAL_HEIGHT + HEIGHT_VARIANCE; height += 2)
      std::vector<Glyph> glyphs;
      for (auto& v: m_guess_params)
      {
         int dx = v[0], dy = v[1], stride = v[2], height = v[3];
         glyphs.clear();
         Glyph g{};
         g.x = x + dx;
         g.y = y + dy;
         g.height = height;
         g.stride = stride;
         size_t quality = 0;
         size_t quality_total = 0;
         Glyph bad_glyph{};
         std::tie(g.g, quality) = SampleGlyph(g.x, g.y, stride, height);
         while(g.g && g.x < word_right)
         {
            const char* vowel = VOWELS[g.g & 63];
            const char* consonant = CONSONANTS[(g.g >> 6) & 63];
            // Not every combination of line segments is a valid glyph.
            if (!vowel[0] && !consonant[0])
            {
               bad_glyph = g;
               break;
            }
            quality_total += quality;
            glyphs.push_back(g);
            

            g.x += stride;
            
            std::tie(g.g, quality) = SampleGlyph(g.x, g.y, stride, height);
         }

         if (quality_total > best_guess)
         {
            best_guess = quality_total;
            retglyphs.swap(glyphs);
            m_bad_glyph = bad_glyph;
            m_estimated_height = height;
            m_estimated_stride = stride;

            gx = dx;
            gy = dy;
            gh = height;
            gs = stride;
         }
      }
      // SDL_Log("%d %d %d %d", gx, gy, gh, gs);
      return best_guess;
   }

   // Vary the stride and the height to try and find the most number of line
   // segments possible
   void ScanForGlyphs()
   {
      int x = m_dst_rect.w / 4;
      int y = m_dst_rect.h / 2;
      std::vector<Glyph> more_glyphs;
      m_detected_glyphs.clear();
      m_bad_glyph = Glyph{};
      size_t score = 0;
      size_t quality = ScanForGlyphSequence(x, y, more_glyphs);
      while (quality != 0)
      {
         score += quality;
         m_detected_glyphs.insert(m_detected_glyphs.end(), more_glyphs.begin(), more_glyphs.end());
         more_glyphs.clear();

         // Realign, append a space and attempt to find the next word.
         x = m_detected_glyphs.back().x + m_estimated_stride;
         x += m_estimated_stride * 6 / 10;
         Glyph g{
            .g = GLYPH_SPACE,
            .x = x,
            .y = y
         };
         
         
         m_detected_glyphs.push_back(g);
         
         quality = ScanForGlyphSequence(x, y, more_glyphs, false);
      }
      

      std::string s;
      for (auto& g: m_detected_glyphs)
      {
         const char* vowel = VOWELS[g.g & 63];
         const char* consonant = CONSONANTS[(g.g >> 6) & 63];
         bool swapped = (g.g & (1 << 12)) != 0;
         if (g.g == GLYPH_SPACE)
         {
            s += ' ';
         }
         else if (swapped)
         {
            s += vowel;
            s += consonant;
         }
         else
         {
            s += consonant;
            s += vowel;
         }
      }

      if (!s.empty())
         SDL_Log("score: %zu glyphs: %zu %dx%d (%d, %d) %s", score, m_detected_glyphs.size(), m_estimated_stride, m_estimated_height, m_detected_glyphs.front().x, m_detected_glyphs.front().y, s.c_str());
   }

private:
   std::shared_ptr<SDL_Window> m_window;
   std::shared_ptr<SDL_Renderer> m_renderer;
   std::shared_ptr<SDL_Texture> m_texture;

   SDL_Rect m_dst_rect{};
   int m_frame_width = WIDTH;
   int m_frame_height = HEIGHT;
   int m_estimated_height = 64;
   int m_estimated_stride = 38;
   struct {int num, den; } scale{1,1};

   std::vector<Glyph> m_detected_glyphs;
   Glyph m_bad_glyph{};
   std::vector<SDL_Rect> m_word_rects;

   int m_dx = 0;
   int m_dy = 0;

   bool m_debug_stamp = false;

   std::vector<cv::Vec4i> m_guess_params;
   
   std::shared_ptr<Text> m_font;

   cv::Mat m_data;
};


int main(int argc, char* argv[]) {
   // Initialize OpenCV Camera
   // cv::VideoCapture cap(0);
   // if (!cap.isOpened()) {
   //    std::cerr << "Error: Could not open camera." << std::endl;
   //    return -1;
   // }
   // int frame_width = cap.get(cv::CAP_PROP_FRAME_WIDTH);
   // int frame_height = cap.get(cv::CAP_PROP_FRAME_HEIGHT);


   cv::Mat frame;
   frame = cv::imread("/home/rian/Downloads/IMG_20260216_090246644_AE~2.jpg");
   cv::resize(frame, frame, {frame.cols / 4, frame.rows / 4});
   auto t = frame.type();
   
   bool dirty = true;

   int frame_width = frame.cols;
   int frame_height = frame.rows;

   

   SdlWindow window;

   if (!window.Create(frame_width, frame_height))
      return -1;

   bool quit = false;
   SDL_Event e;

   bool paused = false;
   uint32_t frame_counter = 0;
   while (!quit) {
      // Handle events
      while (SDL_PollEvent(&e) != 0) {
         switch (e.type)
         {
         case SDL_QUIT:
            quit = true;
            break;
         case SDL_WINDOWEVENT:
            switch (e.window.event)
            {
            case SDL_WINDOWEVENT_RESIZED:
               window.Resize(e.window.data1, e.window.data2);
               break;
            }
            break;
         case SDL_MOUSEBUTTONDOWN:
            // paused = !paused;
            break;
         case SDL_MOUSEMOTION:
            if (e.motion.state)
            {
               // cv::Mat t = (cv::Mat_<double>(2, 3) <<
               //    1, 0, e.motion.xrel,
               //    0, 1, e.motion.yrel
               // );
               // cv::warpAffine(frame, frame, t, frame.size());
               // dirty = true;
               window.Translate(e.motion.xrel, e.motion.yrel);
            }
            // SDL_Log("%d %d", e.motion.x, e.motion.y);
            break;
         case SDL_KEYDOWN:
            switch (e.key.keysym.sym)
            {
            case SDLK_SPACE:
               window.Stamp();
               break;
            }
            break;
         }
      }

      // Capture new frame
      // if (!paused)
      if (dirty)
      {

         //cap >> frame;
         if (!frame.empty())
            window.Updatetexture(frame);

         dirty = false;
      }
      ++frame_counter;

      window.Draw();
   }

   //cap.release();

   TscSampler::Dump();

   return 0;
}