export module VVE:VEEngine;


import std.core;

#include "VEDefine.h"

import :VEUtil;
import :VETypes;
import :VEMap;
import :VEMemory;
import :VETableChunk;
import :VETableState;

#define _NODISCARD [[nodiscard]]
#include <experimental/generator>

export namespace vve {

    using namespace std::experimental;

    generator<bool> loop() {
         
        co_yield true;

        co_yield true;

        co_yield true;

        co_yield false;

    }

    bool computeOneFrame2(typename decltype(loop())::iterator& iter) {
        if (*iter) {
            iter++;
            //JREP;
        }
        return false;
    }

    bool go_on = true;
    bool computeOneFrame() {
        auto f = loop();
        auto iter = f.begin();
        computeOneFrame2(iter);
        return go_on;
    }

    void runGameLoop() {
        //JADD
        while (computeOneFrame()) {};
    }


};


