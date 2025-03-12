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

    class Window : public System {

    public:
        Window( std::string systemName, Engine& engine, std::string windowTitle, int width, int height );
        virtual ~Window();
        static auto GetState(vecs::Registry& registry, const std::string& windowName = "") -> std::tuple<vecs::Handle, vecs::Ref<WindowState>>;

    protected:
        auto GetState2() -> vecs::Ref<WindowState>;
		vecs::Handle 	m_windowStateHandle{};

    };

};   // namespace vve

