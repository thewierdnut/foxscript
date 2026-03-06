#include "MainWindow.hh"

#include "Text.hh"
//#include "TscSampler.hh"

#include <SDL2/SDL.h>

#include <algorithm>
#include <cassert>


volatile bool g_async_camera_ready = false;


#ifdef __ANDROID__
#include "VideoCaptureAndroid.hh"
#include <jni.h>

extern "C"
{
JNIEXPORT void JNICALL Java_com_github_thewierdnut_foxscript_FSActivity_CameraReady(JNIEnv *, jobject)
{
   g_async_camera_ready = true;
}
}
#else
#include "VideoCaptureV4L2.hh"
#endif



namespace
{
   constexpr size_t WIDTH = 640;
   constexpr size_t HEIGHT = 480;
   constexpr int LINE_THICKNESS = 5;
   constexpr uint8_t THRESHOLD = 96;

   // Initial guess at the size of the runes.
   constexpr int RATIO_WIDTH = 60;   // Runes are typically 60% as wide as they are tall
   constexpr int RATIO_HEIGHT = 100;

   // How much to vary the size and height to find them.
   constexpr int STRIDE_VARIANCE = 24;
   constexpr int HEIGHT_VARIANCE = 40;

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
   //
   //                   (0,-45)
   //                /     |     \
   //             /        |        \
   //          /           |           \
   // (-50,-30)            |            (50,-30)
   //         |\           |          /
   //         |   \        |        /
   //         |      \     |     /
   //         |        ( 0, -15)
   // (-50,-10)
   // -----------------------------------------
   //
   // (-50, 20)
   //         |        ( 0,  25)
   //         |      /     |     \
   //         |   /        |        \
   //         |/           |           \
   // (-50, 40)            |            (50, 40)
   //          \           |           /
   //             \        |        /
   //                \     |     /
   //                  ( 0,  55)
   //                    -----
   //                   |0, 60|
   //                    -----
   //
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
   // A circle at the bottom is present if the vowel comes first.
   constexpr struct {int x, y; } VOWEL_FIRST {0, 60};
   

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

