#include "ZoomGesture.hh"

#include <cmath>


namespace {
   static constexpr float FINGER_EPSILON = 20;

   int DistanceSquared(const SDL_Point& p1, const SDL_Point& p2)
   {
      int dx = p1.x - p2.x;
      int dy = p1.y - p2.y;
      return dx * dx + dy * dy;
   }
}

/**
 * Track fingerdown events to check if they are zoom gestures
 * @param pos The finger position in pixel coordinates
 * @param fid The finger id
 * @return false for the first finger, true for the second.
 */
bool ZoomGesture::FingerDown(const SDL_Point& pos, int fid)
{
	if(m_finger1_id == fid)
	{
		// shouldn't happen. reset state
		m_finger_start = m_finger1 = pos;
		m_first_canceled = false;
		m_finger1_was_first = m_finger2_id == -1;
		return m_finger2_id != -1;
	}
	else if(m_finger2_id == fid)
	{
		// shouldn't happen. reset state
		m_finger_start = m_finger2 = pos;
		m_first_canceled = false;
		return m_finger1_id != -1;
	}
	else if(m_finger1_id == -1)
	{
		m_finger1_id = fid;
		m_finger_start = m_finger1 = pos;
		m_first_canceled = false;
		m_finger1_was_first = m_finger2_id == -1;
		return m_finger2_id != -1;
	}
	else if(m_finger2_id == -1)
	{
		m_finger2_id = fid;
		m_finger_start = m_finger2 = pos;
		m_first_canceled = false;
		m_finger1_was_first = m_finger1_id != -1;
		return m_finger1_id != -1;
	}
	// else both fingers are already set. This is not part of a pinch gesture.

	return false;
}



/**
 * Track fingermove events to check if they are zoom gestures
 * @param pos The finger position in pixel coordinates
 * @param fid The finger id
 * @return true if we are positive this is a zoom gesture
 */
bool ZoomGesture::FingerMove(const SDL_Point& pos, int fid)
{
	if(fid != m_finger1_id && fid != m_finger2_id)
		return false;
	
	// If we are only tracking one finger, then make sure it doesn't move too
	// far
	if(m_finger1_id == -1 || m_finger2_id == -1)
	{
      
		if(DistanceSquared(m_finger_start, pos) > FINGER_EPSILON * FINGER_EPSILON)
		{
			// The finger moved too far. reset it.
			(m_finger1_id == -1 ? m_finger2_id : m_finger1_id) = -1;
		}
		else
		{
			(m_finger1_id == fid ? m_finger1 : m_finger2) = pos;
		}
		return false;
	}
	else
	{
		// we are tracking both fingers. This is a zoom event.
		// compare the previous distance to the new distance. Defer the square
		// root operation until after the divide, so that we only have to do it
		// once.
		// previous distance
		float d1s = DistanceSquared(m_finger1, m_finger2);
		// current distance from pos to the "other" finger
		const SDL_Point& other = m_finger1_id == fid ? m_finger2 : m_finger1;
		float d2s = DistanceSquared(pos, other);
		m_zoom = sqrt(d2s/d1s);

		// how far did we drag as well as zoom?
		SDL_Point oldCenter = {
         (m_finger1.x + m_finger2.x) / 2,
         (m_finger1.y + m_finger2.y) / 2,
      };
		SDL_Point newCenter = {
         (pos.x + other.x) / 2,
         (pos.y + other.y) / 2
      };
		m_delta = {
         newCenter.x - oldCenter.x,
         newCenter.y - oldCenter.y
      };

		(m_finger1_id == fid ? m_finger1 : m_finger2) = pos;
		return true;
	}
	return false;
}



/**
 * Track fingerup events to check if they are zoom gestures
 * @param pos The finger position in pixel coordinates
 * @param fid The finger id
 * @return true if we are positive this is a zoom gesture
 */
bool ZoomGesture::FingerUp(const SDL_Point& pos, int fid)
{
	// The fingerdown event returns false for the first finger, but true for the
	// second. Duplicate that logic here.
	if(m_finger2_id == fid)
	{
		m_finger2_id = -1;
		return m_first_canceled || m_finger1_was_first;
	}
	else if(m_finger1_id == fid)
	{
		m_finger1_id = -1;
		return m_first_canceled || !m_finger1_was_first;
	}
	return false;
}