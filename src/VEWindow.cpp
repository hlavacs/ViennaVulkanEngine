#include "VHInclude.h"
#include "VEInclude.h"


namespace vve {

    /**
     * @brief Retrieves window state from the registry by window name
     * @param registry ECS registry containing window states
     * @param windowName Name of the window to find (empty for first window)
     * @return Tuple containing the handle and reference to the window state
     */
    auto Window::GetState(vecs::Registry& registry, const std::string& windowName) -> std::tuple<vecs::Handle, vecs::Ref<WindowState>> {
        for( auto ret: registry.template GetView<vecs::Handle, WindowState&>() ) {
            auto [handle, wstate] = ret;
            if( windowName.empty() ) return ret;
            if( wstate().m_windowName == windowName ) return ret;
        }
        std::cout << "Window not found: " << windowName << std::endl;
        assert(false);
        exit(-1);   
        return { {}, {} };
    }

    /**
     * @brief Retrieves the window state reference for this window instance
     * @return Reference to the window state
     */
    auto Window::GetState2() -> vecs::Ref<WindowState> {
        return m_registry.template Get<WindowState&>(m_windowStateHandle);
    }

    /**
     * @brief Constructs a window system
     * @param systemName Name of the window system
     * @param engine Reference to the engine instance
     * @param windowName Name of the window
     * @param width Width of the window in pixels
     * @param height Height of the window in pixels
     */
    Window::Window(std::string systemName, Engine& engine,std::string windowName, int width, int height )
            : System(systemName, engine) {
    }

    /**
     * @brief Destructor for the window
     */
    Window::~Window(){}

};   // namespace vve
