#pragma once

#include <vector>
#include <cstdint>
#include <variant>
#include <mutex>
#include <functional>
#include <typeindex>
#include <typeinfo>
#include <type_traits>
#include <unordered_set>
#include <filesystem>


namespace vve {

	using Name = vsty::strong_type_t<std::string, vsty::counter<>>;
	using ObjectHandle = vsty::strong_type_t<vecs::Handle, vsty::counter<>>;
	using ParentHandle = vsty::strong_type_t<vecs::Handle, vsty::counter<>>;
	using TextureHandle = vsty::strong_type_t<vecs::Handle, vsty::counter<>>; //need this to use Texture as a unique comonent
	using MeshHandle = vsty::strong_type_t<vecs::Handle, vsty::counter<>>; //need this to use Mesh as a unique component
	using CameraHandle = vsty::strong_type_t<vecs::Handle, vsty::counter<>>; //need this to use Camera as a unique component
}

namespace std {
    template<> struct hash<vve::Name> {
        size_t operator()(vve::Name const& name) const; 
    };

    template<> struct hash<vve::System> {
        size_t operator()(vve::System & system) ; 
    };
};

namespace vve {

    const std::unordered_set<std::string> MsgTypeNames {
        "ANNOUNCE", //System announce themselves
        "EXTENSIONS", //System announce extensions they need
        "INIT",			//initialize the system
        "FRAME_START", //
        "POLL_EVENTS", //
        "UPDATE", //
        "PREPARE_NEXT_FRAME",	// prepare the next frame
        "RECORD_NEXT_FRAME",	// record the next frame	
        "RENDER_NEXT_FRAME",	// render the next frame
        "PRESENT_NEXT_FRAME", // present the next frame
        "FRAME_END", //
        "DELETED", //React to something being deleted
        "SDL", // 
        "SDL_MOUSE_MOVE", //
        "SDL_MOUSE_BUTTON_DOWN", //
        "SDL_MOUSE_BUTTON_UP", //
        "SDL_MOUSE_BUTTON_REPEAT", //
        "SDL_MOUSE_WHEEL", //
        "SDL_KEY_DOWN", //
        "SDL_KEY_UP", //
        "SDL_KEY_REPEAT", //
        "QUIT", //
		//---------------------
		"SCENE_LOAD",	
		"OBJECT_CREATE",	
		"OBJECT_DESTROY",
		"OBJECT_SET_PARENT",	
		"TEXTURE_CREATE",  
		"TEXTURE_DESTROY", 
		"MESH_CREATE",	
		"MESH_DESTROY",	
        "LAST" //
    };


    class System {

	protected:

		struct VulkanState;
	
	    struct MsgTypePhase {
	        size_t m_type;
	        int m_phase;
	    };

	    struct MsgBase {
			MsgBase(std::string type, System* s, System* r=nullptr, double dt=0);
	        size_t m_type;
	        System* m_sender{nullptr};
	        System* m_receiver{nullptr};
	        double m_dt{0};
	        int m_phase{0}; //is set when delivering the message, NOT by sender!
	    };

	    struct MsgAnnounce : public MsgBase { MsgAnnounce(System* s); };
	    struct MsgExtensions : public MsgBase { MsgExtensions(System* s, std::vector<const char*> instExt, std::vector<const char*> devExt ); std::vector<const char*> m_instExt; std::vector<const char*> m_devExt;};
	    struct MsgInit : public MsgBase { MsgInit(System* s, System* r=nullptr); };
	    struct MsgFrameStart : public MsgBase { MsgFrameStart(System* s, System* r, double dt); };
	    struct MsgPollEvents : public MsgBase { MsgPollEvents(System* s, System* r, double dt); };
	    struct MsgUpdate : public MsgBase { MsgUpdate(System* s, System* r, double dt); double m_dt; };
	    struct MsgPrepareNextFrame : public MsgBase { MsgPrepareNextFrame(System* s, System* r, double dt); };
	    struct MsgRenderNextFrame : public MsgBase { MsgRenderNextFrame(System* s, System* r, double dt);  };
	    struct MsgRecordNextFrame : public MsgBase { MsgRecordNextFrame(System* s, System* r, double dt ); };
	    struct MsgPresentNextFrame : public MsgBase { MsgPresentNextFrame(System* s, System* r, double dt);  };
	    struct MsgFrameEnd : public MsgBase { MsgFrameEnd(System* s, System* r, double dt); };
	    struct MsgDelete : public MsgBase { MsgDelete(System* s, System* r, double dt ); void* m_ptr; uint64_t m_id; };
	    struct MsgMouseMove : public MsgBase { MsgMouseMove(System* s, System* r, double dt, int x, int y); int m_x; int m_y; };
	    struct MsgMouseButtonDown : public MsgBase { MsgMouseButtonDown(System* s, System* r, double dt, int button); int m_button; };
	    struct MsgMouseButtonUp : public MsgBase { MsgMouseButtonUp(System* s, System* r, double dt, int button);  int m_button; };
	    struct MsgMouseButtonRepeat : public MsgBase { MsgMouseButtonRepeat(System* s, System* r, double dt, int button);  int m_button; };
	    struct MsgMouseWheel : public MsgBase { MsgMouseWheel(System* s, System* r, double dt, int x, int y);  int m_x; int m_y; };
	    struct MsgKeyDown : public MsgBase { MsgKeyDown(System* s, System* r, double dt, int key);  int m_key; };
	    struct MsgKeyUp : public MsgBase { MsgKeyUp(System* s, System* r, double dt, int key);  int m_key; };
	    struct MsgKeyRepeat : public MsgBase { MsgKeyRepeat(System* s, System* r, double dt, int key); int m_key; };
	    struct MsgSDL : public MsgBase { MsgSDL(System* s, System* r, double dt, SDL_Event event ); double m_dt; SDL_Event m_event; };
		struct MsgQuit : public MsgBase { MsgQuit(System* s, System* r=nullptr); };

