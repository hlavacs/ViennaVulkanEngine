#include <cstdint>
#include <functional>
#include <string>

import VEEngine;

/**
 * @file
 * @brief Regression tests for the public `Handle` type.
 */

/**
 * @brief Executes the handle regression tests.
 * @return Process exit code expected by the lightweight test runner.
 */
int main() {
    {
        constexpr vve::Handle handle{}; // Default construction must yield the invalid sentinel.
        static_assert(sizeof(vve::Handle) == 8);
        static_assert(handle.value() == vve::Handle::invalid_value);
        static_assert(!handle.isValid());
        static_assert(vve::Handle::invalid() == handle);
    }

    {
        // Construction from parts must preserve the exact bit layout.
        constexpr vve::Handle handle{0x89ABCDEFu, 0x01234567u};
        static_assert(handle.low() == 0x89ABCDEFu);
        static_assert(handle.high() == 0x01234567u);
        static_assert(static_cast<std::uint64_t>(handle) == 0x0123456789ABCDEFull);
        static_assert(handle.isValid());

        constexpr auto parts = handle.parts();
        static_assert(parts[0] == 0x89ABCDEFu);
        static_assert(parts[1] == 0x01234567u);
    }

    {
        // Construction from a raw 64-bit value must preserve part accessors.
        constexpr vve::Handle handle{0x0123456789ABCDEFull};
        static_assert(handle.low() == 0x89ABCDEFu);
        static_assert(handle.high() == 0x01234567u);
    }

    {
        // Hash-based construction is the public path used by many engine
        const std::string name = "mesh/albedo"; // systems to derive stable handles from names.
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
