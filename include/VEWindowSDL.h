#pragma once

#include <vector>
#include <set>



namespace vve {


	//-------------------------------------------------------------------------------------------------------
	// SDL Window

    struct WindowSDLState {
        bool 		m_sdl_initialized{false};
        SDL_Window* m_sdlWindow{nullptr};
    };

    class WindowSDL : public Window {
 
    public:
        WindowSDL(std::string systemName, Engine& engine, std::string windowTitle, int width, int height );
        virtual ~WindowSDL();
        static auto GetState(vecs::Registry& registry, const std::string&& windowName = "") -> std::tuple<vecs::Handle, vecs::Ref<WindowState>, vecs::Ref<WindowSDLState>>;

    private:
        bool OnInit(Message message);
        bool OnPollEvents(Message message);
        bool OnQuit(Message message);
        auto GetState2() -> std::tuple<vecs::Ref<WindowState>,  vecs::Ref<WindowSDLState>>;
        
        std::set<SDL_Scancode> 	m_keysDown;
        std::set<uint8_t> 		m_mouseButtonsDown;
    };


};  // namespace vve

