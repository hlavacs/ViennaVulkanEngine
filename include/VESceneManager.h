#pragma once


namespace vve {

	//-------------------------------------------------------------------------------------------------------

	struct Camera {
		real_t m_aspect = 1.0f;
		real_t m_near = 0.1f;
		real_t m_far = 100.0f;
		real_t m_fov = 45.0f;
		auto Matrix() -> mat4_t { mat4_t proj = glm::perspective(glm::radians(m_fov), m_aspect, m_near, m_far); proj[1][1] *= -1; return proj; }
	};

	using ParentHandle = vsty::strong_type_t<vecs::Handle, vsty::counter<>>;
	using Children = vsty::strong_type_t<std::vector<vecs::Handle>, vsty::counter<>>;
	using Position = vsty::strong_type_t<vec3_t, vsty::counter<>>;
	using Rotation = vsty::strong_type_t<mat3_t, vsty::counter<>>;
	using Scale = vsty::strong_type_t<vec3_t, vsty::counter<>>;
	using LocalToParentMatrix = vsty::strong_type_t<mat4_t, vsty::counter<>>;
	using LocalToWorldMatrix = vsty::strong_type_t<mat4_t, vsty::counter<>>;
	using ViewMatrix = vsty::strong_type_t<mat4_t, vsty::counter<>>;
	using ProjectionMatrix = vsty::strong_type_t<mat4_t, vsty::counter<>>;

	using TextureHandle = vsty::strong_type_t<vecs::Handle, vsty::counter<>>; //need this to use Texture as a unique component
	using GeometryHandle = vsty::strong_type_t<vecs::Handle, vsty::counter<>>; //need this to use Geometry as a unique component
	using CameraHandle = vsty::strong_type_t<vecs::Handle, vsty::counter<>>; //need this to use Camera as a unique component

	//-------------------------------------------------------------------------------------------------------

    class SceneManager : public System {

		const std::string m_windowName = "VVE Window";
		const std::string m_rootName = "VVE RootSceneNode";
		const std::string m_cameraName = "VVE Camera";
		const std::string m_cameraNodeName = "VVE CameraNode";

    public:
        SceneManager(std::string systemName, Engine& engine );
        virtual ~SceneManager();
		auto LoadTexture(Name filenName)-> vecs::Handle;
		auto LoadOBJ(Name filenName) -> vecs::Handle;
		auto LoadGLTF(Name filenName) -> vecs::Handle;
		auto GetAsset(Name filenName) -> vecs::Handle;

    private:
		bool OnInit(Message message);
		bool OnUpdate(Message message);
		bool OnLoadObject(Message message);
		bool OnKeyDown(Message message);
		bool OnKeyRepeat(Message message);

		auto GetHandle(Name name) -> vecs::Handle&;	

		std::shared_mutex m_mutex;
		std::unordered_map<Name, vecs::Handle> m_handleMap;
		vecs::Handle m_cameraHandle;
		vecs::Handle m_cameraNodeHandle;
		vecs::Handle m_rootHandle;
    };

};  // namespace vve





