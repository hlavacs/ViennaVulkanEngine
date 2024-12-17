
#include <cassert>

#include "VHInclude.h"
#include "VHVulkan.h"

#include "VESystem.h"


namespace vve {
	

    MsgInit::MsgInit(void* s, void* r) : MsgBase{std::hash<std::string>{}("INIT"), s, r} {};
    MsgAnnounce::MsgAnnounce(void* s) : MsgBase{std::hash<std::string>{}("ANNOUNCE"), s} {};
    MsgFrameStart::MsgFrameStart(void* s, void* r, double dt) : MsgBase{std::hash<std::string>{}("FRAME_START"), s, r, dt} {};
    MsgPollEvents::MsgPollEvents(void* s, void* r, double dt) : MsgBase{std::hash<std::string>{}("POLL_EVENTS"), s, r, dt} {};
    MsgUpdate::MsgUpdate(void* s, void* r, double dt): MsgBase{std::hash<std::string>{}("UPDATE"), s, r, dt} {}; 
    MsgPrepareNextFrame::MsgPrepareNextFrame(void* s, void* r, double dt): MsgBase{std::hash<std::string>{}("PREPARE_NEXT_FRAME"), s, r, dt} {}; 
    MsgRenderNextFrame::MsgRenderNextFrame(void* s, void* r, double dt): MsgBase{std::hash<std::string>{}("RENDER_NEXT_FRAME"), s, r, dt} {}; 
    MsgRecordNextFrame::MsgRecordNextFrame(void* s, void* r, double dt): MsgBase{std::hash<std::string>{}("RECORD_NEXT_FRAME"), s, r, dt} {}; 
    MsgPresentNextFrame::MsgPresentNextFrame(void* s, void* r, double dt): MsgBase{std::hash<std::string>{}("PRESENT_NEXT_FRAME"), s, r, dt} {}; 
    MsgFrameEnd:: MsgFrameEnd(void* s, void* r, double dt): MsgBase{std::hash<std::string>{}("FRAME_END"), s, r, dt} {};
    MsgDelete:: MsgDelete(void* s, void* r, double dt): MsgBase{std::hash<std::string>{}("DELETED"), s, r, dt} {}; 
    MsgMouseMove:: MsgMouseMove(void* s, void* r, double dt, int x, int y): MsgBase{std::hash<std::string>{}("SDL_MOUSE_MOVE"), s, r, dt}, m_x{x}, m_y{y} {}; 
    MsgMouseButtonDown:: MsgMouseButtonDown(void* s, void* r, double dt, int button): MsgBase{std::hash<std::string>{}("SDL_MOUSE_BUTTON_DOWN"), s, r, dt}, m_button{button} {}; 
    MsgMouseButtonUp::MsgMouseButtonUp(void* s, void* r, double dt, int button): MsgBase{std::hash<std::string>{}("SDL_MOUSE_BUTTON_UP"), s, r, dt}, m_button{button} {}; 
    MsgMouseButtonRepeat::MsgMouseButtonRepeat(void* s, void* r, double dt, int button): MsgBase{std::hash<std::string>{}("SDL_MOUSE_BUTTON_REPEAT"), s, r, dt}, m_button{button} {}; 
    MsgMouseWheel::MsgMouseWheel(void* s, void* r, double dt, int x, int y): MsgBase{std::hash<std::string>{}("SDL_MOUSE_WHEEL"), s, r, dt}, m_x{x}, m_y{y} {}; 
    MsgKeyDown::MsgKeyDown(void* s, void* r, double dt, int key): MsgBase{std::hash<std::string>{}("SDL_KEY_DOWN"), s, r, dt}, m_key{key} {}; 
    MsgKeyUp::MsgKeyUp(void* s, void* r, double dt, int key): MsgBase{std::hash<std::string>{}("SDL_KEY_UP"), s, r, dt}, m_key{key} {}; 
    MsgKeyRepeat::MsgKeyRepeat(void* s, void* r, double dt, int key): MsgBase{std::hash<std::string>{}("SDL_KEY_REPEAT"), s, r, dt}, m_key{key} {};   
    MsgQuit::MsgQuit(void* s, void* r) : MsgBase{std::hash<std::string>{}("QUIT"), s, r} {};

   	template<ArchitectureType ATYPE>
    System<ATYPE>::System( std::string systemName, Engine<ATYPE>* engine ) : m_name(systemName), m_engine(engine) {};

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