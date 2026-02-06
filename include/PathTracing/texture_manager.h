#pragma once

/**
 * @file texture_manager.h
 * @brief Texture loading and GPU image management.
 */

namespace vve {
    /** Manages textures, sampler creation, and texture lifecycle. */
    class TextureManager : public System {
    private:
        std::vector<Texture*> textures{};
        VkSampler textureSampler{};
        VkDevice device;
        VkPhysicalDevice physicalDevice;
        CommandManager* commandManager;
        bool textureCreated = false;
    public:

        /**
         * @param systemName System identifier.
         * @param engine Engine reference for messaging.
         * @param device Logical device.
         * @param physicalDevice Physical device for memory queries.
         * @param commandManager Command manager for staging copies.
         */
        TextureManager(std::string systemName, Engine& engine, VkDevice& device, VkPhysicalDevice& physicalDevice, CommandManager* commandManager);

        

        /**
         * Handle texture creation messages.
         * @param message Message payload.
         */
        bool OnTextureCreate(Message message);

        /**
         * Handle texture destruction messages.
         * @param message Message payload.
         */
        bool OnTextureDestroy(Message message);
        /**
         * Record texture updates for the next frame.
         * @param message Message payload.
         */
        bool OnRecordNextFrame(Message message);

        /** @return True if texture list changed since last frame. */
        bool texturesChanged();

        
        /** Release owned Vulkan resources. */
        ~TextureManager();
        /** Explicitly free Vulkan resources (destructor-safe). */
        void freeResources();
        

        /** Create the shared texture sampler. */
        void createTextureSampler();

        /** @return Texture sampler handle. */
        VkSampler getSampler();

        /** @return Texture list. */
        std::vector<Texture*> getTextures();
    };

}
