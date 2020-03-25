#pragma once

/**
*
* \file
* \brief
*
* Details
*
*/



namespace vve::sysass {

	inline const std::string VE_SYSTEM_NAME = "VE SYSTEM ASSETS";
	inline VeHandle VE_SYSTEM_HANDLE = VE_NULL_HANDLE;


	void init();
	void update();
	void cleanUp();
	void close();


}



