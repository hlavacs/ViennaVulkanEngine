

/**
The Vienna Vulkan Engine
VEEventListenerGLFW.h
Purpose: Declare VEEventListenerNuklear class

@author Helmut Hlavacs
@version 1.0
@date 2019
*/

#ifndef VEEVENTLISTENERNUKLEARDEBUG_H
#define VEEVENTLISTENERNUKLEARDEBUG_H

namespace ve
{
	/**
		*
		* \brief This event listener is used for displaying error messages using Nuklear
		*
		* After a fatal error, this event listener is used to display the error message
		* before the engine closes down.
		*
		*/
	class VEEventListenerNuklearDebug : public VEEventListener
	{
	protected:
		virtual void onDrawOverlay(veEvent event);

	public:
		///Constructor of class VEEventListenerNuklearError
		VEEventListenerNuklearDebug(std::string name)
			: VEEventListener(name) {};

		///Destructor of class VEEventListenerNuklearError
		virtual ~VEEventListenerNuklearDebug() {};
	};

} // namespace ve

#endif
