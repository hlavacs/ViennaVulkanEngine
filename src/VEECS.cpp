

#include "VEECS.h"


namespace vve {

	//-------------------------------------------------------------------------
	//component pool



	//-------------------------------------------------------------------------
	//system


	//-------------------------------------------------------------------------
	//entity manager

	VeEntityManager::VeEntityManager( size_t r) : VeSystem() {
		if (!this->init()) return;
		m_entity.reserve(r);
	}

	template<typename T>
	void VeEntityManager::erase(VeHandle_t<T>& h) {

	}


}


