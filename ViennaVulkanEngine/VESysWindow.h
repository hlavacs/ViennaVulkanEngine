#pragma once


/**
*
* \file
* \brief
*
* Details
*
*/


namespace vve::syswin {

	inline const std::string VE_SYSTEM_NAME = "VE SYSTEM WINDOW";
	inline VeHandle VE_SYSTEM_HANDLE = VE_NULL_HANDLE;

	std::vector<const char*> getRequiredInstanceExtensions();
	bool createSurface(VkInstance instance, VkSurfaceKHR* pSurface);

	void init();
	void update(syseve::VeEventTableEntry e);
	void close(syseve::VeEventTableEntry e);
	void closeWin();

	void windowSizeChanged();
}


