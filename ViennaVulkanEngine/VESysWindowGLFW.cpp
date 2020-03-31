/**
*
* \file
* \brief
*
* Details
*
*/

#include "VEDefines.h"
#include "VESysMessages.h"
#include "VESysEngine.h"
#include "VESysWindow.h"
#include "VESysWindowGLFW.h"

namespace vve::syswin::glfw {

	GLFWwindow* g_window;	///<handle to the GLFW window

	const uint32_t WIDTH = 800;
	const uint32_t HEIGHT = 600;
	   	  
	void init() {
		syseng::registerEntity(VE_SYSTEM_NAME);
		VE_SYSTEM_HANDLE = syseng::getEntityHandle(VE_SYSTEM_NAME);

		glfwInit();
		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		g_window = glfwCreateWindow(WIDTH, HEIGHT, "Vienna Vulkan Engine", nullptr, nullptr);
		glfwSetWindowUserPointer(g_window, nullptr);

		//set callbacks
		glfwSetFramebufferSizeCallback(g_window, framebufferResizeCallbackGLFW);
		glfwSetKeyCallback(g_window, key_callbackGLFW);
		glfwSetCursorPosCallback(g_window, cursor_pos_callbackGLFW);
		glfwSetMouseButtonCallback(g_window, mouse_button_callback);
		glfwSetScrollCallback(g_window, mouse_scroll_callback);

		//never miss these events
		glfwSetInputMode(g_window, GLFW_STICKY_KEYS, 1);
		glfwSetInputMode(g_window, GLFW_STICKY_MOUSE_BUTTONS, 1);
	}

	bool g_windowSizeChanged = false;

	void update(VeHandle receiverID) {
		glfwPollEvents();		//inject GLFW events into the callbacks

		if (g_windowSizeChanged) {
			waitForWindowSizeChange();
			g_windowSizeChanged = false;
			windowSizeChanged();
		}

		if (glfwWindowShouldClose(g_window) != 0) {
			closeWin();
			return;
		}
	}


	void close(VeHandle receiverID) {
		glfwDestroyWindow(g_window);
		glfwTerminate();
	}



	///\returns the required Vulkan instance extensions to interact with the local window system
	std::vector<const char*> getRequiredInstanceExtensions() {
		uint32_t glfwExtensionCount = 0;
		const char** glfwExtensions;
		glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
		std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);
		return extensions;
	}


	///\returns the extent of the window
	VkExtent2D getExtent() {
		int width, height;
		glfwGetFramebufferSize(g_window, &width, &height);
		VkExtent2D actualExtent = {
			static_cast<uint32_t>(width),
			static_cast<uint32_t>(height)
		};
		return actualExtent;
	}

	/**
	* \brief Create a KHR surface for interacting with the GLFW window
	* \param[in] instance The Vulkan instance
	* \param[out] pSurface  Pointer to the created surface.
	* \returns boolean falggin success
	*/
	bool createSurface(VkInstance instance, VkSurfaceKHR* pSurface) {			//overload for other windowing systems!
		if (glfwCreateWindowSurface(instance, g_window, nullptr, pSurface) != VK_SUCCESS) return false;
		return true;
	}

	VeIndex g_windowMessageID = 0;
	
	/**
	*
	* \brief Callback to receive GLFW events from the keyboard
	*
	* This callback is called whenever the user does something the keyboard.
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
	void key_callbackGLFW(GLFWwindow* window, int key, int scancode, int action, int mods) {
		if (action == GLFW_REPEAT) 
			return;

		sysmes::VeMessageTableEntry ev;
		ev.m_type = sysmes::VeMessageType::VE_MESSAGE_TYPE_KEYBOARD;
		ev.m_senderID = VE_SYSTEM_HANDLE;

		ev.m_key_button = key < 0 ? VE_NULL_INDEX : (uint32_t)key;
		ev.m_scancode	= scancode < 0 ? VE_NULL_INDEX : (uint32_t)scancode;
		ev.m_action		= action < 0 ? VE_NULL_INDEX : (uint32_t)action;
		ev.m_mods		= mods < 0 ? VE_NULL_INDEX : (uint32_t)mods;

		sysmes::sendMessage(ev);
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
	void cursor_pos_callbackGLFW(GLFWwindow* window, double xpos, double ypos) {
		sysmes::VeMessageTableEntry ev;
		ev.m_type = sysmes::VeMessageType::VE_MESSAGE_TYPE_MOUSEMOVE;
		ev.m_senderID = VE_SYSTEM_HANDLE;

		ev.m_x = xpos;
		ev.m_y = ypos;

		sysmes::sendMessage(ev);
	}

	/**
	*
	* \brief Callback to receive GLFW events from mouse buttons
	*
	* This callback is called whenever the user does something the mouse buttons.
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
	void mouse_button_callback(GLFWwindow* window, int button, int action, int mods) {
		sysmes::VeMessageTableEntry ev;
		ev.m_type = sysmes::VeMessageType::VE_MESSAGE_TYPE_MOUSEBUTTON;
		ev.m_senderID = VE_SYSTEM_HANDLE;

		ev.m_key_button = button < 0 ? VE_NULL_INDEX : (uint32_t)button;
		ev.m_action = action < 0 ? VE_NULL_INDEX : (uint32_t)action;
		ev.m_mods = mods < 0 ? VE_NULL_INDEX : (uint32_t)mods;

		sysmes::sendMessage(ev);
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
	void mouse_scroll_callback(GLFWwindow* window, double xoffset, double yoffset) {
		sysmes::VeMessageTableEntry ev;
		ev.m_type = sysmes::VeMessageType::VE_MESSAGE_TYPE_MOUSESCROLL;
		ev.m_senderID = VE_SYSTEM_HANDLE;

		ev.m_x = xoffset;
		ev.m_y = yoffset;

		sysmes::sendMessage(ev);
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
	void framebufferResizeCallbackGLFW(GLFWwindow* window, int width, int height) {
		g_windowSizeChanged = true;
	}


	void waitForWindowSizeChange() {
		int w = 0, h = 0;
		while (w == 0 || h == 0) {
			glfwGetFramebufferSize(g_window, &w, &h);
			glfwWaitEvents();
		}
	}

}

