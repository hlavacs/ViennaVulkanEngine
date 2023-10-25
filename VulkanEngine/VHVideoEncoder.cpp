#include "VHVideoEncoder.h"

namespace vh {
#ifdef VULKAN_VIDEO_ENCODE
    VkResult VHVideoEncoder::init(
        VkPhysicalDevice physicalDevice,
        VkDevice device,
        VmaAllocator allocator,
        uint32_t computeQueueFamily, VkQueue computeQueue, VkCommandPool computeCommandPool,
        uint32_t encodeQueueFamily, VkQueue encodeQueue, VkCommandPool encodeCommandPool,
        const std::vector<VkImageView>& inputImageViews,
        uint32_t width, uint32_t height, uint32_t fps)
    {
        assert(!m_running);
        if (encodeQueueFamily == -1) {
            std::cout << "Vulkan VideoEncode extension not present.\n";
            return VK_ERROR_EXTENSION_NOT_PRESENT;
        }

        if (m_initialized) {
            if ((width & ~1) == m_width && (height & ~1) == m_height) {
                // nothing changed
                return VK_SUCCESS;
            }

            // resolution changed
            deinit();
        }

        m_physicalDevice = physicalDevice;
        m_device = device;
        m_allocator = allocator;
        m_computeQueue = computeQueue;
        m_encodeQueue = encodeQueue;
        m_computeQueueFamily = computeQueueFamily;
        m_encodeQueueFamily = encodeQueueFamily;
        m_computeCommandPool = computeCommandPool;
        m_encodeCommandPool = encodeCommandPool;
        m_width = width & ~1;
        m_height = height & ~1;

        VHCHECKRESULT(createVideoSession());
        VHCHECKRESULT(allocateVideoSessionMemory());
        VHCHECKRESULT(createVideoSessionParameters(fps));
        VHCHECKRESULT(readBitstreamHeader());
        VHCHECKRESULT(allocateOutputBitStream());
        VHCHECKRESULT(allocateReferenceImages(2));
        VHCHECKRESULT(allocateIntermediateImages());
        VHCHECKRESULT(createOutputQueryPool());
        VHCHECKRESULT(createYUVConversionPipeline(inputImageViews));

        VkSemaphoreCreateInfo semaphoreInfo = {};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        VHCHECKRESULT(vkCreateSemaphore(m_device, &semaphoreInfo, nullptr, &m_interQueueSemaphore));

        VkCommandBuffer cmdBuffer = vhCmdBeginSingleTimeCommands(m_device, m_encodeCommandPool);
        VHCHECKRESULT(initRateControl(cmdBuffer, 20));
        VHCHECKRESULT(transitionImagesInitial(cmdBuffer));
        VHCHECKRESULT(vhCmdEndSingleTimeCommands(m_device, m_encodeQueue, m_encodeCommandPool, cmdBuffer));

        m_frameCount = 0;
        m_initialized = true;
        return VK_SUCCESS;
    }

    VkResult VHVideoEncoder::queueEncode(uint32_t currentImageIx)
    {
        assert(!m_running);
        VHCHECKRESULT(convertRGBtoYUV(currentImageIx));
        VHCHECKRESULT(encodeVideoFrame());
        m_running = true;
        return VK_SUCCESS;
    }

    VkResult VHVideoEncoder::finishEncode(const char*& data, size_t& size)
    {
        size = 0;
        if (!m_running) {
            return VK_NOT_READY;
        }
        if (m_bitStreamHeaderPending) {
            data = m_bitStreamHeader.data();
            size = m_bitStreamHeader.size();
            m_bitStreamHeaderPending = false;
            return VK_SUCCESS;
        }

        VkResult ret = getOutputVideoPacket(data, size);
        if (ret != VK_SUCCESS) {
            std::cout << "readOutputVideoPacket failed";
        }

        vkFreeCommandBuffers(m_device, m_computeCommandPool, 1, &m_computeCommandBuffer);
        vkFreeCommandBuffers(m_device, m_encodeCommandPool, 1, &m_encodeCommandBuffer);
        m_frameCount++;

        m_running = false;
        return ret;
    }
    
    VkResult VHVideoEncoder::createVideoSession()
    {
        m_encodeH264ProfileInfoExt = { VK_STRUCTURE_TYPE_VIDEO_ENCODE_H264_PROFILE_INFO_EXT };
        m_encodeH264ProfileInfoExt.stdProfileIdc = STD_VIDEO_H264_PROFILE_IDC_MAIN;

        m_videoProfile = { VK_STRUCTURE_TYPE_VIDEO_PROFILE_INFO_KHR };
        m_videoProfile.videoCodecOperation = VK_VIDEO_CODEC_OPERATION_ENCODE_H264_BIT_EXT;
        m_videoProfile.chromaSubsampling = VK_VIDEO_CHROMA_SUBSAMPLING_420_BIT_KHR;
        m_videoProfile.chromaBitDepth = VK_VIDEO_COMPONENT_BIT_DEPTH_8_BIT_KHR;
        m_videoProfile.lumaBitDepth = VK_VIDEO_COMPONENT_BIT_DEPTH_8_BIT_KHR;
        m_videoProfile.pNext = &m_encodeH264ProfileInfoExt;

        m_videoProfileList = { VK_STRUCTURE_TYPE_VIDEO_PROFILE_LIST_INFO_KHR };
        m_videoProfileList.profileCount = 1;
        m_videoProfileList.pProfiles = &m_videoProfile;

        static const VkExtensionProperties h264StdExtensionVersion = { VK_STD_VULKAN_VIDEO_CODEC_H264_ENCODE_EXTENSION_NAME, VK_STD_VULKAN_VIDEO_CODEC_H264_ENCODE_SPEC_VERSION };
        VkVideoSessionCreateInfoKHR createInfo = { VK_STRUCTURE_TYPE_VIDEO_SESSION_CREATE_INFO_KHR };
        createInfo.pVideoProfile = &m_videoProfile;
        createInfo.queueFamilyIndex = m_encodeQueueFamily;
        createInfo.pictureFormat = VK_FORMAT_G8_B8R8_2PLANE_420_UNORM;
        createInfo.maxCodedExtent = { m_width, m_height };
        createInfo.maxDpbSlots = 16;
        createInfo.maxActiveReferencePictures = 16;
        createInfo.referencePictureFormat = VK_FORMAT_G8_B8R8_2PLANE_420_UNORM;
        createInfo.pStdHeaderVersion = &h264StdExtensionVersion;

        VkVideoEncodeH264CapabilitiesEXT h264capabilities = {};
        h264capabilities.sType = VK_STRUCTURE_TYPE_VIDEO_ENCODE_H264_CAPABILITIES_EXT;        

        VkVideoEncodeCapabilitiesKHR encodeCapabilities = {};
        encodeCapabilities.sType = VK_STRUCTURE_TYPE_VIDEO_ENCODE_CAPABILITIES_KHR;
        encodeCapabilities.pNext = &h264capabilities;

        VkVideoCapabilitiesKHR capabilities = {};
        capabilities.sType = VK_STRUCTURE_TYPE_VIDEO_CAPABILITIES_KHR;
        capabilities.pNext = &encodeCapabilities;

        VkResult ret = vkGetPhysicalDeviceVideoCapabilitiesKHR(m_physicalDevice, &m_videoProfile, &capabilities);

        return vkCreateVideoSessionKHR(m_device, &createInfo, nullptr, &m_videoSession);
    }

