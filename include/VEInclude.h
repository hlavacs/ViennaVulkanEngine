#pragma once

#include <cstdint>
#include <mutex>


namespace vve {

    enum class ArchitectureType : uint32_t {
        SEQUENTIAL = 0,
        PARALLEL
    };	

    struct Empty {};

    template<ArchitectureType ATYPE>
    using Mutex = std::conditional_t<ATYPE == ArchitectureType::SEQUENTIAL, Empty, std::mutex>;

}



