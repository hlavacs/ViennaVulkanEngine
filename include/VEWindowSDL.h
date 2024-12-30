#pragma once

#include <vector>
#include <set>



namespace vve {


	//-------------------------------------------------------------------------------------------------------
	// SDL Window

    class WindowSDL : public Window {

 
    public:
        WindowSDL(std::string systemName, Engine& engine, std::string windowTitle, int width, int height );
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

