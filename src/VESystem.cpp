
#include <cassert>
#include "VESystem.h"


namespace vve {

    MessageFrameStart::MessageFrameStart(double dt) : MessageBase{FRAME_START}, m_dt{dt} {};
    MessagePollEvents::MessagePollEvents(double dt) : MessageBase{POLL_EVENTS}, m_dt{dt} {};
    MessageUpdate::MessageUpdate(double dt): MessageBase{UPDATE}, m_dt{dt} {}; 
    MessagePrepareNextFrame::MessagePrepareNextFrame(double dt): MessageBase{PREPARE_NEXT_FRAME}, m_dt{dt} {}; 
    MessageRenderNextFrame::MessageRenderNextFrame(double dt): MessageBase{RENDER_NEXT_FRAME}, m_dt{dt} {}; 
    MessageDrawGUI::MessageDrawGUI(): MessageBase{DRAW_GUI} {}; 
    MessageShowNextFrame::MessageShowNextFrame(double dt): MessageBase{SHOW_NEXT_FRAME}, m_dt{dt} {}; 
    MessageFrameEnd:: MessageFrameEnd(double dt): MessageBase{FRAME_END}, m_dt{dt} {};
    MessageDelete:: MessageDelete(): MessageBase{DELETED} {}; 
    MessageMouseMove:: MessageMouseMove(int x, int y): MessageBase{MOUSE_MOVE}, m_x{x}, m_y{y} {}; 
    MessageMouseButtonDown:: MessageMouseButtonDown(int button): MessageBase{MOUSE_BUTTON_DOWN}, m_button{button} {}; 
    MessageMouseButtonUp::MessageMouseButtonUp(int button): MessageBase{MOUSE_BUTTON_UP}, m_button{button} {}; 
    MessageMouseButtonRepeat::MessageMouseButtonRepeat(int button): MessageBase{MOUSE_BUTTON_REPEAT}, m_button{button} {}; 
    MessageMouseWheel::MessageMouseWheel(int x, int y): MessageBase{MOUSE_WHEEL}, m_x{x}, m_y{y} {}; 
    MessageKeyDown::MessageKeyDown(int key): MessageBase{KEY_DOWN}, m_key{key} {}; 
    MessageKeyUp::MessageKeyUp(int key): MessageBase{KEY_UP}, m_key{key} {}; 
    MessageKeyRepeat::MessageKeyRepeat(int key): MessageBase{KEY_REPEAT}, m_key{key} {};   


   	template<ArchitectureType ATYPE>
    System<ATYPE>::System( Engine<ATYPE>& engine) : m_engine(engine) {
        m_onFunctions[FRAME_START] = [this](Message message){ OnFrameStart(message); };
        m_onFunctions[POLL_EVENTS] = [this](Message message){ OnPollEvents(message); };
        m_onFunctions[UPDATE] = [this](Message message){ OnUpdate(message); };
        m_onFunctions[PREPARE_NEXT_FRAME] = [this](Message message){ OnPrepareNextFrame(message); };
        m_onFunctions[RENDER_NEXT_FRAME] = [this](Message message){ OnRenderNextFrame(message); };
        m_onFunctions[DRAW_GUI] = [this](Message message){ OnDrawGUI(message); };
        m_onFunctions[SHOW_NEXT_FRAME] = [this](Message message){ OnShowNextFrame(message); };
        m_onFunctions[FRAME_END] = [this](Message message){ OnFrameEnd(message); };
        m_onFunctions[DELETED] = [this](Message message){ OnDelete(message); };
        m_onFunctions[MOUSE_MOVE] = [this](Message message){ OnMouseMove(message); };
        m_onFunctions[MOUSE_BUTTON_DOWN] = [this](Message message){ OnMouseButtonDown(message); };
        m_onFunctions[MOUSE_BUTTON_UP] = [this](Message message){ OnMouseButtonUp(message); };
        m_onFunctions[MOUSE_BUTTON_REPEAT] = [this](Message message){ OnMouseButtonRepeat(message); };
        m_onFunctions[MOUSE_WHEEL] = [this](Message message){ OnMouseWheel(message); };
        m_onFunctions[KEY_DOWN] = [this](Message message){ OnKeyDown(message); };
        m_onFunctions[KEY_UP] = [this](Message message){ OnKeyUp(message); };
        m_onFunctions[KEY_REPEAT] = [this](Message message){ OnKeyRepeat(message); };
    };

   	template<ArchitectureType ATYPE>
    System<ATYPE>::~System(){};

   	template<ArchitectureType ATYPE>
    void System<ATYPE>::ReceiveMessage(Message message) {
        assert( m_onFunctions.find(message.GetType()) != m_onFunctions.end() );
        if constexpr (ATYPE == vve::ArchitectureType::PARALLEL) {
            if( message.GetType() == MessageType::UPDATE ) {
                m_onFunctions[MessageType::UPDATE](message);
            } else {
                std::lock_guard<Mutex<ATYPE>> lock(m_mutex);
                m_messages.push_back(message);
            }
        }
        else {
            m_onFunctions[message.GetType()](message);
        }
    };

    //-------------------------------------------------------------------------------------------------

   	template<ArchitectureType ATYPE>
    void System<ATYPE>::OnFrameStart(Message message){};

   	template<ArchitectureType ATYPE>
    void System<ATYPE>::OnUpdate(Message message){
        if constexpr (ATYPE == vve::ArchitectureType::PARALLEL) {
            std::lock_guard<Mutex<ATYPE>> lock(m_mutex);
            for( auto& message : m_messages ) {
                assert( m_onFunctions.find(message.GetType()) != m_onFunctions.end() );
                m_onFunctions[message.GetType()](message);
            }
            m_messages.clear();
        }
    };

   	template<ArchitectureType ATYPE>
    void System<ATYPE>::OnPollEvents(Message message) {};

   	template<ArchitectureType ATYPE>
    void System<ATYPE>::OnPrepareNextFrame(Message message) {};

   	template<ArchitectureType ATYPE>
    void System<ATYPE>::OnRenderNextFrame(Message message) {};

    template<ArchitectureType ATYPE>
    void System<ATYPE>::OnDrawGUI(Message message){};
  	
    template<ArchitectureType ATYPE>
    void System<ATYPE>::OnShowNextFrame(Message message) {};

   	template<ArchitectureType ATYPE>
    void System<ATYPE>::OnFrameEnd(Message message) {};

   	template<ArchitectureType ATYPE>
    void System<ATYPE>::OnDelete(Message message) {};

   	template<ArchitectureType ATYPE>
    void System<ATYPE>::OnMouseMove(Message message){};

   	template<ArchitectureType ATYPE>
    void System<ATYPE>::OnMouseButtonDown(Message message){};

   	template<ArchitectureType ATYPE>
    void System<ATYPE>::OnMouseButtonUp(Message message){};

   	template<ArchitectureType ATYPE>
    void System<ATYPE>::OnMouseButtonRepeat(Message message){};

   	template<ArchitectureType ATYPE>
    void System<ATYPE>::OnMouseWheel(Message message){};

   	template<ArchitectureType ATYPE>
    void System<ATYPE>::OnKeyDown(Message message){};

   	template<ArchitectureType ATYPE>
    void System<ATYPE>::OnKeyUp(Message message){};

   	template<ArchitectureType ATYPE>
    void System<ATYPE>::OnKeyRepeat(Message message){};

    template class System<ArchitectureType::SEQUENTIAL>;
    template class System<ArchitectureType::PARALLEL>;

};   // namespace vve


