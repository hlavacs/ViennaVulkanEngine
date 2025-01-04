#pragma once


namespace vve {
	using Name = vsty::strong_type_t<std::string, vsty::counter<>>;
}


namespace std {
    template<> struct hash<vve::Name> {
        size_t operator()(vve::Name const& name) const; 
    };
}

namespace vve {

	//-------------------------------------------------------------------------------------------------------

	struct Transform {
		vec3_t m_position{0.0, 0.0, 0.0};
		quat_t m_rotation{};
		vec3_t m_scale{1.0, 1.0, 1.0};
		mat4_t m_matrix{1.0};
		auto Matrix() -> mat4_t { m_matrix = glm::translate(mat4_t{}, m_position) * glm::mat4_cast(m_rotation) * glm::scale(mat4_t{}, m_scale); return m_matrix; }
	};

	struct SceneNode {
		using Children = std::variant<std::pair<vecs::Handle,vecs::Handle>, std::vector<vecs::Handle>>;
		
		Transform 		m_localToParentT;
		vecs::Handle 	m_parent{};
		std::vector<vecs::Handle> m_children;
		mat4_t 			m_localToWorldM{1.0};
		void EraseChild(vecs::Handle handle)  { if(m_children.size()>0) m_children.erase(std::remove(m_children.begin(), m_children.end(), handle), m_children.end()); }
	};

	struct Camera {
		real_t m_aspect = 1.0f;
		real_t m_near = 0.1f;
		real_t m_far = 100.0f;
		real_t m_fov = 45.0f;
		mat4_t m_proj{};
		auto Matrix() -> mat4_t { m_proj = glm::perspective(glm::radians(m_fov), m_aspect, m_near, m_far); return m_proj; }
	};

	//using Name = vsty::strong_type_t<std::string, vsty::counter<>>;
	using Parent = vsty::strong_type_t<vecs::Handle, vsty::counter<>>;
	using Children = vsty::strong_type_t<std::vector<vecs::Handle>, vsty::counter<>>;
	using Position = vsty::strong_type_t<vec3_t, vsty::counter<>>;
	using Orientation = vsty::strong_type_t<quat_t, vsty::counter<>>;
	using Scale = vsty::strong_type_t<vec3_t, vsty::counter<>>;
	using LocalToParentMatrix = vsty::strong_type_t<mat4_t, vsty::counter<>>;
	using LocalToWorldMatrix = vsty::strong_type_t<mat4_t, vsty::counter<>>;
	using ViewMatrix = vsty::strong_type_t<mat4_t, vsty::counter<>>;

	using SceneNodeHandle = vsty::strong_type_t<vecs::Handle, vsty::counter<>>; //need this to use SceneNode as a unique componen
	using TransformHandle = vsty::strong_type_t<vecs::Handle, vsty::counter<>>; //need this to use SceneNode as a unique componen
	using TextureHandle = vsty::strong_type_t<vecs::Handle, vsty::counter<>>; //need this to use Texture as a unique component
	using GeometryHandle = vsty::strong_type_t<vecs::Handle, vsty::counter<>>; //need this to use Geometry as a unique component
	using CameraHandle = vsty::strong_type_t<vecs::Handle, vsty::counter<>>; //need this to use Camera as a unique component

	//-------------------------------------------------------------------------------------------------------

    class SceneManager : public System {

		const std::string m_windowName = "VVE Window";
		const std::string m_rootName = "VVE RootSceneNode";
		const std::string m_cameraName = "VVE Camera";

    public:
        SceneManager(std::string systemName, Engine& engine );
        virtual ~SceneManager();
		auto LoadTexture(Name filenName)-> vecs::Handle;
		auto LoadOBJ(Name filenName) -> vecs::Handle;
		auto LoadGLTF(Name filenName) -> vecs::Handle;
		auto GetAsset(Name filenName) -> vecs::Handle;

    private:
		void OnInit(Message message);
		void OnUpdate(Message message);
		void OnLoadObject(Message message);
		auto GetHandle(Name name) -> vecs::Handle&;	

		std::shared_mutex m_mutex;
		std::unordered_map<Name, vecs::Handle> m_handleMap;
    };

};  // namespace vve





