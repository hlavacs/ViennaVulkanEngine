
#include <cassert>
#include "VHInclude.h"
#include "VEInclude.h"

namespace vve {
	
    template<ArchitectureType ATYPE> System<ATYPE>::MsgAnnounce::MsgAnnounce(System<ATYPE>* s) : MsgBase{std::hash<std::string>{}("ANNOUNCE"), s} {};
    template<ArchitectureType ATYPE> System<ATYPE>::MsgExtensions::MsgExtensions(System<ATYPE>* s, std::vector<const char*> instExt, std::vector<const char*> devExt) : MsgBase{std::hash<std::string>{}("EXTENSIONS"), s}, m_instExt{instExt}, m_devExt{devExt} {};
   	template<ArchitectureType ATYPE> System<ATYPE>::MsgInit::MsgInit(System<ATYPE>* s, System<ATYPE>* r) : MsgBase{std::hash<std::string>{}("INIT"), s, r} {};
    template<ArchitectureType ATYPE> System<ATYPE>::MsgFrameStart::MsgFrameStart(System<ATYPE>* s, System<ATYPE>* r, double dt) : MsgBase{std::hash<std::string>{}("FRAME_START"), s, r, dt} {};
    template<ArchitectureType ATYPE> System<ATYPE>::MsgPollEvents::MsgPollEvents(System<ATYPE>* s, System<ATYPE>* r, double dt) : MsgBase{std::hash<std::string>{}("POLL_EVENTS"), s, r, dt} {};
    template<ArchitectureType ATYPE> System<ATYPE>::MsgUpdate::MsgUpdate(System<ATYPE>* s, System<ATYPE>* r, double dt): MsgBase{std::hash<std::string>{}("UPDATE"), s, r, dt} {}; 
    template<ArchitectureType ATYPE> System<ATYPE>::MsgPrepareNextFrame::MsgPrepareNextFrame(System<ATYPE>* s, System<ATYPE>* r, double dt): MsgBase{std::hash<std::string>{}("PREPARE_NEXT_FRAME"), s, r, dt} {}; 
    template<ArchitectureType ATYPE> System<ATYPE>::MsgRenderNextFrame::MsgRenderNextFrame(System<ATYPE>* s, System<ATYPE>* r, double dt): MsgBase{std::hash<std::string>{}("RENDER_NEXT_FRAME"), s, r, dt} {}; 
    template<ArchitectureType ATYPE> System<ATYPE>::MsgRecordNextFrame::MsgRecordNextFrame(System<ATYPE>* s, System<ATYPE>* r, double dt): MsgBase{std::hash<std::string>{}("RECORD_NEXT_FRAME"), s, r, dt} {}; 
    template<ArchitectureType ATYPE> System<ATYPE>::MsgPresentNextFrame::MsgPresentNextFrame(System<ATYPE>* s, System<ATYPE>* r, double dt): MsgBase{std::hash<std::string>{}("PRESENT_NEXT_FRAME"), s, r, dt} {}; 
    template<ArchitectureType ATYPE> System<ATYPE>::MsgFrameEnd:: MsgFrameEnd(System<ATYPE>* s, System<ATYPE>* r, double dt): MsgBase{std::hash<std::string>{}("FRAME_END"), s, r, dt} {};
    template<ArchitectureType ATYPE> System<ATYPE>::MsgDelete:: MsgDelete(System<ATYPE>* s, System<ATYPE>* r, double dt): MsgBase{std::hash<std::string>{}("DELETED"), s, r, dt} {}; 
    template<ArchitectureType ATYPE> System<ATYPE>::MsgMouseMove:: MsgMouseMove(System<ATYPE>* s, System<ATYPE>* r, double dt, int x, int y): MsgBase{std::hash<std::string>{}("SDL_MOUSE_MOVE"), s, r, dt}, m_x{x}, m_y{y} {}; 
    template<ArchitectureType ATYPE> System<ATYPE>::MsgMouseButtonDown:: MsgMouseButtonDown(System<ATYPE>* s, System<ATYPE>* r, double dt, int button): MsgBase{std::hash<std::string>{}("SDL_MOUSE_BUTTON_DOWN"), s, r, dt}, m_button{button} {}; 
    template<ArchitectureType ATYPE> System<ATYPE>::MsgMouseButtonUp::MsgMouseButtonUp(System<ATYPE>* s, System<ATYPE>* r, double dt, int button): MsgBase{std::hash<std::string>{}("SDL_MOUSE_BUTTON_UP"), s, r, dt}, m_button{button} {}; 
    template<ArchitectureType ATYPE> System<ATYPE>::MsgMouseButtonRepeat::MsgMouseButtonRepeat(System<ATYPE>* s, System<ATYPE>* r, double dt, int button): MsgBase{std::hash<std::string>{}("SDL_MOUSE_BUTTON_REPEAT"), s, r, dt}, m_button{button} {}; 
    template<ArchitectureType ATYPE> System<ATYPE>::MsgMouseWheel::MsgMouseWheel(System<ATYPE>* s, System<ATYPE>* r, double dt, int x, int y): MsgBase{std::hash<std::string>{}("SDL_MOUSE_WHEEL"), s, r, dt}, m_x{x}, m_y{y} {}; 
    template<ArchitectureType ATYPE> System<ATYPE>::MsgKeyDown::MsgKeyDown(System<ATYPE>* s, System<ATYPE>* r, double dt, int key): MsgBase{std::hash<std::string>{}("SDL_KEY_DOWN"), s, r, dt}, m_key{key} {}; 
    template<ArchitectureType ATYPE> System<ATYPE>::MsgKeyUp::MsgKeyUp(System<ATYPE>* s, System<ATYPE>* r, double dt, int key): MsgBase{std::hash<std::string>{}("SDL_KEY_UP"), s, r, dt}, m_key{key} {}; 
    template<ArchitectureType ATYPE> System<ATYPE>::MsgKeyRepeat::MsgKeyRepeat(System<ATYPE>* s, System<ATYPE>* r, double dt, int key): MsgBase{std::hash<std::string>{}("SDL_KEY_REPEAT"), s, r, dt}, m_key{key} {};   
    template<ArchitectureType ATYPE> System<ATYPE>::MsgSDL::MsgSDL(System<ATYPE>* s, System<ATYPE>* r, double dt, SDL_Event event): MsgBase{std::hash<std::string>{}("SDL"), s, r}, m_dt{dt}, m_event{event} {};   
    template<ArchitectureType ATYPE> System<ATYPE>::MsgQuit::MsgQuit(System<ATYPE>* s, System<ATYPE>* r) : MsgBase{std::hash<std::string>{}("QUIT"), s, r} {};
    
