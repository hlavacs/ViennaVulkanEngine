#pragma once

#include "VEInclude.h"
#include "VESystem.h"
#include "VECS.h"

namespace vve {

  	template<ArchitectureType ATYPE>
    class SceneManager : public System<ATYPE> {

		struct Transform {
			vec3_t m_position{};
			quat_t m_rotation{};
			vec3_t m_scale{};
			mat4_t GetMatrix();
		};

		struct SceneNode {
			std::string m_name;
			Transform m_transform;
			size_t m_parent;
			std::vector<size_t> m_children;
			std::vector<vecs::Handle> m_objects;
		};

    public:
        SceneManager(std::string systemName, Engine<ATYPE>* engine );
        virtual ~SceneManager();
		bool LoadTexture(std::string filename);
		bool LoadOBJ(std::string filename);
		bool LoadGLTF(std::string filename);

    private:
    };

};  // namespace vve

