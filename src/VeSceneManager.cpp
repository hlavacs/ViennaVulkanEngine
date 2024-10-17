
#include "VESystem.h"
#include "VESceneManager.h"

namespace vve {

   	template<ArchitectureType ATYPE>
    SceneManager<ATYPE>::SceneManager(Engine<ATYPE>& engine ) : m_engine{engine} {}

   	template<ArchitectureType ATYPE>
    SceneManager<ATYPE>::~SceneManager() {}

   	template<ArchitectureType ATYPE>
    void SceneManager<ATYPE>::DrawScene() {

        
    }

    template class SceneManager<ArchitectureType::SEQUENTIAL>;
    template class SceneManager<ArchitectureType::PARALLEL>;

};  // namespace vve