    VkResult VHVideoEncoder::allocateVideoSessionMemory()
    {
        uint32_t videoSessionMemoryRequirementsCount = 0;
        VHCHECKRESULT(vkGetVideoSessionMemoryRequirementsKHR(m_device, m_videoSession,
            &videoSessionMemoryRequirementsCount, nullptr));
        std::vector<VkVideoSessionMemoryRequirementsKHR> encodeSessionMemoryRequirements(videoSessionMemoryRequirementsCount);
        for (uint32_t i = 0; i < videoSessionMemoryRequirementsCount; i++) {
            memset(&encodeSessionMemoryRequirements[i], 0, sizeof(VkVideoSessionMemoryRequirementsKHR));
            encodeSessionMemoryRequirements[i].sType = VK_STRUCTURE_TYPE_VIDEO_SESSION_MEMORY_REQUIREMENTS_KHR;
        }
        VHCHECKRESULT(vkGetVideoSessionMemoryRequirementsKHR(m_device, m_videoSession,
            &videoSessionMemoryRequirementsCount,
            encodeSessionMemoryRequirements.data()));

        std::vector<VkBindVideoSessionMemoryInfoKHR> encodeSessionBindMemory(videoSessionMemoryRequirementsCount);
        m_allocations.resize(videoSessionMemoryRequirementsCount);
        for (uint32_t memIdx = 0; memIdx < videoSessionMemoryRequirementsCount; memIdx++) {
            VmaAllocationCreateInfo allocCreateInfo = {};
            allocCreateInfo.memoryTypeBits = encodeSessionMemoryRequirements[memIdx].memoryRequirements.memoryTypeBits;

            VmaAllocationInfo allocInfo;
            VHCHECKRESULT(vmaAllocateMemory(m_allocator, &encodeSessionMemoryRequirements[memIdx].memoryRequirements, &allocCreateInfo, &m_allocations[memIdx], &allocInfo));

            encodeSessionBindMemory[memIdx].sType = VK_STRUCTURE_TYPE_BIND_VIDEO_SESSION_MEMORY_INFO_KHR;
            encodeSessionBindMemory[memIdx].pNext = nullptr;
            encodeSessionBindMemory[memIdx].memory = allocInfo.deviceMemory;

            encodeSessionBindMemory[memIdx].memoryBindIndex = encodeSessionMemoryRequirements[memIdx].memoryBindIndex;
            encodeSessionBindMemory[memIdx].memoryOffset = allocInfo.offset;
            encodeSessionBindMemory[memIdx].memorySize = allocInfo.size;
        }
        return vkBindVideoSessionMemoryKHR(m_device, m_videoSession, videoSessionMemoryRequirementsCount,
            encodeSessionBindMemory.data());
    }

    VkResult VHVideoEncoder::createVideoSessionParameters(uint32_t fps)
    {
        m_vui = h264::getStdVideoH264SequenceParameterSetVui(fps);
        m_sps = h264::getStdVideoH264SequenceParameterSet(m_width, m_height, &m_vui);
        m_pps = h264::getStdVideoH264PictureParameterSet();

        VkVideoEncodeH264SessionParametersAddInfoEXT encodeH264SessionParametersAddInfo = { VK_STRUCTURE_TYPE_VIDEO_ENCODE_H264_SESSION_PARAMETERS_ADD_INFO_EXT };
        encodeH264SessionParametersAddInfo.pNext = nullptr;
        encodeH264SessionParametersAddInfo.stdSPSCount = 1;
        encodeH264SessionParametersAddInfo.pStdSPSs = &m_sps;
        encodeH264SessionParametersAddInfo.stdPPSCount = 1;
        encodeH264SessionParametersAddInfo.pStdPPSs = &m_pps;

        VkVideoEncodeH264SessionParametersCreateInfoEXT encodeH264SessionParametersCreateInfo = { VK_STRUCTURE_TYPE_VIDEO_ENCODE_H264_SESSION_PARAMETERS_CREATE_INFO_EXT };
        encodeH264SessionParametersCreateInfo.pNext = nullptr;
        encodeH264SessionParametersCreateInfo.maxStdSPSCount = 1;
        encodeH264SessionParametersCreateInfo.maxStdPPSCount = 1;
        encodeH264SessionParametersCreateInfo.pParametersAddInfo = &encodeH264SessionParametersAddInfo;

        VkVideoSessionParametersCreateInfoKHR sessionParametersCreateInfo = { VK_STRUCTURE_TYPE_VIDEO_SESSION_PARAMETERS_CREATE_INFO_KHR };
        sessionParametersCreateInfo.pNext = &encodeH264SessionParametersCreateInfo;
        sessionParametersCreateInfo.videoSessionParametersTemplate = nullptr;
        sessionParametersCreateInfo.videoSession = m_videoSession;

        return vkCreateVideoSessionParametersKHR(m_device, &sessionParametersCreateInfo, nullptr, &m_videoSessionParameters);
    }

