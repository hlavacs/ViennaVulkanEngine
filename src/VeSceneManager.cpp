
#include "VEInclude.h"
#include "VHInclude.h"
#include "VHVulkan.h"
#include "VESystem.h"
#include "VEEngine.h"
#include "VESceneManager.h"

namespace vve {

   	template<ArchitectureType ATYPE>
    SceneManager<ATYPE>::SceneManager(Engine<ATYPE>* engine, std::string name) 
		: System<ATYPE>{engine, name } {

	}

   	template<ArchitectureType ATYPE>
    SceneManager<ATYPE>::~SceneManager() {}

   	template<ArchitectureType ATYPE>
	bool SceneManager<ATYPE>::LoadTexture(std::string filename) {

		return false;
	}

   	template<ArchitectureType ATYPE>
	bool SceneManager<ATYPE>::LoadOBJ(std::string filename) {

		return false;
	}
	
   	template<ArchitectureType ATYPE>
	bool SceneManager<ATYPE>::LoadGLTF(std::string filename) {
		return false;
	}


   	template<ArchitectureType ATYPE>
	glm::mat4 SceneManager<ATYPE>::Transform::GetMatrix() {
		glm::mat4 matrix = glm::mat4(1.0f);
		matrix = glm::translate(matrix, m_position);
		matrix = matrix * glm::mat4_cast(m_rotation);
		matrix = glm::scale(matrix, m_scale);
		return matrix;
	}

    template class SceneManager<ENGINETYPE_SEQUENTIAL>;
    template class SceneManager<ENGINETYPE_PARALLEL>;

};  // namespace vve

