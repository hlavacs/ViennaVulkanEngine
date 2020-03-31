#pragma once

/**
*
* \file
* \brief
*
* Details
*
*/

namespace vve::syswin::glfw {

	inline const std::string VE_SYSTEM_NAME = "VE SYSTEM WINDOW GLFW";
	inline VeHandle VE_SYSTEM_HANDLE = VE_NULL_HANDLE;

	void init();
	void update(VeHandle receiverID);
	void close(VeHandle receiverID);


	std::vector<const char*> getRequiredInstanceExtensions();			//return GLFW Vulkan extensions
	bool createSurface(VkInstance instance, VkSurfaceKHR* pSurface);	//create a Vulkan surface
	VkExtent2D getExtent();
	void waitForWindowSizeChange();

	void framebufferResizeCallbackGLFW(GLFWwindow* window, int width, int height);
	void key_callbackGLFW(GLFWwindow* window, int key, int scancode, int action, int mods);
	void cursor_pos_callbackGLFW(GLFWwindow* window, double xpos, double ypos);
	void mouse_button_callback(GLFWwindow* window, int button, int action, int mods);
	void mouse_scroll_callback(GLFWwindow* window, double xoffset, double yoffset);

}


