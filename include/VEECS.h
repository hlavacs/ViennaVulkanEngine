#ifndef VEENGINE_H
#define VEENGINE_H

#include <limits>
#include <typeinfo>
#include <typeindex>
#include "glm.hpp"
#include "gtc/quaternion.hpp"
#include "VGJS.h"
#include "VEContainer.h"
#include "VEUtil.h"
#include "VEComponent.h"

namespace vve {

	template<typename... Ts>
	struct VeEntity;

	template<typename T, typename... Ts>
	class VeSystem {
	protected:

	public:

	};

	struct VeEntityData;

	struct VeHandle {
		VeEntityData* m_ptr = nullptr;
		uint32_t	  m_counter = 0;
	};

	struct VeEntityData {
		uint32_t	  m_counter = 0;
		VeEntityData* m_next = nullptr;
	};


	template<typename T>
	class VeComponentPool {
	protected:
		using VeComponentPoolPtr = variant_type<to_ptr<to_class<VeComponentPool,VeComponentTypeList>>>;

		struct VeComponentData {
			T					m_component;
			VeComponentPoolPtr	m_next_pool;
		};

		VeVector<VeComponentData> m_data;
		
	public:

	};



	template <typename Seq>
	struct VeEntityManager;

	template<template <typename...> typename Seq, typename... Ts>
	class VeEntityManager<Seq<Ts...>> {
	public:
		VeEntityManager( size_t reserve) {
			if (m_init_counter > 0) return;
			auto cnt = m_init_counter.fetch_add(1);
			if (cnt > 0) return;
			m_data.reserve(1<<10);
		};
		std::optional<VeHandle> create();
		void erase(VeHandle& h);

	protected:
		static inline std::atomic<uint64_t> m_init_counter = 0;
		static inline VeTable<VeEntityData> m_data;
		static inline std::atomic<bool>		m_init = false;
	};

}

#endif