   /// Assuming a monochrome yuyv image, find the extents of every black shape.
   /// This uses a simplistic 4-connected path tracing algorithm that doesn't
   /// track the entire shape, just its extents. This will mutate the image.
   /// @param p yuyv image.
   /// @param width the pixel width.
   /// @param height the pixel height.
   /// @param stride the byte width.
   /// @param rects The extents found.
   void FindRects(uint8_t* p, int width, int height, int pitch, std::vector<SDL_Rect>& rects)
   {
      // The performance of this function isn't as good as the combination of
      // cv::findContours + finding the rects from the contours, but that
      // might be because findContours only works directly on monochrome data,
      // and this is designed to work directly on the yuyv source. If you you
      // include the yuyv -> monochrome conversion, then the performance is
      // equivalent, at around 3ms for a 1280x1024 image. I can probably find
      // more ways to optimize this by removing a lot of the branching logic.

      auto px = [&](int x, int y) -> uint8_t& {
         return p[y * pitch + x * 2];
      };
      // First, clear out the edges so that they can't be part of a shape.
      for (int i = 0; i < width; ++i)
      {
         px(i, 0) = 255;
         px(i, height - 1) = 255;
      }
      for (int i = 0; i < height; ++i)
      {
         px(0, i) = 255;
         px(width - 1, i) = 255;
      }

      for (int y = 1; y < height-1; ++y)
      {
         bool nested = false;
         uint8_t prev_pixel = 0xff;
         for (int x = 1; x < width-1; ++x)
         {
            uint8_t& pixel = px(x, y);
            if (!nested && prev_pixel >= 0x80 && pixel == 0)
            {
               // New shape. Trace.
               nested = true;
               int leftx = x;
               int rightx = x;
               int topy = y;
               int bottomy = y;

               int tx = x;
               int ty = y;
               enum {RIGHT, DOWN, LEFT, UP} dir = RIGHT;
               while (px(tx, ty) <= 1)
               {
                  px(tx, ty) = 1;
                  if (tx < leftx) leftx = tx;
                  if (tx > rightx) rightx = tx;
                  if (ty < topy) topy = ty;
                  if (ty > bottomy) bottomy = ty;

                  // Check four neighbors clockwise. If needed, we can check
                  // diagonals too.
                  // TODO: There has got to be a more clever way to do this.
                  if (dir == RIGHT)
                  {
                     if (tx == x && ty-1 == y)      { break;           }
                     else if (px(tx, ty-1) <= 1)    { --ty; dir=UP;    }
                     else if (tx+1 == x && ty == y) { break;           }
                     else if (px(tx+1, ty) <= 1)    { ++tx; /* RIGHT */}
                     else if (tx == x && ty+1 == y) { break;           }
                     else if (px(tx, ty+1) <= 1)    { ++ty; dir=DOWN;  }
                     else if (tx-1 == x && ty == y) { break;           }
                     else if (px(tx-1, ty) <= 1)    { --tx; dir=LEFT;  }
                     else break;
                  }
                  else if (dir == DOWN)
                  {
                     if (tx+1 == x && ty == y)      { break;           }
                     else if (px(tx+1, ty) <= 1)    { ++tx; dir=RIGHT; }
                     else if (tx == x && ty+1 == y) { break;           }
                     else if (px(tx, ty+1) <= 1)    { ++ty; /* DOWN */ }
                     else if (tx-1 == x && ty == y) { break;           }
                     else if (px(tx-1, ty) <= 1)    { --tx; dir=LEFT;  }
                     else if (tx == x && ty-1 == y) { break;           }
                     else if (px(tx, ty-1) <= 1)    { --ty; dir=UP;    }
                     else break;
                  }
                  else if (dir == LEFT)
                  {
                     if (tx == x && ty+1 == y)      { break;           }
                     else if (px(tx, ty+1) <= 1)    { ++ty; dir=DOWN;  }
                     else if (tx-1 == x && ty == y) { break;           }
                     else if (px(tx-1, ty) <= 1)    { --tx; /* LEFT */ }
                     else if (tx == x && ty-1 == y) { break;           }
                     else if (px(tx, ty-1) <= 1)    { --ty; dir=UP;    }
                     else if (tx+1 == x && ty == y) { break;           }
                     else if (px(tx+1, ty) <= 1)    { ++tx; dir=RIGHT; }
                     else break;
                  }
                  else // dir == UP
                  {
                     if (tx-1 == x && ty == y)      { break;           }
                     else if (px(tx-1, ty) <= 1)    { --tx; dir=LEFT;  }
                     else if (tx == x && ty-1 == y) { break;           }
                     else if (px(tx, ty-1) <= 1)    { --ty; /* UP */   }
                     else if (tx+1 == x && ty == y) { break;           }
                     else if (px(tx+1, ty) <= 1)    { ++tx; dir=RIGHT; }
                     else if (tx == x && ty+1 == y) { break;           }
                     else if (px(tx, ty+1) <= 1)    { ++ty; dir=DOWN;  }
                     else                           { break;           }
                  }
               }
               // We have 3 sample points for each glyph segment, which can be
               // two segments wide, and three segments tall. Reject rects
               // that are too small. We will filter out ones that are too big
               // later once we know the zoom level.
               if (rightx - leftx > 10 && bottomy - topy > 15)
               {
                  rects.emplace_back(SDL_Rect{leftx, topy, rightx - leftx + 1, bottomy - topy + 1});
               }
            }
            else if (!nested && prev_pixel >= 0x80 && pixel == 1)
            {
               // Entering a shape we already found.
               nested = true;
            }
            else if (nested && prev_pixel == 1 && pixel >= 0x80)
            {
               // Exiting a shape
               nested = false;
            }
            if (pixel <= 1)
               assert(nested);
            prev_pixel = pixel;
         }
      }
   }
}


MainWindow::MainWindow():
   m_open_button({WIDTH - 64, HEIGHT - 64, 64, 64}, "open.png", [this](){Open();}),
   m_camera_button({WIDTH - 128, HEIGHT - 64, 64, 64}, "camera.png", [this](){Camera();}),
   // m_stamp_button({WIDTH - 192, HEIGHT - 64, 64, 64}, "test_square.png", [this](){Stamp();}),
   m_window_size{WIDTH, HEIGHT},
   m_img()
{
   // Prepopulate the parameters we vary to find the glyphs, since they
   // should be sorted by how "abnormal" they are.
   for (int dx = -4; dx <= 10; dx += 2)
   {
      for (int dy = -8; dy <= 8; dy += 2)
      {
         // Ratio to height, which is typically around 60%, varied by 5%
         for (int ratio = (int)(PARAM_DEN * .55); ratio < (int)(PARAM_DEN * .65); ratio += 3)
         {
            // How much to vary the size, varied by 50%
            for (int height = (int)(PARAM_DEN * .75); height < (int)(PARAM_DEN * 1.25); height += 5)
            {
               m_guess_params.push_back({dx, dy, ratio, height});
            }
         }
      }
   }
   std::sort(m_guess_params.begin(), m_guess_params.end(),
      [](const Params& a, const Params& b) {
         return abs(a.dx) + abs(a.dy) + abs(a.ratio - 70) + abs(a.scale - 100)
               < abs(b.dx) + abs(b.dy) + abs(b.ratio - 70) + abs(b.scale - 100);
      }
   );

   m_dst_rect = SDL_Rect{0, 0, WIDTH, HEIGHT};
   Resize(WIDTH, HEIGHT);
}


