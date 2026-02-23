#include "src/MainWindow.hh"
#include "src/ZoomGesture.hh"



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

int main(int argc, char* argv[]) {
   // Initialize OpenCV Camera
   cv::VideoCapture cap(0);
   if (!cap.isOpened()) {
      std::cerr << "Error: Could not open camera." << std::endl;
      return -1;
   }
   int frame_width = cap.get(cv::CAP_PROP_FRAME_WIDTH);
   int frame_height = cap.get(cv::CAP_PROP_FRAME_HEIGHT);

   int debug = 0;


   cv::Mat frame;
   bool dirty = true;
   // // frame = cv::imread("/home/rian/Downloads/IMG_20260216_090246644_AE~2.jpg");
   // frame = cv::imread("/home/rian/Downloads/IMG_20260219_132357937_AE~2.jpg");
   // cv::resize(frame, frame, {frame.cols / 4, frame.rows / 4}, cv::INTER_CUBIC);
   // auto t = frame.type();
   //
   // int frame_width = frame.cols;
   // int frame_height = frame.rows;


   // For android, game loop does not run on the main thread, and some of these
   // events need handled from within the java callback context. Handle them
   // directly rather than relying on the sdl event loop. This filter needs
   // set before any SDL events get generated, because it drops any events
   // that happen to be in the queue (such as window sizing events)
   SDL_SetEventFilter(EventFilter, nullptr);
   SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0"); // turn off mouse emulation
   SDL_SetHint(SDL_HINT_MOUSE_TOUCH_EVENTS, "0"); // turn off mouse emulation

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
                  frame = cv::imread("/home/rian/Downloads/IMG_20260216_090246644_AE~2.jpg");
                  cv::resize(frame, frame, {frame_width, frame_height}, cv::INTER_CUBIC);
                  break;
               case 2:
                  frame = cv::imread("/home/rian/Downloads/IMG_20260219_132357937_AE~2.jpg");
                  cv::resize(frame, frame, {frame_width, frame_height}, cv::INTER_CUBIC);
                  break;
               default:
                  debug = 0;
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
      if (!window.Paused() || dirty)
      {
         if (debug == 0)
            cap >> frame;
         if (!frame.empty())
            window.Updatetexture(frame);

         dirty = false;
      }

      window.Draw();
   }

   cap.release();

   // TscSampler::Dump();

   return 0;
}