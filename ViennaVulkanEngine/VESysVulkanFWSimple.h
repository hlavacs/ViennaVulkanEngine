#pragma once

/**
*
* \file
* \brief
*
* Details
*
*/


namespace vve::sysvul::fwsimple {

	inline const std::string VE_SYSTEM_NAME = "VE SYSTEM VULKAN RENDERER FWSIMPLE";
	inline VeHandle VE_SYSTEM_HANDLE = VE_NULL_HANDLE;

	void init();
	void update(VeHandle receiverID);
	void close(VeHandle receiverID);




}