MainWindow::~MainWindow()
{
   // TscSampler::Dump(); //x86_64 only
   SDL_Quit();
}

bool MainWindow::Create()
{
   m_dst_rect.w = m_frame_width;
   m_dst_rect.h = m_frame_height;


   // Initialize SDL
   if (SDL_Init(SDL_INIT_VIDEO) < 0) {
      SDL_Log("SDL could not initialize! SDL_Error: %s", SDL_GetError());
      return false;
   }

   m_window.reset(SDL_CreateWindow("foxscript",
                                 SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                 WIDTH, HEIGHT, SDL_WINDOW_SHOWN|SDL_WINDOW_RESIZABLE),
                  SDL_DestroyWindow);
   if (!m_window)
      return false;

   // Get the actual size. I would expect a resize event, but that doesn't seem
   // to happen.
   int initialized_width = WIDTH;
   int initialized_height = HEIGHT;
   SDL_GetWindowSize(m_window.get(), &initialized_width, &initialized_height);
   Resize(initialized_width, initialized_height);

   m_renderer.reset(SDL_CreateRenderer(m_window.get(), -1, SDL_RENDERER_ACCELERATED),
                     SDL_DestroyRenderer);
   if (!m_renderer)
      return false;
   

   SDL_SetRenderDrawBlendMode(m_renderer.get(), SDL_BLENDMODE_BLEND);

   // Create a placeholder texture for the camera frames
   // m_texture.reset(SDL_CreateTexture(m_renderer.get(), SDL_PIXELFORMAT_YUY2,
   //                               SDL_TEXTUREACCESS_STREAMING, m_frame_width, m_frame_height),
   //                   SDL_DestroyTexture);
   

   m_font.reset(new Text(m_renderer, "text_48.png"));

   
   auto cres = GetResource("circle.png");
   if (cres.first)
   {
      std::vector<uint8_t> cbytes;
      unsigned long width = 0;
      unsigned long height = 0;
      if (decodePNG(cbytes, width, height, cres.first, cres.second))
      {
         throw std::runtime_error("Unable to load circle.png");
      }
      // Create a texture for the vowel order circle
      m_circle_texture.reset(SDL_CreateTexture(m_renderer.get(), SDL_PIXELFORMAT_RGBA8888,
                                 SDL_TEXTUREACCESS_STATIC, width, height),
                     SDL_DestroyTexture);
      SDL_SetTextureBlendMode(m_circle_texture.get(), SDL_BLENDMODE_BLEND);
      SDL_UpdateTexture(m_circle_texture.get(), nullptr, cbytes.data(), width * 4);
   }

   return true;
}

void MainWindow::UpdateTexture(uint8_t* p, int width, int height, int pitch)
{
   if (!p)
      return;

   if (!m_texture || m_frame_height != height || m_frame_width != width || m_frame_pitch != pitch)
   {
      m_texture.reset(
         SDL_CreateTexture(
            m_renderer.get(),
            SDL_PIXELFORMAT_YUY2,
            SDL_TEXTUREACCESS_STREAMING,
            width, height
         ),
         SDL_DestroyTexture
      );

      m_frame_pitch = pitch;
      m_frame_height = height;
      m_frame_width = width;
      m_dst_rect = {0, 0, width, height};
   }
   // TODO: gemini lied to me. We should use SDL_LockTexture here.
   // SDL_UpdateTexture(m_texture.get(), NULL, p, pitch);
   
   m_yuv_data = p;

   // Does this have white text on black, or black text on white?
   size_t black_pixels = 0;
   size_t total_pixels = 0;
   for (int y = m_frame_height / 4; y < m_frame_height / 4 * 3; y += 10)
   {
      for (int x = m_frame_width / 4; x < m_frame_width / 4 * 3; x += 10)
      {
         black_pixels += Pixel(x, y) < 128;
         total_pixels += 1;
      }
   }
   m_black_on_white = black_pixels < total_pixels / 2;

   // Run threshold algorithm on Y channels
   if (m_black_on_white)
   {
      uint8_t* y = m_yuv_data;
      for (int i = 0; i < m_frame_height; ++i)
      {
         for (int j = 0; j < pitch; j += 2)
         {
            *y = *y > THRESHOLD ? 255 : 0;
            ++y;
            *y = 128;
            ++y;
         }
      }
   }
   else
   {
      uint8_t* y = m_yuv_data;
      for (int i = 0; i < m_frame_height; ++i)
      {
         for (int j = 0; j < pitch; j += 2)
         {
            *y = *y > (255 - THRESHOLD) ? 0 : 255;
            ++y;
            *y = 128;
            ++y;
         }
      }
   }
   SDL_UpdateTexture(m_texture.get(), NULL, m_yuv_data, pitch);
   

   // Words are joined together, usually with the horizontal centerline,
   // but not always. Try to collect and save off rects representing each
   // word, so that we know to insert a space if we leave it.
   
   m_word_rects.clear();
   FindRects((uint8_t*)m_yuv_data, m_frame_width, m_frame_height, m_frame_pitch, m_word_rects);

   for (auto it = m_word_rects.begin(); it != m_word_rects.end();)
   {
      if (it->h > m_estimated_height * 2)
         it = m_word_rects.erase(it);
      else
         ++it;
   }
   
   ScanForGlyphs();
}