    VkResult VHVideoEncoder::readBitstreamHeader()
    {
        VkVideoEncodeH264SessionParametersGetInfoEXT h264getInfo = {};
        h264getInfo.sType = VK_STRUCTURE_TYPE_VIDEO_ENCODE_H264_SESSION_PARAMETERS_GET_INFO_EXT;
        h264getInfo.stdSPSId = 0;
        h264getInfo.stdPPSId = 0;
        h264getInfo.writeStdPPS = VK_TRUE;
        h264getInfo.writeStdSPS = VK_TRUE;
        VkVideoEncodeSessionParametersGetInfoKHR getInfo = {};
        getInfo.sType = VK_STRUCTURE_TYPE_VIDEO_ENCODE_SESSION_PARAMETERS_GET_INFO_KHR;
        getInfo.pNext = &h264getInfo;
        getInfo.videoSessionParameters = m_videoSessionParameters;

        VkVideoEncodeH264SessionParametersFeedbackInfoEXT h264feedback = {};
        h264feedback.sType = VK_STRUCTURE_TYPE_VIDEO_ENCODE_H264_SESSION_PARAMETERS_FEEDBACK_INFO_EXT;
        VkVideoEncodeSessionParametersFeedbackInfoKHR feedback = {};
        feedback.sType = VK_STRUCTURE_TYPE_VIDEO_ENCODE_SESSION_PARAMETERS_FEEDBACK_INFO_KHR;
        feedback.pNext = &h264feedback;
        size_t datalen = 1024;
        VHCHECKRESULT(vkGetEncodedVideoSessionParametersKHR(m_device, &getInfo, nullptr, &datalen, nullptr));
        m_bitStreamHeader.resize(datalen);
        VHCHECKRESULT(vkGetEncodedVideoSessionParametersKHR(m_device, &getInfo, &feedback, &datalen, m_bitStreamHeader.data()));
        m_bitStreamHeader.resize(datalen);
        m_bitStreamHeaderPending = true;
        return VK_SUCCESS;
    }

    VkResult VHVideoEncoder::allocateOutputBitStream()
    {
        VHCHECKRESULT(vhBufCreateBuffer(m_allocator, 4 * 1024 * 1024,
            VK_BUFFER_USAGE_VIDEO_ENCODE_DST_BIT_KHR, VMA_MEMORY_USAGE_GPU_TO_CPU,
            &m_bitStreamBuffer, &m_bitStreamBufferAllocation, &m_videoProfileList));
        VHCHECKRESULT(vmaMapMemory(m_allocator, m_bitStreamBufferAllocation, reinterpret_cast<void**>(&m_bitStreamData)));

        return VK_SUCCESS;
    }

    VkResult VHVideoEncoder::allocateReferenceImages(uint32_t count)
    {
        m_dpbImages.resize(count);
        m_dpbImageAllocations.resize(count);
        m_dpbImageViews.resize(count);
        for (uint32_t i = 0; i < count; i++) {
            VkImageCreateInfo tmpImgCreateInfo;
            tmpImgCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
            tmpImgCreateInfo.pNext = &m_videoProfileList;
            tmpImgCreateInfo.imageType = VK_IMAGE_TYPE_2D;
            tmpImgCreateInfo.format = VK_FORMAT_G8_B8R8_2PLANE_420_UNORM;
            tmpImgCreateInfo.extent = { m_width, m_height, 1 };
            tmpImgCreateInfo.mipLevels = 1;
            tmpImgCreateInfo.arrayLayers = 1;
            tmpImgCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
            tmpImgCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
            tmpImgCreateInfo.usage = VK_IMAGE_USAGE_VIDEO_ENCODE_DPB_BIT_KHR; // DPB ONLY
            tmpImgCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE; // VK_SHARING_MODE_EXCLUSIVE here makes it not check for queueFamily
            tmpImgCreateInfo.queueFamilyIndexCount = 1;
            tmpImgCreateInfo.pQueueFamilyIndices = &m_encodeQueueFamily;
            tmpImgCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            tmpImgCreateInfo.flags = 0;
            VmaAllocationCreateInfo allocInfo = {};
            allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
            VHCHECKRESULT(vmaCreateImage(m_allocator, &tmpImgCreateInfo, &allocInfo, &m_dpbImages[i], &m_dpbImageAllocations[i], nullptr));
            VHCHECKRESULT(vhBufCreateImageView(m_device, m_dpbImages[i], VK_FORMAT_G8_B8R8_2PLANE_420_UNORM, VK_IMAGE_VIEW_TYPE_2D, 1, VK_IMAGE_ASPECT_COLOR_BIT, &m_dpbImageViews[i]));
        }
        return VK_SUCCESS;
    }

