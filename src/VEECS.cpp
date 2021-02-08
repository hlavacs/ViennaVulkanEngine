

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

	template<typename T>
	VeEntityManager<T>::VeEntityManager( size_t reserve) : VeSystem() {
		if (!this->init()) return;
		m_entity.reserve(reserve);
	}

	template<typename T>
	VeHandle VeEntityManager<T>::create() {
		auto h = m_entity.add({});
		if (h.has_value()) {
			auto idx = h.value();

		}

		return {};
	}

	template<typename T>
	void VeEntityManager<T>::erase(VeHandle& h) {

	}


}


