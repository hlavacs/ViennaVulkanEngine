#pragma once


namespace vve::syswin::glfw {


	void init();
	void tick();
	void sync();
	void close();

	void framebufferResizeCallbackGLFW(GLFWwindow* window, int width, int height);
	void key_callbackGLFW(GLFWwindow* window, int key, int scancode, int action, int mods);
	void cursor_pos_callbackGLFW(GLFWwindow* window, double xpos, double ypos);
	void mouse_button_callback(GLFWwindow* window, int button, int action, int mods);
	void mouse_scroll_callback(GLFWwindow* window, double xoffset, double yoffset);

	std::vector<const char*> getRequiredInstanceExtensions();				//return GLFW Vulkan extensions
	bool createSurface(VkInstance instance, VkSurfaceKHR* pSurface);	//create a Vulkan surface
	bool windowShouldClose();											//winddow was closed by user?
	void waitForWindowSizeChange();										//wait for window size change to end
	void pollEvents() { glfwPollEvents(); };							//inject GLFW events into the callbacks

}


