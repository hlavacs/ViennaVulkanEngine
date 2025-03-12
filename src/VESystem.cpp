
#include <cassert>
#include "VHInclude.h"
#include "VEInclude.h"

namespace vve {
	
	System::MsgBase::MsgBase(std::string type, double dt) : m_dt{dt} {
		assert( MsgTypeNames.find(type) != MsgTypeNames.end() );
		m_type = std::hash<std::string>{}(type);
	};

    System::MsgExtensions::MsgExtensions(std::vector<const char*> instExt, std::vector<const char*> devExt) : MsgBase{"EXTENSIONS"}, m_instExt{instExt}, m_devExt{devExt} {};
   	System::MsgInit::MsgInit() : MsgBase{"INIT"} {};
   	System::MsgLoadLevel::MsgLoadLevel(std::string level) : MsgBase{"LOAD_LEVEL"}, m_level{level} {};
    System::MsgWindowSize::MsgWindowSize() : MsgBase{"WINDOW_SIZE"} {};
    System::MsgPlaySound::MsgPlaySound(Filename filepath, int cont, int volume) : MsgBase{"PLAY_SOUND"}, m_filepath{filepath}, m_cont{cont}, m_volume{volume} {};
    System::MsgSetVolume::MsgSetVolume(int volume) : MsgBase{"SET_VOLUME"}, m_volume{volume} {};
    System::MsgQuit::MsgQuit() : MsgBase{"QUIT"} {};

	//------------------------------------------------------------------------
	
    System::MsgFrameStart::MsgFrameStart(double dt) : MsgBase{"FRAME_START", dt} {};
	System::MsgPollEvents::MsgPollEvents(double dt) : MsgBase{"POLL_EVENTS", dt} {};
    System::MsgUpdate::MsgUpdate(double dt): MsgBase{"UPDATE", dt} {}; 
	System::MsgPrepareNextFrame::MsgPrepareNextFrame(double dt): MsgBase{"PREPARE_NEXT_FRAME", dt} {}; 
    System::MsgRenderNextFrame::MsgRenderNextFrame(double dt): MsgBase{"RENDER_NEXT_FRAME", dt} {}; 
    System::MsgRecordNextFrame::MsgRecordNextFrame(double dt): MsgBase{"RECORD_NEXT_FRAME", dt} {}; 
    System::MsgPresentNextFrame::MsgPresentNextFrame(double dt): MsgBase{"PRESENT_NEXT_FRAME", dt} {}; 
    System::MsgFrameEnd:: MsgFrameEnd(double dt): MsgBase{"FRAME_END", dt} {};
    
	//------------------------------------------------------------------------

	System::MsgMouseMove:: MsgMouseMove(double dt, int x, int y): MsgBase{"SDL_MOUSE_MOVE", dt}, m_x{x}, m_y{y} {}; 
    System::MsgMouseButtonDown:: MsgMouseButtonDown(double dt, int button): MsgBase{"SDL_MOUSE_BUTTON_DOWN", dt}, m_button{button} {}; 
    System::MsgMouseButtonUp::MsgMouseButtonUp(double dt, int button): MsgBase{"SDL_MOUSE_BUTTON_UP", dt}, m_button{button} {}; 
    System::MsgMouseButtonRepeat::MsgMouseButtonRepeat(double dt, int button): MsgBase{"SDL_MOUSE_BUTTON_REPEAT", dt}, m_button{button} {}; 
    System::MsgMouseWheel::MsgMouseWheel(double dt, int x, int y): MsgBase{"SDL_MOUSE_WHEEL", dt}, m_x{x}, m_y{y} {}; 
    System::MsgKeyDown::MsgKeyDown(double dt, int key): MsgBase{"SDL_KEY_DOWN", dt}, m_key{key} {}; 
    System::MsgKeyUp::MsgKeyUp(double dt, int key): MsgBase{"SDL_KEY_UP", dt}, m_key{key} {}; 
    System::MsgKeyRepeat::MsgKeyRepeat(double dt, int key): MsgBase{"SDL_KEY_REPEAT", dt}, m_key{key} {};   
    System::MsgSDL::MsgSDL(double dt, SDL_Event event): MsgBase{"SDL"}, m_dt{dt}, m_event{event} {};   
    
	//------------------------------------------------------------------------

	System::MsgSceneLoad::MsgSceneLoad(Filename sceneName, aiPostProcessSteps ai_flags) : MsgBase{"SCENE_LOAD"}, m_sceneName{sceneName}, m_ai_flags{ai_flags} {};

	System::MsgSceneCreate::MsgSceneCreate(ObjectHandle object, ParentHandle parent, Filename sceneName, aiPostProcessSteps ai_flags) : 
		MsgBase{"SCENE_CREATE"}, m_object{object}, m_parent{parent}, m_sceneName{sceneName}, m_ai_flags{ai_flags} {};

	System::MsgObjectCreate::MsgObjectCreate(ObjectHandle object, ParentHandle parent, System* sender) 
		: MsgBase{"OBJECT_CREATE"}, m_object{object}, m_parent{parent}, m_sender{sender} {};
	
	System::MsgObjectSetParent::MsgObjectSetParent(ObjectHandle object, ParentHandle parent) : MsgBase("OBJECT_SET_PARENT"), m_object{object}, m_parent{parent} {};
	System::MsgObjectDestroy::MsgObjectDestroy(ObjectHandle handle) : MsgBase("OBJECT_DESTROY"), m_handle{handle} {};

	//------------------------------------------------------------------------

	System::MsgTextureCreate::MsgTextureCreate(TextureHandle handle, System* sender) : MsgBase{"TEXTURE_CREATE"}, m_handle{handle}, m_sender{sender} {};
    System::MsgTextureDestroy::MsgTextureDestroy(TextureHandle handle) : MsgBase{"TEXTURE_DESTROY"}, m_handle{handle} {};
	System::MsgMeshCreate::MsgMeshCreate(MeshHandle handle) : MsgBase{"MESH_CREATE"}, m_handle{handle} {};
    System::MsgMeshDestroy::MsgMeshDestroy(MeshHandle handle) : MsgBase{"MESH_DESTROY"}, m_handle{handle} {};
    System::MsgDeleted:: MsgDeleted(double dt): MsgBase{"DELETED"} {}; 

	//------------------------------------------------------------------------

    System::System( std::string systemName, Engine& engine ) : m_name(systemName), m_engine(engine), m_registry{engine.GetRegistry()} {};
    System::~System(){};


};   // namespace vve


namespace std {
	size_t hash<vve::System>::operator()(vve::System& system)  {
		return std::hash<decltype(system.GetName())>{}(system.GetName());
    };

};  // namespace std

