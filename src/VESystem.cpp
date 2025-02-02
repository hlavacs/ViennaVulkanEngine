
#include <cassert>
#include "VHInclude.h"
#include "VEInclude.h"

namespace vve {
	
	System::MsgBase::MsgBase(std::string type, System* s, System* r, double dt) : m_sender{s}, m_receiver{r}, m_dt{dt} {
		assert( MsgTypeNames.find(type) != MsgTypeNames.end() );
		m_type = std::hash<std::string>{}(type);
	};

    System::MsgAnnounce::MsgAnnounce(System* s) : MsgBase{"ANNOUNCE", s} {};
    System::MsgExtensions::MsgExtensions(System* s, std::vector<const char*> instExt, std::vector<const char*> devExt) : MsgBase{"EXTENSIONS", s}, m_instExt{instExt}, m_devExt{devExt} {};
   	System::MsgInit::MsgInit(System* s, System* r) : MsgBase{"INIT", s, r} {};
   	System::MsgLoadLevel::MsgLoadLevel(System* s, System* r, std::string level) : MsgBase{"LOAD_LEVEL", s, r}, m_level{level} {};
    
	//------------------------------------------------------------------------
	
    System::MsgFrameStart::MsgFrameStart(System* s, System* r, double dt) : MsgBase{"FRAME_START", s, r, dt} {};
	System::MsgPollEvents::MsgPollEvents(System* s, System* r, double dt) : MsgBase{"POLL_EVENTS", s, r, dt} {};
    System::MsgUpdate::MsgUpdate(System* s, System* r, double dt): MsgBase{"UPDATE", s, r, dt} {}; 
	System::MsgPrepareNextFrame::MsgPrepareNextFrame(System* s, System* r, double dt): MsgBase{"PREPARE_NEXT_FRAME", s, r, dt} {}; 
    System::MsgRenderNextFrame::MsgRenderNextFrame(System* s, System* r, double dt): MsgBase{"RENDER_NEXT_FRAME", s, r, dt} {}; 
    System::MsgRecordNextFrame::MsgRecordNextFrame(System* s, System* r, double dt): MsgBase{"RECORD_NEXT_FRAME", s, r, dt} {}; 
    System::MsgPresentNextFrame::MsgPresentNextFrame(System* s, System* r, double dt): MsgBase{"PRESENT_NEXT_FRAME", s, r, dt} {}; 
    System::MsgFrameEnd:: MsgFrameEnd(System* s, System* r, double dt): MsgBase{"FRAME_END", s, r, dt} {};
    
	//------------------------------------------------------------------------

	System::MsgMouseMove:: MsgMouseMove(System* s, System* r, double dt, int x, int y): MsgBase{"SDL_MOUSE_MOVE", s, r, dt}, m_x{x}, m_y{y} {}; 
    System::MsgMouseButtonDown:: MsgMouseButtonDown(System* s, System* r, double dt, int button): MsgBase{"SDL_MOUSE_BUTTON_DOWN", s, r, dt}, m_button{button} {}; 
    System::MsgMouseButtonUp::MsgMouseButtonUp(System* s, System* r, double dt, int button): MsgBase{"SDL_MOUSE_BUTTON_UP", s, r, dt}, m_button{button} {}; 
    System::MsgMouseButtonRepeat::MsgMouseButtonRepeat(System* s, System* r, double dt, int button): MsgBase{"SDL_MOUSE_BUTTON_REPEAT", s, r, dt}, m_button{button} {}; 
    System::MsgMouseWheel::MsgMouseWheel(System* s, System* r, double dt, int x, int y): MsgBase{"SDL_MOUSE_WHEEL", s, r, dt}, m_x{x}, m_y{y} {}; 
    System::MsgKeyDown::MsgKeyDown(System* s, System* r, double dt, int key): MsgBase{"SDL_KEY_DOWN", s, r, dt}, m_key{key} {}; 
    System::MsgKeyUp::MsgKeyUp(System* s, System* r, double dt, int key): MsgBase{"SDL_KEY_UP", s, r, dt}, m_key{key} {}; 
    System::MsgKeyRepeat::MsgKeyRepeat(System* s, System* r, double dt, int key): MsgBase{"SDL_KEY_REPEAT", s, r, dt}, m_key{key} {};   
    System::MsgSDL::MsgSDL(System* s, System* r, double dt, SDL_Event event): MsgBase{"SDL", s, r}, m_dt{dt}, m_event{event} {};   
    System::MsgQuit::MsgQuit(System* s, System* r) : MsgBase{"QUIT", s, r} {};
    
	//------------------------------------------------------------------------

	System::MsgSceneLoad::MsgSceneLoad(System* s, System* r, ObjectHandle object, ParentHandle parent, Name sceneName) : 
		MsgBase{"SCENE_LOAD", s, r}, m_object{object}, m_parent{parent}, m_sceneName{sceneName} {};

	System::MsgSceneCreate::MsgSceneCreate(System* s, System* r, ObjectHandle object, ParentHandle parent, Name sceneName) : 
		MsgBase{"SCENE_CREATE", s, r}, m_object{object}, m_parent{parent}, m_sceneName{sceneName} {};

	System::MsgObjectCreate::MsgObjectCreate(System* s, System* r, ObjectHandle object, ParentHandle parent) : 
		MsgBase{"OBJECT_CREATE", s, r}, m_object{object}, m_parent{parent} {};
	
	System::MsgObjectSetParent::MsgObjectSetParent(System* s, System* r, ObjectHandle object, ParentHandle parent) : MsgBase("OBJECT_SET_PARENT", s, r), m_object{object}, m_parent{parent} {};
	System::MsgObjectDestroy::MsgObjectDestroy(System* s, System* r, vecs::Handle handle) : MsgBase("OBJECT_DESTROY", s, r), m_handle{handle} {};

	//------------------------------------------------------------------------

	System::MsgTextureCreate::MsgTextureCreate(System* s, System* r, void *pixels, vecs::Handle handle) : MsgBase{"TEXTURE_CREATE", s, r}, m_pixels{pixels}, m_handle{handle} {};
    System::MsgTextureDestroy::MsgTextureDestroy(System* s, System* r, vecs::Handle handle) : MsgBase{"TEXTURE_DESTROY", s, r}, m_handle{handle} {};
	System::MsgMeshCreate::MsgMeshCreate(System* s, System* r, vecs::Handle handle) : MsgBase{"MESH_CREATE", s, r}, m_handle{handle} {};
    System::MsgMeshDestroy::MsgMeshDestroy(System* s, System* r, vecs::Handle handle) : MsgBase{"MESH_DESTROY", s, r}, m_handle{handle} {};
    System::MsgDeleted:: MsgDeleted(System* s, System* r, double dt): MsgBase{"DELETED", s, r, dt} {}; 

	//------------------------------------------------------------------------

    System::System( std::string systemName, Engine& engine ) : 
		m_name(systemName), m_engine(engine), m_registry{engine.GetRegistry()} {

		if( this != &engine ) {
			engine.RegisterCallback( { 
				{this, 0, "ANNOUNCE", [this](Message& message){ return OnAnnounce(message);} }
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
	
	size_t hash<vve::Name>::operator()(vve::Name const& name) const {
		return std::hash<std::string>{}(name());
	}

	size_t hash<vve::System>::operator()(vve::System& system)  {
		return std::hash<decltype(system.GetName())>{}(system.GetName());
    };

};  // namespace std