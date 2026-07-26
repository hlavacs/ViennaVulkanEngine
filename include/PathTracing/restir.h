namespace vve {
    //16 bytes nice alignment
    struct ReservoirDI {
        glm::vec4 samplePosition;
        uint32_t lightSelected;
        float lightWeight;
        float W_sum;
        uint32_t M;

        ReservoirDI() {
            lightSelected = 0;
            samplePosition = glm::vec4(0.0);
            lightWeight = 0.0;
            W_sum = 0.0;
            M = 0;
        }
    };


    // 12 + 12 + 12 + 12 = 48 bytes but gpu uses 16 byte alignemnt for float3 this leads to alignment issues
    /*
    struct ReservoirGI {
        glm::vec3 position;
        glm::vec3 normal;
        glm::vec3 radiance;
        float W_sum;
        uint32_t M;
        uint32_t age;

        ReservoirGI() {
            position = glm::vec3(0.0);
            normal = glm::vec3(0.0);
            radiance = glm::vec3(0.0);
            W_sum = 0.0;
            M = 0;
            age = 0;
        }
    };
    */

    struct ReservoirGI {
        glm::vec3 position;
        glm::vec3 normal;
        glm::vec3 radiance;
        float W_sum;
        uint32_t M;
        uint32_t age;
        float pad;

        ReservoirGI() {
            position = glm::vec3(0.0);
            normal = glm::vec3(0.0);
            radiance = glm::vec3(0.0);
            W_sum = 0.0;
            M = 0;
            age = 0;
            pad = 0.0;
        }
    };

    //112 bytes
    struct LightVertex {
        glm::vec4 radiance;
        glm::vec4 samplePosition;
        glm::vec4 hitPosition;
        glm::vec4 normal;
        glm::vec4 tangent;
        glm::vec2 uv;
        uint32_t lightIndex;
        uint32_t materialIndex;
        uint32_t age;
        uint32_t lastReevaluation;
        float selectionPdf;
        float importance;
      
        LightVertex() {
            radiance = glm::vec4(0.0);
            samplePosition = glm::vec4(0.0);
            hitPosition = glm::vec4(0.0);
            normal = glm::vec4(0.0);
            tangent = glm::vec4(0.0);
            uv = glm::vec2(0.0);
            uint32_t lightIndex = 0;
            uint32_t materialIndex = 0;
            uint32_t age = 0;
            uint32_t lastReevaluation = 0;
            float selectionPdf = 0.0;
            float importance = 0.0;

        }
    };

    //112 bytes
    struct VPL {
        glm::vec3 position;
        glm::vec3 normal;
        glm::vec3 radiance;
        uint32_t age;

        VPL() {
            position = glm::vec3(0.0);
            normal = glm::vec3(0.0);
            radiance = glm::vec3(0.0);
            uint32_t age = 0;
        }
    };

    struct InstantRadiosityUniforms {
        uint32_t LVCSize;
    };

    //16 bytes
    // also used for restir IR
    struct ReservoirLVC {
        uint32_t lightSelected;
        float lightWeight;
        float W_sum;
        uint32_t M;

        ReservoirLVC() {
            lightSelected = 0;
            lightWeight = 0.0;
            W_sum = 0.0;
            M = 0;
        }
    };

    struct BidirectionalUniforms {
        uint32_t LVCSize;
    };

}