    VkResult VHVideoEncoder::allocateIntermediateImages()
    {
        uint32_t queueFamilies[] = {m_computeQueueFamily, m_encodeQueueFamily };
        VkImageCreateInfo tmpImgCreateInfo;
        tmpImgCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        tmpImgCreateInfo.pNext = &m_videoProfileList;
        tmpImgCreateInfo.imageType = VK_IMAGE_TYPE_2D;
        tmpImgCreateInfo.format = VK_FORMAT_G8_B8R8_2PLANE_420_UNORM;
        tmpImgCreateInfo.extent = { m_width, m_height, 1 };
        tmpImgCreateInfo.mipLevels = 1;
        tmpImgCreateInfo.arrayLayers = 1;
        tmpImgCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        tmpImgCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        tmpImgCreateInfo.usage = VK_IMAGE_USAGE_VIDEO_ENCODE_SRC_BIT_KHR | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_STORAGE_BIT;
        tmpImgCreateInfo.sharingMode = VK_SHARING_MODE_CONCURRENT;
        tmpImgCreateInfo.queueFamilyIndexCount = 2;
        tmpImgCreateInfo.pQueueFamilyIndices = queueFamilies;
        tmpImgCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        tmpImgCreateInfo.flags = 0;
        VmaAllocationCreateInfo allocInfo = {};
        allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
        VHCHECKRESULT(vmaCreateImage(m_allocator, &tmpImgCreateInfo, &allocInfo, &m_yuvImage, &m_yuvImageAllocation, nullptr));
        VHCHECKRESULT(vhBufCreateImageView(m_device, m_yuvImage, VK_FORMAT_G8_B8R8_2PLANE_420_UNORM, VK_IMAGE_VIEW_TYPE_2D, 1, VK_IMAGE_ASPECT_COLOR_BIT, &m_yuvImageView));
        VHCHECKRESULT(vhBufCreateImageView(m_device, m_yuvImage, VK_FORMAT_G8_B8R8_2PLANE_420_UNORM, VK_IMAGE_VIEW_TYPE_2D, 1, VK_IMAGE_ASPECT_PLANE_0_BIT, &m_yuvImagePlane0View));

        tmpImgCreateInfo.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_STORAGE_BIT;
        tmpImgCreateInfo.format = VK_FORMAT_R8G8_UNORM;
        tmpImgCreateInfo.extent = { m_width / 2, m_height / 2, 1 };
        VHCHECKRESULT(vmaCreateImage(m_allocator, &tmpImgCreateInfo, &allocInfo, &m_yuvImageChroma, &m_yuvImageChromaAllocation, nullptr));
        VHCHECKRESULT(vhBufCreateImageView(m_device, m_yuvImageChroma, VK_FORMAT_R8G8_UNORM, VK_IMAGE_VIEW_TYPE_2D, 1, VK_IMAGE_ASPECT_COLOR_BIT, &m_yuvImageChromaView));
        return VK_SUCCESS;
    }

    VkResult VHVideoEncoder::createOutputQueryPool()
    {
        VkQueryPoolVideoEncodeFeedbackCreateInfoKHR queryPoolVideoEncodeFeedbackCreateInfo = { VK_STRUCTURE_TYPE_QUERY_POOL_VIDEO_ENCODE_FEEDBACK_CREATE_INFO_KHR };
        queryPoolVideoEncodeFeedbackCreateInfo.encodeFeedbackFlags = VK_VIDEO_ENCODE_FEEDBACK_BITSTREAM_BUFFER_OFFSET_BIT_KHR | VK_VIDEO_ENCODE_FEEDBACK_BITSTREAM_BYTES_WRITTEN_BIT_KHR;
        queryPoolVideoEncodeFeedbackCreateInfo.pNext = &m_videoProfile;
        VkQueryPoolCreateInfo queryPoolCreateInfo = { VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO };
        queryPoolCreateInfo.queryType = VK_QUERY_TYPE_VIDEO_ENCODE_FEEDBACK_KHR;
        queryPoolCreateInfo.queryCount = 1;
        queryPoolCreateInfo.pNext = &queryPoolVideoEncodeFeedbackCreateInfo;
        return vkCreateQueryPool(m_device, &queryPoolCreateInfo, NULL, &m_queryPool);
    }

