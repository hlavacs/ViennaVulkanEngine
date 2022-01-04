/**
* The Vienna Vulkan Engine
*
* (c) bei Helmut Hlavacs, University of Vienna
*
*/

#ifndef VHHELPER_H
#define VHHELPER_H

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <map>
#include <mutex>
#include <random>
#include <set>
#include <thread>
#include <unordered_map>
#include <vector>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/hash.hpp>
#include <glm/gtx/transform.hpp>

#define _USE_MATH_DEFINES

#include <math.h>

#define VK_NO_PROTOTYPES
#include "vulkan/vulkan.h"
#include "VHFunctions.h"

#include "vk_mem_alloc.h"

#include <ThreadPool.h>
#include <stb_image.h>
#include <stb_image_write.h>
//#include <gli/gli.hpp>
#include "CLInclude.h"

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#define GLFW_INCLUDE_VULKAN

#include <GLFW/glfw3.h>

#include <irrKlang.h>

//NV helpers
#include "BottomLevelASGenerator.h"
#include "TopLevelASGenerator.h"

namespace vh
{
	//--------------------------------------------------------------------------------------------------------------------------------
	///need only for start up
	struct QueueFamilyIndices
	{
		int graphicsFamily = -1; ///<Index of graphics family
		int presentFamily = -1; ///<Index of present family

		///\returns true if the structure is filled completely
		bool isComplete()
		{
			return graphicsFamily >= 0 && presentFamily >= 0;
		}
	};

	///need only for start up
	struct SwapChainSupportDetails
	{
		VkSurfaceCapabilitiesKHR capabilities; ///<Surface capabilities
		std::vector<VkSurfaceFormatKHR> formats; ///<Surface formats available
		std::vector<VkPresentModeKHR> presentModes; ///<Possible present modes
	};

	//--------------------------------------------------------------------------------------------------------------------------------
	//vertex data

	///per vertex data that is stored in the vertex buffers
	struct vhVertex
	{
		glm::vec3 pos; ///<Vertex position
		glm::vec3 normal; ///<Vertex normal vector
		glm::vec3 tangent; ///<Tangent vector
		glm::vec2 texCoord; ///<Texture coordinates
		int entityId = 0; ///<for alignment

		///\returns the binding description of this vertex data structure
		static VkVertexInputBindingDescription getBindingDescription()
		{
			VkVertexInputBindingDescription bindingDescription = {};
			bindingDescription.binding = 0;
			bindingDescription.stride = sizeof(vhVertex);
			bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

			return bindingDescription;
		}

		///\returns the vertex attribute description of the vertex data
		static std::array<VkVertexInputAttributeDescription, 5> getAttributeDescriptions()
		{
			std::array<VkVertexInputAttributeDescription, 5> attributeDescriptions = {};

			attributeDescriptions[0].binding = 0;
			attributeDescriptions[0].location = 0;
			attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
			attributeDescriptions[0].offset = offsetof(vhVertex, pos);

			attributeDescriptions[1].binding = 0;
			attributeDescriptions[1].location = 1;
			attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
			attributeDescriptions[1].offset = offsetof(vhVertex, normal);

			attributeDescriptions[2].binding = 0;
			attributeDescriptions[2].location = 2;
			attributeDescriptions[2].format = VK_FORMAT_R32G32B32_SFLOAT;
			attributeDescriptions[2].offset = offsetof(vhVertex, tangent);

			attributeDescriptions[3].binding = 0;
			attributeDescriptions[3].location = 3;
			attributeDescriptions[3].format = VK_FORMAT_R32G32_SFLOAT;
			attributeDescriptions[3].offset = offsetof(vhVertex, texCoord);

			attributeDescriptions[4].binding = 0;
			attributeDescriptions[4].location = 4;
			attributeDescriptions[4].format = VK_FORMAT_R32_SINT;
			attributeDescriptions[4].offset = offsetof(vhVertex, entityId);

			return attributeDescriptions;
		}

		///Operator for comparing two vertices
		bool operator==(const vhVertex &other) const
		{
			return pos == other.pos && normal == other.normal && tangent == other.tangent &&
				texCoord == other.texCoord && entityId == other.entityId;
		}
	};

