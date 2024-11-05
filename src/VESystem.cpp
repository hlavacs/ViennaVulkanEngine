
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
    System<ATYPE>::System( Engine<ATYPE>* engine, std::string name ) : m_engine(engine), m_name(name) {
    };

   	template<ArchitectureType ATYPE>
    System<ATYPE>::~System(){};

    template class System<ArchitectureType::SEQUENTIAL>;
    template class System<ArchitectureType::PARALLEL>;

};   // namespace vve


