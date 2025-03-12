#pragma once

namespace std {
    template<> struct hash<vve::System> {
        size_t operator()(vve::System & system) ; 
    };
};

namespace vve {

    const std::unordered_set<std::string> MsgTypeNames {
        "EXTENSIONS", //System announce extensions they need
        "INIT",			//initialize the system
		"LOAD_LEVEL",	//Load a level
		"WINDOW_SIZE",	//Window size has changed
		"PLAY_SOUND",	//Play a sound
        "QUIT", //Quit the game
		//---------------------
        "FRAME_START", //
        "POLL_EVENTS", //
        "UPDATE", //
        "PREPARE_NEXT_FRAME",	// prepare the next frame
        "RECORD_NEXT_FRAME",	// record the next frame	
        "RENDER_NEXT_FRAME",	// render the next frame
        "PRESENT_NEXT_FRAME", // present the next frame
        "FRAME_END", //
		//---------------------
        "SDL", // 
        "SDL_MOUSE_MOVE", //
        "SDL_MOUSE_BUTTON_DOWN", //
        "SDL_MOUSE_BUTTON_UP", //
        "SDL_MOUSE_BUTTON_REPEAT", //
        "SDL_MOUSE_WHEEL", //
        "SDL_KEY_DOWN", //
        "SDL_KEY_UP", //
        "SDL_KEY_REPEAT", //
		//---------------------
		"SCENE_LOAD",	
		"SCENE_CREATE",	
		"OBJECT_CREATE",	
		"OBJECT_DESTROY",
		"OBJECT_SET_PARENT",	
		"TEXTURE_CREATE",  
		"TEXTURE_DESTROY", 
		"MESH_CREATE",	
		"MESH_DESTROY",	
        "DELETED", //React to something being deleted
        "LAST" //
    };


    class System {

	public:

	    struct MsgTypePhase {
	        size_t m_type;
	        int m_phase;
	    };

	    struct MsgBase {
			MsgBase(std::string type, double dt=0);
	        size_t m_type;
	        double m_dt{0};
	        int m_phase{0}; //is set when delivering the message, NOT by sender!
	    };

	    struct MsgExtensions : public MsgBase { MsgExtensions(std::vector<const char*> instExt, std::vector<const char*> devExt ); std::vector<const char*> m_instExt; std::vector<const char*> m_devExt;};
	    struct MsgInit : public MsgBase { MsgInit(); };
	    struct MsgLoadLevel : public MsgBase { MsgLoadLevel(std::string level); std::string m_level;};
	    struct MsgWindowSize : public MsgBase { MsgWindowSize(); };
	    struct MsgPlaySound : public MsgBase { MsgPlaySound(Filename filepath, int cont=1, int volume=100); Filename m_filepath; int m_cont; int m_volume; SoundHandle m_soundHandle{}; }; //0..stop 1..play once 2..loop
	    struct MsgQuit : public MsgBase { MsgQuit(); };

		//------------------------------------------------------------------------------------------------
		
		struct MsgFrameStart : public MsgBase { MsgFrameStart(double dt); };
	    struct MsgPollEvents : public MsgBase { MsgPollEvents(double dt); };
	    struct MsgUpdate : public MsgBase { MsgUpdate(double dt); };
	    struct MsgPrepareNextFrame : public MsgBase { MsgPrepareNextFrame(double dt); };
	    struct MsgRenderNextFrame : public MsgBase { MsgRenderNextFrame(double dt);  };
	    struct MsgRecordNextFrame : public MsgBase { MsgRecordNextFrame(double dt ); };
	    struct MsgPresentNextFrame : public MsgBase { MsgPresentNextFrame(double dt);  };
	    struct MsgFrameEnd : public MsgBase { MsgFrameEnd(double dt); };
	    
		//------------------------------------------------------------------------------------------------

		struct MsgMouseMove : public MsgBase { MsgMouseMove(double dt, int x, int y); int m_x; int m_y; };
	    struct MsgMouseButtonDown : public MsgBase { MsgMouseButtonDown(double dt, int button); int m_button; };
	    struct MsgMouseButtonUp : public MsgBase { MsgMouseButtonUp(double dt, int button);  int m_button; };
	    struct MsgMouseButtonRepeat : public MsgBase { MsgMouseButtonRepeat(double dt, int button);  int m_button; };
	    struct MsgMouseWheel : public MsgBase { MsgMouseWheel(double dt, int x, int y);  int m_x; int m_y; };
	    struct MsgKeyDown : public MsgBase { MsgKeyDown(double dt, int key);  int m_key; };
	    struct MsgKeyUp : public MsgBase { MsgKeyUp(double dt, int key);  int m_key; };
	    struct MsgKeyRepeat : public MsgBase { MsgKeyRepeat(double dt, int key); int m_key; };
	    struct MsgSDL : public MsgBase { MsgSDL(double dt, SDL_Event event ); double m_dt; SDL_Event m_event; };

		//------------------------------------------------------------------------------------------------

	    struct MsgSceneLoad : public MsgBase { MsgSceneLoad(Filename sceneName, aiPostProcessSteps ai_flags=aiProcess_Triangulate); Filename m_sceneName; aiPostProcessSteps m_ai_flags; };

	    struct MsgSceneCreate : public MsgBase { 
			MsgSceneCreate(ObjectHandle object, ParentHandle parent, Filename sceneName, aiPostProcessSteps ai_flags=aiProcess_Triangulate); 
			ObjectHandle m_object{}; 
			ParentHandle m_parent{}; 
			Filename m_sceneName;
			aiPostProcessSteps m_ai_flags;
			const C_STRUCT aiScene* m_scene{};
		};

	    struct MsgObjectCreate : public MsgBase { 
			MsgObjectCreate(ObjectHandle object, ParentHandle parent, System* sender=nullptr); 
			ObjectHandle m_object{}; 
			ParentHandle m_parent{}; 
			System* m_sender{};
		};

		struct MsgObjectSetParent : public MsgBase { MsgObjectSetParent( ObjectHandle object, ParentHandle Parent); ObjectHandle m_object; ParentHandle m_parent;};
		struct MsgObjectDestroy : public MsgBase { MsgObjectDestroy(ObjectHandle); ObjectHandle m_handle; };

		//------------------------------------------------------------------------------------------------

		struct MsgTextureCreate : public MsgBase { MsgTextureCreate(TextureHandle handle, System* sender=nullptr); TextureHandle m_handle; System* m_sender; };
	    struct MsgTextureDestroy : public MsgBase { MsgTextureDestroy(TextureHandle handle); TextureHandle m_handle; };
	    struct MsgMeshCreate : public MsgBase { MsgMeshCreate( MeshHandle handle); MeshHandle m_handle; };
	    struct MsgMeshDestroy : public MsgBase { MsgMeshDestroy(MeshHandle handle); MeshHandle m_handle; };
		struct MsgDeleted : public MsgBase { MsgDeleted(double dt ); void* m_ptr; uint64_t m_id; };

		//------------------------------------------------------------------------------------------------

	    struct Message {
	        template<typename T>
	            requires (std::is_base_of_v<MsgBase, std::decay_t<T>> && sizeof(std::decay_t<T>) <= MAX_MESSAGE_SIZE)
	        Message( T&& msg ) {
	            m_typeID = std::type_index(typeid(T)).hash_code();
	            std::memcpy(m_data, &msg, sizeof(T));
	        };

	        auto GetType() -> size_t { return reinterpret_cast<MsgBase*>(m_data)->m_type; };
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
        SystemName 		m_name;
        Engine& 		m_engine;
		vecs::Registry&	m_registry;
    };

};



