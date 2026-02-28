#include "src/Image.hh"
#include "src/MainWindow.hh"
#include "src/VideoCaptureV4L2.hh"
#include "src/ZoomGesture.hh"

#include <fstream>


int EventFilter(void* userdata, SDL_Event* event)
{
	// This callback is guaranteed to be handled in the device callback, rather
	// than the event loop.
	if (event->type == SDL_APP_DIDENTERBACKGROUND)
	{
		
	}
	else if (event->type == SDL_APP_DIDENTERFOREGROUND)
	{
		
	}

	return 1;
}

int main(int argc, char* argv[])
{
   // Initialize OpenCV Camera
   VideoCaptureV4L2 cap;
   if (!cap.Open(0))
   {
      SDL_Log("Error: Could not open camera.");
      return -1;
   }
   int frame_width = cap.Width();
   int frame_height = cap.Height();

   int debug = 0;

   // For android, game loop does not run on the main thread, and some of these
   // events need handled from within the java callback context. Handle them
   // directly rather than relying on the sdl event loop. This filter needs
   // set before any SDL events get generated, because it drops any events
   // that happen to be in the queue (such as window sizing events)
   SDL_SetEventFilter(EventFilter, nullptr);
   SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0"); // turn off mouse emulation
   SDL_SetHint(SDL_HINT_MOUSE_TOUCH_EVENTS, "0"); // turn off mouse emulation

   Image test_img(frame_width, frame_height);

   MainWindow window;

   if (!window.Create(frame_width, frame_height))
      return -1;

   bool quit = false;
   SDL_Event e;
   ZoomGesture zg;

   bool paused = false;
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
            window.MouseDown({e.button.x, e.button.y}, e.button.button);
            break;
         case SDL_MOUSEBUTTONUP:
            window.MouseUp({e.button.x, e.button.y}, e.button.button);
            break;
         case SDL_MOUSEMOTION:
            window.MouseMotion({e.motion.xrel, e.motion.yrel}, e.motion.state);
            break;
         case SDL_MOUSEWHEEL:
            window.MouseWheel({e.wheel.mouseX, e.wheel.mouseY}, e.wheel.y);
            break;
         case SDL_KEYDOWN:
            switch (e.key.keysym.sym)
            {
            case SDLK_d:
               switch(++debug)
               {
               case 1:
                  test_img.Load("test_img_1.png");
                  break;
               case 2:
                  test_img.Load("test_img_2.png");
                  break;
               default:
                  debug = 0;
                  break;
               }
            }
            break;
         case SDL_FINGERDOWN:
            window.FingerDown({(int)(e.tfinger.x * window.Size().x), (int)(e.tfinger.y * window.Size().y)}, e.tfinger.fingerId);
            break;
         case SDL_FINGERUP:
            window.FingerUp({(int)(e.tfinger.x * window.Size().x), (int)(e.tfinger.y * window.Size().y)}, e.tfinger.fingerId);
            break;
         case SDL_FINGERMOTION:
            window.FingerMotion({(int)(e.tfinger.x * window.Size().x), (int)(e.tfinger.y * window.Size().y)}, e.tfinger.fingerId);
            break;
         }
      }

      // Capture new frame
      // if (!paused)
      if (!window.Paused())
      {

         if (debug != 0)
         {
            window.UpdateTexture(test_img.Data(), test_img.Pitch());
         }
         else
         {
            void* p = cap.GetFrame();
            if (p)
               window.UpdateTexture(p, cap.Stride());
         }
      }

      window.Draw();
   }

   cap.Close();

   // TscSampler::Dump();

   return 0;
}