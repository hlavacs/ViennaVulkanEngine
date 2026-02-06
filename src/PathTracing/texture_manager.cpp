/**
 * @file texture_manager.cpp
 * @brief TextureManager implementation.
 */

#include "VHInclude.h"
#include "VEInclude.h"

namespace vve {

    TextureManager::TextureManager(std::string systemName, Engine& engine, VkDevice& device, VkPhysicalDevice& physicalDevice, CommandManager* commandManager) :
        System{ systemName, engine }, device(device), physicalDevice(physicalDevice), commandManager(commandManager) {
        createTextureSampler();

        engine.RegisterCallbacks({
            {this,   1000, "TEXTURE_CREATE",   [this](Message& message) { return OnTextureCreate(message); } },
            {this,   1000, "TEXTURE_DESTROY",  [this](Message& message) { return OnTextureDestroy(message); } },
            {this,  1997, "RECORD_NEXT_FRAME", [this](Message& message) { return OnRecordNextFrame(message); } }
            });
    }

    TextureManager::~TextureManager() {

    }

    void TextureManager::freeResources() {
        for (Texture* texture : textures) {
            delete texture->image;
        }
        vkDestroySampler(device, textureSampler, nullptr);
    }

    bool TextureManager::OnTextureCreate(Message message) {
        auto msg = message.template GetData<MsgTextureCreate>();
        auto handle = msg.m_handle;
        auto texture = m_registry.template Get<vvh::Image&>(handle);
        auto pixels = texture().m_pixels;
        auto witdh = texture().m_width;
        auto height = texture().m_height;

        Image* image;
   
        image = new Image(static_cast<uint8_t*>(pixels), witdh, height, texture().format, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_IMAGE_ASPECT_COLOR_BIT, commandManager, device, physicalDevice);
        
        Texture* vrtTexture = new Texture();
        vrtTexture->image = image;

        vrtTexture->textureIndex = textures.size();
        texture().index = textures.size();

        textures.push_back(vrtTexture);

        textureCreated = true;

        return false;
    }

    bool TextureManager::OnRecordNextFrame(Message message) {
        textureCreated = false;
        return false;
    }

    bool TextureManager::OnTextureDestroy(Message message) {
        return false;
    }

    bool TextureManager::texturesChanged() {
        return textureCreated;
    }


    void TextureManager::createTextureSampler() {

        VkPhysicalDeviceProperties properties{};
        vkGetPhysicalDeviceProperties(physicalDevice, &properties);

        VkSamplerCreateInfo samplerInfo{};
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.magFilter = VK_FILTER_LINEAR;
        samplerInfo.minFilter = VK_FILTER_LINEAR;
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.anisotropyEnable = VK_TRUE;
        samplerInfo.maxAnisotropy = properties.limits.maxSamplerAnisotropy;
        samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
        samplerInfo.unnormalizedCoordinates = VK_FALSE;
        samplerInfo.compareEnable = VK_FALSE;
        samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        samplerInfo.mipLodBias = 0.0f;
        samplerInfo.minLod = 0.0f;
        samplerInfo.maxLod = 0.0f;

        if (vkCreateSampler(device, &samplerInfo, nullptr, &textureSampler) != VK_SUCCESS) {
            throw std::runtime_error("failed to create texture sampler!");
        }
    }

    VkSampler TextureManager::getSampler() {
        return textureSampler;
    }

    std::vector<Texture*> TextureManager::getTextures() {
        return textures;
    }
}
