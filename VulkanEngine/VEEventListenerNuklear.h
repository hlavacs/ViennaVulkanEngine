/**
The Vienna Vulkan Engine
VEEventListenerGLFW.h
Purpose: Declare VEEventListenerNuklear class

@author Helmut Hlavacs
@version 1.0
@date 2019
*/

#ifndef VEEVENTLISTENERNUKLEAR_H
#define VEEVENTLISTENERNUKLEAR_H

namespace ve
{
	/**
		*
		* \brief Example Nuklear GUI
		*
		* This is an example for a Nuklear GUI, implemented in an event listener
		*
		*/
	class VEEventListenerNuklear : public VEEventListener
	{
	protected:
		virtual void onDrawOverlay(veEvent event);

	public:
		enum
		{
			UP,
			DOWN
		}; ///<example data

		float m_bg_color[4]; ///<example data
		uint8_t m_orientation = UP; ///<example data
		int m_zoom = 0; ///<example data
		struct nk_color m_background; ///<example data

		///Constructor of class VEEventListenerNuklear
		VEEventListenerNuklear(std::string name)
			: VEEventListener(name) {};

		///Destructor of class VEEventListenerNuklear
		virtual ~VEEventListenerNuklear() {};
	};

} // namespace ve

#endif
