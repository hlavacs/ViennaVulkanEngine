#pragma once

namespace vve {
    class TextureManager {
    private:
        std::vector<Texture*> textures{};
        VkSampler textureSampler{};
        VkDevice device;
        VkPhysicalDevice physicalDevice;
        CommandManager* commandManager;
    public:

        TextureManager(VkDevice& device, VkPhysicalDevice& physicalDevice, CommandManager* commandManager);

        Texture* loadTexture(std::string filepath);

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