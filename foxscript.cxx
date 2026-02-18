#include <SDL2/SDL.h>
#include <opencv2/opencv.hpp>
#include <iostream>

constexpr size_t WIDTH = 640;
constexpr size_t HEIGHT = 480;
constexpr int LINE_THICKNESS = 5;

// Initial guess at the size of the runes.
constexpr int INITIAL_STRIDE = 38;
constexpr int INITIAL_HEIGHT = 64;

// How much to vary the size and height to find them.
constexpr int STRIDE_VARIANCE = 10;
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
   SdlWindow() {}
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
      SDL_UpdateTexture(m_texture.get(), NULL, frame.data, frame.step);

      cv::cvtColor(frame, m_data, cv::COLOR_BGR2GRAY);
      cv::threshold(m_data, m_data, 64, 255, 0);

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
      cv::Mat color;
      cv::cvtColor(m_data, color, cv::COLOR_GRAY2BGR);
      SDL_UpdateTexture(m_texture.get(), NULL, color.data, color.step);

      SDL_RenderCopy(m_renderer.get(), m_texture.get(), NULL, &r);

      SDL_SetRenderDrawColor(m_renderer.get(), 255, 0, 0, 64);
      SDL_Rect horzline = m_dst_rect;
      horzline.y = horzline.h / 2 - LINE_THICKNESS/2;
      horzline.h = LINE_THICKNESS;
      SDL_RenderFillRect(m_renderer.get(), &horzline);

      SDL_Rect vertline{
         .x = horzline.w / 4 - LINE_THICKNESS/2,
         .y = horzline.y - m_estimated_height / 2,
         .w = LINE_THICKNESS,
         .h = m_estimated_height + LINE_THICKNESS
      };
      SDL_RenderFillRect(m_renderer.get(), &vertline);

      for (auto& g: m_detected_glyphs)
      {
         RenderGlyph(g);
      }

      if (m_bad_glyph.g)
      {
         RenderGlyph(m_bad_glyph, 255, 0, 0);
      }

      SDL_SetRenderDrawColor(m_renderer.get(), 32, 32, 255, 255);
      SDL_Rect dr{20, 20, 20, 20};
      SDL_RenderFillRect(m_renderer.get(), &dr);

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
         SampleGlyph(g.x, g.y, m_estimated_stride, m_estimated_height);
      }

      m_debug_stamp = false;
   }

