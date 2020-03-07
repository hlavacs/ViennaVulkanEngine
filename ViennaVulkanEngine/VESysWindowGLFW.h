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


	void init();
	void tick();
	void close();

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


