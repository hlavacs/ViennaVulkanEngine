/**
* The Vienna Vulkan Engine
*
* (c) bei Helmut Hlavacs, University of Vienna
*
*/

#ifndef VEWINDOWGLFW_H
#define VEWINDOWGLFW_H

#define GLFW_INCLUDE_VULKAN

#include <GLFW/glfw3.h>

namespace ve
{
	class VEEngine;

	class VERenderer;

	class VERendererTurorial;

	/**
		*
		* \brief This class manages windows using the GLFW subsystem
		*
		*/
	class VEWindowGLFW : public VEWindow
	{
		friend VERenderer;
		friend VERendererTurorial;

	protected:
		GLFWwindow *m_window; ///<handle to the GLFW window

		//callbacks
		static void framebufferResizeCallbackGLFW(GLFWwindow *window, int width, int height);

		static void key_callbackGLFW(GLFWwindow *window, int key, int scancode, int action, int mods);

		static void cursor_pos_callbackGLFW(GLFWwindow *window, double xpos, double ypos);

		static void mouse_button_callback(GLFWwindow *window, int button, int action, int mods);

		static void mouse_scroll_callback(GLFWwindow *window, double xoffset, double yoffset);

		static void window_close_callback(GLFWwindow *window);

		virtual void initWindow(int width, int height); //create the window
		virtual std::vector<const char *>
			getRequiredInstanceExtensions(); //return GLFW Vulkan extensions
		virtual bool createSurface(VkInstance instance, VkSurfaceKHR *pSurface); //create a Vulkan surface
		virtual bool windowShouldClose()
		{
			return glfwWindowShouldClose(m_window) != 0;
		}; //winddow was closed by user?
		virtual void waitForWindowSizeChange(); //wait for window size change to end
		virtual void pollEvents()
		{
			glfwPollEvents();
		}; //inject GLFW events into the callbacks
		virtual void closeWindow(); //close window

	public:
		///Constructor
		VEWindowGLFW()
			: VEWindow() {};

		///Destructor
		~VEWindowGLFW() {};

		virtual VkExtent2D getExtent(); //width and height of window
		///<\returns the GLFW window handle.
		virtual GLFWwindow *getWindowHandle()
		{
			return m_window;
		};
	};

} // namespace ve

#endif