void MainWindow::Step()
{
   if (g_async_camera_ready)
   {
      SDL_Log("Asynchronously requested to start camera capture");
      g_async_camera_ready = false;
      m_yuv_data = nullptr;

      if (!m_camera)
      {
         // TODO: select a camera?
         int camera_idx = 0;
#ifndef __ANDROID__
         m_camera.reset(new VideoCaptureV4L2());
#else
         m_camera.reset(new VideoCaptureAndroid());
         // camera_idx = 2;
#endif
         // TODO: Select which camera?
         if (!m_camera->Open(camera_idx))
         {
            SDL_Log("Failed to start camera");
            m_camera.reset();
         }
         else
         {
            m_paused = false;
         }
      }
   }

   if (m_camera && !m_paused)
   {
      UpdateTexture((uint8_t*)m_camera->GetFrame(), m_camera->Width(), m_camera->Height(), m_camera->Pitch());
   }
}

void MainWindow::Draw()
{
   // Render
   SDL_SetRenderDrawColor(m_renderer.get(), 0, 0, 0, 255);
   SDL_RenderClear(m_renderer.get());

   if (m_texture)
   {
      // if (m_yuv_data)
      //    SDL_UpdateTexture(m_texture.get(), NULL, m_yuv_data, m_frame_pitch);

      SDL_RenderCopy(m_renderer.get(), m_texture.get(), NULL, &m_dst_rect);
   }

   // // Debug, drawing shape rects
   // SDL_SetRenderDrawColor(m_renderer.get(), 0, 0, 255, 128);
   // for (const SDL_Rect& r: m_word_rects)
   // {
   //    // These are in image coordinates.
   //    SDL_Rect rs{
   //       .x = ImgToScreen(r.x) + m_dst_rect.x,
   //       .y = ImgToScreen(r.y) + m_dst_rect.y,
   //       .w = ImgToScreen(r.w),
   //       .h = ImgToScreen(r.h)
   //    };
   //    SDL_RenderFillRect(m_renderer.get(), &rs);
   // }

   // Horizontal baseline;
   SDL_SetRenderDrawColor(m_renderer.get(), 255, 0, 0, 64);
   SDL_Rect horzline = {
      .x = 0,
      .y = m_window_size.y / 2 - LINE_THICKNESS/2,
      .w = m_window_size.x,
      .h = LINE_THICKNESS
   };
   SDL_RenderFillRect(m_renderer.get(), &horzline);

   // Vertial alignment marker
   SDL_Rect vertline{
      .x = horzline.w / 4 - LINE_THICKNESS/2,
      .y = horzline.y - m_estimated_height / 2,
      .w = LINE_THICKNESS,
      .h = m_estimated_height + LINE_THICKNESS
   };
   SDL_RenderFillRect(m_renderer.get(), &vertline);
   vertline.y = horzline.y - m_initial_height / 2;
   vertline.h = m_initial_height + LINE_THICKNESS;
   SDL_RenderFillRect(m_renderer.get(), &vertline);


   // Glyph overlay and translated text
   if (!m_detected_glyphs.empty())
   {
      SDL_Point text_pos{
         .y = horzline.y + m_initial_height
      };

      SDL_Rect text_background{
         .x = 0,
         .y = text_pos.y - m_font->Height() / 2,
         .w = horzline.w,
         .h = m_font->Height() * 2
      };
      SDL_SetRenderDrawColor(m_renderer.get(), 0, 0, 0, 128);
      SDL_RenderFillRect(m_renderer.get(), &text_background);

      
      bool space = true;
   
      for (auto& g: m_detected_glyphs)
      {
         RenderGlyph(g);

         if (space)
         {
            if (text_pos.x + m_font->Stride() > g.x - m_initial_stride/2)
            {
               text_pos.x += m_font->Stride();
            }
            else
            {
               text_pos.x = g.x - m_initial_stride/2;
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
   // else
   // {
   //    int x = m_window_size.x / 4;
   //    int y = m_window_size.y / 2;
   //    SDL_SetRenderDrawColor(m_renderer.get(), 0, 255, 0, 128);

   //    SDL_Rect initial_guess{
   //       .x = x - m_initial_stride / 2,
   //       .y = y - m_initial_height / 2,
   //       .w = m_initial_stride,
   //       .h = m_initial_height
   //    };
   //    SDL_RenderFillRect(m_renderer.get(), &initial_guess);
   // }

   // If we saw a glyph that was invalid, chances are we detected part of a
   // good one. Show it so that the user knows they are close to matching.
   if (m_bad_glyph.g)
   {
      RenderGlyph(m_bad_glyph, 255, 0, 0);
   }

   m_open_button.Draw(m_renderer);
   m_camera_button.Draw(m_renderer);
   // m_stamp_button.Draw(m_renderer);

   // m_font->Render(m_renderer, {0, m_window_size.y - m_font->Height()}, m_black_on_white ? "Black" : "White" );

   SDL_RenderPresent(m_renderer.get());
}

void MainWindow::Resize(int new_width, int new_height)
{
   // // Scale the target rect for the video so that it always fits and stays
   // // the right aspect ratio.
   // m_dst_rect.w = new_width;
   // m_dst_rect.h = new_width * m_frame_height / m_frame_width;
   // m_scale.num = new_width;
   // m_scale.den = m_frame_width;
   // if (m_dst_rect.h > new_height)
   // {
   //    m_dst_rect.h = new_height;
   //    m_dst_rect.w = new_height * m_frame_width / m_frame_height;
   //    m_scale.num = new_height;
   //    m_scale.den = m_frame_height;
   // }


   // ScanForGlyphs();

   m_window_size.x = new_width;
   m_window_size.y = new_height;
   m_initial_height = new_height / 6;
   m_initial_stride = m_initial_height * RATIO_WIDTH / RATIO_HEIGHT;
   m_estimated_height = m_initial_height;
   m_estimated_stride = m_initial_stride;

   m_open_button.SetPosition(SDL_Rect{new_width - m_initial_height, new_height - m_initial_height, m_initial_height, m_initial_height});
   m_camera_button.SetPosition(SDL_Rect{new_width - m_initial_height * 2, new_height - m_initial_height, m_initial_height, m_initial_height});
   // m_stamp_button.SetPosition(SDL_Rect{new_width - m_initial_height * 3, new_height - m_initial_height, m_initial_height, m_initial_height});

   ScanForGlyphs();
}

void MainWindow::Translate(int dx, int dy)
{
   m_dst_rect.x += dx;
   m_dst_rect.y += dy;
   ScanForGlyphs();
}

bool MainWindow::MouseDown(const SDL_Point& p, int button)
{
   //SDL_Log("MouseDown()");
   if (m_open_button.MouseDown(p, button))
      return true;
   if (m_camera_button.MouseDown(p, button))
      return true;
   // if (m_stamp_button.MouseDown(p, button))
   //    return true;
   return false;
}

bool MainWindow::MouseUp(const SDL_Point& p, int button)
{
   //SDL_Log("MouseUp()");
   if (m_open_button.MouseUp(p, button))
      return true;
   if (m_camera_button.MouseUp(p, button))
      return true;
   // if (m_stamp_button.MouseUp(p, button))
   //    return true;
   return false;
}

bool MainWindow::MouseMotion(const SDL_Point& p, int button)
{
   if (m_open_button.MouseMotion(p, button))
      return true;
   if (m_camera_button.MouseMotion(p, button))
      return true;
   // if (m_stamp_button.MouseMotion(p, button))
   //    return true;

   if (button)
      Translate(p.x, p.y);
   return true;
}

bool MainWindow::MouseWheel(const SDL_Point& p, int s)
{
   // if (m_open_button.MouseWheel(y))
   //    return true;

   Zoom(p, m_zoom * (s > 0 ? 1.05 : .95));

   return true;
}

bool MainWindow::FingerDown(const SDL_Point& p, int fid)
{
   //SDL_Log("FingerDown()");
   if (m_zg.FingerDown(p, fid))
      return true;
   return MouseDown(p, SDL_BUTTON_LEFT);
}

bool MainWindow::FingerUp(const SDL_Point& p, int fid)
{
   //SDL_Log("FingerUp()");
   if (m_zg.FingerUp(p, fid))
      return true;
   return MouseUp(p, SDL_BUTTON_LEFT);
}

bool MainWindow::FingerMotion(const SDL_Point& p, const SDL_Point& dp, int fid)
{
   if (m_zg.FingerMove(p, fid))
   {
      Zoom({
            m_dst_rect.x + m_zg.CenterDelta().x,
            m_dst_rect.y + m_zg.CenterDelta().y
         },
         m_zoom * m_zg.Zoom()
      );
      MouseMotion(m_zg.CenterDelta(), SDL_BUTTON_LMASK);
      return true;
   }
   // SDL_Log("FingerMotion(%d, %d)", dp.x, dp.y);
   return MouseMotion(dp, SDL_BUTTON_LMASK);
}

// x and y are in screen coordinates
void MainWindow::RenderGlyph(const Glyph& g, uint8_t cr, uint8_t cg, uint8_t cb)
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
         s.p1.x = g.x + s.p1.x * g.stride / GLYPH_SCALE;
         s.p1.y = g.y + s.p1.y * g.height / GLYPH_SCALE;
         s.p2.x = g.x + s.p2.x * g.stride / GLYPH_SCALE;
         s.p2.y = g.y + s.p2.y * g.height / GLYPH_SCALE;
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
         .x = g.x + VOWEL_FIRST.x * g.stride / GLYPH_SCALE - 6,
         .y = g.y + VOWEL_FIRST.y * g.height / GLYPH_SCALE - 6,
         .w = 12,
         .h = 12,
      };
      SDL_SetTextureColorMod(m_circle_texture.get(), cr, cg, cb);
      SDL_RenderCopy(m_renderer.get(), m_circle_texture.get(), nullptr, &rect);
   }
}

// x, y, stride, height given in screen units.
// @returns glyph mask, number of sampled black pixels.
size_t MainWindow::SampleGlyph(Glyph& g)
{
   //TscSampler ts("sg");
   //uint16_t accuracy = 0;
   g.g = 0;
   int px = ScreenToImg(g.x - m_dst_rect.x);
   int py = ScreenToImg(g.y - m_dst_rect.y);

   int p_stride = ScreenToImg(g.stride);
   int p_height = ScreenToImg(g.height);

   // Don't read off the edge of the picture.
   if (p_stride * 3 / 2 > px || px > m_frame_width - p_stride * 3 / 2 ||
         p_height * 3 / 2 > py || py > m_frame_height - p_height * 3 / 2)
   {
      return 0;
   }
   // For each segment position, sample three pixels and 30, 50, and 70%
   // of the line segment. If they are all dark, then assume the line
   // segment is drawn.
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
      // const SDL_Point sp[] = {
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
      const SDL_Point sp[] = {
         {(s.p1.x * 1 + s.p2.x *15) / 16, (s.p1.y * 1 + s.p2.y *15) / 16},
         {(s.p1.x * 8 + s.p2.x * 8) / 16, (s.p1.y * 8 + s.p2.y * 8) / 16},
         {(s.p1.x *15 + s.p2.x * 1) / 16, (s.p1.y *15 + s.p2.y * 1) / 16},
      };
      const uint32_t sample_points = sizeof(sp)/sizeof(*sp);
      for (auto& p: sp)
      {
         l += Pixel(p.x, p.y) <= 128;
         // if (m_debug_stamp)
         // {
         //    Pixel(p.x, p.y) = Pixel(p.x, p.y) <= 128 ? 127 : 129;
         // }
      }

      // uint8_t l1 = luminance(s1x, s1y);
      // uint8_t l2 = luminance(s2x, s2y);
      // uint8_t l3 = luminance(s3x, s3y);
      // cv::line(m_data, {s.p1.x, s.p1.y}, {s.p2.x, s.p2.y}, {31, 0, 31}, 3);

      //if (l1 < 32 && l2 < 32 && l3 < 32)
      if (l == sample_points) //* 3 / 4)
      {
         black_pixels += l;
         g.g |= (1 << i);
      }
   }

   // Sample at the vowel/consanant swap marker. This is drawn as a circle.
   int cx = px + VOWEL_FIRST.x * p_stride / GLYPH_SCALE;
   int cy = py + VOWEL_FIRST.y * p_height / GLYPH_SCALE;
   const int thickness = p_stride * 8 / 64; // About 15% of the width of the glyph

   int outer[4] = {};
   int hole = 0;
   int inner[4] = {};
   
   for (int i = -thickness; i < thickness; ++i)
   {
      if (i < -2)
      {
         if (Pixel(cx + i, cy)     < 64) ++outer[0];
         if (Pixel(cx, cy + i)     < 64) ++outer[1];
         if (Pixel(cx + i, cy + i) < 64) ++outer[2];
         if (Pixel(cx + i, cy - i) < 64) ++outer[3];
            
      }
      else if (i <= 2)
      {
         if (Pixel(cx + i, cy) > 192) ++hole;
         if (Pixel(cx, cy + i) > 192) ++hole;
      }
      else
      {
         if (Pixel(cx + i, cy)     < 64) ++inner[0];
         if (Pixel(cx, cy + i)     < 64) ++inner[1];
         if (Pixel(cx + i, cy + i) < 64) ++inner[2];
         if (Pixel(cx + i, cy - i) < 64) ++inner[3];
      }

      // if (m_debug_stamp)
      // {
      //    Pixel(cx + i, cy)     = 128;
      //    Pixel(cx, cy + i)     = 128;
      //    Pixel(cx + i, cy + i) = 128;
      //    Pixel(cx + i, cy - i) = 128;
      // }
   }
   if (hole > 4 &&
       outer[0] && outer[1] && outer[2] && outer[3] &&
       inner[0] && inner[1] && inner[2] && inner[3])
   {
      g.g |= (1 << 12);
      for (int i = 0; i < 4; ++i)
         black_pixels += inner[i] + outer[i];
      black_pixels += hole;
      // std::string sample_line;
      // for (int i = -LINE_THICKNESS; i < LINE_THICKNESS; ++i)
      // {
      //    sample_line += m_data.at<uint8_t>(cy + LINE_THICKNESS, cx + i) == 0 ? '0' : '1';
      // }
      // SDL_Log("Circle at %d %d %s", cx, cy, sample_line.c_str());
   }

   // int outer = 0;
   // int inner = 0;

   // inner += Pixel(cx, cy) > 128;
   // inner += Pixel(cx-1, cy) > 128;
   // inner += Pixel(cx, cy-1) > 128;
   // inner += Pixel(cx+1, cy) > 128;
   // inner += Pixel(cx, cy+1) > 128;

   // outer += Pixel(cx-thickness, cy) < 128;
   // outer += Pixel(cx, cy-thickness) < 128;
   // outer += Pixel(cx+thickness, cy) < 128;
   // outer += Pixel(cx, cy+thickness) < 128;

   // int sqrt2over2ish = thickness * 3 / 4;
   // outer += Pixel(cx-sqrt2over2ish, cy-sqrt2over2ish) < 128;
   // outer += Pixel(cx+sqrt2over2ish, cy-sqrt2over2ish) < 128;
   // outer += Pixel(cx+sqrt2over2ish, cy+sqrt2over2ish) < 128;
   // outer += Pixel(cx-sqrt2over2ish, cy+sqrt2over2ish) < 128;
   // if (m_debug_stamp)
   // {
   //    Pixel(cx, cy) = 192;
   //    Pixel(cx-1, cy) = 192;
   //    Pixel(cx, cy-1) = 192;
   //    Pixel(cx+1, cy) = 192;
   //    Pixel(cx, cy+1) = 192;

   //    Pixel(cx-thickness, cy) = 64;
   //    Pixel(cx, cy-thickness) = 64;
   //    Pixel(cx+thickness, cy) = 64;
   //    Pixel(cx, cy+thickness) = 64;
   //    Pixel(cx-sqrt2over2ish, cy-sqrt2over2ish) = 64;
   //    Pixel(cx+sqrt2over2ish, cy-sqrt2over2ish) = 64;
   //    Pixel(cx+sqrt2over2ish, cy+sqrt2over2ish) = 64;
   //    Pixel(cx-sqrt2over2ish, cy+sqrt2over2ish) = 64;
   // }
   // if (inner == 5 && outer == 8)
   // {
   //    g.g |= (1 << 12);
   //    black_pixels |= 13;
   // }

   return black_pixels;
}

