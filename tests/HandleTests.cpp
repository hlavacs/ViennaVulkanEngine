#include <cstdint>
#include <functional>
#include <string>

import VEEngine;

int main() {
    {
        constexpr vve::Handle handle{0x89ABCDEFu, 0x01234567u};
        static_assert(handle.low() == 0x89ABCDEFu);
        static_assert(handle.high() == 0x01234567u);
        static_assert(static_cast<std::uint64_t>(handle) == 0x0123456789ABCDEFull);

        constexpr auto parts = handle.parts();
        static_assert(parts[0] == 0x89ABCDEFu);
        static_assert(parts[1] == 0x01234567u);
    }

    {
        constexpr vve::Handle handle{0x0123456789ABCDEFull};
        static_assert(handle.low() == 0x89ABCDEFu);
        static_assert(handle.high() == 0x01234567u);
    }

    {
        const std::string name = "mesh/albedo";
        const vve::Handle handle{name};
        const auto expected = static_cast<std::uint64_t>(std::hash<std::string>{}(name));

        if (handle.value() != expected) {
            return 1;
        }

        const auto from_parts = vve::Handle::fromParts(handle.low(), handle.high());
        if (from_parts != handle) {
            return 2;
        }

        const auto from_hash = vve::Handle::fromHash(name);
        if (from_hash != handle) {
            return 3;
        }
    }

    return 0;
}
