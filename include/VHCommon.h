#pragma once
#include <type_traits>

namespace vvh {
	// Converts enum class values to underlying type
	template<typename E>
	constexpr std::underlying_type_t<E> ToUnderlying(E e) noexcept {
		return static_cast<std::underlying_type_t<E>>(e);
	}
} // namespace vvh