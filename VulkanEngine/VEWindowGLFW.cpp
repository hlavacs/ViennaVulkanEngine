/**
* The Vienna Vulkan Engine
*
* (c) bei Helmut Hlavacs, University of Vienna
*
*/

#include "VEInclude.h"

namespace ve
{
	/**
		*
		* \brief Initialize a new GLFW window and set callbacks
		*
		* \param[in] WIDTH Width of the new window
		* \param[in] HEIGHT Height of the new window
		*
		*/
	void VEWindowGLFW::initWindow(int WIDTH, int HEIGHT)
	{
		glfwInit();
		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		m_window = glfwCreateWindow(WIDTH, HEIGHT, "Vienna Vulkan Engine", nullptr, nullptr);
		glfwSetWindowUserPointer(m_window, this);

		//set callbacks
		glfwSetFramebufferSizeCallback(m_window, framebufferResizeCallbackGLFW);
		glfwSetKeyCallback(m_window, key_callbackGLFW);
		glfwSetCursorPosCallback(m_window, cursor_pos_callbackGLFW);
		glfwSetMouseButtonCallback(m_window, mouse_button_callback);
		glfwSetScrollCallback(m_window, mouse_scroll_callback);
		glfwSetWindowCloseCallback(m_window, window_close_callback);

		//never miss these events
		glfwSetInputMode(m_window, GLFW_STICKY_KEYS, 1);
		glfwSetInputMode(m_window, GLFW_STICKY_MOUSE_BUTTONS, 1);
	}

	///\returns the required Vulkan instance extensions to interact with the local window system
	std::vector<const char *> VEWindowGLFW::getRequiredInstanceExtensions()
	{
		uint32_t glfwExtensionCount = 0;
		const char **glfwExtensions;
		glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

		std::vector<const char *> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

		return extensions;
	}

	///\returns the extent of the window
	VkExtent2D VEWindowGLFW::getExtent()
	{
		int width, height;
		glfwGetFramebufferSize(m_window, &width, &height);
		VkExtent2D actualExtent = {
			static_cast<uint32_t>(width),
			static_cast<uint32_t>(height) };
		return actualExtent;
	}

	/**
		* \brief Create a KHR surface for intercting with the GLFW window
		* \param[in] instance The Vulkan instance
		* \param[out] pSurface  Pointer to the created surface.
		* \returns boolean falggin success
		*/
	bool VEWindowGLFW::createSurface(VkInstance instance,
		VkSurfaceKHR *pSurface)
	{ //overload for other windowing systems!
		if (glfwCreateWindowSurface(instance, m_window, nullptr, pSurface) != VK_SUCCESS)
			return false;
		return true;
	}

	///Wait for the window to finish size change
	void VEWindowGLFW::waitForWindowSizeChange()
	{
		int width = 0, height = 0;
		while (width == 0 || height == 0)
		{
			glfwGetFramebufferSize(m_window, &width, &height);
			glfwWaitEvents();
		}
	}

	///Close the GLFW window
	void VEWindowGLFW::closeWindow()
	{
		glfwDestroyWindow(m_window);
		glfwTerminate();
	}

	/**
		*
		* \brief Callback to receive GLFW events from the keyboard
		*
		* This callback is called whenever the user does something the keyboard.
		* Events cause VEEvents to be created and sent to the VEEngine to be processed.
		* Pressing a key will actually cause two events. One indicating that the key has been pressed,
		* and another one indicating that the key is still pressed. The latter one is continuous until the
		* key is released again. Also it will not be processed immediately but only in the following
		* render loop, hence using the loop counter to schedule it for the future.
		*
		* \param[in] window Pointer to the GLFW window that caused this event
		* \param[in] key Key ID that caused this event
		* \param[in] scancode Not used
		* \param[in] action ID of what happened (press, release, continue)
		* \param[in] mods Not used
		*
		*/
	void VEWindowGLFW::key_callbackGLFW(GLFWwindow *window, int key, int scancode, int action, int mods)
	{
		auto app = reinterpret_cast<VEWindowGLFW *>(glfwGetWindowUserPointer(window));

		if (action == GLFW_REPEAT)
			return; //no need for this

		veEvent event(veEvent::VE_EVENT_SUBSYSTEM_GLFW, veEvent::VE_EVENT_KEYBOARD);
		event.idata1 = key;
		event.idata2 = scancode;
		event.idata3 = action;
		event.idata4 = mods;
		event.ptr = window;
		app->processEvent(event);

		if (action == GLFW_PRESS)
		{ //just started to press a key -> remember it in the engine

			event.notBeforeTime = getEnginePointer()->getLoopCount() + 1;
			event.idata3 = GLFW_REPEAT;
			event.lifeTime = veEvent::VE_EVENT_LIFETIME_CONTINUOUS;
			app->processEvent(event);
		}

		if (action == GLFW_RELEASE)
		{ //just released a key -> remove from the event list
			event.idata3 = GLFW_REPEAT;
			getEnginePointer()->deleteEvent(event);
		}
	}

