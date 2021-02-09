

#include "VEECS.h"


namespace vve {


	//-------------------------------------------------------------------------
	//reference pool

	template<typename T>
	VeComponentReferencePool<T>::VeComponentReferencePool(size_t r) {
		if (!this->init()) return;
		m_ref_index.reserve(r);
	};


	//-------------------------------------------------------------------------
	//entity manager

	VeEntityManager::VeEntityManager(size_t r) : VeSystem() {
		if (!this->init()) return;
		m_entity.reserve(r);
	}



}


