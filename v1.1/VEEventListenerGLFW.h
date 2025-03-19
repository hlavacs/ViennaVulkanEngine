/**
	The Vienna Vulkan Engine
	VEEventListenerGLFW.h
	Purpose: Declare VEEventListenerGLFW class

	@author Helmut Hlavacs
	@version 1.0
	@date 2019
*/

#ifndef VEEVENTLISTENERGLFW_H
#define VEEVENTLISTENERGLFW_H

namespace ve
{
	/**
		*
		* \brief An event listener for GLFW events
		*
		* This is the default event listener for GLFW events. Basically it steers the standard camera around,
		* just as you would expect from a first person shooter.
		*
		*/
	class VEEventListenerGLFW : public VEEventListener
	{
	protected:
		bool m_usePrevCursorPosition = false; ///<Can I use the previous cursor position for moving the camera?
		bool m_rightButtonClicked = false; ///<Is the left button currently clicked?
		float m_cursorPrevX = 0; ///<Previous X position of cursor
		float m_cursorPrevY = 0; ///<Previous Y position of cursor
		bool m_makeScreenshot = false; ///<Should I make a screeshot after frame is done?
		bool m_makeScreenshotDepth = false; ///<Should I make a screeshot after frame is done?
		uint32_t m_numScreenshot = 0; ///<Screenshot ID

		virtual void onFrameEnded(veEvent event);

		virtual bool onKeyboard(veEvent event);

		virtual bool onMouseMove(veEvent event);

		virtual bool onMouseButton(veEvent event);

		virtual bool onMouseScroll(veEvent event);

	public:
		///Constructor
		VEEventListenerGLFW(std::string name)
			: VEEventListener(name) {};

		///Destructor
		virtual ~VEEventListenerGLFW() {};
	};
} // namespace ve

#endif
