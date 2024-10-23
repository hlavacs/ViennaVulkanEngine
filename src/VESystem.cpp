
#include <cassert>
#include "VESystem.h"


namespace vve {

    MessageInit::MessageInit(void* s, void* r) : MessageBase{INIT, s, r} {};
    MessageAnnounce::MessageAnnounce(void* s) : MessageBase{ANNOUNCE, s} {};
    MessageFrameStart::MessageFrameStart(void* s, void* r, double dt) : MessageBase{FRAME_START, s, r, dt} {};
    MessagePollEvents::MessagePollEvents(void* s, void* r, double dt) : MessageBase{POLL_EVENTS, s, r, dt} {};
    MessageUpdate::MessageUpdate(void* s, void* r, double dt): MessageBase{UPDATE, s, r, dt} {}; 
    MessagePrepareNextFrame::MessagePrepareNextFrame(void* s, void* r, double dt): MessageBase{PREPARE_NEXT_FRAME, s, r, dt} {}; 
    MessageRenderNextFrame::MessageRenderNextFrame(void* s, void* r, double dt): MessageBase{RENDER_NEXT_FRAME, s, r, dt} {}; 
    MessageRecordNextFrame::MessageRecordNextFrame(void* s, void* r, double dt): MessageBase{RECORD_NEXT_FRAME, s, r, dt} {}; 
    MessagePresentNextFrame::MessagePresentNextFrame(void* s, void* r, double dt): MessageBase{PRESENT_NEXT_FRAME, s, r, dt} {}; 
    MessageFrameEnd:: MessageFrameEnd(void* s, void* r, double dt): MessageBase{FRAME_END, s, r, dt} {};
    MessageDelete:: MessageDelete(void* s, void* r, double dt): MessageBase{DELETED, s, r, dt} {}; 
    MessageMouseMove:: MessageMouseMove(void* s, void* r, double dt, int x, int y): MessageBase{MOUSE_MOVE, s, r, dt}, m_x{x}, m_y{y} {}; 
    MessageMouseButtonDown:: MessageMouseButtonDown(void* s, void* r, double dt, int button): MessageBase{MOUSE_BUTTON_DOWN, s, r, dt}, m_button{button} {}; 
    MessageMouseButtonUp::MessageMouseButtonUp(void* s, void* r, double dt, int button): MessageBase{MOUSE_BUTTON_UP, s, r, dt}, m_button{button} {}; 
    MessageMouseButtonRepeat::MessageMouseButtonRepeat(void* s, void* r, double dt, int button): MessageBase{MOUSE_BUTTON_REPEAT, s, r, dt}, m_button{button} {}; 
    MessageMouseWheel::MessageMouseWheel(void* s, void* r, double dt, int x, int y): MessageBase{MOUSE_WHEEL, s, r, dt}, m_x{x}, m_y{y} {}; 
    MessageKeyDown::MessageKeyDown(void* s, void* r, double dt, int key): MessageBase{KEY_DOWN, s, r, dt}, m_key{key} {}; 
    MessageKeyUp::MessageKeyUp(void* s, void* r, double dt, int key): MessageBase{KEY_UP, s, r, dt}, m_key{key} {}; 
    MessageKeyRepeat::MessageKeyRepeat(void* s, void* r, double dt, int key): MessageBase{KEY_REPEAT, s, r, dt}, m_key{key} {};   
    MessageQuit::MessageQuit(void* s, void* r) : MessageBase{QUIT, s, r} {};


   	template<ArchitectureType ATYPE>
    System<ATYPE>::System( std::string name, Engine<ATYPE>& engine) : m_name(name), m_engine(engine) {
        m_onFunctions[INIT] = [this](Message message){ OnInit(message); };
        m_onFunctions[ANNOUNCE] = [this](Message message){ OnAnnounce(message); };
        m_onFunctions[FRAME_START] = [this](Message message){ OnFrameStart(message); };
        m_onFunctions[POLL_EVENTS] = [this](Message message){ OnPollEvents(message); };
        m_onFunctions[UPDATE] = [this](Message message){ OnUpdate(message); };
        m_onFunctions[PREPARE_NEXT_FRAME] = [this](Message message){ OnPrepareNextFrame(message); };
        m_onFunctions[RENDER_NEXT_FRAME] = [this](Message message){ OnRenderNextFrame(message); };
        m_onFunctions[RECORD_NEXT_FRAME] = [this](Message message){ OnRecordNextFrame(message); };
        m_onFunctions[PRESENT_NEXT_FRAME] = [this](Message message){ OnPresentNextFrame(message); };
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
        m_onFunctions[QUIT] = [this](Message message){ OnQuit(message); };
    };

   	template<ArchitectureType ATYPE>
    System<ATYPE>::~System(){};

   	template<ArchitectureType ATYPE>
    void System<ATYPE>::ReceiveMessage(Message message) {
        assert( m_onFunctions.contains(message.GetType()) );
        m_onFunctions[message.GetType()](message);
    };

    //-------------------------------------------------------------------------------------------------

   	template<ArchitectureType ATYPE>
    void System<ATYPE>::OnInit(Message message){};

   	template<ArchitectureType ATYPE>
    void System<ATYPE>::OnAnnounce(Message message){};

   	template<ArchitectureType ATYPE>
    void System<ATYPE>::OnFrameStart(Message message){};

   	template<ArchitectureType ATYPE>
    void System<ATYPE>::OnUpdate(Message message){};

   	template<ArchitectureType ATYPE>
    void System<ATYPE>::OnPollEvents(Message message) {};

   	template<ArchitectureType ATYPE>
    void System<ATYPE>::OnPrepareNextFrame(Message message) {};

   	template<ArchitectureType ATYPE>
    void System<ATYPE>::OnRenderNextFrame(Message message) {};

    template<ArchitectureType ATYPE>
    void System<ATYPE>::OnRecordNextFrame(Message message){};
  	
    template<ArchitectureType ATYPE>
    void System<ATYPE>::OnPresentNextFrame(Message message) {};

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

   	template<ArchitectureType ATYPE>
    void System<ATYPE>::OnQuit(Message message){};

    template class System<ArchitectureType::SEQUENTIAL>;
    template class System<ArchitectureType::PARALLEL>;

};   // namespace vve


