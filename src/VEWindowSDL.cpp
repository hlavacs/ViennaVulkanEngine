
#include "VHInclude.h"
#include "VEInclude.h"


namespace vve {


	//-------------------------------------------------------------------------------------------------------
	// SDL Window

	/**
	 * @brief Retrieves the window state from the registry by name
	 * @param registry Reference to the entity registry
	 * @param windowName Name of the window to find (empty for first window)
	 * @return Tuple containing handle, window state, and SDL window state
	 */
    auto WindowSDL::GetState(vecs::Registry& registry, const std::string&& windowName) -> std::tuple<vecs::Handle, vecs::Ref<WindowState>, vecs::Ref<WindowSDLState>> {
        for( auto ret: registry.template GetView<vecs::Handle, WindowState&, WindowSDLState&>() ) {
            auto [handle, wstate, wsdlstate] = ret;
            if( windowName.empty() ) return ret;
            if( wstate().m_windowName == windowName ) return ret;
        }
        std::cout << "Window not found: " << windowName << std::endl;
        assert(false);
        exit(-1);   
        return { {}, {}, {} };
    }

	/**
	 * @brief Retrieves this window's state from the registry
	 * @return Tuple containing window state and SDL window state
	 */
    auto WindowSDL::GetState2() -> std::tuple<vecs::Ref<WindowState>, vecs::Ref<WindowSDLState>> {
        return m_registry.template Get<WindowState&, WindowSDLState&>(m_windowStateHandle);
    }

	/**
	 * @brief Constructs an SDL window and registers event callbacks
	 * @param systemName Name of the window system
	 * @param engine Reference to the engine instance
	 * @param windowName Name for the window
	 * @param width Window width in pixels
	 * @param height Window height in pixels
	 */
    WindowSDL::WindowSDL( std::string systemName, Engine& engine,std::string windowName, int width, int height) 
                : Window(systemName, engine, windowName, width, height ) {

        engine.RegisterCallbacks( { 
			{this,     0, "INIT", [this](Message& message){ return OnInit(message);} },
			{this,     0, "POLL_EVENTS", [this](Message& message){ return OnPollEvents(message);} },
			{this,  3000, "QUIT", [this](Message& message){ return OnQuit(message);} },
		} );

        m_windowStateHandle = m_registry.Insert(WindowState{width, height, windowName}, WindowSDLState{});
    }

	/**
	 * @brief Destructor for SDL window
	 */
    WindowSDL::~WindowSDL() {}

	/**
	 * @brief Initializes SDL and creates the window with Vulkan support
	 * @param message Initialization message
	 * @return false to continue message propagation
	 */
    bool WindowSDL::OnInit(Message message) {
        auto state = GetState2();
        auto wstate = std::get<0>(state);
        auto wsdlstate = std::get<1>(state);
        
        if(!wsdlstate().m_sdl_initialized) {
            if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
                SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to init SDL: %s", SDL_GetError());
                SDL_Quit();
                return EXIT_FAILURE;
            }
            // From 2.0.18: Enable native IME.
        #ifdef SDL_HINT_IME_SHOW_UI
            SDL_SetHint(SDL_HINT_IME_SHOW_UI, "1");
        #endif
            wsdlstate().m_sdl_initialized = true;
        }

        // Create window with Vulkan graphics context
        SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
        auto sdlWindow = SDL_CreateWindow(wstate().m_windowName.c_str(), 
                            wstate().m_width, wstate().m_height, window_flags);

        m_registry.Put(m_windowStateHandle, WindowSDLState{true, sdlWindow});

        if (sdlWindow == nullptr) {
            printf("Error: SDL_CreateWindow(): %s\n", SDL_GetError());
            exit(1);
        }
        uint32_t extensions_count = 0;
        std::vector<const char*> extensions;
        
        unsigned int sdlExtensionCount = 0;
        const char * const *instance_extensions = SDL_Vulkan_GetInstanceExtensions(&extensions_count);
        for (unsigned int i = 0; i < extensions_count; i++) {
            extensions.push_back(instance_extensions[i]);
        }

