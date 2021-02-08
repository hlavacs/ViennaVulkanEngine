

#include "VEECS.h"


namespace vve {

	//-------------------------------------------------------------------------
	//component pool

	template<typename T>
	VeComponentPool<T>::VeComponentPool() {
		if (!this->init()) return;
	}

	//-------------------------------------------------------------------------
	//system


	//-------------------------------------------------------------------------
	//entity manager

	VeEntityManager::VeEntityManager( size_t reserve) : VeSystem() {
		if (!this->init()) return;
		m_entity.reserve(reserve);
	}

	template<typename E>
	VeHandle VeEntityManager::create() {
		auto h = m_entity.push_back({});
		//auto idx = h.value();

		return h;
	}

	void VeEntityManager::erase(VeHandle& h) {

	}


}


