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
	

	struct VectorDefaultValue { static constexpr auto value = vec3_t{INFINITY, INFINITY, INFINITY}; };
	struct MaxtrixDefaultValue { static constexpr auto value = mat4_t{INFINITY}; };

	using Position = vsty::strong_type_t<vec3_t, vsty::counter<>, VectorDefaultValue >;
	using Rotation = vsty::strong_type_t<mat3_t, vsty::counter<>, MaxtrixDefaultValue>;
	using Scale = vsty::strong_type_t<vec3_t, vsty::counter<>, VectorDefaultValue>;
	using LocalToParentMatrix = vsty::strong_type_t<mat4_t, vsty::counter<>, MaxtrixDefaultValue>;
	using LocalToWorldMatrix = vsty::strong_type_t<mat4_t, vsty::counter<>, MaxtrixDefaultValue>;
	using ViewMatrix = vsty::strong_type_t<mat4_t, vsty::counter<>, MaxtrixDefaultValue>;
	using ProjectionMatrix = vsty::strong_type_t<mat4_t, vsty::counter<>, MaxtrixDefaultValue>;
	using Children = vsty::strong_type_t<std::vector<vecs::Handle>, vsty::counter<>>;

	//-------------------------------------------------------------------------------------------------------

    class SceneManager : public System {

		const std::string m_windowName = "VVE Window";
		const std::string m_rootName = "VVE RootSceneNode";
		const std::string m_cameraName = "VVE Camera";
		const std::string m_cameraNodeName = "VVE CameraNode";

    public:
        SceneManager(std::string systemName, Engine& engine );
        virtual ~SceneManager();

    private:
		bool OnInit(Message message);
		bool OnUpdate(Message message);
		bool OnSceneLoad(Message message);
		bool OnObjectLoad(Message message);
		bool OnObjectSetParent(Message message);
		bool OnKeyDown(Message message);
		
		std::shared_mutex m_mutex;
		vecs::Handle m_cameraHandle;
		vecs::Handle m_cameraNodeHandle;
		vecs::Handle m_rootHandle;
    };

};  // namespace vve





