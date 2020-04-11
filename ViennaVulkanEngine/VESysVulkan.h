#pragma once

/**
*
* \file
* \brief
*
* Details
*
*/


//use this macro to check the function result, if its not VK_SUCCESS then return the error
#define VECHECKRESULT(x) { \
		VkResult retval = (x); \
		assert (retval == VK_SUCCESS); \
	}


namespace vve::sysvul {

	inline const std::string VE_SYSTEM_NAME = "VE SYSTEM VULKAN";
	inline VeHandle VE_SYSTEM_HANDLE = VE_NULL_HANDLE;

	void init();
	void preupdate(VeHandle receiverID);
	void update(VeHandle receiverID);
	void postupdate(VeHandle receiverID);
	void close(VeHandle receiverID);


	void windowSizeChanged();

}

