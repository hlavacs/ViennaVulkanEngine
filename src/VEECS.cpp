

#include "VEECS.h"


namespace vve {

	//-------------------------------------------------------------------------
	//component pool

	template<typename T>
	VeComponentPool<T>::VeComponentPool() {
		if (!base_crtp::init()) return;
	}

	//-------------------------------------------------------------------------
	//system


	//-------------------------------------------------------------------------
	//entity manager

	template<typename T>
	VeEntityManager<T>::VeEntityManager( size_t reserve) : VeSystem() {
		if (!base_crtp::init()) return;
		m_data.reserve(reserve);
	}

	template<typename T>
	std::optional<VeHandle> VeEntityManager<T>::create() {
		auto h = m_data.add({});
		if (h.has_value()) {
			auto idx = h.value();

		}

		return {};
	}

	template<typename T>
	void VeEntityManager<T>::erase(VeHandle& h) {

	}


}


