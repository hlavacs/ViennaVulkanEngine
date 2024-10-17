
#include <cassert>
#include "VESystem.h"


namespace vve {

   	template<ArchitectureType ATYPE>
    System<ATYPE>::System( Engine<ATYPE>& engine) : m_engine(engine) {
        m_onFunctions[FRAME_START] = [this](Message message){ onFrameStart(message); };
        m_onFunctions[UPDATE] = [this](Message message){ onUpdate(message); };
        m_onFunctions[FRAME_END] = [this](Message message){ onFrameEnd(message); };
        m_onFunctions[DELETED] = [this](Message message){ onDelete(message); };
        m_onFunctions[DRAW_GUI] = [this](Message message){ onDrawGUI(message); };
        m_onFunctions[MOUSE_MOVE] = [this](Message message){ onMouseMove(message); };
        m_onFunctions[MOUSE_BUTTON_DOWN] = [this](Message message){ onMouseButtonDown(message); };
        m_onFunctions[MOUSE_BUTTON_UP] = [this](Message message){ onMouseButtonUp(message); };
        m_onFunctions[MOUSE_BUTTON_REPEAT] = [this](Message message){ onMouseButtonRepeat(message); };
        m_onFunctions[MOUSE_WHEEL] = [this](Message message){ onMouseWheel(message); };
        m_onFunctions[KEY_DOWN] = [this](Message message){ onKeyDown(message); };
        m_onFunctions[KEY_UP] = [this](Message message){ onKeyUp(message); };
        m_onFunctions[KEY_REPEAT] = [this](Message message){ onKeyRepeat(message); };
    };

   	template<ArchitectureType ATYPE>
    System<ATYPE>::~System(){};

   	template<ArchitectureType ATYPE>
    void System<ATYPE>::receiveMessage(Message message) {
        assert( m_onFunctions.find(message.getType()) != m_onFunctions.end() );
        if constexpr (ATYPE == vve::ArchitectureType::PARALLEL) {
            if( message.getType() == MessageType::UPDATE ) {
                m_onFunctions[MessageType::UPDATE](message);
            } else {
                std::lock_guard<Mutex<ATYPE>> lock(m_mutex);
                m_messages.push_back(message);
            }
        }
        else {
            m_onFunctions[message.getType()](message);
        }
    };

    //-------------------------------------------------------------------------------------------------

   	template<ArchitectureType ATYPE>
    void System<ATYPE>::onFrameStart(Message message){};

   	template<ArchitectureType ATYPE>
    void System<ATYPE>::onUpdate(Message message){
        if constexpr (ATYPE == vve::ArchitectureType::PARALLEL) {
            std::lock_guard<Mutex<ATYPE>> lock(m_mutex);
            for( auto& message : m_messages ) {
                assert( m_onFunctions.find(message.getType()) != m_onFunctions.end() );
                m_onFunctions[message.getType()](message);
            }
            m_messages.clear();
        }
    };

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


