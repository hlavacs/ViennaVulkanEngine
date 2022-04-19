/**
* The Vienna Vulkan Engine
*
* (c) bei Helmut Hlavacs, University of Vienna
*
*/

#include "VEInclude.h"

namespace ve
{
	VEWindow *g_pVEWindowSingleton = nullptr; ///<Singleton pointer to the only VEEngine instance

	VEWindow::VEWindow()
	{
		g_pVEWindowSingleton = this;
	}

	/**
		* \brief Communicate to the engine that the window size has changed
		*/
	void VEWindow::windowSizeChanged()
	{
		getEnginePointer()->windowSizeChanged();
	};

	/**
		* \brief Send a window event to the engine
		*/
	void VEWindow::processEvent(veEvent event)
	{
		getEnginePointer()->addEvent(event);
	};

} // namespace ve