// @param x, y in screen coordinates
// @param has_space give the horizontal coordinate more leeway since its a
//        new word
// Try and find multiple glyphs in a row (ie a "word")
size_t MainWindow::ScanForGlyphSequence(int x, int y, std::vector<Glyph>& retglyphs)
{
   // Limit our guessing to the rect containing our word
   const int ix = ScreenToImg(x - m_dst_rect.x);
   const int iy = ScreenToImg(y - m_dst_rect.y);
   const int iheight = ScreenToImg(m_initial_height);
   const int iwidth = ScreenToImg(m_initial_stride);

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
         word_left = ImgToScreen(r.x - (iwidth / 16)) + m_dst_rect.x;
         word_right = ImgToScreen(r.x + r.w + iwidth / 8) + m_dst_rect.x;
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

   size_t best_guess = 0;
   retglyphs.clear();
   std::vector<Glyph> glyphs;
   for (auto& v: m_guess_params)
   {
      int dx = v.dx;
      int dy = v.dy;
      int height = m_initial_height * v.scale / PARAM_DEN;
      int stride = height * v.ratio / PARAM_DEN;
      glyphs.clear();
      Glyph g{};
      g.x = x + dx;
      g.y = y + dy;
      g.height = height;
      g.stride = stride;
      size_t quality = 0;
      size_t quality_total = 0;
      Glyph bad_glyph{};
      quality = SampleGlyph(g);
      while(quality && g.x < word_right)
      {
         uint16_t vowel_idx = g.g & 63;
         uint16_t cons_idx = (g.g >> 6) & 63;
         const char* vowel = VOWELS[vowel_idx];
         const char* consonant = CONSONANTS[cons_idx];
         // Not every combination of line segments is a valid glyph.
         if (vowel_idx && !vowel[0] ||
             cons_idx && !consonant[0])
         {
            bad_glyph = g;
            break;
         }
         quality_total += quality;
         glyphs.push_back(g);
         g.x += stride;
         
         quality = SampleGlyph(g);
      }

      if (quality_total > best_guess)
      {
         best_guess = quality_total;
         retglyphs.swap(glyphs);
         m_bad_glyph = bad_glyph;
         m_estimated_height = height;
         m_estimated_stride = stride;
      }
   }
   return best_guess;
}

