#pragma once

#include "VEEngine.h"

namespace vve {

  	template<ArchitectureType ATYPE>
    class SceneManager {
        friend class engine;
    public:
        SceneManager(Engine<ATYPE>& engine);
        virtual ~SceneManager();

    private:
        virtual void DrawScene();
        Engine<ATYPE>& m_engine;
    };

};  // namespace vve

