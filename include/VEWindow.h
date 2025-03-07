#pragma once


namespace vve {

    struct WindowState {
        int 			            m_width{0};
        int 			            m_height{0};
        std::string 	            m_windowName{""};
        glm::vec4 		            m_clearColor{0.45f, 0.55f, 0.60f, 1.00f};
        bool 			            m_isMinimized{false};
        std::vector<const char*>    m_instanceExtensions{};
    };

    auto GetWindowState(vecs::Registry& registry) -> std::tuple<vecs::Handle, vecs::Ref<WindowState>>;

    class Window : public System {

    public:
        Window( std::string systemName, Engine& engine, std::string windowTitle, int width, int height );
        virtual ~Window();

        auto GetInstanceExtensions() -> std::vector<const char*> { return m_instanceExtensions; }; 

    protected:
        auto GetWindowState2() -> vecs::Ref<WindowState>;

        std::vector<const char*> m_instanceExtensions;
		vecs::Handle 	m_windowStateHandle{};

    };

};   // namespace vve

