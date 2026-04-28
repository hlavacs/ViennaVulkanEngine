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
        glm::vec4 position;
        glm::vec4 normal;
        glm::vec4 radiance;
        float W_sum;
        uint32_t M;
        uint32_t age;
        float pad;

        ReservoirGI() {
            position = glm::vec4(0.0);
            normal = glm::vec4(0.0);
            radiance = glm::vec4(0.0);
            W_sum = 0.0;
            M = 0;
            age = 0;
            pad = 0.0;
        }
    };
}