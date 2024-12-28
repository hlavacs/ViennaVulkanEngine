#pragma once

#include <vector>
#include <set>



namespace vve {


	//-------------------------------------------------------------------------------------------------------
	// SDL Window

   	template<ArchitectureType ATYPE>
    class WindowSDL : public Window<ATYPE> {

        using Window<ATYPE>::m_engine;
        using Window<ATYPE>::m_width;
        using Window<ATYPE>::m_height;
        using Window<ATYPE>::m_windowName;
        using Window<ATYPE>::m_surface;
        using Window<ATYPE>::m_instanceExtensions;
		using Window<ATYPE>::m_isMinimized;
   
    public:
        WindowSDL(std::string systemName, Engine<ATYPE>& engine, std::string windowTitle, int width, int height );
        virtual ~WindowSDL();
        auto GetSDLWindow() -> SDL_Window* { return m_sdlWindow; }

    private:
        virtual void OnInit(Message message);
        virtual void OnInit2(Message message);
        virtual void OnPollEvents(Message message);
        virtual void OnQuit(Message message);

        inline static bool 		m_sdl_initialized{false};
        SDL_Window* 			m_sdlWindow{nullptr};
        int 					m_minImageCount = 2;
        bool 					m_swapChainRebuild = false;
        std::set<SDL_Scancode> 	m_keysDown;
        std::set<uint8_t> 		m_mouseButtonsDown;
    };


};  // namespace vve