		//------------------------------------------------------------------------------------------------

	    struct MsgSceneLoad : public MsgBase { 
			MsgSceneLoad(System* s, System* r, ObjectHandle object, ParentHandle parent, Name sceneName); 
			ObjectHandle m_object{}; 
			ParentHandle m_parent{}; 
			Name m_sceneName;
			const C_STRUCT aiScene* m_scene{};
		};

	    struct MsgObjectCreate : public MsgBase { 
			MsgObjectCreate(System* s, System* r, ObjectHandle object, ParentHandle parent); 
			ObjectHandle m_object{}; 
			ParentHandle m_parent{}; 
		};

		struct MsgObjectSetParent : public MsgBase { MsgObjectSetParent(System* s, System* r, ObjectHandle object, ParentHandle Parent); ObjectHandle m_object; ParentHandle m_parent;};
		struct MsgObjectDestroy : public MsgBase { MsgObjectDestroy(System* s, System* r, vecs::Handle handle); vecs::Handle m_handle; };

		//------------------------------------------------------------------------------------------------

		struct MsgTextureCreate : public MsgBase { MsgTextureCreate(System* s, System* r, void *pixels, vecs::Handle handle); void* m_pixels; vecs::Handle m_handle; };
	    struct MsgTextureDestroy : public MsgBase { MsgTextureDestroy(System* s, System* r, vecs::Handle handle); vecs::Handle m_handle; };
	    struct MsgMeshCreate : public MsgBase { MsgMeshCreate(System* s, System* r, vecs::Handle handle); vecs::Handle m_handle; };
	    struct MsgMeshDestroy : public MsgBase { MsgMeshDestroy(System* s, System* r, vecs::Handle handle); vecs::Handle m_handle; };
	

	    struct Message {
	        template<typename T>
	            requires (std::is_base_of_v<MsgBase, std::decay_t<T>> && sizeof(std::decay_t<T>) <= MAX_MESSAGE_SIZE)
	        Message( T&& msg ) {
	            m_typeID = std::type_index(typeid(T)).hash_code();
	            std::memcpy(m_data, &msg, sizeof(T));
	        };

	        auto GetType() -> size_t { return reinterpret_cast<MsgBase*>(m_data)->m_type; };
	        auto GetSender() -> void* { return reinterpret_cast<MsgBase*>(m_data)->m_sender; };
	        auto GetReceiver() -> void* { return reinterpret_cast<MsgBase*>(m_data)->m_receiver; };
	        auto GetDt() -> double { return reinterpret_cast<MsgBase*>(m_data)->m_dt; };
	        void SetPhase(int phase) { reinterpret_cast<MsgBase*>(m_data)->m_phase = phase; };
	        auto GetPhase() -> int { return reinterpret_cast<MsgBase*>(m_data)->m_phase; };

	        template<typename T>
	            requires (std::is_base_of_v<MsgBase, T> && sizeof(T) <= MAX_MESSAGE_SIZE)
	        auto GetData() -> T& {
	            assert(m_typeID == std::type_index(typeid(T)).hash_code() );
	            return *reinterpret_cast<T*>(m_data);
	        };

	        template<typename T>
	            requires (std::is_base_of_v<MsgBase, T> && sizeof(T) <= MAX_MESSAGE_SIZE)
	        auto HasType() -> bool {
	            return m_typeID == std::type_index(typeid(T)).hash_code();
	        };

	    private:
	        size_t m_typeID{};
	        uint8_t m_data[MAX_MESSAGE_SIZE];
	    };
		

    public:
        System( std::string systemName, Engine& engine );
        virtual ~System();
        auto GetName() -> std::string { return m_name; };

    protected:
		bool OnAnnounce(Message message);
        std::string 							m_name;
        Engine& 								m_engine;
		vecs::Registry<VVE_ARCHITECTURE_TYPE>&	m_registry;
    };

};



