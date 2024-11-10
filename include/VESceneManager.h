#pragma once

#include "VEInclude.h"
#include "VESystem.h"

namespace vve {

  	template<ArchitectureType ATYPE>
    class SceneManager : public System<ATYPE> {
        friend class engine;
    public:
        SceneManager(Engine<ATYPE>* engine, std::string name = "VVE SceneManager" );
        virtual ~SceneManager();

    private:

    };

};  // namespace vve