// Vary the stride and the height to try and find the most number of line
// segments possible
void MainWindow::ScanForGlyphs()
{
   int x = m_window_size.x / 4;
   int y = m_window_size.y / 2;
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
      
      quality = ScanForGlyphSequence(x, y, more_glyphs);
   }
}

void MainWindow::Open()
{
   m_camera.reset();

   switch(++m_debug)
   {
   case 1:
      m_img.Load("test_img_1.png");
      UpdateTexture(m_img.Data(), m_img.Width(), m_img.Height(), m_img.Pitch());
      break;
   case 2:
      m_img.Load("test_img_2.png");
      UpdateTexture(m_img.Data(), m_img.Width(), m_img.Height(), m_img.Pitch());
      break;
   case 3:
      m_img.Load("test_img_3.png");
      UpdateTexture(m_img.Data(), m_img.Width(), m_img.Height(), m_img.Pitch());
      break;
   default:
      m_debug = 0;
      break;
   }
#ifndef __ANDROID__
   SDL_Log("This is where we pop up a file open dialog");
   switch(++m_debug)
   {
   case 1:
      m_img.Load("test_img_1.png");
      UpdateTexture(m_img.Data(), m_img.Width(), m_img.Height(), m_img.Pitch());
      break;
   case 2:
      m_img.Load("test_img_2.png");
      UpdateTexture(m_img.Data(), m_img.Width(), m_img.Height(), m_img.Pitch());
      break;
   default:
      m_debug = 0;
      break;
   }
   
#else
   // SDL_Log("Attempting to call CheckCameraPermissions");

   // // something android-ish
   // auto* env = (JNIEnv*)SDL_AndroidGetJNIEnv();
   // SDL_Log("   env: %p", env);
   // env->PushLocalFrame(16);

   // /* context = SDLActivity.getContext(); */
   // jobject context = (jobject)SDL_AndroidGetActivity();
   // SDL_Log("   context: %p", context);

   // /* context.GetNewCameraImage(); */
   // jmethodID mid = env->GetMethodID(env->GetObjectClass(context),
   //          "CheckCameraPermissions", "()Z");
   // SDL_Log("   mid: %p", mid);

   // bool success = env->CallBooleanMethod(context, mid);
   // env->PopLocalFrame(nullptr);

   // SDL_Log("CheckCameraPermissions() returned %s", success ? "true": "false");

#endif
}

