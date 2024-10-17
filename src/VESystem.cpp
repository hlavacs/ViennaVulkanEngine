

#include "VESystem.h"


namespace vve {

   	template<ArchitectureType ATYPE>
    System<ATYPE>::System( Engine<ATYPE>& engine) : m_engine(engine) {};

   	template<ArchitectureType ATYPE>
    System<ATYPE>::~System(){};

   	template<ArchitectureType ATYPE>
    void System<ATYPE>::receiveMessage(Message message) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_messages.push_back(message);
    };

   	template<ArchitectureType ATYPE>
    void System<ATYPE>::onMessage(Message message) {
        switch (message.m_type) {
        case MessageType::FRAME_START:
            onFrameStart(message);
            break;
        case MessageType::UPDATE:
            onUpdate(message);
            break;
        case MessageType::FRAME_END:
            onFrameEnd(message);
            break;
        case MessageType::DELETED:
            onDelete(message);
            break;
        case MessageType::DRAW_GUI:
            onDrawGUI(message);
            break;
        case MessageType::MOUSE_MOVE:
            onMouseMove(message);
            break;
        case MessageType::MOUSE_BUTTON_DOWN:
            onMouseButtonDown(message);
            break;
        case MessageType::MOUSE_BUTTON_UP:
            onMouseButtonUp(message);
            break;
        case MessageType::MOUSE_BUTTON_REPEAT:
            onMouseButtonRepeat(message);
            break;
        case MessageType::MOUSE_WHEEL:
            onMouseWheel(message);
            break;
        case MessageType::KEY_DOWN:
            onKeyDown(message);
            break;
        case MessageType::KEY_UP:
            onKeyUp(message);
            break;
        case MessageType::KEY_REPEAT:
            onKeyRepeat(message);
            break;
        default:
            break;
        }
    };
    
   	template<ArchitectureType ATYPE>
    void System<ATYPE>::onFrameStart(Message message){};

   	template<ArchitectureType ATYPE>
    void System<ATYPE>::onUpdate(Message message){};

   	template<ArchitectureType ATYPE>
    void System<ATYPE>::onFrameEnd(Message message){};

   	template<ArchitectureType ATYPE>
    void System<ATYPE>::onDelete(Message message){};

   	template<ArchitectureType ATYPE>
    void System<ATYPE>::onDrawGUI(Message message){};

   	template<ArchitectureType ATYPE>
    void System<ATYPE>::onMouseMove(Message message){};

   	template<ArchitectureType ATYPE>
    void System<ATYPE>::onMouseButtonDown(Message message){};

   	template<ArchitectureType ATYPE>
    void System<ATYPE>::onMouseButtonUp(Message message){};

   	template<ArchitectureType ATYPE>
    void System<ATYPE>::onMouseButtonRepeat(Message message){};

   	template<ArchitectureType ATYPE>
    void System<ATYPE>::onMouseWheel(Message message){};

   	template<ArchitectureType ATYPE>
    void System<ATYPE>::onKeyDown(Message message){};

   	template<ArchitectureType ATYPE>
    void System<ATYPE>::onKeyUp(Message message){};

   	template<ArchitectureType ATYPE>
    void System<ATYPE>::onKeyRepeat(Message message){};

    template class System<ArchitectureType::SEQUENTIAL>;
    template class System<ArchitectureType::PARALLEL>;

};   // namespace vve