	//--------------------------------------------------------------------------------------------------------------------------------
	//structures for managing blocks of memory in the GPU

	struct vhMemoryHandle;

	///A block of N entries, which are UBOs that e.g. define world matrices etc.
	struct vhMemoryBlock
	{
		VkDevice device; ///<logical device to be used to create descriptor sets
		VmaAllocator allocator; ///<VMA allocator
		std::vector<VkBuffer> buffers; ///<One buffer for each framebuffer frame
		std::vector<VmaAllocation> allocations; ///<VMA information for the UBOs
		VkDescriptorPool descriptorPool; ///<Descriptor pool
		VkDescriptorSetLayout descriptorLayout; ///<Descriptor layout
		std::vector<VkDescriptorSet> descriptorSets; ///<Descriptor sets for UBO

		int8_t *pMemory; ///<pointer to the host memory containing a copy of the block
		uint32_t maxNumEntries; ///<maximum number of entries
		uint32_t sizeEntry; ///<length of one entry
		std::vector<vhMemoryHandle *> handles = {}; ///<list of pointers to the entry handles
		std::vector<bool> dirty; ///<if dirty this block needs to be updated

		///mark all UBO blocks as dirty
		void setDirty()
		{
			for (auto d : dirty)
				d = true; //mark as dirty
		};
	};

	///A handle into an entry of a memory block
	struct vhMemoryHandle
	{
		void *owner; ///<pointer to the owner of this entry
		vhMemoryBlock *pMemBlock; ///<pointer to the memory block
		uint32_t entryIndex = 0; ///<index into the entry list of the block

		///\returns the pointer to the UBO
		void *getPointer()
		{
			return pMemBlock->pMemory + pMemBlock->sizeEntry * entryIndex;
		};
	};

	// Ray tracing structures
	struct vhRayTracingScratchBuffer
	{
		uint64_t deviceAddress = 0;
		VkBuffer buffer = VK_NULL_HANDLE;
		void *mapped = nullptr;
		VmaAllocation allocation;
	};

	struct vhAccelerationStructure
	{
		// Common
		bool isDirty = false;
		VkBuffer instancesBuffer = VK_NULL_HANDLE;
		VkBuffer resultBuffer = VK_NULL_HANDLE;

		// RayTracingKHR
		VkAccelerationStructureKHR handleKHR = VK_NULL_HANDLE;
		uint64_t deviceAddress = 0;
		VkAccelerationStructureGeometryKHR geometry;
		VkAccelerationStructureBuildRangeInfoKHR rangeInfo;
		VmaAllocation resultBufferAllocation;
		VmaAllocation instancesBufferAllocation;

		// RayTracingNV
		VkAccelerationStructureNV handleNV = VK_NULL_HANDLE;
		VkBuffer scratchBuffer = VK_NULL_HANDLE;
		VkDeviceMemory scratchMem = VK_NULL_HANDLE;
		VkDeviceMemory resultMem = VK_NULL_HANDLE;
		VkDeviceMemory instancesMem = VK_NULL_HANDLE;

		// NV helpers
		nv_helpers_vk::BottomLevelASGenerator blasGenerator = {};
		nv_helpers_vk::TopLevelASGenerator tlasGenerator = {};
	};

	//--------------------------------------------------------------------------------------------------------------------------------
	//declaration of all helper functions

	//use this macro to check the function result, if its not VK_SUCCESS then return the error
#define VHCHECKRESULT(x)          \
    {                             \
        VkResult retval = (x);    \
        if (retval != VK_SUCCESS) \
        {                         \
            return retval;        \
        }                         \
    }

//--------------------------------------------------------------------------------------------------------------------------------

//instance (device)

//create a Vulkan instance
	VkResult vhDevCreateInstance(std::vector<const char *> &extensions, std::vector<const char *> &validationLayers, VkInstance *instance);

	//physical device
	VkResult
		vhDevPickPhysicalDevice(VkInstance instance, VkSurfaceKHR surface, std::vector<const char *> requiredExtensions, VkPhysicalDevice *physicalDevice, VkPhysicalDeviceFeatures *pFeatures, VkPhysicalDeviceLimits *limits);

