
#include "VEECS.h"


namespace vve {

	template<template <typename...> typename Seq, typename... Ts>
	std::optional<VeHandle> VeEntityManager<Seq<Ts...>>::create() {
		auto h = m_data.add({});
		if (h.has_value()) {
			auto idx = h.value();

		}

		return {};
	}

	template<template <typename...> typename Seq, typename... Ts>
	void VeEntityManager<Seq<Ts...>>::erase(VeHandle& h) {

	}


}


