// foxscript
// Copyright (C) 2026 thewierdnut
// 
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
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