void MainWindow::Camera()
{
   SDL_Log("This is where we show camera output");
   if (m_camera)
   {
      m_paused = !m_paused;
   }
   else
   {
#ifndef __ANDROID__
      g_async_camera_ready = true;
#else
      SDL_Log("Attempting to call CheckCameraPermissions");

      // something android-ish
      auto* env = (JNIEnv*)SDL_AndroidGetJNIEnv();
      SDL_Log("   env: %p", env);
      env->PushLocalFrame(16);

      /* context = SDLActivity.getContext(); */
      jobject context = (jobject)SDL_AndroidGetActivity();
      SDL_Log("   context: %p", context);

      /* context.GetNewCameraImage(); */
      jmethodID mid = env->GetMethodID(env->GetObjectClass(context),
               "CheckCameraPermissions", "()Z");
      SDL_Log("   mid: %p", mid);

      bool success = env->CallBooleanMethod(context, mid);
      env->PopLocalFrame(nullptr);

      SDL_Log("CheckCameraPermissions() returned %s", success ? "true": "false");
      if (success)
         g_async_camera_ready = true;
#endif

   }
}



void MainWindow::Zoom(const SDL_Point& pos, float z)
{
   float scale = z / m_zoom;
   m_zoom = z;

   // Move the image to the origin.
   m_dst_rect.x -= pos.x;
   m_dst_rect.y -= pos.y;

   // scale it
   m_dst_rect.x *= scale;
   m_dst_rect.y *= scale;

   // Move it back to position
   m_dst_rect.x += pos.x;
   m_dst_rect.y += pos.y;
   

   //m_dst_rect.x -= dx;
   //m_dst_rect.y -= dy;
   m_dst_rect.w = m_frame_width * z;
   m_dst_rect.h = m_frame_height * z;

   // SDL_Log("f: %f dst_rect: %d %d %d %d", m_zoom, m_dst_rect.x, m_dst_rect.y, m_dst_rect.w, m_dst_rect.h);

   m_scale.num = m_dst_rect.w;
   m_scale.den = m_frame_width;

   ScanForGlyphs();
}