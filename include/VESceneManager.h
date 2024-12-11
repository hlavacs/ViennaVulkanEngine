#pragma once


#include "VESystem.h"
#include "VERendererVulkan.h"
#include "VECS.h"

namespace vve {

	//-------------------------------------------------------------------------------------------------------

	struct Transform {
		vec3_t m_position{0.0, 0.0, 0.0};
		quat_t m_rotation{};
		vec3_t m_scale{1.0, 1.0, 1.0};
		mat4_t m_transMatrix{1.0};
		Transform() { Matrix(); }
		auto Matrix() -> mat4_t { m_transMatrix = glm::translate(mat4_t{}, m_position) * glm::mat4_cast(m_rotation) * glm::scale(mat4_t{}, m_scale); return m_transMatrix; }
	};

	struct SceneNode {
		mat4_t m_worldTransformMatrix{1.0};
		Transform m_parentTransform;
		vecs::Handle m_parent{};
		std::vector<vecs::Handle> m_children;
		std::vector<vecs::Handle> m_objects;
		void EraseChild(vecs::Handle handle)  { if(m_children.size()>0) m_children.erase(std::remove(m_children.begin(), m_children.end(), handle), m_children.end()); }
		void EraseObject(vecs::Handle handle) { if(m_objects.size()>0)  m_objects.erase(std::remove(m_objects.begin(), m_objects.end(), handle), m_objects.end()); }
	};

	struct Camera {
		real_t m_near = 0.1f;
		real_t m_far = 100.0f;
		real_t m_fov = 45.0f;
		mat43_t m_projMatrix{1.0};
		Camera() { Matrix(); }
		auto Matrix() -> mat43_t { m_projMatrix = glm::perspective(glm::radians(m_fov), (real_t)1.0, m_near, m_far); return m_projMatrix; }
	};


	//-------------------------------------------------------------------------------------------------------

  	template<ArchitectureType ATYPE>
    class SceneManager : public System<ATYPE> {

		using System<ATYPE>::m_engine;

		struct SceneNodeWrapper {
			SceneNode m_sceneNode;
		};

    public:
        SceneManager(std::string systemName, Engine<ATYPE>* engine );
        virtual ~SceneManager();
		auto LoadTexture(std::string filename)-> vecs::Handle;
		auto LoadOBJ(std::string filename) -> vecs::Handle;
		auto LoadGLTF(std::string filename) -> vecs::Handle;

		auto GetAsset(std::string filename) -> vecs::Handle;

    private:
		virtual void OnInit(Message message);
		virtual void OnUpdate(Message message);

		std::unordered_map<std::string, vecs::Handle> m_files;
		vecs::Handle m_rootNode;
    };

};  // namespace vve

