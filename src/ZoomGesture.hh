#pragma once

#include <SDL2/SDL.h>

/**
 * Recognizes zoom gestures.
 */
class ZoomGesture
{
public:
   bool FingerDown(const SDL_Point& pos, int fid);
   bool FingerMove(const SDL_Point& pos, int fid);
   bool FingerUp(const SDL_Point& pos, int fid);

   float Zoom() const { return m_zoom; }
   const SDL_Point& CenterDelta() const { return m_delta; }
   int FidToCancel() const { m_first_canceled = true; return m_finger1_was_first ? m_finger1_id : m_finger2_id; }

private:
   int m_finger1_id = -1;
   int m_finger2_id = -1;
   bool m_finger1_was_first = false;
   mutable bool m_first_canceled = false;
   SDL_Point m_finger_start;
   SDL_Point m_finger1;
   SDL_Point m_finger2;
   SDL_Point m_delta;
   float m_zoom = 1.0;
};

