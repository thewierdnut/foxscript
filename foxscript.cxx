#include "src/MainWindow.hh"


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
   //frame = cv::imread("/home/rian/Downloads/IMG_20260219_132357937_AE~2.jpg");
   cv::resize(frame, frame, {frame.cols / 4, frame.rows / 4});
   auto t = frame.type();
   
   bool dirty = true;

   int frame_width = frame.cols;
   int frame_height = frame.rows;

   

   MainWindow window;

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
               // window.Stamp();
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

   // TscSampler::Dump();

   return 0;
}