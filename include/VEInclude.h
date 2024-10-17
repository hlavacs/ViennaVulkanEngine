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

    enum MessageType {
        FRAME_START = 0,
        UPDATE,
        FRAME_END,
        DELETED,
        DRAW_GUI,
        MOUSE_MOVE,
        MOUSE_BUTTON_DOWN,
        MOUSE_BUTTON_UP,
        MOUSE_BUTTON_REPEAT,
        MOUSE_WHEEL,
        KEY_DOWN,
        KEY_UP,
        KEY_REPEAT,
        LAST
    };
    
}



