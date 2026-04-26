namespace vve {
    //16 bytes nice alignment
    struct ReservoirDI {
        uint32_t lightSelected;
        float lightWeight;
        float W_sum;
        uint32_t M;

        ReservoirDI() {
            lightSelected = 0;
            lightWeight = 0.0;
            W_sum = 0.0;
            M = 0;
        }
    };


    // 12 + 12 + 12 + 12 = 48 bytes 
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
}