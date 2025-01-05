
#include <cassert>
#include "VHInclude.h"
#include "VEInclude.h"

namespace vve {
	
    System::MsgAnnounce::MsgAnnounce(System* s) : MsgBase{std::hash<std::string>{}("ANNOUNCE"), s} {};
    System::MsgExtensions::MsgExtensions(System* s, std::vector<const char*> instExt, std::vector<const char*> devExt) : MsgBase{std::hash<std::string>{}("EXTENSIONS"), s}, m_instExt{instExt}, m_devExt{devExt} {};
   	System::MsgInit::MsgInit(System* s, System* r) : MsgBase{std::hash<std::string>{}("INIT"), s, r} {};
    System::MsgFrameStart::MsgFrameStart(System* s, System* r, double dt) : MsgBase{std::hash<std::string>{}("FRAME_START"), s, r, dt} {};
    System::MsgPollEvents::MsgPollEvents(System* s, System* r, double dt) : MsgBase{std::hash<std::string>{}("POLL_EVENTS"), s, r, dt} {};
    System::MsgUpdate::MsgUpdate(System* s, System* r, double dt): MsgBase{std::hash<std::string>{}("UPDATE"), s, r, dt} {}; 
    System::MsgPrepareNextFrame::MsgPrepareNextFrame(System* s, System* r, double dt): MsgBase{std::hash<std::string>{}("PREPARE_NEXT_FRAME"), s, r, dt} {}; 
    System::MsgRenderNextFrame::MsgRenderNextFrame(System* s, System* r, double dt): MsgBase{std::hash<std::string>{}("RENDER_NEXT_FRAME"), s, r, dt} {}; 
    System::MsgRecordNextFrame::MsgRecordNextFrame(System* s, System* r, double dt): MsgBase{std::hash<std::string>{}("RECORD_NEXT_FRAME"), s, r, dt} {}; 
    System::MsgPresentNextFrame::MsgPresentNextFrame(System* s, System* r, double dt): MsgBase{std::hash<std::string>{}("PRESENT_NEXT_FRAME"), s, r, dt} {}; 
    System::MsgFrameEnd:: MsgFrameEnd(System* s, System* r, double dt): MsgBase{std::hash<std::string>{}("FRAME_END"), s, r, dt} {};
    System::MsgDelete:: MsgDelete(System* s, System* r, double dt): MsgBase{std::hash<std::string>{}("DELETED"), s, r, dt} {}; 
    System::MsgMouseMove:: MsgMouseMove(System* s, System* r, double dt, int x, int y): MsgBase{std::hash<std::string>{}("SDL_MOUSE_MOVE"), s, r, dt}, m_x{x}, m_y{y} {}; 
    System::MsgMouseButtonDown:: MsgMouseButtonDown(System* s, System* r, double dt, int button): MsgBase{std::hash<std::string>{}("SDL_MOUSE_BUTTON_DOWN"), s, r, dt}, m_button{button} {}; 
    System::MsgMouseButtonUp::MsgMouseButtonUp(System* s, System* r, double dt, int button): MsgBase{std::hash<std::string>{}("SDL_MOUSE_BUTTON_UP"), s, r, dt}, m_button{button} {}; 
    System::MsgMouseButtonRepeat::MsgMouseButtonRepeat(System* s, System* r, double dt, int button): MsgBase{std::hash<std::string>{}("SDL_MOUSE_BUTTON_REPEAT"), s, r, dt}, m_button{button} {}; 
    System::MsgMouseWheel::MsgMouseWheel(System* s, System* r, double dt, int x, int y): MsgBase{std::hash<std::string>{}("SDL_MOUSE_WHEEL"), s, r, dt}, m_x{x}, m_y{y} {}; 
    System::MsgKeyDown::MsgKeyDown(System* s, System* r, double dt, int key): MsgBase{std::hash<std::string>{}("SDL_KEY_DOWN"), s, r, dt}, m_key{key} {}; 
    System::MsgKeyUp::MsgKeyUp(System* s, System* r, double dt, int key): MsgBase{std::hash<std::string>{}("SDL_KEY_UP"), s, r, dt}, m_key{key} {}; 
    System::MsgKeyRepeat::MsgKeyRepeat(System* s, System* r, double dt, int key): MsgBase{std::hash<std::string>{}("SDL_KEY_REPEAT"), s, r, dt}, m_key{key} {};   
    System::MsgSDL::MsgSDL(System* s, System* r, double dt, SDL_Event event): MsgBase{std::hash<std::string>{}("SDL"), s, r}, m_dt{dt}, m_event{event} {};   
    System::MsgQuit::MsgQuit(System* s, System* r) : MsgBase{std::hash<std::string>{}("QUIT"), s, r} {};
    
	//------------------------------------------------------------------------

	System::MsgFileLoadObject::MsgFileLoadObject(System* s, System* r, std::string txtName, std::string objName) : MsgBase{std::hash<std::string>{}("FILE_LOAD_OBJECT"), s, r}, m_txtName{txtName}, m_objName{objName} {};
	
	System::MsgObjectCreate::MsgObjectCreate(System* s, System* r, vecs::Handle handle) : MsgBase{std::hash<std::string>{}("OBJECT_CREATE"), s, r}, m_handle{handle} {};
		
	System::MsgTextureCreate::MsgTextureCreate(System* s, System* r, void *pixels, vecs::Handle handle) : MsgBase{std::hash<std::string>{}("TEXTURE_CREATE"), s, r}, m_handle{handle} {};
    System::MsgTextureDestroy::MsgTextureDestroy(System* s, System* r, vecs::Handle handle) : MsgBase{std::hash<std::string>{}("TEXTURE_DESTROY"), s, r}, m_handle{handle} {};
	System::MsgGeometryCreate::MsgGeometryCreate(System* s, System* r, vecs::Handle handle) : MsgBase{std::hash<std::string>{}("GEOMETRY_CREATE"), s, r}, m_handle{handle} {};
    System::MsgGeometryDestroy::MsgGeometryDestroy(System* s, System* r, vecs::Handle handle) : MsgBase{std::hash<std::string>{}("GEOMETRY_DESTROY"), s, r}, m_handle{handle} {};

    System::System( std::string systemName, Engine& engine ) : 
		m_name(systemName), m_engine(engine), m_registry{engine.GetRegistry()} {

		if( this != &engine ) {
			engine.RegisterCallback( { 
				{this, 0, "ANNOUNCE", [this](Message message){ return OnAnnounce(message);} }
			} );
		}
	};

    System::~System(){};

    bool System::OnAnnounce(Message message) {
		auto msg = message.template GetData<MsgAnnounce>();
		if( msg.m_sender == &m_engine ) {
			m_engine.SendMessage( MsgAnnounce{this} );
		}
		return false;
    }


};   // namespace vve


namespace std {

	size_t hash<vve::System>::operator()(vve::System& system)  {
		return std::hash<std::string>{}(system.GetName());
    };


};  // namespace std