#pragma once


namespace vve {

	//-------------------------------------------------------------------------------------------------------
	// Window base class

    /**
     * @brief Holds window state information
     */
    struct WindowState {
        int 			            m_width{0};
        int 			            m_height{0};
        std::string 	            m_windowName{""};
        glm::vec4 		            m_clearColor{0.45f, 0.55f, 0.60f, 1.00f};
        bool 			            m_isMinimized{false};
        std::vector<const char*>    m_instanceExtensions{};
    };

    /**
     * @brief Base window class for managing application windows
     */
    class Window : public System {

    public:
        /**
         * @brief Constructor for Window
         * @param systemName Name of the system
         * @param engine Reference to the engine
         * @param windowTitle Title of the window
         * @param width Window width
         * @param height Window height
         */
        Window( std::string systemName, Engine& engine, std::string windowTitle, int width, int height );
        /**
         * @brief Destructor for Window
         */
        virtual ~Window();
        /**
         * @brief Get window state from registry
         * @param registry Reference to the VECS registry
         * @param windowName Name of the window (default: empty)
         * @return Tuple containing handle and window state reference
         */
        static auto GetState(vecs::Registry& registry, const std::string& windowName = "") -> std::tuple<vecs::Handle, vecs::Ref<WindowState>>;

    protected:
        /**
         * @brief Get window state reference
         * @return Reference to window state
         */
        auto GetState2() -> vecs::Ref<WindowState>;
		vecs::Handle 	m_windowStateHandle{};

    };

};   // namespace vve

