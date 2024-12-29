
#include "VHInclude.h"
#include "VEInclude.h"


namespace vve {


	//-------------------------------------------------------------------------------------------------------
	// SDL Window
	
   	template<ArchitectureType ATYPE>
    WindowSDL<ATYPE>::WindowSDL( std::string systemName, Engine<ATYPE>& engine,std::string windowName, int width, int height) 
                : Window<ATYPE>(systemName, engine, windowName, width, height ) {

        engine.RegisterCallback( { 
  			{this,     0, "ANNOUNCE", [this](Message message){this->OnAnnounce(message);} },
			{this, -4000, "INIT", [this](Message message){this->OnInit(message);} },
			{this,     0, "INIT", [this](Message message){this->OnInit2(message);} },
			{this,     0, "POLL_EVENTS", [this](Message message){this->OnPollEvents(message);} },
			{this,     0, "QUIT", [this](Message message){this->OnQuit(message);} },
		} );
    }

   	template<ArchitectureType ATYPE>
    WindowSDL<ATYPE>::~WindowSDL() {}

   	template<ArchitectureType ATYPE>
    void WindowSDL<ATYPE>::OnAnnounce(Message message) {
		auto msg = message.template GetData<MsgAnnounce>();
		if( msg.m_sender->GetName() == "VVE Renderer Vulkan" ) {
			m_vulkan = (RendererVulkan<ATYPE>*)msg.m_sender;
		}
    }

   	template<ArchitectureType ATYPE>
    void WindowSDL<ATYPE>::OnInit(Message message) {
        if(!m_sdl_initialized) {
            if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER) != 0) {
                printf("Error: %s\n", SDL_GetError());
                return;
            }
            // From 2.0.18: Enable native IME.
        #ifdef SDL_HINT_IME_SHOW_UI
            SDL_SetHint(SDL_HINT_IME_SHOW_UI, "1");
        #endif
            m_sdl_initialized = true;
        }
        // Create window with Vulkan graphics context
        SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
        m_sdlWindow = SDL_CreateWindow(m_windowName.c_str(), SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, m_width, m_height, window_flags);
        if (m_sdlWindow == nullptr) {
            printf("Error: SDL_CreateWindow(): %s\n", SDL_GetError());
            return;
        }
        uint32_t extensions_count = 0;
        std::vector<const char*> extensions;
        SDL_Vulkan_GetInstanceExtensions(m_sdlWindow, &extensions_count, nullptr);
        extensions.resize(extensions_count);
        SDL_Vulkan_GetInstanceExtensions(m_sdlWindow, &extensions_count, extensions.data());
        m_instanceExtensions.insert(m_instanceExtensions.end(), extensions.begin(), extensions.end());

		m_engine.SendMessage( MsgExtensions{this, m_instanceExtensions, {}} );
		m_engine.SendMessage( MsgAnnounce{this} );
    }
        
   	template<ArchitectureType ATYPE>
    void WindowSDL<ATYPE>::OnInit2(Message message) {
        if (SDL_Vulkan_CreateSurface(m_sdlWindow, m_vulkan->GetInstance(), &m_surface) == 0) {
            printf("Failed to create Vulkan surface.\n");
        }
    }

   	template<ArchitectureType ATYPE>
    void WindowSDL<ATYPE>::OnPollEvents(Message message) {
        // Poll and handle events (inputs, window resize, etc.)
        // You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
        // - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application, or clear/overwrite your copy of the mouse data.
        // - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application, or clear/overwrite your copy of the keyboard data.
        // Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.

        static std::vector<SDL_Scancode> key;
        static std::vector<int8_t> button;
        key.clear();
        button.clear();

        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            m_engine.SendMessage( MsgSDL{this, nullptr, message.GetDt(), event} );

            if (event.type == SDL_WINDOWEVENT) {
                switch (event.window.event) {
					case SDL_WINDOWEVENT_CLOSE:
						m_engine.Stop();
						break;
                	case SDL_WINDOWEVENT_MINIMIZED: 
                    	m_isMinimized = true;
                    	break;
                	case SDL_WINDOWEVENT_MAXIMIZED: 
                    	m_isMinimized = false;
                    	break;
                	case SDL_WINDOWEVENT_RESTORED:
                    	m_isMinimized = false;
                    	break;
                }
            } else {
			    switch( event.type ) {
					case SDL_QUIT:
	                	m_engine.Stop();
						break;
	                case SDL_MOUSEMOTION:
	                    m_engine.SendMessage( MsgMouseMove{this, nullptr, message.GetDt(), event.motion.x, event.motion.y} );
	                    break;
	                case SDL_MOUSEBUTTONDOWN:
	                    m_engine.SendMessage( MsgMouseButtonDown{this, nullptr, message.GetDt(), event.button.button} );
	                    button.push_back( event.button.button );
	                    break;
	                case SDL_MOUSEBUTTONUP:
	                    m_engine.SendMessage( MsgMouseButtonUp{this, nullptr, message.GetDt(), event.button.button} );
	                    m_mouseButtonsDown.erase( event.button.button );
	                    break;
	                case SDL_MOUSEWHEEL:
	                    m_engine.SendMessage( MsgMouseWheel{this, nullptr, message.GetDt(), event.wheel.x, event.wheel.y} );
	                    break;
	                case SDL_KEYDOWN:
	                    if( event.key.repeat ) continue;
	                    m_engine.SendMessage( MsgKeyDown{this, nullptr, message.GetDt(), event.key.keysym.scancode} );
	                    key.push_back(event.key.keysym.scancode);
	                    break;
	                case SDL_KEYUP:
	                    m_engine.SendMessage( MsgKeyUp{this, nullptr, message.GetDt(), event.key.keysym.scancode} );
	                    m_keysDown.erase(event.key.keysym.scancode);
	                    break;
	                default:
	                    break;
	            }
			}
        }

        for( auto& key : m_keysDown ) { m_engine.SendMessage( MsgKeyRepeat{this, nullptr, message.GetDt(), key} ); }
        for( auto& button : m_mouseButtonsDown ) { m_engine.SendMessage( MsgMouseButtonRepeat{this, nullptr, message.GetDt(), button} ); }

        if(key.size() > 0) { for( auto& k : key ) { m_keysDown.insert(k) ; } }
        if(button.size() > 0) { for( auto& b : button ) {m_mouseButtonsDown.insert(b);} }

        if (SDL_GetWindowFlags(m_sdlWindow) & SDL_WINDOW_MINIMIZED) {
            SDL_Delay(10);
            return;
        }

        // Resize swap chain?
        SDL_GetWindowSize(m_sdlWindow, &m_width, &m_height);
       
        return;
    }

   	template<ArchitectureType ATYPE>
    void WindowSDL<ATYPE>::OnQuit(Message message) {
        auto rend = ((RendererVulkan<ATYPE>*)(m_engine.GetSystem("VVE Renderer Vulkan")));
        vkDestroySurfaceKHR(rend->GetInstance(), m_surface, nullptr);
		SDL_DestroyWindow(m_sdlWindow);
        SDL_Quit(); 
   }

    template class WindowSDL<ENGINETYPE_SEQUENTIAL>;
    template class WindowSDL<ENGINETYPE_PARALLEL>;

};  // namespace vve
