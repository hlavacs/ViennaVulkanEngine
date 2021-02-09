

#include "VEECS.h"


namespace vve {


	//-------------------------------------------------------------------------
	//reference pool



	//-------------------------------------------------------------------------
	//entity manager

	VeEntityManager::VeEntityManager(size_t r) : VeSystem() {
		if (!this->init()) return;
		m_entity.reserve(r);
	}



}


