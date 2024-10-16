#pragma once

#include "VEEngine.h"

namespace vve {

    class SceneManager {
        friend class engine;
    public:
        SceneManager(Engine& engine);
        virtual ~SceneManager();

    private:
        virtual void DrawScene();
        Engine& m_engine;
    };

};  // namespace vve

