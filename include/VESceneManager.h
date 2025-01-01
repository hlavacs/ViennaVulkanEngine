#pragma once


namespace vve {


	//-------------------------------------------------------------------------------------------------------

	struct Transform {
		vec3_t m_position{0.0, 0.0, 0.0};
		quat_t m_rotation{};
		vec3_t m_scale{1.0, 1.0, 1.0};
		mat4_t m_matrix{1.0};
		Transform() { Matrix(); }
		auto Matrix() -> mat4_t { m_matrix = glm::translate(mat4_t{}, m_position) * glm::mat4_cast(m_rotation) * glm::scale(mat4_t{}, m_scale); return m_matrix; }
	};


	struct SceneNode {
		using Children = std::variant<std::pair<vecs::Handle,vecs::Handle>, std::vector<vecs::Handle>>;
		
		mat4_t m_localToWorldM{1.0};
		Transform m_localToParentT;
		vecs::Handle m_parent{};
		std::vector<vecs::Handle> m_children;
		void EraseChild(vecs::Handle handle)  { if(m_children.size()>0) m_children.erase(std::remove(m_children.begin(), m_children.end(), handle), m_children.end()); }
	};

	struct Camera {
		real_t m_near = 0.1f;
		real_t m_far = 100.0f;
		real_t m_fov = 45.0f;
		mat4_t m_projMatrix{1.0};
		Camera() { Matrix(); }
		auto Matrix() -> mat4_t { m_projMatrix = glm::perspective(glm::radians(m_fov), (real_t)1.0, m_near, m_far); return m_projMatrix; }
	};

	using SceneNodeHandle = vsty::strong_type_t<vecs::Handle, vsty::counter<>>; //need this to use SceneNode as a unique componen
	using TransformHandle = vsty::strong_type_t<vecs::Handle, vsty::counter<>>; //need this to use SceneNode as a unique componen
	using TextureHandle = vsty::strong_type_t<vecs::Handle, vsty::counter<>>; //need this to use Texture as a unique component
	using GeometryHandle = vsty::strong_type_t<vecs::Handle, vsty::counter<>>; //need this to use Geometry as a unique component
	using CameraHandle = vsty::strong_type_t<vecs::Handle, vsty::counter<>>; //need this to use Camera as a unique component

	//-------------------------------------------------------------------------------------------------------

    class SceneManager : public System {

		//using SceneNodeWrapper = vsty::strong_type_t<SceneNode, vsty::counter<>>; //need this to use SceneNode as a unique component
		const std::string m_rootName = "VVE RootSceneNode";

    public:
        SceneManager(std::string systemName, Engine& engine );
        virtual ~SceneManager();
		auto LoadTexture(std::string filename)-> vecs::Handle;
		auto LoadOBJ(std::string filename) -> vecs::Handle;
		auto LoadGLTF(std::string filename) -> vecs::Handle;

		auto GetAsset(std::string filename) -> vecs::Handle;

    private:
		void OnInit(Message message);
		void OnUpdate(Message message);
		void OnLoadObject(Message message);

		std::shared_mutex m_mutex;
		std::unordered_map<std::string, vecs::Handle> m_handleMap;
    };

};  // namespace vve

