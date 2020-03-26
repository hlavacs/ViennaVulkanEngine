#pragma once

/**
*
* \file
* \brief
*
* Details
*
*/


namespace vve::syssce {

	inline const std::string VE_SYSTEM_NAME = "VE SYSTEM SCENE";
	inline VeHandle VE_SYSTEM_HANDLE = VE_NULL_HANDLE;


	void init();
	void update(sysmes::VeMessageTableEntry e);
	void close(sysmes::VeMessageTableEntry e);


}


