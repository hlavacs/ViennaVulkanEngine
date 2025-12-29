#pragma once

namespace vve {
    class TextureManager : public System {
    private:
        std::vector<Texture*> textures{};
        VkSampler textureSampler{};
        VkDevice device;
        VkPhysicalDevice physicalDevice;
        CommandManager* commandManager;
        bool textureCreated = false;
    public:

        TextureManager(std::string systemName, Engine& engine, VkDevice& device, VkPhysicalDevice& physicalDevice, CommandManager* commandManager);

        

        bool OnTextureCreate(Message message);

        bool OnTextureDestroy(Message message);
        bool OnRecordNextFrame(Message message);

        bool texturesChanged();

        /*
        ~TextureManager() {
            for (Texture* texture : textures) {
                vkDestroyImageView(device, texture->textureImageView, nullptr);
                vkDestroyImage(device, texture->textureImage, nullptr);
                vkFreeMemory(device, texture->textureImageMemory, nullptr);
            }
        }
        */

        void createTextureSampler();

        VkSampler getSampler();

        std::vector<Texture*> getTextures();
    };

}