
#include "VHInclude.h"
#include "VEInclude.h"


namespace vve {


	//-------------------------------------------------------------------------------------------------------
	// SDL Window

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

    auto WindowSDL::GetState2() -> std::tuple<vecs::Ref<WindowState>, vecs::Ref<WindowSDLState>> {
        return m_registry.template Get<WindowState&, WindowSDLState&>(m_windowStateHandle);
    }
	
    WindowSDL::WindowSDL( std::string systemName, Engine& engine,std::string windowName, int width, int height) 
                : Window(systemName, engine, windowName, width, height ) {

        engine.RegisterCallback( { 
			{this,     0, "INIT", [this](Message& message){ return OnInit(message);} },
			{this,     0, "POLL_EVENTS", [this](Message& message){ return OnPollEvents(message);} },
			{this,  3000, "QUIT", [this](Message& message){ return OnQuit(message);} },
		} );

        m_windowStateHandle = m_registry.Insert(WindowState{width, height, windowName}, WindowSDLState{});
    }

    WindowSDL::~WindowSDL() {}

    bool WindowSDL::OnInit(Message message) {
        auto state = GetState2();
        auto wstate = std::get<0>(state);
        auto wsdlstate = std::get<1>(state);
        
        if(!wsdlstate().m_sdl_initialized) {
            if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER | SDL_INIT_AUDIO) != 0) {
                printf("Error: %s\n", SDL_GetError());
                exit(1);
            }
            // From 2.0.18: Enable native IME.
        #ifdef SDL_HINT_IME_SHOW_UI
            SDL_SetHint(SDL_HINT_IME_SHOW_UI, "1");
        #endif
        wsdlstate().m_sdl_initialized = true;
        }

        // Create window with Vulkan graphics context
        SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
        auto sdlWindow = SDL_CreateWindow(wstate().m_windowName.c_str(), 
                            SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 
                            wstate().m_width, wstate().m_height, window_flags);

        m_registry.Put(m_windowStateHandle, WindowSDLState{true, sdlWindow});

        if (sdlWindow == nullptr) {
            printf("Error: SDL_CreateWindow(): %s\n", SDL_GetError());
            exit(1);
        }
        uint32_t extensions_count = 0;
        std::vector<const char*> extensions;
        SDL_Vulkan_GetInstanceExtensions(sdlWindow, &extensions_count, nullptr);
        extensions.resize(extensions_count);
        SDL_Vulkan_GetInstanceExtensions(sdlWindow, &extensions_count, extensions.data());
        wstate().m_instanceExtensions.insert(wstate().m_instanceExtensions.end(), extensions.begin(), extensions.end());

		m_engine.SendMessage( MsgExtensions{ wstate().m_instanceExtensions, {}} );
		return false;
    }

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
            m_engine.SendMessage( MsgSDL{message.GetDt(), event} );

            if (event.type == SDL_WINDOWEVENT) {
                switch (event.window.event) {
					case SDL_WINDOWEVENT_CLOSE:
						m_engine.Stop();
						break;
                	case SDL_WINDOWEVENT_MINIMIZED: 
                    	wstate().m_isMinimized = true;
                    	break;
                	case SDL_WINDOWEVENT_MAXIMIZED: 
                        wstate().m_isMinimized = false;
                    	break;
                	case SDL_WINDOWEVENT_RESTORED:
                        wstate().m_isMinimized = false;
                    	break;
                }
            } else {
			    switch( event.type ) {
					case SDL_QUIT:
	                	m_engine.Stop();
						break;
	                case SDL_MOUSEMOTION:
	                    m_engine.SendMessage( MsgMouseMove{message.GetDt(), event.motion.x, event.motion.y} );
	                    break;
	                case SDL_MOUSEBUTTONDOWN:
	                    m_engine.SendMessage( MsgMouseButtonDown{message.GetDt(), event.button.button} );
	                    button.push_back( event.button.button );
	                    break;
	                case SDL_MOUSEBUTTONUP:
	                    m_engine.SendMessage( MsgMouseButtonUp{message.GetDt(), event.button.button} );
	                    m_mouseButtonsDown.erase( event.button.button );
	                    break;
	                case SDL_MOUSEWHEEL:
	                    m_engine.SendMessage( MsgMouseWheel{message.GetDt(), event.wheel.x, event.wheel.y} );
	                    break;
	                case SDL_KEYDOWN:
	                    if( event.key.repeat ) continue;
	                    m_engine.SendMessage( MsgKeyDown{message.GetDt(), event.key.keysym.scancode} );
	                    key.push_back(event.key.keysym.scancode);
	                    break;
	                case SDL_KEYUP:
	                    m_engine.SendMessage( MsgKeyUp{message.GetDt(), event.key.keysym.scancode} );
	                    m_keysDown.erase(event.key.keysym.scancode);
	                    break;
	                default:
	                    break;
	            }
			}
        }

        for( auto& key1 : m_keysDown ) { m_engine.SendMessage( MsgKeyRepeat{message.GetDt(), key1} ); }
        for( auto& button1 : m_mouseButtonsDown ) { m_engine.SendMessage( MsgMouseButtonRepeat{message.GetDt(), button1} ); }

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

    bool WindowSDL::OnQuit(Message message) {
		SDL_DestroyWindow(std::get<1>(GetState2())().m_sdlWindow);
        SDL_Quit(); 
		return false;
    }


};  // namespace vve