    VkResult VHVideoEncoder::createYUVConversionPipeline(const std::vector<VkImageView>& inputImageViews)
    {
        auto computeShaderCode = vhFileRead("media/shader/Video/comp.spv");
        VkShaderModule computeShaderModule = vhPipeCreateShaderModule(m_device, computeShaderCode);
        VkPipelineShaderStageCreateInfo computeShaderStageInfo{};
        computeShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        computeShaderStageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        computeShaderStageInfo.module = computeShaderModule;
        computeShaderStageInfo.pName = "main";

        std::array<VkDescriptorSetLayoutBinding, 3> layoutBindings{};
        for (uint32_t i = 0; i < layoutBindings.size(); i++) {
            layoutBindings[i].binding = i;
            layoutBindings[i].descriptorCount = 1;
            layoutBindings[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            layoutBindings[i].pImmutableSamplers = nullptr;
            layoutBindings[i].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        }

        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = (uint32_t)layoutBindings.size();
        layoutInfo.pBindings = layoutBindings.data();
        VHCHECKRESULT(vkCreateDescriptorSetLayout(m_device, &layoutInfo, nullptr, &m_computeDescriptorSetLayout));

        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = 1;
        pipelineLayoutInfo.pSetLayouts = &m_computeDescriptorSetLayout;
        VHCHECKRESULT(vkCreatePipelineLayout(m_device, &pipelineLayoutInfo, nullptr, &m_computePipelineLayout));

        VkComputePipelineCreateInfo pipelineInfo{};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        pipelineInfo.layout = m_computePipelineLayout;
        pipelineInfo.stage = computeShaderStageInfo;
        VHCHECKRESULT(vkCreateComputePipelines(m_device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_computePipeline));

        vkDestroyShaderModule(m_device, computeShaderModule, nullptr);
    
        const int maxFramesCount = static_cast<uint32_t>(inputImageViews.size());
        std::array<VkDescriptorPoolSize, 1> poolSizes{};
        poolSizes[0].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        poolSizes[0].descriptorCount = 3 * maxFramesCount;
        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.poolSizeCount = (uint32_t)poolSizes.size();
        poolInfo.pPoolSizes = poolSizes.data();
        poolInfo.maxSets = maxFramesCount;
        VHCHECKRESULT(vkCreateDescriptorPool(m_device, &poolInfo, nullptr, &m_descriptorPool));

        std::vector<VkDescriptorSetLayout> layouts(maxFramesCount, m_computeDescriptorSetLayout);
        VkDescriptorSetAllocateInfo descAllocInfo{};
        descAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        descAllocInfo.descriptorPool = m_descriptorPool;
        descAllocInfo.descriptorSetCount = maxFramesCount;
        descAllocInfo.pSetLayouts = layouts.data();

        m_computeDescriptorSets.resize(maxFramesCount);
        VHCHECKRESULT(vkAllocateDescriptorSets(m_device, &descAllocInfo, m_computeDescriptorSets.data()));
        for (size_t i = 0; i < maxFramesCount; i++) {
            std::array<VkWriteDescriptorSet, 3> descriptorWrites{};

            VkDescriptorImageInfo imageInfo0{};
            imageInfo0.imageView = inputImageViews[i];
            imageInfo0.imageLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
            descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrites[0].dstSet = m_computeDescriptorSets[i];
            descriptorWrites[0].dstBinding = 0;
            descriptorWrites[0].dstArrayElement = 0;
            descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            descriptorWrites[0].descriptorCount = 1;
            descriptorWrites[0].pImageInfo = &imageInfo0;

            VkDescriptorImageInfo imageInfo1{};
            imageInfo1.imageView = m_yuvImagePlane0View;
            imageInfo1.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
            descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrites[1].dstSet = m_computeDescriptorSets[i];
            descriptorWrites[1].dstBinding = 1;
            descriptorWrites[1].dstArrayElement = 0;
            descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            descriptorWrites[1].descriptorCount = 1;
            descriptorWrites[1].pImageInfo = &imageInfo1;

            VkDescriptorImageInfo imageInfo2{};
            imageInfo2.imageView = m_yuvImageChromaView;
            imageInfo2.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
            descriptorWrites[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrites[2].dstSet = m_computeDescriptorSets[i];
            descriptorWrites[2].dstBinding = 2;
            descriptorWrites[2].dstArrayElement = 0;
            descriptorWrites[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            descriptorWrites[2].descriptorCount = 1;
            descriptorWrites[2].pImageInfo = &imageInfo2;

            vkUpdateDescriptorSets(m_device, (uint32_t)descriptorWrites.size(), descriptorWrites.data(), 0, nullptr);
        }
        return VK_SUCCESS;
    }

    VkResult VHVideoEncoder::initRateControl(VkCommandBuffer cmdBuf, uint32_t qp)
    {
        VkVideoBeginCodingInfoKHR encodeBeginInfo = { VK_STRUCTURE_TYPE_VIDEO_BEGIN_CODING_INFO_KHR };
        encodeBeginInfo.videoSession = m_videoSession;
        encodeBeginInfo.videoSessionParameters = m_videoSessionParameters;

        VkVideoEncodeH264FrameSizeEXT encodeH264FrameSize;
        encodeH264FrameSize.frameISize = 0;
        encodeH264FrameSize.framePSize = 0;

        VkVideoEncodeH264QpEXT encodeH264Qp;
        encodeH264Qp.qpI = qp;
        encodeH264Qp.qpP = qp;

        VkVideoEncodeH264RateControlLayerInfoEXT encodeH264RateControlLayerInfo = { VK_STRUCTURE_TYPE_VIDEO_ENCODE_H264_RATE_CONTROL_LAYER_INFO_EXT };
        encodeH264RateControlLayerInfo.useMinQp = VK_TRUE;
        encodeH264RateControlLayerInfo.minQp = encodeH264Qp;
        encodeH264RateControlLayerInfo.useMaxQp = VK_TRUE;
        encodeH264RateControlLayerInfo.maxQp = encodeH264Qp;
        encodeH264RateControlLayerInfo.useMaxFrameSize = VK_TRUE;
        encodeH264RateControlLayerInfo.maxFrameSize = encodeH264FrameSize;

        VkVideoEncodeRateControlLayerInfoKHR encodeRateControlLayerInfo = { VK_STRUCTURE_TYPE_VIDEO_ENCODE_RATE_CONTROL_LAYER_INFO_KHR };
        encodeRateControlLayerInfo.pNext = &encodeH264RateControlLayerInfo;

        VkVideoEncodeQualityLevelInfoKHR encodeQualityLevelInfo = { VK_STRUCTURE_TYPE_VIDEO_ENCODE_QUALITY_LEVEL_INFO_KHR };
        encodeQualityLevelInfo.qualityLevel = 0;

        VkVideoEncodeH264RateControlInfoEXT encodeH264RateControlInfo = { VK_STRUCTURE_TYPE_VIDEO_ENCODE_H264_RATE_CONTROL_INFO_EXT };
        //encodeH264RateControlInfo.flags = 0;
        encodeH264RateControlInfo.pNext = &encodeQualityLevelInfo;
        encodeH264RateControlInfo.flags = VK_VIDEO_ENCODE_H264_RATE_CONTROL_REGULAR_GOP_BIT_EXT;
        encodeH264RateControlInfo.gopFrameCount = 16;
        encodeH264RateControlInfo.idrPeriod = 16;
        encodeH264RateControlInfo.consecutiveBFrameCount = 0;
        encodeH264RateControlInfo.temporalLayerCount = 1;

        VkVideoEncodeRateControlInfoKHR encodeRateControlInfo = { VK_STRUCTURE_TYPE_VIDEO_ENCODE_RATE_CONTROL_INFO_KHR };
        encodeRateControlInfo.rateControlMode = VK_VIDEO_ENCODE_RATE_CONTROL_MODE_DISABLED_BIT_KHR;
        encodeRateControlInfo.pNext = &encodeH264RateControlInfo;
        encodeRateControlInfo.layerCount = 1;
        encodeRateControlInfo.pLayers = &encodeRateControlLayerInfo;

        VkVideoCodingControlInfoKHR codingControlInfo = { VK_STRUCTURE_TYPE_VIDEO_CODING_CONTROL_INFO_KHR };
        codingControlInfo.flags = VK_VIDEO_CODING_CONTROL_RESET_BIT_KHR | VK_VIDEO_CODING_CONTROL_ENCODE_RATE_CONTROL_BIT_KHR | VK_VIDEO_CODING_CONTROL_ENCODE_QUALITY_LEVEL_BIT_KHR;
        codingControlInfo.pNext = &encodeRateControlInfo;

        VkVideoEndCodingInfoKHR encodeEndInfo = { VK_STRUCTURE_TYPE_VIDEO_END_CODING_INFO_KHR };

        // Reset the video session before first use and apply QP values.
        vkCmdBeginVideoCodingKHR(cmdBuf, &encodeBeginInfo);
        vkCmdControlVideoCodingKHR(cmdBuf, &codingControlInfo);
        vkCmdEndVideoCodingKHR(cmdBuf, &encodeEndInfo);
        return VK_SUCCESS;
    }

    VkResult VHVideoEncoder::transitionImagesInitial(VkCommandBuffer cmdBuf)
    {
        for (auto& dpbImage : m_dpbImages) {
            VHCHECKRESULT(vhBufTransitionImageLayout(m_device, m_encodeQueue, cmdBuf,
                dpbImage, VK_FORMAT_G8_B8R8_2PLANE_420_UNORM, VK_IMAGE_ASPECT_COLOR_BIT, 1, 1,
                VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_VIDEO_ENCODE_DPB_KHR));
        }
        VHCHECKRESULT(vhBufTransitionImageLayout(m_device, m_encodeQueue, cmdBuf,
                m_yuvImage, VK_FORMAT_G8_B8R8_2PLANE_420_UNORM, VK_IMAGE_ASPECT_COLOR_BIT, 1, 1,
            VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_VIDEO_ENCODE_SRC_KHR));
        VHCHECKRESULT(vhBufTransitionImageLayout(m_device, m_encodeQueue, cmdBuf,
            m_yuvImageChroma, VK_FORMAT_R8G8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT, 1, 1,
            VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL));            
        return VK_SUCCESS;
    }

    VkResult VHVideoEncoder::convertRGBtoYUV(uint32_t currentImageIx)
    {
        // begin command buffer for compute shader
        VHCHECKRESULT(vhCmdCreateCommandBuffers(m_device, m_computeCommandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, 1, &m_computeCommandBuffer));
        VHCHECKRESULT(vhCmdBeginCommandBuffer(m_device, m_computeCommandBuffer, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT));

        // transition YUV image (full and chroma) to be shader target
        VHCHECKRESULT(vhBufTransitionImageLayout(m_device, m_computeQueue, m_computeCommandBuffer,
            m_yuvImage, VK_FORMAT_G8_B8R8_2PLANE_420_UNORM, VK_IMAGE_ASPECT_PLANE_0_BIT, 1, 1,
            VK_IMAGE_LAYOUT_VIDEO_ENCODE_SRC_KHR, VK_IMAGE_LAYOUT_GENERAL));
        VHCHECKRESULT(vhBufTransitionImageLayout(m_device, m_computeQueue, m_computeCommandBuffer,
            m_yuvImageChroma, VK_FORMAT_R8G8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT, 1, 1,
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL));

        // run the RGB->YUV conversion shader
        vkCmdBindPipeline(m_computeCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_computePipeline);
        vkCmdBindDescriptorSets(m_computeCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_computePipelineLayout, 0, 1, &m_computeDescriptorSets[currentImageIx], 0, 0);
        vkCmdDispatch(m_computeCommandBuffer, (m_width+15)/16, (m_height+15)/16, 1); // work item local size = 16x16


        // transition the YUV image as copy target and the chroma image to be copy source
        VHCHECKRESULT(vhBufTransitionImageLayout(m_device, m_computeQueue, m_computeCommandBuffer,
            m_yuvImage, VK_FORMAT_G8_B8R8_2PLANE_420_UNORM, VK_IMAGE_ASPECT_PLANE_1_BIT, 1, 1,
            VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL));
        VHCHECKRESULT(vhBufTransitionImageLayout(m_device, m_computeQueue, m_computeCommandBuffer,
            m_yuvImageChroma, VK_FORMAT_R8G8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT, 1, 1,
            VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL));
      
        // copy the full chroma image into the 2nd plane of the YUV image
        VkImageCopy regions;
        regions.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        regions.srcSubresource.baseArrayLayer = 0;
        regions.srcSubresource.layerCount = 1;
        regions.srcSubresource.mipLevel = 0;
        regions.srcOffset = { 0, 0, 0 };
        regions.dstSubresource.aspectMask = VK_IMAGE_ASPECT_PLANE_1_BIT;
        regions.dstSubresource.baseArrayLayer = 0;
        regions.dstSubresource.layerCount = 1;
        regions.dstSubresource.mipLevel = 0;
        regions.dstOffset = { 0, 0, 0 };
        regions.extent = { m_width / 2, m_height / 2, 1 };
        vkCmdCopyImage(m_computeCommandBuffer, m_yuvImageChroma, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, m_yuvImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &regions);

        VHCHECKRESULT(vkEndCommandBuffer(m_computeCommandBuffer));
        VHCHECKRESULT(vhCmdSubmitCommandBuffer(m_device, m_computeQueue, m_computeCommandBuffer, VK_NULL_HANDLE, m_interQueueSemaphore, VK_NULL_HANDLE));
        return VK_SUCCESS;
    }

    VkResult VHVideoEncoder::encodeVideoFrame()
    {
        const uint32_t GOP_LENGTH = 16;
        const uint32_t gopFrameCount = m_frameCount % GOP_LENGTH;
        // begin command buffer for video encode
        VHCHECKRESULT(vhCmdCreateCommandBuffers(m_device, m_encodeCommandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, 1, &m_encodeCommandBuffer));
        VHCHECKRESULT(vhCmdBeginCommandBuffer(m_device, m_encodeCommandBuffer, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT));

        // start a video encode session
        // set an image view as DPB (decoded output picture)
        VkVideoPictureResourceInfoKHR dpbPicResource = { VK_STRUCTURE_TYPE_VIDEO_PICTURE_RESOURCE_INFO_KHR };
        dpbPicResource.imageViewBinding = m_dpbImageViews[gopFrameCount & 1];
        dpbPicResource.codedOffset = { 0, 0 };
        dpbPicResource.codedExtent = { m_width, m_height };
        dpbPicResource.baseArrayLayer = 0;
        // set an image view as reference picture
        VkVideoPictureResourceInfoKHR refPicResource = { VK_STRUCTURE_TYPE_VIDEO_PICTURE_RESOURCE_INFO_KHR };
        refPicResource.imageViewBinding = m_dpbImageViews[!(gopFrameCount & 1)];
        refPicResource.codedOffset = { 0, 0 };
        refPicResource.codedExtent = { m_width, m_height };
        refPicResource.baseArrayLayer = 0;

        const uint32_t MaxPicOrderCntLsb = 1 << (m_sps.log2_max_pic_order_cnt_lsb_minus4 + 4);
        StdVideoEncodeH264ReferenceInfo dpbRefInfo = {};
        dpbRefInfo.FrameNum = gopFrameCount;
        dpbRefInfo.PicOrderCnt = (dpbRefInfo.FrameNum * 2) % MaxPicOrderCntLsb;
        dpbRefInfo.primary_pic_type = dpbRefInfo.FrameNum == 0 ? STD_VIDEO_H264_PICTURE_TYPE_IDR : STD_VIDEO_H264_PICTURE_TYPE_P;
        VkVideoEncodeH264DpbSlotInfoEXT dpbSlotInfo = { VK_STRUCTURE_TYPE_VIDEO_ENCODE_H264_DPB_SLOT_INFO_EXT };
        dpbSlotInfo.pNext = nullptr;
        dpbSlotInfo.pStdReferenceInfo = &dpbRefInfo;

        StdVideoEncodeH264ReferenceInfo refRefInfo = {};
        refRefInfo.FrameNum = gopFrameCount - 1;
        refRefInfo.PicOrderCnt = (refRefInfo.FrameNum * 2) % MaxPicOrderCntLsb;
        refRefInfo.primary_pic_type = refRefInfo.FrameNum == 0 ? STD_VIDEO_H264_PICTURE_TYPE_IDR : STD_VIDEO_H264_PICTURE_TYPE_P;
        VkVideoEncodeH264DpbSlotInfoEXT refSlotInfo = { VK_STRUCTURE_TYPE_VIDEO_ENCODE_H264_DPB_SLOT_INFO_EXT };
        refSlotInfo.pNext = nullptr;
        refSlotInfo.pStdReferenceInfo = &refRefInfo;

        VkVideoReferenceSlotInfoKHR referenceSlots[2];
        referenceSlots[0].sType = VK_STRUCTURE_TYPE_VIDEO_REFERENCE_SLOT_INFO_KHR;
        referenceSlots[0].pNext = &dpbSlotInfo;
        referenceSlots[0].slotIndex = 0;
        referenceSlots[0].pPictureResource = &dpbPicResource;
        referenceSlots[1].sType = VK_STRUCTURE_TYPE_VIDEO_REFERENCE_SLOT_INFO_KHR;
        referenceSlots[1].pNext = &refSlotInfo;
        referenceSlots[1].slotIndex = 1;
        referenceSlots[1].pPictureResource = &refPicResource;

        VkVideoBeginCodingInfoKHR encodeBeginInfo = { VK_STRUCTURE_TYPE_VIDEO_BEGIN_CODING_INFO_KHR };
        encodeBeginInfo.videoSession = m_videoSession;
        encodeBeginInfo.videoSessionParameters = m_videoSessionParameters;
        encodeBeginInfo.referenceSlotCount = gopFrameCount == 0 ? 1 : 2;
        encodeBeginInfo.pReferenceSlots = referenceSlots;
        vkCmdBeginVideoCodingKHR(m_encodeCommandBuffer, &encodeBeginInfo);

        // transition the YUV image to be a video encode source
        VHCHECKRESULT(vhBufTransitionImageLayout(m_device, m_encodeQueue, m_encodeCommandBuffer,
            m_yuvImage, VK_FORMAT_G8_B8R8_2PLANE_420_UNORM, VK_IMAGE_ASPECT_PLANE_1_BIT, 1, 1,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_VIDEO_ENCODE_SRC_KHR));

        // set the YUV image as input picture for the encoder
        VkVideoPictureResourceInfoKHR inputPicResource = { VK_STRUCTURE_TYPE_VIDEO_PICTURE_RESOURCE_INFO_KHR };
        inputPicResource.imageViewBinding = m_yuvImageView;
        inputPicResource.codedOffset = { 0, 0 };
        inputPicResource.codedExtent = { m_width, m_height };
        inputPicResource.baseArrayLayer = 0;

        // set all the frame parameters
        h264::FrameInfo frameInfo(gopFrameCount, m_width, m_height,
            m_sps,
            m_pps,
            gopFrameCount == 0);
        VkVideoEncodeH264PictureInfoEXT* encodeH264FrameInfo = frameInfo.getEncodeH264FrameInfo();

        // combine all structures in one control structure
        VkVideoEncodeInfoKHR videoEncodeInfo = { VK_STRUCTURE_TYPE_VIDEO_ENCODE_INFO_KHR };
        videoEncodeInfo.pNext = encodeH264FrameInfo;
        //videoEncodeInfo.qualityLevel = 0;
        videoEncodeInfo.dstBuffer = m_bitStreamBuffer;
        videoEncodeInfo.dstBufferOffset = 0;
        videoEncodeInfo.srcPictureResource = inputPicResource;
        videoEncodeInfo.pSetupReferenceSlot = &referenceSlots[0];

        if (gopFrameCount > 0) {
            videoEncodeInfo.referenceSlotCount = 1;
            videoEncodeInfo.pReferenceSlots = &referenceSlots[1];
        }

        // prepare the query pool for the resulting bitstream
        const uint32_t querySlotId = 0;
        vkCmdResetQueryPool(m_encodeCommandBuffer, m_queryPool, querySlotId, 1);
        vkCmdBeginQuery(m_encodeCommandBuffer, m_queryPool, querySlotId, VkQueryControlFlags());
        // encode the frame as video
        vkCmdEncodeVideoKHR(m_encodeCommandBuffer, &videoEncodeInfo);
        // end the query for the result
        vkCmdEndQuery(m_encodeCommandBuffer, m_queryPool, querySlotId);
        // finish the video session
        VkVideoEndCodingInfoKHR encodeEndInfo = { VK_STRUCTURE_TYPE_VIDEO_END_CODING_INFO_KHR };
        vkCmdEndVideoCodingKHR(m_encodeCommandBuffer, &encodeEndInfo);

        // run the encoding
        VHCHECKRESULT(vkEndCommandBuffer(m_encodeCommandBuffer));
        VHCHECKRESULT(vhCmdSubmitCommandBuffer(m_device, m_encodeQueue, m_encodeCommandBuffer, m_interQueueSemaphore, VK_NULL_HANDLE, VK_NULL_HANDLE));
        return VK_SUCCESS;
    }

    VkResult VHVideoEncoder::getOutputVideoPacket(const char*& data, size_t& size)
    {
        struct nvVideoEncodeStatus {
            uint32_t bitstreamStartOffset;
            uint32_t bitstreamSize;
            VkQueryResultStatusKHR status;
        };
        // get the resulting bitstream
        nvVideoEncodeStatus encodeResult[2]; // 2nd slot is non vcl data
        memset(&encodeResult, 0, sizeof(encodeResult));
        const uint32_t querySlotId = 0;
        VHCHECKRESULT(vkGetQueryPoolResults(m_device, m_queryPool, querySlotId, 1, sizeof(nvVideoEncodeStatus),
            &encodeResult[0], sizeof(nvVideoEncodeStatus), VK_QUERY_RESULT_WITH_STATUS_BIT_KHR | VK_QUERY_RESULT_WAIT_BIT));

        //std::cout << "status: " << encodeResult[0].status << " offset: " << encodeResult[0].bitstreamStartOffset << " size: " << encodeResult[0].bitstreamSize << std::endl;
        // invalidate host caches
        vmaInvalidateAllocation(m_allocator, m_bitStreamBufferAllocation, encodeResult[0].bitstreamStartOffset, encodeResult[0].bitstreamSize);
        // write the bitstream into the file 
        data = m_bitStreamData + encodeResult[0].bitstreamStartOffset;
        size = encodeResult[0].bitstreamSize;

        return VK_SUCCESS;
    }

    void VHVideoEncoder::deinit()
    {
        if (!m_initialized) {
            return;
        }

        if (m_running) {
            const char* data;
            size_t size;
            getOutputVideoPacket(data, size);
            vkFreeCommandBuffers(m_device, m_computeCommandPool, 1, &m_computeCommandBuffer);
            vkFreeCommandBuffers(m_device, m_encodeCommandPool, 1, &m_encodeCommandBuffer);
        }

        vkDestroySemaphore(m_device, m_interQueueSemaphore, nullptr);
        vkDestroyPipeline(m_device, m_computePipeline, nullptr);
        vkDestroyPipelineLayout(m_device, m_computePipelineLayout, nullptr);
        vkDestroyDescriptorPool(m_device, m_descriptorPool, nullptr);
        vkDestroyDescriptorSetLayout(m_device, m_computeDescriptorSetLayout, nullptr);

        vkDestroyVideoSessionParametersKHR(m_device, m_videoSessionParameters, nullptr);
        vkDestroyQueryPool(m_device, m_queryPool, nullptr);
        vmaUnmapMemory(m_allocator, m_bitStreamBufferAllocation);
        vmaDestroyBuffer(m_allocator, m_bitStreamBuffer, m_bitStreamBufferAllocation);
        vkDestroyImageView(m_device, m_yuvImageChromaView, nullptr);
        vmaDestroyImage(m_allocator, m_yuvImageChroma, m_yuvImageChromaAllocation);
        vkDestroyImageView(m_device, m_yuvImagePlane0View, nullptr);
        vkDestroyImageView(m_device, m_yuvImageView, nullptr);
        vmaDestroyImage(m_allocator, m_yuvImage, m_yuvImageAllocation);
        for (uint32_t i = 0; i < m_dpbImages.size(); i++) {
            vkDestroyImageView(m_device, m_dpbImageViews[i], nullptr);
            vmaDestroyImage(m_allocator, m_dpbImages[i], m_dpbImageAllocations[i]);
        }
        vkDestroyVideoSessionKHR(m_device, m_videoSession, nullptr);
        for (VmaAllocation& allocation : m_allocations) {
            vmaFreeMemory(m_allocator, allocation);
        }
        m_allocations.clear();
        m_bitStreamHeader.clear();

        m_initialized = false;
    }
#else
    VkResult VHVideoEncoder::init(
        VkDevice device,
        VmaAllocator allocator,
        uint32_t computeQueueFamily, VkQueue computeQueue, VkCommandPool computeCommandPool,
        uint32_t encodeQueueFamily, VkQueue encodeQueue, VkCommandPool encodeCommandPool,
        const std::vector<VkImageView>& inputImageViews,
        uint32_t width, uint32_t height, uint32_t fps)
    {
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }

    VkResult VHVideoEncoder::queueEncode(uint32_t currentImageIx)
    {
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }

    VkResult VHVideoEncoder::finishEncode(const char*& data, size_t& size)
    {
        size = 0;
        return VK_SUCCESS;
    }
        
    void VHVideoEncoder::deinit()
    {
    }
#endif
};
