#pragma once


namespace vve::syswin {

	std::vector<const char*> getRequiredInstanceExtensions();
	bool createSurface(VkInstance instance, VkSurfaceKHR* pSurface);

	void init();
	void tick();
	void sync();
	void close();


}


