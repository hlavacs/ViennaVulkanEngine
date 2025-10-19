#pragma once

namespace std {
    /**
     * @brief Hash specialization for vve::System
     */
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
		"SET_VOLUME",	//Set the sound volume to something btw 0 and 100
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
        "LAST", //
		//---------------------
		"SHADOW_MAP_RECREATED",
		"OBJECT_CHANGED"
    };


    /**
     * @brief Base class for all engine systems
     *
     * Provides message handling and registration with the engine
     */
    class System {

	public:

	    /**
	     * @brief Structure combining message type and phase information
	     */
	    struct MsgTypePhase {
	        size_t m_type;
	        int m_phase;
	    };

	    /**
	     * @brief Base structure for all messages
	     */
	    struct MsgBase {
			/**
			 * @brief Constructor
			 * @param type Message type name
			 * @param dt Delta time (default: 0)
			 */
			MsgBase(std::string type, double dt=0);
	        size_t m_type;
	        double m_dt{0};
	        int m_phase{0}; //is set when delivering the message, NOT by sender!
	    };

	    /** @brief Message for announcing required extensions */
	    struct MsgExtensions : public MsgBase { MsgExtensions(std::vector<const char*> instExt, std::vector<const char*> devExt ); std::vector<const char*> m_instExt; std::vector<const char*> m_devExt;};
	    /** @brief Message for system initialization */
	    struct MsgInit : public MsgBase { MsgInit(); };
	    /** @brief Message for loading a level */
	    struct MsgLoadLevel : public MsgBase { MsgLoadLevel(std::string level); std::string m_level;};
	    /** @brief Message for window size changes */
	    struct MsgWindowSize : public MsgBase { MsgWindowSize(); };
	    /** @brief Message for playing a sound file */
	    struct MsgPlaySound : public MsgBase { MsgPlaySound(Filename filepath, int cont=1, int volume=100); Filename m_filepath; int m_cont; int m_volume; SoundHandle m_soundHandle{}; }; //0..stop 1..play once 2..loop
	    /** @brief Message for setting volume */
	    struct MsgSetVolume : public MsgBase { MsgSetVolume(int volume=100); int m_volume; };
	    /** @brief Message for quitting the application */
	    struct MsgQuit : public MsgBase { MsgQuit(); };

		//------------------------------------------------------------------------------------------------

		/** @brief Message for frame start */
		struct MsgFrameStart : public MsgBase { MsgFrameStart(double dt); };
	    /** @brief Message for polling events */
	    struct MsgPollEvents : public MsgBase { MsgPollEvents(double dt); };
	    /** @brief Message for updating systems */
	    struct MsgUpdate : public MsgBase { MsgUpdate(double dt); };
	    /** @brief Message for preparing the next frame */
	    struct MsgPrepareNextFrame : public MsgBase { MsgPrepareNextFrame(double dt); };
	    /** @brief Message for rendering the next frame */
	    struct MsgRenderNextFrame : public MsgBase { MsgRenderNextFrame(double dt);  };
	    /** @brief Message for recording the next frame */
	    struct MsgRecordNextFrame : public MsgBase { MsgRecordNextFrame(double dt ); };
	    /** @brief Message for presenting the next frame */
	    struct MsgPresentNextFrame : public MsgBase { MsgPresentNextFrame(double dt);  };
	    /** @brief Message for frame end */
	    struct MsgFrameEnd : public MsgBase { MsgFrameEnd(double dt); };
	    
		//------------------------------------------------------------------------------------------------

		/** @brief Message for mouse move events */
		struct MsgMouseMove : public MsgBase { MsgMouseMove(double dt, float x, float y); float m_x; float m_y; };
	    /** @brief Message for mouse button down events */
	    struct MsgMouseButtonDown : public MsgBase { MsgMouseButtonDown(double dt, int button); int m_button; };
	    /** @brief Message for mouse button up events */
	    struct MsgMouseButtonUp : public MsgBase { MsgMouseButtonUp(double dt, int button);  int m_button; };
	    /** @brief Message for mouse button repeat events */
	    struct MsgMouseButtonRepeat : public MsgBase { MsgMouseButtonRepeat(double dt, int button);  int m_button; };
	    /** @brief Message for mouse wheel events */
	    struct MsgMouseWheel : public MsgBase { MsgMouseWheel(double dt, float x, float y);  float m_x; float m_y; };
	    /** @brief Message for key down events */
	    struct MsgKeyDown : public MsgBase { MsgKeyDown(double dt, int key);  int m_key; };
	    /** @brief Message for key up events */
	    struct MsgKeyUp : public MsgBase { MsgKeyUp(double dt, int key);  int m_key; };
	    /** @brief Message for key repeat events */
	    struct MsgKeyRepeat : public MsgBase { MsgKeyRepeat(double dt, int key); int m_key; };
	    /** @brief Message for SDL events */
	    struct MsgSDL : public MsgBase { MsgSDL(double dt, SDL_Event event ); double m_dt; SDL_Event m_event; };

		//------------------------------------------------------------------------------------------------

		/** @brief Message for shadow map recreation */
		struct MsgShadowMapRecreated : public MsgBase { MsgShadowMapRecreated(); };
		/** @brief Message for light tranformation notification */
		struct MsgObjectChanged : public MsgBase { MsgObjectChanged(ObjectHandle object); ObjectHandle m_object{}; };

		//------------------------------------------------------------------------------------------------

	    /** @brief Message for loading a scene from file */
	    struct MsgSceneLoad : public MsgBase { MsgSceneLoad(Filename sceneName, aiPostProcessSteps ai_flags=aiProcess_Triangulate); Filename m_sceneName; aiPostProcessSteps m_ai_flags; };

	    /** @brief Message for creating a scene */
	    struct MsgSceneCreate : public MsgBase {
			MsgSceneCreate(ObjectHandle object, ParentHandle parent, Filename sceneName, aiPostProcessSteps ai_flags=aiProcess_Triangulate);
			ObjectHandle m_object{};
			ParentHandle m_parent{};
			Filename m_sceneName;
			aiPostProcessSteps m_ai_flags;
			const C_STRUCT aiScene* m_scene{};
		};

	    /** @brief Message for creating an object */
	    struct MsgObjectCreate : public MsgBase {
			MsgObjectCreate(ObjectHandle object, ParentHandle parent, System* sender=nullptr);
			ObjectHandle m_object{};
			ParentHandle m_parent{};
			System* m_sender{};
		};

		/** @brief Message for setting object parent */
		struct MsgObjectSetParent : public MsgBase { MsgObjectSetParent( ObjectHandle object, ParentHandle Parent); ObjectHandle m_object; ParentHandle m_parent;};
		/** @brief Message for destroying an object */
		struct MsgObjectDestroy : public MsgBase { MsgObjectDestroy(ObjectHandle); ObjectHandle m_handle; };

		//------------------------------------------------------------------------------------------------

		/** @brief Message for texture creation */
		struct MsgTextureCreate : public MsgBase { MsgTextureCreate(TextureHandle handle, System* sender=nullptr); TextureHandle m_handle; System* m_sender; };
	    /** @brief Message for texture destruction */
	    struct MsgTextureDestroy : public MsgBase { MsgTextureDestroy(TextureHandle handle); TextureHandle m_handle; };
	    /** @brief Message for mesh creation */
	    struct MsgMeshCreate : public MsgBase { MsgMeshCreate( MeshHandle handle); MeshHandle m_handle; };
	    /** @brief Message for mesh destruction */
	    struct MsgMeshDestroy : public MsgBase { MsgMeshDestroy(MeshHandle handle); MeshHandle m_handle; };
		/** @brief Message for deleted resources */
		struct MsgDeleted : public MsgBase { MsgDeleted(double dt ); void* m_ptr; uint64_t m_id; };

		//------------------------------------------------------------------------------------------------

	    /**
	     * @brief Generic message container
	     *
	     * Provides type-safe message passing between systems
	     */
	    struct Message {
	        /**
	         * @brief Constructor from message type
	         * @tparam T Message type derived from MsgBase
	         * @param msg Message instance
	         */
	        template<typename T>
	            requires (std::is_base_of_v<MsgBase, std::decay_t<T>> && sizeof(std::decay_t<T>) <= MAX_MESSAGE_SIZE)
	        Message( T&& msg ) {
	            m_typeID = std::type_index(typeid(T)).hash_code();
	            std::memcpy(m_data, &msg, sizeof(T));
	        };

	        /**
	         * @brief Get message type
	         * @return Message type identifier
	         */
	        auto GetType() -> size_t { return reinterpret_cast<MsgBase*>(m_data)->m_type; };
	        /**
	         * @brief Get delta time
	         * @return Delta time value
	         */
	        auto GetDt() -> double { return reinterpret_cast<MsgBase*>(m_data)->m_dt; };
	        /**
	         * @brief Set message phase
	         * @param phase Phase value to set
	         */
	        void SetPhase(int phase) { reinterpret_cast<MsgBase*>(m_data)->m_phase = phase; };
	        /**
	         * @brief Get message phase
	         * @return Current phase value
	         */
	        auto GetPhase() -> int { return reinterpret_cast<MsgBase*>(m_data)->m_phase; };

	        /**
	         * @brief Get message data as specific type
	         * @tparam T Message type
	         * @return Reference to message data
	         */
	        template<typename T>
	            requires (std::is_base_of_v<MsgBase, T> && sizeof(T) <= MAX_MESSAGE_SIZE)
	        auto GetData() -> T& {
	            assert(m_typeID == std::type_index(typeid(T)).hash_code() );
	            return *reinterpret_cast<T*>(m_data);
	        };

	        /**
	         * @brief Check if message has specific type
	         * @tparam T Message type to check
	         * @return True if message is of type T
	         */
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
        /**
         * @brief Constructor for System
         * @param systemName Name of the system
         * @param engine Reference to the engine
         */
        System( std::string systemName, Engine& engine );
        /**
         * @brief Destructor for System
         */
        virtual ~System();
        /**
         * @brief Get system name
         * @return System name
         */
        auto GetName() -> std::string { return m_name; };

    protected:
        SystemName 		m_name;
        Engine& 		m_engine;
		vecs::Registry&	m_registry;
    };

};



