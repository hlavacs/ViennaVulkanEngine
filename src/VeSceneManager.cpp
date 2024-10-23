
#include "VESystem.h"
#include "VESceneManager.h"

namespace vve {

   	template<ArchitectureType ATYPE>
    SceneManager<ATYPE>::SceneManager(Engine<ATYPE>* engine, std::string name) : System<ATYPE>{engine, name } {}

   	template<ArchitectureType ATYPE>
    SceneManager<ATYPE>::~SceneManager() {}

   	template<ArchitectureType ATYPE>
    void SceneManager<ATYPE>::DrawScene() {

        
    }

    template class SceneManager<ArchitectureType::SEQUENTIAL>;
    template class SceneManager<ArchitectureType::PARALLEL>;

};  // namespace vve

