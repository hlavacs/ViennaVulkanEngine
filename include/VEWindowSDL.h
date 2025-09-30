#pragma once

#include <vector>
#include <set>



namespace vve {

	//-------------------------------------------------------------------------------------------------------
	// SDL Window

    /**
     * @brief Holds SDL-specific window state
     */
    struct WindowSDLState {
        bool 		m_sdl_initialized{false};
        SDL_Window* m_sdlWindow{nullptr};
    };

    /**
     * @brief SDL window implementation
     */
    class WindowSDL : public Window {
 
    public:
        /**
         * @brief Constructor for SDL Window
         * @param systemName Name of the system
         * @param engine Reference to the engine
         * @param windowTitle Title of the window
         * @param width Window width
         * @param height Window height
         */
        WindowSDL(std::string systemName, Engine& engine, std::string windowTitle, int width, int height );
        /**
         * @brief Destructor for SDL Window
         */
        virtual ~WindowSDL();
        /**
         * @brief Get SDL window state from registry
         * @param registry Reference to the VECS registry
         * @param windowName Name of the window (default: empty)
         * @return Tuple containing handle and state references
         */
        static auto GetState(vecs::Registry& registry, const std::string&& windowName = "") -> std::tuple<vecs::Handle, vecs::Ref<WindowState>, vecs::Ref<WindowSDLState>>;

    private:
        bool OnInit(Message message);
        bool OnPollEvents(Message message);
        bool OnQuit(Message message);
        /**
         * @brief Get both window and SDL state references
         * @return Tuple containing both state references
         */
        auto GetState2() -> std::tuple<vecs::Ref<WindowState>,  vecs::Ref<WindowSDLState>>;
        
        std::set<SDL_Scancode> 	m_keysDown;
        std::set<uint8_t> 		m_mouseButtonsDown;
    };


};  // namespace vve

