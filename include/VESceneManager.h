#pragma once

#include "VEEngine.h"

namespace vve {

  	template<ArchitectureType ATYPE>
    class SceneManager : public System<ATYPE> {
        friend class engine;
    public:
        SceneManager(Engine<ATYPE>* engine);
        virtual ~SceneManager();

    private:
        virtual void DrawScene();
    };

};  // namespace vve