	//------------------------------------------------------------------------

	template<ArchitectureType ATYPE> System<ATYPE>::MsgFileLoadObject::MsgFileLoadObject(System<ATYPE>* s, System<ATYPE>* r, std::string txtName, std::string objName) : MsgBase{std::hash<std::string>{}("FILE_LOAD_OBJECT"), s, r}, m_txtName{txtName}, m_objName{objName} {};
	
	template<ArchitectureType ATYPE> System<ATYPE>::MsgObjectCreate::MsgObjectCreate(System<ATYPE>* s, System<ATYPE>* r, vecs::Handle handle) : MsgBase{std::hash<std::string>{}("OBJECT_CREATE"), s, r}, m_handle{handle} {};
		
	template<ArchitectureType ATYPE> System<ATYPE>::MsgTextureCreate::MsgTextureCreate(System<ATYPE>* s, System<ATYPE>* r, void *pixels, vecs::Handle handle) : MsgBase{std::hash<std::string>{}("TEXTURE_CREATE"), s, r}, m_handle{handle} {};
    template<ArchitectureType ATYPE> System<ATYPE>::MsgTextureDestroy::MsgTextureDestroy(System<ATYPE>* s, System<ATYPE>* r, vecs::Handle handle) : MsgBase{std::hash<std::string>{}("TEXTURE_DESTROY"), s, r}, m_handle{handle} {};
	template<ArchitectureType ATYPE> System<ATYPE>::MsgGeometryCreate::MsgGeometryCreate(System<ATYPE>* s, System<ATYPE>* r, vecs::Handle handle) : MsgBase{std::hash<std::string>{}("GEOMETRY_CREATE"), s, r}, m_handle{handle} {};
    template<ArchitectureType ATYPE> System<ATYPE>::MsgGeometryDestroy::MsgGeometryDestroy(System<ATYPE>* s, System<ATYPE>* r, vecs::Handle handle) : MsgBase{std::hash<std::string>{}("GEOMETRY_DESTROY"), s, r}, m_handle{handle} {};

   	template<ArchitectureType ATYPE>
    System<ATYPE>::System( std::string systemName, Engine<ATYPE>& engine ) : 
		m_name(systemName), m_engine(engine), m_registry{engine.GetRegistry()} {

		if( this != &engine ) {
			engine.RegisterCallback( { 
				{this, 0, "ANNOUNCE", [this](Message message){this->OnAnnounce(message);} }
			} );
		}
	};

   	template<ArchitectureType ATYPE>
    System<ATYPE>::~System(){};

	template<ArchitectureType ATYPE>
    void System<ATYPE>::OnAnnounce(Message message) {
		auto msg = message.template GetData<MsgAnnounce>();
		if( msg.m_sender == &m_engine ) {
			m_engine.SendMessage( MsgAnnounce{this} );
		}
    }

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