	/**
		*
		* \brief Callback to receive GLFW events that mouse has moved
		*
		* \param[in] window Pointer to the GLFW window that caused this event
		* \param[in] xpos New x-position of the cursor
		* \param[in] ypos New y-position of the cursor
		*
		*/
	void VEWindowGLFW::cursor_pos_callbackGLFW(GLFWwindow *window, double xpos, double ypos)
	{
		auto app = reinterpret_cast<VEWindowGLFW *>(glfwGetWindowUserPointer(window));
		veEvent event(veEvent::VE_EVENT_SUBSYSTEM_GLFW, veEvent::VE_EVENT_MOUSEMOVE);
		event.fdata1 = (float)xpos;
		event.fdata2 = (float)ypos;
		event.ptr = window;

		app->processEvent(event);
	}

	/**
		*
		* \brief Callback to receive GLFW events from mouse buttons
		*
		* This callback is called whenever the user does something the mouse buttons.
		* Events cause VEEvents to be created and sent to the VEEngine to be processed.
		* Pressing a mouse button will actually cause two events. One indicating that the button has been pressed,
		* and another one indicating that the button is still pressed. The latter one is continuous until the
		* button is released again. Also it will not be processed immediately but only in the following
		* render loop, hence using the loop counter to schedule it for the future.
		*
		* \param[in] window Pointer to the GLFW window that caused this event
		* \param[in] button ID of the mouse button that was interacted with
		* \param[in] action ID of the action (press, release, repeat)
		* \param[in] mods Not used
		*
		*/
	void VEWindowGLFW::mouse_button_callback(GLFWwindow *window, int button, int action, int mods)
	{
		auto app = reinterpret_cast<VEWindowGLFW *>(glfwGetWindowUserPointer(window));

		if (action == GLFW_REPEAT)
			return; //no need for this

		veEvent event(veEvent::VE_EVENT_SUBSYSTEM_GLFW, veEvent::VE_EVENT_MOUSEBUTTON);
		event.idata1 = button;
		event.idata3 = action;
		event.idata4 = mods;
		event.ptr = window;
		app->processEvent(event);

		if (action == GLFW_PRESS)
		{ //just started to press a key -> remember it in the engine
			event.notBeforeTime = getEnginePointer()->getLoopCount() + 1;
			event.idata3 = GLFW_REPEAT;
			event.lifeTime = veEvent::VE_EVENT_LIFETIME_CONTINUOUS;
			app->processEvent(event);
		}

		if (action == GLFW_RELEASE)
		{ //just released a key -> remove from pressed keys list
			event.idata3 = GLFW_REPEAT;
			getEnginePointer()->deleteEvent(event);
		}
	}

	/**
		*
		* \brief Callback to receive GLFW events from mouse scroll wheel
		*
		* \param[in] window Pointer to the GLFW window that caused this event
		* \param[in] xoffset Xoffset of scroll wheel
		* \param[in] yoffset Yoffset of scroll wheel
		*
		*/
	void VEWindowGLFW::mouse_scroll_callback(GLFWwindow *window, double xoffset, double yoffset)
	{
		auto app = reinterpret_cast<VEWindowGLFW *>(glfwGetWindowUserPointer(window));
		veEvent event(veEvent::VE_EVENT_SUBSYSTEM_GLFW, veEvent::VE_EVENT_MOUSESCROLL);
		event.fdata1 = (float)xoffset;
		event.fdata2 = (float)yoffset;
		event.ptr = window;

		app->processEvent(event);
	}

	void VEWindowGLFW::window_close_callback(GLFWwindow *window)
	{
		getEnginePointer()->end();
	}

	/**
		*
		* \brief Callback to receive GLFW events from window size change
		*
		* \param[in] window Pointer to the GLFW window that caused this event
		* \param[in] width New width of the window
		* \param[in] height New height of the window
		*
		*/
	void VEWindowGLFW::framebufferResizeCallbackGLFW(GLFWwindow *window, int width, int height)
	{
		auto app = reinterpret_cast<VEWindowGLFW *>(glfwGetWindowUserPointer(window));
		app->windowSizeChanged();
	}

} // namespace ve