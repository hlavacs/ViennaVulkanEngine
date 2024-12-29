#pragma once

#include <vector>
#include <set>



namespace vve {


	//-------------------------------------------------------------------------------------------------------
	// SDL Window

   	template<ArchitectureType ATYPE>
    class WindowSDL : public Window<ATYPE> {

		using typename System<ATYPE>::Message;
		using typename System<ATYPE>::MsgAnnounce;
		using typename System<ATYPE>::MsgExtensions;
		using typename System<ATYPE>::MsgSDL;
		using typename System<ATYPE>::MsgKeyUp;
		using typename System<ATYPE>::MsgKeyDown;
		using typename System<ATYPE>::MsgKeyRepeat;
		using typename System<ATYPE>::MsgMouseMove;
		using typename System<ATYPE>::MsgMouseButtonDown;
		using typename System<ATYPE>::MsgMouseButtonUp;
		using typename System<ATYPE>::MsgMouseButtonRepeat;
		using typename System<ATYPE>::MsgMouseWheel;

        using Window<ATYPE>::m_engine;
        using Window<ATYPE>::m_width;
        using Window<ATYPE>::m_height;
        using Window<ATYPE>::m_windowName;
        using Window<ATYPE>::m_instanceExtensions;
		using Window<ATYPE>::m_isMinimized;
   
    public:
        WindowSDL(std::string systemName, Engine<ATYPE>& engine, std::string windowTitle, int width, int height );
        virtual ~WindowSDL();
        auto GetSDLWindow() -> SDL_Window* { return m_sdlWindow; }

    private:
        void OnInit(Message message);
        void OnPollEvents(Message message);
        void OnQuit(Message message);

        inline static bool 		m_sdl_initialized{false};
        SDL_Window* 			m_sdlWindow{nullptr};
        int 					m_minImageCount = 2;
        bool 					m_swapChainRebuild = false;
        std::set<SDL_Scancode> 	m_keysDown;
        std::set<uint8_t> 		m_mouseButtonsDown;
    };


};  // namespace vve

