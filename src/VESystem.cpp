
#include <cassert>

#include "VHInclude.h"
#include "VHVulkan.h"

#include "VESystem.h"


namespace vve {

    MessageInit::MessageInit(void* s, void* r) : MessageBase{MessageType::INIT, s, r} {};
    MessageAnnounce::MessageAnnounce(void* s) : MessageBase{MessageType::ANNOUNCE, s} {};
    MessageFrameStart::MessageFrameStart(void* s, void* r, double dt) : MessageBase{MessageType::FRAME_START, s, r, dt} {};
    MessagePollEvents::MessagePollEvents(void* s, void* r, double dt) : MessageBase{MessageType::POLL_EVENTS, s, r, dt} {};
    MessageUpdate::MessageUpdate(void* s, void* r, double dt): MessageBase{MessageType::UPDATE, s, r, dt} {}; 
    MessagePrepareNextFrame::MessagePrepareNextFrame(void* s, void* r, double dt): MessageBase{MessageType::PREPARE_NEXT_FRAME, s, r, dt} {}; 
    MessageRenderNextFrame::MessageRenderNextFrame(void* s, void* r, double dt): MessageBase{MessageType::RENDER_NEXT_FRAME, s, r, dt} {}; 
    MessageRecordNextFrame::MessageRecordNextFrame(void* s, void* r, double dt): MessageBase{MessageType::RECORD_NEXT_FRAME, s, r, dt} {}; 
    MessagePresentNextFrame::MessagePresentNextFrame(void* s, void* r, double dt): MessageBase{MessageType::PRESENT_NEXT_FRAME, s, r, dt} {}; 
    MessageFrameEnd:: MessageFrameEnd(void* s, void* r, double dt): MessageBase{MessageType::FRAME_END, s, r, dt} {};
    MessageDelete:: MessageDelete(void* s, void* r, double dt): MessageBase{MessageType::DELETED, s, r, dt} {}; 
    MessageMouseMove:: MessageMouseMove(void* s, void* r, double dt, int x, int y): MessageBase{MessageType::MOUSE_MOVE, s, r, dt}, m_x{x}, m_y{y} {}; 
    MessageMouseButtonDown:: MessageMouseButtonDown(void* s, void* r, double dt, int button): MessageBase{MessageType::MOUSE_BUTTON_DOWN, s, r, dt}, m_button{button} {}; 
    MessageMouseButtonUp::MessageMouseButtonUp(void* s, void* r, double dt, int button): MessageBase{MessageType::MOUSE_BUTTON_UP, s, r, dt}, m_button{button} {}; 
    MessageMouseButtonRepeat::MessageMouseButtonRepeat(void* s, void* r, double dt, int button): MessageBase{MessageType::MOUSE_BUTTON_REPEAT, s, r, dt}, m_button{button} {}; 
    MessageMouseWheel::MessageMouseWheel(void* s, void* r, double dt, int x, int y): MessageBase{MessageType::MOUSE_WHEEL, s, r, dt}, m_x{x}, m_y{y} {}; 
    MessageKeyDown::MessageKeyDown(void* s, void* r, double dt, int key): MessageBase{MessageType::KEY_DOWN, s, r, dt}, m_key{key} {}; 
    MessageKeyUp::MessageKeyUp(void* s, void* r, double dt, int key): MessageBase{MessageType::KEY_UP, s, r, dt}, m_key{key} {}; 
    MessageKeyRepeat::MessageKeyRepeat(void* s, void* r, double dt, int key): MessageBase{MessageType::KEY_REPEAT, s, r, dt}, m_key{key} {};   
    MessageQuit::MessageQuit(void* s, void* r) : MessageBase{MessageType::QUIT, s, r} {};

   	template<ArchitectureType ATYPE>
    System<ATYPE>::System( std::string systemName, Engine<ATYPE>* engine ) : m_name(systemName), m_engine(engine) {
    };

   	template<ArchitectureType ATYPE>
    System<ATYPE>::~System(){};

    template class System<ENGINETYPE_SEQUENTIAL>;
    template class System<ENGINETYPE_PARALLEL>;

};   // namespace vve


namespace std {

	size_t hash<vve::System<vve::ENGINETYPE_SEQUENTIAL>>::operator()(vve::System<vve::ENGINETYPE_SEQUENTIAL> & system)  {
		return std::hash<std::string>{}(system.GetName());
    };

	size_t hash<vve::System<vve::ENGINETYPE_PARALLEL>>::operator()(vve::System<vve::ENGINETYPE_PARALLEL> & system)  {
		return std::hash<std::string>{}(system.GetName());
    };

};  // namespace std