protected:
   struct Glyph { uint16_t g; int x; int y; };

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
            .x = g.x + m_estimated_stride / 2 - LINE_THICKNESS/2, .y = g.y - m_estimated_height / 2,
            .w = LINE_THICKNESS, .h = m_estimated_height
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
      //uint16_t accuracy = 0;
      uint16_t glyph = 0;
      int px = ScreenToImg(x - m_dx);
      int py = ScreenToImg(y - m_dy);

      int p_stride = ScreenToImg(stride);
      int p_height = ScreenToImg(height);

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
         if (0 > s.p1.x || s.p1.x >= m_data.cols ||
             0 > s.p2.x || s.p2.x >= m_data.cols ||
             0 > s.p1.y || s.p1.y >= m_data.rows ||
             0 > s.p2.y || s.p2.y >= m_data.rows)
            return {0, 0};


         int s1x = (s.p1.x * 2 + s.p2.x * 8) / 10; // 20%
         int s1y = (s.p1.y * 2 + s.p2.y * 8) / 10;
         int s2x = (s.p1.x + s.p2.x) / 2;          // 50%
         int s2y = (s.p1.y + s.p2.y) / 2;
         int s3x = (s.p1.x * 8 + s.p2.x * 2) / 10; // 80%
         int s3y = (s.p1.y * 8 + s.p2.y * 2) / 10;

         cv::LineIterator it(m_data, {s1x, s1y}, {s3x, s3y});
         uint32_t l = 0;
         uint32_t sample_points = it.count;
         for (int i = 0; i < it.count; ++i, ++it)
         {
            // cv::Vec3b& pixel = m_data.at<cv::Vec3b>(it.pos());
            // if ((int)pixel.val[0] + pixel.val[1] + pixel.val[2] < 96)
            //    ++l;
            // if (m_debug_stamp)
            // {
            //    pixel.val[0] = 255;
            //    pixel.val[1] = 255;
            //    pixel.val[2] = 255;
            // }
            if (m_data.at<uint8_t>(it.pos()) == 0)
               ++l;

            if (m_debug_stamp)
               m_data.at<uint8_t>(it.pos()) = 100;
         }

         // uint32_t l = 0;
         // uint32_t sample_points = 0;
         // for (int dx = -1; dx <= 1; ++dx)
         // {
         //    for (int dy = -1; dy <= 1; ++dy)
         //    {
         //       l += is_black(s1x + dx, s1y + dy);
         //       ++sample_points;
         //    }
         // }
         // for (int dx = -1; dx <= 1; ++dx)
         // {
         //    for (int dy = -1; dy <= 1; ++dy)
         //    {
         //       l += is_black(s2x + dx, s2y + dy);
         //       ++sample_points;
         //    }
         // }
         // for (int dx = -1; dx <= 1; ++dx)
         // {
         //    for (int dy = -1; dy <= 1; ++dy)
         //    {
         //       l += is_black(s3x + dx, s3y + dy);
         //       ++sample_points;
         //    }
         // }

         // uint8_t l1 = luminance(s1x, s1y);
         // uint8_t l2 = luminance(s2x, s2y);
         // uint8_t l3 = luminance(s3x, s3y);
         // cv::line(m_data, {s.p1.x, s.p1.y}, {s.p2.x, s.p2.y}, {31, 0, 31}, 3);

         //if (l1 < 32 && l2 < 32 && l3 < 32)
         if (l >= sample_points) //* 3 / 4)
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
      size_t best_guess = 0;
      retglyphs.clear();
      for (int dx = has_space ? -12 : -6; dx < 7; dx += 2)
      {
         for (int dy = -6; dy < 7; ++dy)
         {
            for (int stride = INITIAL_STRIDE - STRIDE_VARIANCE; stride < INITIAL_STRIDE + STRIDE_VARIANCE; stride += 1)
            {
               for (int height = INITIAL_HEIGHT - HEIGHT_VARIANCE; height < INITIAL_HEIGHT + HEIGHT_VARIANCE; height += 2)
      // for (int dx = 0; dx < 1; ++dx)
      // {
      //    for (int dy = 0; dy < 1; ++dy)
      //    {
      //       for (int stride = INITIAL_STRIDE; stride <= INITIAL_STRIDE; stride += 1)
      //       {
      //          for (int height = INITIAL_HEIGHT; height <= INITIAL_HEIGHT; height += 2)
               {
                  std::vector<Glyph> glyphs;
                  Glyph g{};
                  g.x = x + dx;
                  g.y = y + dy;
                  size_t quality = 0;
                  size_t quality_total = 0;
                  Glyph bad_glyph{};
                  std::tie(g.g, quality) = SampleGlyph(g.x, g.y, stride, height);
                  while(g.g)
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
                  }
                  glyphs.clear();
               }
            }
         }
      }
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
         x += m_detected_glyphs.size() * m_estimated_stride;
         Glyph g{
            .g = GLYPH_SPACE,
            .x = x,
            .y = y
         };
         
         m_detected_glyphs.push_back(g);
         break;
         quality = ScanForGlyphSequence(x, y, more_glyphs, true);
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

   int m_dx = 0;
   int m_dy = 0;

   bool m_debug_stamp = false;

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
   for (int i = 0; i < 20; ++i)
   {
      for (int j = 0; j < 20; ++j)
      {
         auto& p =frame.at<cv::Vec3b>(20 + i, 20 + j);
         p.val[0] = 0;
         p.val[1] = 0;
         p.val[2] = 0;
      }
   }
   
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

   return 0;
}