	QueueFamilyIndices vhDevFindQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface);

	VkFormat vhDevFindSupportedFormat(VkPhysicalDevice physicalDevice, const std::vector<VkFormat> &candidates, VkImageTiling tiling, VkFormatFeatureFlags features);

	VkFormat vhDevFindDepthFormat(VkPhysicalDevice physicalDevice);

	//--------------------------------------------------------------------------------------------------------------------------------
	//logical device
	VkResult vhDevCreateLogicalDevice(VkInstance instance,
		VkPhysicalDevice physicalDevice,
		VkSurfaceKHR surface,
		std::vector<const char *> requiredDeviceExtensions,
		std::vector<const char *> requiredValidationLayers,
		void *pNextChain,
		VkDevice *device,
		VkQueue *graphicsQueue,
		VkQueue *presentQueue);

	//--------------------------------------------------------------------------------------------------------------------------------
	//swapchain
	SwapChainSupportDetails vhDevQuerySwapChainSupport(VkPhysicalDevice device, VkSurfaceKHR surface);

	VkResult vhSwapCreateSwapChain(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, VkDevice device, VkExtent2D frameBufferExtent, VkSwapchainKHR *swapChain, std::vector<VkImage> &swapChainImages, std::vector<VkImageView> &swapChainImageViews, VkFormat *swapChainImageFormat, VkExtent2D *swapChainExtent);

	//--------------------------------------------------------------------------------------------------------------------------------
	//buffer
	VkResult vhBufCreateBuffer(VmaAllocator
		allocator,
		VkDeviceSize size,
		VkBufferUsageFlags
		usage,
		VmaMemoryUsage vmaUsage,
		VkBuffer *buffer,
		VmaAllocation *allocation);

	VkResult vhBufCopyBuffer(VkDevice device, VkQueue graphicsQueue, VkCommandPool commandPool, VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);

	VkResult
		vhBufCreateImageView(VkDevice device, VkImage image, VkFormat format, VkImageViewType viewtype, uint32_t layerCount, VkImageAspectFlags aspectFlags, VkImageView *imageView);

	VkResult vhBufCreateDepthResources(VkDevice device, VmaAllocator allocator, VkQueue graphicsQueue, VkCommandPool commandPool, VkExtent2D swapChainExtent, VkFormat depthFormat, VkImage *depthImage, VmaAllocation *depthImageAllocation, VkImageView *depthImageView);

	VkResult vhBufCreateOffscreenResources(VkDevice device, VmaAllocator allocator, VkQueue graphicsQueue, VkCommandPool commandPool, VkExtent2D extent, VkFormat format, VkImage *image, VmaAllocation *colorImageAllocation, VkImageView *colorImageView);

	VkResult vhBufCreateImage(VmaAllocator
		allocator,
		uint32_t width,
		uint32_t
		height,
		uint32_t miplevels,
		uint32_t
		arrayLayers,
		VkFormat format,
		VkImageTiling
		tiling,
		VkImageUsageFlags usage,
		VkImageCreateFlags
		flags,
		VkImage *image,
		VmaAllocation *allocation);

	VkResult vhBufCopyBufferToImage(VkDevice device, VkQueue queue, VkCommandPool commandPool, VkBuffer buffer, VkImage image, uint32_t layerCount, uint32_t width, uint32_t height);

	VkResult vhBufCopyBufferToImage(VkDevice device, VkQueue queue, VkCommandPool commandPool, VkBuffer buffer, VkImage image, std::vector<VkBufferImageCopy> &regions, uint32_t width, uint32_t height);

	VkResult vhBufCopyImageToBuffer(VkDevice device, VkQueue queue, VkCommandPool commandPool, VkImage image, VkImageAspectFlagBits aspect, VkBuffer buffer, uint32_t layerCount, uint32_t width, uint32_t height);

	VkResult vhBufCopyImageToBuffer(VkDevice device, VkQueue queue, VkCommandPool commandPool, VkImage image, VkBuffer buffer, std::vector<VkBufferImageCopy> &regions, uint32_t width, uint32_t height);

	VkResult vhBufTransitionImageLayout(VkDevice device, VkQueue graphicsQueue, VkCommandPool commandPool, VkImage image, VkFormat format, VkImageAspectFlagBits aspect, uint32_t miplevels, uint32_t layerCount, VkImageLayout oldLayout, VkImageLayout newLayout);

	VkResult vhBufTransitionImageLayout(VkDevice device, VkQueue graphicsQueue, VkCommandBuffer commandBuffer, VkImage image, VkFormat format, VkImageAspectFlagBits aspect, uint32_t miplevels, uint32_t layerCount, VkImageLayout oldLayout, VkImageLayout newLayout);

	VkResult
		vhBufCreateTextureImage(VkDevice device, VmaAllocator allocator, VkQueue graphicsQueue, VkCommandPool commandPool, std::string basedir, std::vector<std::string> names, VkImageCreateFlags flags, VkImage *textureImage, VmaAllocation *textureImageAllocation, VkExtent2D *extent);

	//VkResult vhBufCreateTexturecubeImage(VkDevice device, VmaAllocator allocator, VkQueue graphicsQueue, VkCommandPool commandPool, gli::texture_cube &cube, VkImage *textureImage, VmaAllocation *textureImageAllocation, VkFormat *pformat);
	VkResult vhBufCreateTextureSampler(VkDevice device, VkSampler *textureSampler);

	VkResult vhBufCreateFramebuffers(VkDevice device, std::vector<VkImageView> imageViews, std::vector<VkImageView> depthImageViews, VkRenderPass renderPass, VkExtent2D extent, std::vector<VkFramebuffer> &frameBuffers);

	VkResult vhBufCreateFramebuffersOffscreen(VkDevice device, std::vector<VkImageView> positionImageViews, std::vector<VkImageView> normalImageViews, std::vector<VkImageView> albedoImageViews, std::vector<VkImageView> depthImageViews, VkRenderPass renderPass, VkExtent2D extent, std::vector<VkFramebuffer> &frameBuffers);

	VkResult vhBufCopySwapChainImageToHost(VkDevice device, VmaAllocator allocator, VkQueue graphicsQueue, VkCommandPool commandPool, VkImage image, VkFormat format, VkImageAspectFlagBits aspect, VkImageLayout layout,
		/*gli::byte*/ unsigned char *bufferData,
		uint32_t width,
		uint32_t height,
		uint32_t imageSize);

	VkResult vhBufCopyImageToHost(VkDevice device, VmaAllocator allocator, VkQueue graphicsQueue, VkCommandPool commandPool, VkImage image, VkFormat format, VkImageAspectFlagBits aspect, VkImageLayout layout,
		/*gli::byte*/ unsigned char *bufferData,
		uint32_t width,
		uint32_t height,
		uint32_t imageSize);

	VkResult vhBufCreateVertexBuffer(VkDevice device, VmaAllocator allocator, VkQueue graphicsQueue, VkCommandPool commandPool, std::vector<vh::vhVertex> &vertices, VkBuffer *vertexBuffer, VmaAllocation *vertexBufferAllocation);

	VkResult
		vhBufCreateIndexBuffer(VkDevice device, VmaAllocator allocator, VkQueue graphicsQueue, VkCommandPool commandPool, std::vector<uint32_t> &indices, VkBuffer *indexBuffer, VmaAllocation *indexBufferAllocation);

	VkResult vhBufCreateUniformBuffers(VmaAllocator
		allocator,
		uint32_t numberBuffers,
		VkDeviceSize
		bufferSize,
		std::vector<VkBuffer> &uniformBuffers,
		std::vector<VmaAllocation> &uniformBuffersAllocation);

	//--------------------------------------------------------------------------------------------------------------------------------
	//rendering
	VkResult vhRenderCreateRenderPass(VkDevice device, VkFormat swapChainImageFormat, VkFormat depthFormat, VkAttachmentLoadOp loadOp, VkRenderPass *renderPass);

	VkResult vhRenderCreateRenderPassOffscreen(VkDevice device, VkFormat depthFormat, VkRenderPass *renderPass);

	VkResult vhRenderCreateRenderPassRayTracing(VkDevice device, VkFormat swapChainImageFormat, VkFormat depthFormat, VkRenderPass *renderPass);

	VkResult vhRenderCreateRenderPassShadow(VkDevice device, VkFormat depthFormat, VkRenderPass *renderPass);

	VkResult vhRenderCreateDescriptorSetLayout(VkDevice device, std::vector<uint32_t> counts, std::vector<VkDescriptorType> types, std::vector<VkShaderStageFlags> stageFlags, VkDescriptorSetLayout *descriptorSetLayout);

	VkResult vhRenderCreateDescriptorPool(VkDevice device, std::vector<VkDescriptorType> types, std::vector<uint32_t> numberDesc, VkDescriptorPool *descriptorPool);

	VkResult vhRenderCreateDescriptorSets(VkDevice device, uint32_t numberDesc, VkDescriptorSetLayout descriptorSetLayout, VkDescriptorPool descriptorPool, std::vector<VkDescriptorSet> &descriptorSets);

	VkResult vhRenderUpdateDescriptorSet(VkDevice device, VkDescriptorSet descriptorSet, std::vector<VkBuffer> uniformBuffers, std::vector<unsigned long long> bufferRanges, std::vector<std::vector<VkImageView>> textureImageViews, std::vector<std::vector<VkSampler>> textureSamplers);

	VkResult vhRenderUpdateDescriptorSet(VkDevice device, VkDescriptorSet descriptorSet, std::vector<VkDescriptorType> descriptorTypes, std::vector<VkBuffer> uniformBuffers, std::vector<unsigned long long> bufferRanges, std::vector<std::vector<VkImageView>> textureImageViews, std::vector<std::vector<VkSampler>> textureSamplers);

	VkResult vhRenderUpdateDescriptorSetMaps(VkDevice device,
		VkDescriptorSet descriptorSet,
		uint32_t binding,
		uint32_t offset,
		uint32_t descriptorCount,
		std::vector<std::vector<VkDescriptorImageInfo>> &maps);

	VkResult vhRenderBeginRenderPass(VkCommandBuffer
		commandBuffer,
		VkRenderPass renderPass,
		VkFramebuffer
		frameBuffer,
		VkExtent2D extent,
		VkSubpassContents
		subPassContents);

	VkResult vhRenderBeginRenderPass(VkCommandBuffer
		commandBuffer,
		VkRenderPass renderPass,
		VkFramebuffer
		frameBuffer,
		std::vector<VkClearValue> &clearValues,
		VkExtent2D
		extent,
		VkSubpassContents subPassContents);

	VkResult vhRenderPresentResult(VkQueue presentQueue, VkSwapchainKHR swapChain, uint32_t imageIndex, VkSemaphore signalSemaphore);

	VkResult
		vhPipeCreateGraphicsPipelineLayout(VkDevice device, std::vector<VkDescriptorSetLayout> descriptorSetLayouts, std::vector<VkPushConstantRange> pushConstantRanges, VkPipelineLayout *pipelineLayout);

	VkShaderModule vhPipeCreateShaderModule(VkDevice device, const std::vector<char> &code);

	VkResult vhPipeCreateGraphicsPipeline(VkDevice device, std::vector<std::string> shaderFileNames, VkExtent2D swapChainExtent, VkPipelineLayout pipelineLayout, VkRenderPass renderPass, std::vector<VkDynamicState> dynamicStates, VkPipeline *graphicsPipeline, VkCullModeFlags cullMode = VK_CULL_MODE_NONE, int32_t blendAttachmentSize = 1);

	VkResult vhPipeCreateGraphicsShadowPipeline(VkDevice device, std::string verShaderFilename, VkExtent2D shadowMapExtent, VkPipelineLayout pipelineLayout, VkRenderPass renderPass, VkPipeline *graphicsPipeline);

	//--------------------------------------------------------------------------------------------------------------------------------
	//file
	std::vector<char> vhFileRead(const std::string &filename);

	//timing functions
	std::chrono::high_resolution_clock::time_point vhTimeNow();

	float vhTimeDuration(std::chrono::high_resolution_clock::time_point t_prev);

	float vhAverage(float new_val, float avgerage, float weight = 0.8f);

	//--------------------------------------------------------------------------------------------------------------------------------
	//command
	VkResult vhCmdCreateCommandPool(VkPhysicalDevice physicalDevice, VkDevice device, VkSurfaceKHR surface, VkCommandPool *commandPool);

	VkResult vhCmdCreateCommandBuffers(VkDevice device, VkCommandPool commandPool, VkCommandBufferLevel level, uint32_t count, VkCommandBuffer *pBuffers);

	VkResult
		vhCmdBeginCommandBuffer(VkDevice device, VkCommandBuffer commandBuffer, VkCommandBufferUsageFlagBits usageFlags);

	VkResult
		vhCmdBeginCommandBuffer(VkDevice device, VkRenderPass renderPass, uint32_t subpass, VkFramebuffer framebuffer, VkCommandBuffer commandBuffer, VkCommandBufferUsageFlagBits usageFlags);

	VkResult vhCmdSubmitCommandBuffer(VkDevice device, VkQueue graphicsQueue, VkCommandBuffer commandBuffer, VkSemaphore waitSemaphore, VkSemaphore signalSemaphore, VkFence waitFence);

	VkCommandBuffer vhCmdBeginSingleTimeCommands(VkDevice device, VkCommandPool commandPool);

	VkResult vhCmdEndSingleTimeCommands(VkDevice device, VkQueue graphicsQueue, VkCommandPool commandPool, VkCommandBuffer commandBuffer);

	VkResult vhCmdEndSingleTimeCommands(VkDevice device, VkQueue graphicsQueue, VkCommandPool commandPool, VkCommandBuffer commandBuffer, VkSemaphore waitSemaphore, VkSemaphore signalSemaphore, VkFence waitFence);

	//--------------------------------------------------------------------------------------------------------------------------------
	//memory
	uint32_t
		vhMemFindMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties);

	VkResult vhMemCreateVMAAllocator(VkInstance instance, VkPhysicalDevice physicalDevice, VkDevice device, VmaAllocator &allocator);

	//Memory blocks
	VkResult vhMemBlockListInit(VkDevice device, VmaAllocator allocator, VkDescriptorPool descriptorPool, VkDescriptorSetLayout descriptorLayout, uint32_t maxNumEntries, uint32_t sizeEntry, uint32_t numBuffers, std::vector<vhMemoryBlock *> &blocklist);

	VkResult vhMemBlockInit(VkDevice device, VmaAllocator allocator, VkDescriptorPool descriptorPool, VkDescriptorSetLayout descriptorLayout, uint32_t maxNumEntries, uint32_t sizeEntry, uint32_t numBuffers, vhMemoryBlock *pBlock);

	VkResult vhMemBlockListAdd(std::vector<vhMemoryBlock *> &blocklist, void *owner, vhMemoryHandle *handle);

	VkResult vhMemBlockAdd(vhMemoryBlock *pBlock, void *owner, vhMemoryHandle *handle);

	VkResult vhMemBlockUpdateEntry(vhMemoryHandle *pHandle, void *data);

	VkResult vhMemBlockUpdateBlockList(std::vector<vhMemoryBlock *> &blocklist, uint32_t index);

	VkResult vhMemBlockRemoveEntry(vhMemoryHandle *pHandle);

	VkResult vhMemBlockListClear(std::vector<vhMemoryBlock *> &blocklist);

	VkResult vhMemBlockDeallocate(vhMemoryBlock *pBlock);

	//--------------------------------------------------------------------------------------------------------------------------------
	//ray tracing
	vhRayTracingScratchBuffer vhCreateScratchBuffer(VkDevice device, VmaAllocator vmaAllocator, VkDeviceSize size);

	void vhDeleteScratchBuffer(VmaAllocator
		vmaAllocator,
		vhRayTracingScratchBuffer &scratchBuffer);

	uint64_t vhGetBufferDeviceAddress(VkDevice device, VkBuffer buffer);

	VkResult vhCreateAccelerationStructureKHR(VmaAllocator
		vmaAllocator,
		vhAccelerationStructure &accelerationStructure,
		VkAccelerationStructureBuildSizesInfoKHR
		buildSizeInfo);

	void vhDestroyAccelerationStructure(VkDevice device, VmaAllocator vmaAllocator, vhAccelerationStructure &as);

	VkResult
		vhCreateBottomLevelAccelerationStructureKHR(VkDevice device, VmaAllocator vmaAllocator, VkCommandPool commandPool, VkQueue graphicsQueue, vhAccelerationStructure &blas, const VkBuffer vertexBuffer, uint32_t vertexCount, const VkBuffer indexBuffer, uint32_t indexCount, const VkBuffer transformBuffer, uint32_t transformOffset);

	VkResult
		vhUpdateBottomLevelAccelerationStructureKHR(VkDevice device, VmaAllocator vmaAllocator, VkCommandPool commandPool, VkQueue graphicsQueue, vhAccelerationStructure &blas, const VkBuffer vertexBuffer, uint32_t vertexCount, const VkBuffer indexBuffer, uint32_t indexCount, const VkBuffer transformBuffer, uint32_t transformOffset);

	VkResult
		vhCreateTopLevelAccelerationStructureKHR(VkDevice device, VmaAllocator vmaAllocator, VkCommandPool commandPool, VkQueue graphicsQueue, std::vector<vhAccelerationStructure> &blas, vhAccelerationStructure &tlas);

	VkResult
		vhUpdateTopLevelAccelerationStructureKHR(VkDevice device, VmaAllocator vmaAllocator, VkCommandPool commandPool, VkQueue graphicsQueue, std::vector<vhAccelerationStructure> &blas, vhAccelerationStructure &tlas);

	VkResult vhCreateBottomLevelAccelerationStructureNV(VkPhysicalDevice physicalDevice, VkDevice device, VmaAllocator vmaAllocator, VkCommandPool commandPool, VkQueue graphicsQueue, vhAccelerationStructure &blas, const VkBuffer vertexBuffer, uint32_t vertexCount, const VkBuffer indexBuffer, uint32_t indexCount, const VkBuffer transformBuffer, uint32_t transformOffset);

	VkResult
		vhUpdateBottomLevelAccelerationStructureNV(VkDevice device, VmaAllocator vmaAllocator, VkCommandPool commandPool, VkQueue graphicsQueue, vhAccelerationStructure &blas, const VkBuffer vertexBuffer, uint32_t vertexCount, const VkBuffer indexBuffer, uint32_t indexCount, const VkBuffer transformBuffer, uint32_t transformOffset);

	VkResult
		vhCreateTopLevelAccelerationStructureNV(VkPhysicalDevice physicalDevice, VkDevice device, VmaAllocator vmaAllocator, VkCommandPool commandPool, VkQueue graphicsQueue, std::vector<vhAccelerationStructure> &blas, vhAccelerationStructure &tlas);

	VkResult vhUpdateTopLevelAccelerationStructureNV(VkDevice device, VmaAllocator vmaAllocator, VkCommandPool commandPool, VkQueue graphicsQueue, std::vector<vhAccelerationStructure> &blas, vhAccelerationStructure &tlas);

	//--------------------------------------------------------------------------------------------------------------------------------
	//debug
	VKAPI_ATTR VkBool32

		VKAPI_CALL
		vhDebugCallback(VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT objType, uint64_t obj, size_t location, int32_t code, const char *layerPrefix, const char *msg, void *userData);

	VkResult vhDebugCreateReportCallbackEXT(VkInstance instance, const VkDebugReportCallbackCreateInfoEXT *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkDebugReportCallbackEXT *pCallback);

	void vhDebugDestroyReportCallbackEXT(VkInstance instance, VkDebugReportCallbackEXT callback, const VkAllocationCallbacks *pAllocator);

	void vhSetupDebugCallback(VkInstance instance, VkDebugReportCallbackEXT *callback);

} // namespace vh

#include "nuklear-glfw-vulkan.h"

#endif