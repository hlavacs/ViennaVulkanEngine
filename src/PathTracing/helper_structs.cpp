#include "VHInclude.h"
#include "VEInclude.h"

namespace vve {

    std::array<VkVertexInputBindingDescription, 2> getBindingDescriptions() {
        std::array<VkVertexInputBindingDescription, 2> bindings{};

        // Vertex buffer (binding 0)
        bindings[0].binding = 0;
        bindings[0].stride = sizeof(Vertex);
        bindings[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        // Instance buffer (binding 1)
        bindings[1].binding = 1;
        bindings[1].stride = sizeof(vvh::Instance);
        bindings[1].inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;

        return bindings;
    }

    std::array<VkVertexInputAttributeDescription, 14> getAttributeDescriptions() {
        std::array<VkVertexInputAttributeDescription, 14> attributes{};

        // --- Vertex attributes (binding 0) ---
        attributes[0].binding = 0;
        attributes[0].location = 0;
        attributes[0].format = VK_FORMAT_R32G32B32A32_SFLOAT;
        attributes[0].offset = offsetof(Vertex, pos);

        attributes[1].binding = 0;
        attributes[1].location = 1;
        attributes[1].format = VK_FORMAT_R32G32B32A32_SFLOAT;
        attributes[1].offset = offsetof(Vertex, normal);

        attributes[2].binding = 0;
        attributes[2].location = 2;
        attributes[2].format = VK_FORMAT_R32G32B32A32_SFLOAT;
        attributes[2].offset = offsetof(Vertex, texCoord);

        // a shorter way to write the same thing as above
        attributes[3] = { 3, 1, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(vvh::Instance, model) + sizeof(glm::vec4) * 0 };
        attributes[4] = { 4, 1, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(vvh::Instance, model) + sizeof(glm::vec4) * 1 };
        attributes[5] = { 5, 1, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(vvh::Instance, model) + sizeof(glm::vec4) * 2 };
        attributes[6] = { 6, 1, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(vvh::Instance, model) + sizeof(glm::vec4) * 3 };

        attributes[7] = { 7, 1, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(vvh::Instance, model) + sizeof(glm::vec4) * 0 };
        attributes[8] = { 8, 1, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(vvh::Instance, model) + sizeof(glm::vec4) * 1 };
        attributes[9] = { 9, 1, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(vvh::Instance, model) + sizeof(glm::vec4) * 2 };
        attributes[10] = { 10, 1, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(vvh::Instance, model) + sizeof(glm::vec4) * 3 };



        attributes[11] = { 11, 1, VK_FORMAT_R32_UINT, offsetof(vvh::Instance, materialIndex) };

        attributes[12] = { 12, 1, VK_FORMAT_R32_UINT, offsetof(vvh::Instance, firstIndex) };

        attributes[13] = { 13, 1, VK_FORMAT_R32G32_SFLOAT, offsetof(vvh::Instance, uvScale) };

        return attributes;
    }
}