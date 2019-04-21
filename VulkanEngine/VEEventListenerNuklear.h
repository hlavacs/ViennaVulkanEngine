/**
The Vienna Vulkan Engine
VEEventListenerGLFW.h
Purpose: Declare VEEventListenerGLFW class

@author Helmut Hlavacs
@version 1.0
@date 2019
*/

#pragma once

namespace ve {

	/**
	*
	* \brief An event listener for GLFW events
	*
	* This is the default event listener for GLFW events. Basically it steers the standard camera around,
	* just as you would expect from a first person shooter.
	*
	*/
	class VEEventListenerNuklear : public VEEventListener {

	protected:
		virtual void onFrameEnded(veEvent event);

	public:
		enum { UP, DOWN };

		struct overlay_settings {
			float bg_color[4];
			uint8_t orientation;
			int zoom;
		};

		overlay_settings settings;
		struct nk_color background;

		///Constructor of class VEEventListenerNuklear
		VEEventListenerNuklear(std::string name) : VEEventListener(name) { };
		///Destructor of class VEEventListenerNuklear
		virtual ~VEEventListenerNuklear() {};
	};
}

