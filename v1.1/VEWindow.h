/**
* The Vienna Vulkan Engine
*
* (c) bei Helmut Hlavacs, University of Vienna
*
*/

#ifndef VEWINDOW_H
#define VEWINDOW_H

#include <vector>

#ifndef getWindowPointer
#define getWindowPointer() g_pVEWindowSingleton
#endif

namespace ve
{
	class VEEngine;

	class VERendererForward;

	class VEWindow;

	extern VEWindow *g_pVEWindowSingleton; ///<Pointer to the only class instance

	/**
		* \brief Base class for managing windows
		*
		* This base class defines a set of interfaces for managing windows. The engine will use only one window,
		* and for interacting with the local windoing system, a derived class must be used.
		*
		*/
	class VEWindow
	{
		friend VEEngine;
		friend VERendererForward;

	protected:
		/**
			* \brief Initialize the window
			* \param[in] width Width fo the new window
			* \param[in] height Height fo the new window
			*/
		virtual void initWindow(int width, int height) {};

		///\returns a list of required instance extensions for interacting with the local window system
		virtual std::vector<const char *> getRequiredInstanceExtensions()
		{
			return {};
		};

		/**
			* \brief Create a KHR surface for intercting with the window
			* \param[in] instance The Vulkan instance
			* \param[out] pSurface  Pointer to the created surface.
			* \returns boolean falggin success
			*/
		virtual bool createSurface(VkInstance instance, VkSurfaceKHR *pSurface)
		{
			return false;
		};

		///Poll whether there have been window events, inject them into the message loop
		virtual void pollEvents() {};

		virtual void processEvent(veEvent event);

		void windowSizeChanged();

		///Wait for the window to finish size change
		virtual void waitForWindowSizeChange() {};

		///\returns whether window wants to be closed
		virtual bool windowShouldClose()
		{
			return false;
		};

		///Close the window
		virtual void closeWindow() {};

	public:
		///Constructor
		VEWindow();

		///Destructor
		~VEWindow() {};

		///\returns the extent of the window.
		virtual VkExtent2D getExtent()
		{
			return VkExtent2D() = { 0, 0 };
		};
	};

} // namespace ve

#endif