        wstate().m_instanceExtensions.insert(wstate().m_instanceExtensions.end(), extensions.begin(), extensions.end());

		m_engine.SendMsg( MsgExtensions{ wstate().m_instanceExtensions, {}} );
		return false;
    }

	/**
	 * @brief Polls SDL events and dispatches input messages to the engine
	 * @param message Poll events message
	 * @return false to continue message propagation
	 */
    bool WindowSDL::OnPollEvents(Message message) {
        // Poll and handle events (inputs, window resize, etc.)
        // You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
        // - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application, or clear/overwrite your copy of the mouse data.
        // - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application, or clear/overwrite your copy of the keyboard data.
        // Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.

        auto state = GetState2();
        auto wstate = std::get<0>(state);
        auto wsdlstate = std::get<1>(state);

        static std::vector<SDL_Scancode> key;
        static std::vector<int8_t> button;
        key.clear();
        button.clear();

        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            m_engine.SendMsg( MsgSDL{message.GetDt(), event} );

            switch (event.type) {
                case SDL_EVENT_QUIT:
	            case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
		            m_engine.Stop();
		            break;
            	case SDL_EVENT_WINDOW_MINIMIZED: 
                	wstate().m_isMinimized = true;
                	break;
            	case SDL_EVENT_WINDOW_MAXIMIZED: 
                    wstate().m_isMinimized = false;
                	break;
            	case SDL_EVENT_WINDOW_RESTORED:
                    wstate().m_isMinimized = false;
                	break;
                case SDL_EVENT_MOUSE_MOTION:
                    m_engine.SendMsg( MsgMouseMove{message.GetDt(), event.motion.x, event.motion.y} );
                    break;
                case SDL_EVENT_MOUSE_BUTTON_DOWN:
                    m_engine.SendMsg( MsgMouseButtonDown{message.GetDt(), event.button.button} );
                    button.push_back( event.button.button );
                    break;
                case SDL_EVENT_MOUSE_BUTTON_UP:
                    m_engine.SendMsg( MsgMouseButtonUp{message.GetDt(), event.button.button} );
                    m_mouseButtonsDown.erase( event.button.button );
                    break;
                case SDL_EVENT_MOUSE_WHEEL:
                    m_engine.SendMsg( MsgMouseWheel{message.GetDt(), event.wheel.x, event.wheel.y} );
                    break;
                case SDL_EVENT_KEY_DOWN:
                    if( event.key.repeat ) continue;
                    m_engine.SendMsg( MsgKeyDown{message.GetDt(), event.key.scancode} );
                    key.push_back(event.key.scancode);
                    break;
                case SDL_EVENT_KEY_UP:
                    m_engine.SendMsg( MsgKeyUp{message.GetDt(), event.key.scancode} );
	                m_keysDown.erase(event.key.scancode);
	                break;
	            default:
	                break;
			}
        }

        for( auto& key1 : m_keysDown ) { m_engine.SendMsg( MsgKeyRepeat{message.GetDt(), key1} ); }
        for( auto& button1 : m_mouseButtonsDown ) { m_engine.SendMsg( MsgMouseButtonRepeat{message.GetDt(), button1} ); }

        if(key.size() > 0) { for( auto& k : key ) { m_keysDown.insert(k) ; } }
        if(button.size() > 0) { for( auto& b : button ) {m_mouseButtonsDown.insert(b);} }

        if (SDL_GetWindowFlags(wsdlstate().m_sdlWindow) & SDL_WINDOW_MINIMIZED) {
            SDL_Delay(10);
            return false;
        }

        // Resize swap chain?
        SDL_GetWindowSize(wsdlstate().m_sdlWindow, &wstate().m_width, &wstate().m_height);
       
        return false;
    }

	/**
	 * @brief Cleans up SDL window resources when shutting down
	 * @param message Quit message
	 * @return false to continue message propagation
	 */
    bool WindowSDL::OnQuit(Message message) {
		SDL_DestroyWindow(std::get<1>(GetState2())().m_sdlWindow);
        SDL_Quit(); 
		return false;
    }


};  // namespace vve
