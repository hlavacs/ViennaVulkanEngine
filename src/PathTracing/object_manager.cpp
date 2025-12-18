#include "VHInclude.h"
#include "VEInclude.h"

namespace vve {

    ObjectManager::ObjectManager(VkDevice& device, VkPhysicalDevice& physicalDevice, CommandManager* commandManager, VkPhysicalDeviceAccelerationStructurePropertiesKHR m_asProperties) :
        device(device), physicalDevice(physicalDevice), commandManager(commandManager), m_asProperties(m_asProperties) {
        loadRayTracingFunctions();
    }

    Object* ObjectManager::loadModel(std::string path, MaterialInfo* material) {
        
        return nullptr;
    }

    void ObjectManager::createVertexBuffer() {
        vertexBuffer = new DeviceBuffer<Vertex>(vertices.data(), vertices.size(), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
            VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT, commandManager, device, physicalDevice);
    }


    void ObjectManager::createIndexBuffer() {
        indexBuffer = new DeviceBuffer<uint32_t>(indices.data(), indices.size(), VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
            VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT, commandManager, device, physicalDevice);
    }

    Instance* ObjectManager::addInstance(Object* object) {

        glm::mat4 m;

        Instance* instance = new Instance();
        instance->materialIndex = object->materialIndex;
        instance->firstIndex = object->firstIndex;
        object->instances.push_back(instance);
        return instance;
    }
    /*
    Instance* ObjectManager::addInstanceb(Object* object, glm::mat4 model) {
        Instance* instance = new Instance();
        instance->model = model;
        object->instances.push_back(instance);
        return instance;
    }
    */

    std::vector<Instance> ObjectManager::accumulateInstances() {
        std::vector<Instance> instances;
        for (Object* object : objects) {
            object->firstInstance = instances.size();
            for (Instance* instance : object->instances) {
                instances.push_back(*instance);
            }
            object->instanceCount = instances.size() - object->firstInstance;
        }
        return instances;
    }

    void ObjectManager::createInstanceBuffers() {
        std::vector<Instance> instances = accumulateInstances();
        for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            HostBuffer<Instance>* instanceBuffer = new HostBuffer<Instance>(instances.data(), instances.size(), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, 0, device, physicalDevice);
            instanceBuffers.push_back(instanceBuffer);
        }
    }

    void ObjectManager::updateInstanceBuffer(const int currentImage) {
        std::vector<Instance> instances = accumulateInstances();

        if (instances.size() < maxInstances) {
            instanceBuffers[currentImage]->updateBuffer(instances.data(), instances.size());
        }
        else {
            //createInstanceBuffer(MAX_FRAMES_IN_FLIGHT, device, physicalDevice);

            //create single new instnace buffer
            // memcopy
        }
        /*
        if (instances.size() < maxInstances) {
            memcpy(instanceBuffersMapped[currentImage], instances.data(), sizeof(Instance) * instances.size());
        }
        else {
            //createInstanceBuffer(MAX_FRAMES_IN_FLIGHT, device, physicalDevice);

            //create single new instnace buffer
            // memcopy
        }
        */
    }

    std::vector<Object*> ObjectManager::getobjects() {
        return objects;
    }

    DeviceBuffer<Vertex>* ObjectManager::getVertexBuffer() {
        return vertexBuffer;
    }

    DeviceBuffer<uint32_t>* ObjectManager::getIndexBuffer() {
        return indexBuffer;
    }

    std::vector<HostBuffer<Instance>*> ObjectManager::getInstanceBuffers() {
        return instanceBuffers;
    }
}