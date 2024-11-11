
#define SDL_MAIN_HANDLED

#include "VESystem.h"
#include "VHInclude.h"
#include "VEWindowSDL.h"
#include "VEEngine.h"
#include "VERendererVulkan.h"

namespace vve {

    MessageSDL::MessageSDL(void* s, void* r, double dt, SDL_Event event): MessageBase{MessageType::SDL, s, r}, m_dt{dt}, m_event{event} {};   

   	template<ArchitectureType ATYPE>
    WindowSDL<ATYPE>::WindowSDL( Engine<ATYPE>* engine,std::string windowName
            , int width, int height, std::string name) 
                : Window<ATYPE>(engine, windowName, width, height, name ) {

        engine->RegisterSystem( { 
			{this, -100000, MessageType::INIT, [this](Message message){this->OnInit(message);} },
			{this,       0, MessageType::INIT, [this](Message message){this->OnInit2(message);} },
			{this,       0, MessageType::POLL_EVENTS, [this](Message message){this->OnPollEvents(message);} },
			{this,       0, MessageType::PREPARE_NEXT_FRAME, [this](Message message){this->OnPrepareNextFrame(message);} },
			{this,       0, MessageType::RENDER_NEXT_FRAME, [this](Message message){this->OnRenderNextFrame(message);} },
			{this,       0, MessageType::PRESENT_NEXT_FRAME, [this](Message message){this->OnPresentNextFrame(message);} },
			{this,  100000, MessageType::QUIT, [this](Message message){this->OnQuit(message);} }
		} );
    }

   	template<ArchitectureType ATYPE>
    WindowSDL<ATYPE>::~WindowSDL() {}

   	template<ArchitectureType ATYPE>
    void WindowSDL<ATYPE>::OnInit(Message message) {
        if(!sdl_initialized) {
            if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER) != 0) {
                printf("Error: %s\n", SDL_GetError());
                return;
            }
            // From 2.0.18: Enable native IME.
        #ifdef SDL_HINT_IME_SHOW_UI
            SDL_SetHint(SDL_HINT_IME_SHOW_UI, "1");
        #endif
            sdl_initialized = true;
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
    }
        
   	template<ArchitectureType ATYPE>
    void WindowSDL<ATYPE>::OnInit2(Message message) {
        auto rend = ((RendererVulkan<ATYPE>*)(m_engine->GetSystem("VVE RendererVulkan")));

        if (SDL_Vulkan_CreateSurface(m_sdlWindow, rend->GetInstance(), &m_surface) == 0) {
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
            m_engine->SendMessage( MessageSDL{this, nullptr, message.GetDt(), event} );

            if (event.type == SDL_QUIT)
                m_engine->Stop();
            if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE && event.window.windowID == SDL_GetWindowID(m_sdlWindow))
                m_engine->Stop();

            switch( event.type ) {
                case SDL_MOUSEMOTION:
                    m_engine->SendMessage( MessageMouseMove{this, nullptr, message.GetDt(), event.motion.x, event.motion.y} );
                    break;
                case SDL_MOUSEBUTTONDOWN:
                    m_engine->SendMessage( MessageMouseButtonDown{this, nullptr, message.GetDt(), event.button.button} );
                    button.push_back( event.button.button );
                    break;
                case SDL_MOUSEBUTTONUP:
                    m_engine->SendMessage( MessageMouseButtonUp{this, nullptr, message.GetDt(), event.button.button} );
                    m_mouseButtonsDown.erase( event.button.button );
                    break;
                case SDL_MOUSEWHEEL:
                    m_engine->SendMessage( MessageMouseWheel{this, nullptr, message.GetDt(), event.wheel.x, event.wheel.y} );
                    break;
                case SDL_KEYDOWN:
                    if( event.key.repeat ) continue;
                    m_engine->SendMessage( MessageKeyDown{this, nullptr, message.GetDt(), event.key.keysym.scancode} );
                    key.push_back(event.key.keysym.scancode);
                    break;
                case SDL_KEYUP:
                    m_engine->SendMessage( MessageKeyUp{this, nullptr, message.GetDt(), event.key.keysym.scancode} );
                    m_keysDown.erase(event.key.keysym.scancode);
                    break;
                default:
                    break;
            }
        }

        for( auto& key : m_keysDown ) { m_engine->SendMessage( MessageKeyRepeat{this, nullptr, message.GetDt(), key} ); }
        for( auto& button : m_mouseButtonsDown ) { m_engine->SendMessage( MessageMouseButtonRepeat{this, nullptr, message.GetDt(), button} ); }

        if(key.size() > 0) { for( auto& k : key ) { m_keysDown.insert(k) ; } }
        if(button.size() > 0) { for( auto& b : button ) {m_mouseButtonsDown.insert(b);} }

        if (SDL_GetWindowFlags(m_sdlWindow) & SDL_WINDOW_MINIMIZED)
        {
            SDL_Delay(10);
            return;
        }

        // Resize swap chain?
        SDL_GetWindowSize(m_sdlWindow, &m_width, &m_height);
       
        return;
    }

   	template<ArchitectureType ATYPE>
    void WindowSDL<ATYPE>::OnPrepareNextFrame(Message message) {
    }

   	template<ArchitectureType ATYPE>
    void WindowSDL<ATYPE>::OnRenderNextFrame(Message message) {
    }

   	template<ArchitectureType ATYPE>
    void WindowSDL<ATYPE>::OnPresentNextFrame(Message message) {
    }

   	template<ArchitectureType ATYPE>
    void WindowSDL<ATYPE>::OnQuit(Message message) {
		SDL_DestroyWindow(m_sdlWindow);
        SDL_Quit();
    }

    template class WindowSDL<ArchitectureType::SEQUENTIAL>;
    template class WindowSDL<ArchitectureType::PARALLEL>;

};  // namespace vve
