#include "H264ParameterSet.h"
#include "VHVideoEncoder.h"

namespace vh {

    VkResult VHVideoEncoder::init(VkDevice device, VmaAllocator allocator, uint32_t computeQueueFamily, VkQueue computeQueue, VkCommandPool computeCommandPool, uint32_t encodeQueueFamily, VkQueue encodeQueue, VkCommandPool encodeCommandPool, uint32_t width, uint32_t height)
    {
        if (m_initialized) {
            return VK_SUCCESS;
        }
        m_device = device;
        m_allocator = allocator;
        m_computeQueue = computeQueue;
        m_encodeQueue = encodeQueue;
        m_computeQueueFamily = computeQueueFamily;
        m_encodeQueueFamily = encodeQueueFamily;
        m_computeCommandPool = computeCommandPool;
        m_encodeCommandPool = encodeCommandPool;
        m_width = width;
        m_height = height;

        VHCHECKRESULT(createVideoSession());
        VHCHECKRESULT(allocateVideoSessionMemory());
        VHCHECKRESULT(createVideoSessionParameters());
        VHCHECKRESULT(allocateOutputBitStream());
        VHCHECKRESULT(allocateReferenceImages());
        VHCHECKRESULT(allocateIntermediateImages());
        VHCHECKRESULT(createOutputQueryPool());
        VHCHECKRESULT(createYUVConversionPipeline());

        VkCommandBuffer cmdBuffer = vhCmdBeginSingleTimeCommands(m_device, m_encodeCommandPool);
        VHCHECKRESULT(initRateControl(cmdBuffer, 20));
        VHCHECKRESULT(transitionImagesInitial(cmdBuffer));
        VHCHECKRESULT(vhCmdEndSingleTimeCommands(m_device, m_encodeQueue, m_encodeCommandPool, cmdBuffer));

        m_outfile.open("hwenc.264", std::ios::binary);
        h264::encodeSps(m_sps).writeTo(m_outfile);
        h264::encodePps(m_pps).writeTo(m_outfile);

        m_frameCount = 0;
        m_initialized = true;
        return VK_SUCCESS;
    }

    VkResult VHVideoEncoder::queueEncode(VkImageView inputImageView)
    {        
        VHCHECKRESULT(convertRGBtoYUV(inputImageView));
        VHCHECKRESULT(encodeVideoFrame());
        VHCHECKRESULT(readOutputVideoPacket());
        return VK_SUCCESS;
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

    VkResult VHVideoEncoder::createVideoSessionParameters()
    {
        m_sps = h264::getStdVideoH264SequenceParameterSet(m_width, m_height, nullptr);
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

    VkResult VHVideoEncoder::allocateOutputBitStream()
    {
        return vhBufCreateBuffer(m_allocator, 4 * 1024 * 1024,
            VK_BUFFER_USAGE_VIDEO_ENCODE_DST_BIT_KHR, VMA_MEMORY_USAGE_GPU_TO_CPU,
            &m_bitStreamBuffer, &m_bitStreamBufferAllocation);
    }

    VkResult VHVideoEncoder::allocateReferenceImages()
    {
        VkImageCreateInfo tmpImgCreateInfo;
        tmpImgCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        tmpImgCreateInfo.pNext = &m_videoProfile;
        tmpImgCreateInfo.imageType = VK_IMAGE_TYPE_2D;
        tmpImgCreateInfo.format = VK_FORMAT_G8_B8R8_2PLANE_420_UNORM;
        tmpImgCreateInfo.extent = { m_width, m_height, 1 };
        tmpImgCreateInfo.mipLevels = 1;
        tmpImgCreateInfo.arrayLayers = 1;
        tmpImgCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        tmpImgCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        tmpImgCreateInfo.usage = VK_IMAGE_USAGE_VIDEO_ENCODE_DPB_BIT_KHR; // DPB ONLY
        tmpImgCreateInfo.sharingMode = VK_SHARING_MODE_CONCURRENT; // VK_SHARING_MODE_EXCLUSIVE here makes it not check for queueFamily
        tmpImgCreateInfo.queueFamilyIndexCount = 1;
        tmpImgCreateInfo.pQueueFamilyIndices = &m_encodeQueueFamily;
        tmpImgCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        tmpImgCreateInfo.flags = 0;
        VmaAllocationCreateInfo allocInfo = {};
        allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
        VHCHECKRESULT(vmaCreateImage(m_allocator, &tmpImgCreateInfo, &allocInfo, &m_dpbImage, &m_dpbImageAllocation, nullptr));
        VHCHECKRESULT(vhBufCreateImageView(m_device, m_dpbImage, VK_FORMAT_G8_B8R8_2PLANE_420_UNORM, VK_IMAGE_VIEW_TYPE_2D, 1, VK_IMAGE_ASPECT_COLOR_BIT, &m_dpbImageView));
        return VK_SUCCESS;
    }

    VkResult VHVideoEncoder::allocateIntermediateImages()
    {
        uint32_t queueFamilies[] = {m_computeQueueFamily, m_encodeQueueFamily };
        VkImageCreateInfo tmpImgCreateInfo;
        tmpImgCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        tmpImgCreateInfo.pNext = &m_videoProfile;
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

    VkResult VHVideoEncoder::createYUVConversionPipeline()
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
    
        std::array<VkDescriptorPoolSize, 1> poolSizes{};
        poolSizes[0].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        poolSizes[0].descriptorCount = 3;
        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.poolSizeCount = (uint32_t)poolSizes.size();
        poolInfo.pPoolSizes = poolSizes.data();
        poolInfo.maxSets = 1;
        VHCHECKRESULT(vkCreateDescriptorPool(m_device, &poolInfo, nullptr, &m_descriptorPool));

        std::vector<VkDescriptorSetLayout> layouts(1, m_computeDescriptorSetLayout);
        VkDescriptorSetAllocateInfo descAllocInfo{};
        descAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        descAllocInfo.descriptorPool = m_descriptorPool;
        descAllocInfo.descriptorSetCount = 1;
        descAllocInfo.pSetLayouts = layouts.data();

        m_computeDescriptorSets.resize(1);
        VHCHECKRESULT(vkAllocateDescriptorSets(m_device, &descAllocInfo, m_computeDescriptorSets.data()));
        for (size_t i = 0; i < 1; i++) {
            std::array<VkWriteDescriptorSet, 2> descriptorWrites{};

            VkDescriptorImageInfo imageInfo0{};
            imageInfo0.imageView = m_yuvImagePlane0View;
            imageInfo0.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
            descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrites[0].dstSet = m_computeDescriptorSets[i];
            descriptorWrites[0].dstBinding = 1;
            descriptorWrites[0].dstArrayElement = 0;
            descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            descriptorWrites[0].descriptorCount = 1;
            descriptorWrites[0].pImageInfo = &imageInfo0;

            VkDescriptorImageInfo imageInfo1{};
            imageInfo1.imageView = m_yuvImageChromaView;
            imageInfo1.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
            descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrites[1].dstSet = m_computeDescriptorSets[i];
            descriptorWrites[1].dstBinding = 2;
            descriptorWrites[1].dstArrayElement = 0;
            descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            descriptorWrites[1].descriptorCount = 1;
            descriptorWrites[1].pImageInfo = &imageInfo1;

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

        VkVideoEncodeH264QpEXT encodeH264Qp;
        encodeH264Qp.qpI = qp;

        VkVideoEncodeH264RateControlLayerInfoEXT encodeH264RateControlLayerInfo = { VK_STRUCTURE_TYPE_VIDEO_ENCODE_H264_RATE_CONTROL_LAYER_INFO_EXT };
        encodeH264RateControlLayerInfo.useInitialRcQp = VK_TRUE;
        encodeH264RateControlLayerInfo.initialRcQp = encodeH264Qp;
        encodeH264RateControlLayerInfo.useMinQp = VK_TRUE;
        encodeH264RateControlLayerInfo.minQp = encodeH264Qp;
        encodeH264RateControlLayerInfo.useMaxQp = VK_TRUE;
        encodeH264RateControlLayerInfo.maxQp = encodeH264Qp;
        encodeH264RateControlLayerInfo.useMaxFrameSize = VK_TRUE;
        encodeH264RateControlLayerInfo.maxFrameSize = encodeH264FrameSize;
        encodeH264RateControlLayerInfo.temporalLayerId = 0;

        VkVideoEncodeRateControlLayerInfoKHR encodeRateControlLayerInfo = { VK_STRUCTURE_TYPE_VIDEO_ENCODE_RATE_CONTROL_LAYER_INFO_KHR };
        encodeRateControlLayerInfo.pNext = &encodeH264RateControlLayerInfo;

        VkVideoEncodeH264RateControlInfoEXT encodeH264RateControlInfo = { VK_STRUCTURE_TYPE_VIDEO_ENCODE_H264_RATE_CONTROL_INFO_EXT };
        encodeH264RateControlInfo.gopFrameCount = UINT32_MAX;
        encodeH264RateControlInfo.idrPeriod = UINT32_MAX;
        encodeH264RateControlInfo.consecutiveBFrameCount = 0;
        encodeH264RateControlInfo.temporalLayerCount = 1;
        encodeH264RateControlInfo.rateControlStructure = VK_VIDEO_ENCODE_H264_RATE_CONTROL_STRUCTURE_UNKNOWN_EXT;

        VkVideoEncodeRateControlInfoKHR encodeRateControlInfo = { VK_STRUCTURE_TYPE_VIDEO_ENCODE_RATE_CONTROL_INFO_KHR };
        encodeRateControlInfo.rateControlMode = VK_VIDEO_ENCODE_RATE_CONTROL_MODE_DISABLED_BIT_KHR;
        encodeRateControlInfo.pNext = &encodeH264RateControlInfo;
        encodeRateControlInfo.layerCount = 1;
        encodeRateControlInfo.pLayers = &encodeRateControlLayerInfo;

        VkVideoCodingControlInfoKHR codingControlInfo = { VK_STRUCTURE_TYPE_VIDEO_CODING_CONTROL_INFO_KHR };
        codingControlInfo.flags = VK_VIDEO_CODING_CONTROL_RESET_BIT_KHR | VK_VIDEO_CODING_CONTROL_ENCODE_RATE_CONTROL_BIT_KHR;
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
        VHCHECKRESULT(vhBufTransitionImageLayout(m_device, m_encodeQueue, cmdBuf,
            m_dpbImage, VK_FORMAT_G8_B8R8_2PLANE_420_UNORM, VK_IMAGE_ASPECT_COLOR_BIT, 1, 1,
            VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_VIDEO_ENCODE_DPB_KHR));
        VHCHECKRESULT(vhBufTransitionImageLayout(m_device, m_encodeQueue, cmdBuf,
                m_yuvImage, VK_FORMAT_G8_B8R8_2PLANE_420_UNORM, VK_IMAGE_ASPECT_COLOR_BIT, 1, 1,
            VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_VIDEO_ENCODE_SRC_KHR));
        VHCHECKRESULT(vhBufTransitionImageLayout(m_device, m_encodeQueue, cmdBuf,
            m_yuvImageChroma, VK_FORMAT_R8G8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT, 1, 1,
            VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL));            
        return VK_SUCCESS;
    }

    VkResult VHVideoEncoder::convertRGBtoYUV(VkImageView inputImageView)
    {
        // set the input image view into the descriptor for the compute shader
        std::array<VkWriteDescriptorSet, 1> descriptorWrites{};
        VkDescriptorImageInfo imageInfo{};
        imageInfo.imageView = inputImageView;
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[0].dstSet = m_computeDescriptorSets[0];
        descriptorWrites[0].dstBinding = 0;
        descriptorWrites[0].dstArrayElement = 0;
        descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        descriptorWrites[0].descriptorCount = 1;
        descriptorWrites[0].pImageInfo = &imageInfo;
        vkUpdateDescriptorSets(m_device, (uint32_t)descriptorWrites.size(), descriptorWrites.data(), 0, nullptr);

        // begin command buffer for compute shader
        VkCommandBuffer cmdBuffer = vhCmdBeginSingleTimeCommands(m_device, m_computeCommandPool);

        // transition YUV image (full and chroma) to be shader target
        VHCHECKRESULT(vhBufTransitionImageLayout(m_device, m_computeQueue, cmdBuffer,
            m_yuvImage, VK_FORMAT_G8_B8R8_2PLANE_420_UNORM, VK_IMAGE_ASPECT_PLANE_0_BIT, 1, 1,
            VK_IMAGE_LAYOUT_VIDEO_ENCODE_SRC_KHR, VK_IMAGE_LAYOUT_GENERAL));
        VHCHECKRESULT(vhBufTransitionImageLayout(m_device, m_encodeQueue, cmdBuffer,
            m_yuvImageChroma, VK_FORMAT_R8G8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT, 1, 1,
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL));

        // run the RGB->YUV conversion shader
        vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_computePipeline);
        vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_computePipelineLayout, 0, 1, &m_computeDescriptorSets[0], 0, 0);
        vkCmdDispatch(cmdBuffer, 800/16, 600/16, 1); // work item local size = 16x16

        VHCHECKRESULT(vhCmdEndSingleTimeCommands(m_device, m_computeQueue, m_computeCommandPool, cmdBuffer));


        // begin command buffer for copying chroma image into the YUV image plane 2
        cmdBuffer = vhCmdBeginSingleTimeCommands(m_device, m_computeCommandPool);

        // transition the YUV image as copy target and the chroma image to be copy source
        VHCHECKRESULT(vhBufTransitionImageLayout(m_device, m_computeQueue, cmdBuffer,
            m_yuvImage, VK_FORMAT_G8_B8R8_2PLANE_420_UNORM, VK_IMAGE_ASPECT_PLANE_1_BIT, 1, 1,
            VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL));
        VHCHECKRESULT(vhBufTransitionImageLayout(m_device, m_computeQueue, cmdBuffer,
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
        regions.extent = { 400, 300, 1 };
        vkCmdCopyImage(cmdBuffer, m_yuvImageChroma, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, m_yuvImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &regions);

        // transition the YUV image to be a video encode source
        VHCHECKRESULT(vhBufTransitionImageLayout(m_device, m_computeQueue, cmdBuffer,
            m_yuvImage, VK_FORMAT_G8_B8R8_2PLANE_420_UNORM, VK_IMAGE_ASPECT_PLANE_1_BIT, 1, 1,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_VIDEO_ENCODE_SRC_KHR));

        VHCHECKRESULT(vhCmdEndSingleTimeCommands(m_device, m_computeQueue, m_computeCommandPool, cmdBuffer));
        return VK_SUCCESS;
    }

    VkResult VHVideoEncoder::encodeVideoFrame()
    {
        // begin command buffer for video encode
        VkCommandBuffer cmdBuffer = vhCmdBeginSingleTimeCommands(m_device, m_encodeCommandPool);

        // start a video encode session (without reference pics -> no I or B frames)
        VkVideoBeginCodingInfoKHR encodeBeginInfo = { VK_STRUCTURE_TYPE_VIDEO_BEGIN_CODING_INFO_KHR };
        encodeBeginInfo.videoSession = m_videoSession;
        encodeBeginInfo.videoSessionParameters = m_videoSessionParameters;
        encodeBeginInfo.referenceSlotCount = 0;
        encodeBeginInfo.pReferenceSlots = nullptr;
        vkCmdBeginVideoCodingKHR(cmdBuffer, &encodeBeginInfo);

        // set the YUV image as input picture for the encoder
        VkVideoPictureResourceInfoKHR inputPicResource = { VK_STRUCTURE_TYPE_VIDEO_PICTURE_RESOURCE_INFO_KHR };
        inputPicResource.imageViewBinding = m_yuvImageView;
        inputPicResource.codedOffset = { 0, 0 };
        inputPicResource.codedExtent = { m_width, m_height };
        inputPicResource.baseArrayLayer = 0;

        // set an image view as DPB (decoded output picture), later (with I frame support) used as reference picture
        VkVideoPictureResourceInfoKHR dpbPicResource = { VK_STRUCTURE_TYPE_VIDEO_PICTURE_RESOURCE_INFO_KHR };
        dpbPicResource.imageViewBinding = m_dpbImageView;
        dpbPicResource.codedOffset = { 0, 0 };
        dpbPicResource.codedExtent = { m_width, m_height };
        dpbPicResource.baseArrayLayer = 0;

        // no reference pictures used
        VkVideoReferenceSlotInfoKHR referenceSlot{ VK_STRUCTURE_TYPE_VIDEO_REFERENCE_SLOT_INFO_KHR };
        referenceSlot.pNext = nullptr;
        referenceSlot.slotIndex = 0;
        referenceSlot.pPictureResource = &dpbPicResource;

        // set all the frame parameters
        h264::IntraFrameInfo intraFrameInfo(m_frameCount, m_width, m_height,
            m_sps,
            m_pps,
            m_frameCount == 0);
        VkVideoEncodeH264VclFrameInfoEXT* encodeH264FrameInfo = intraFrameInfo.getEncodeH264FrameInfo();

        // combine all structures in one control structure
        VkVideoEncodeInfoKHR videoEncodeInfo = { VK_STRUCTURE_TYPE_VIDEO_ENCODE_INFO_KHR };
        videoEncodeInfo.pNext = encodeH264FrameInfo;
        videoEncodeInfo.qualityLevel = 0;
        videoEncodeInfo.dstBuffer = m_bitStreamBuffer;
        videoEncodeInfo.dstBufferOffset = 0;
        videoEncodeInfo.srcPictureResource = inputPicResource;
        videoEncodeInfo.pSetupReferenceSlot = &referenceSlot;

        // prepare the query pool for the resulting bitstream
        const uint32_t querySlotId = 0;
        vkCmdResetQueryPool(cmdBuffer, m_queryPool, querySlotId, 1);
        vkCmdBeginQuery(cmdBuffer, m_queryPool, querySlotId, VkQueryControlFlags());
        // encode the frame as video
        vkCmdEncodeVideoKHR(cmdBuffer, &videoEncodeInfo);
        // end the query for the result
        vkCmdEndQuery(cmdBuffer, m_queryPool, querySlotId);
        // finish the video session
        VkVideoEndCodingInfoKHR encodeEndInfo = { VK_STRUCTURE_TYPE_VIDEO_END_CODING_INFO_KHR };
        vkCmdEndVideoCodingKHR(cmdBuffer, &encodeEndInfo);

        // run the encoding
        VHCHECKRESULT(vhCmdEndSingleTimeCommands(m_device, m_encodeQueue, m_encodeCommandPool, cmdBuffer));
        return VK_SUCCESS;
    }

    VkResult VHVideoEncoder::readOutputVideoPacket()
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

        std::cout << "status: " << encodeResult[0].status << " offset: " << encodeResult[0].bitstreamStartOffset << " size: " << encodeResult[0].bitstreamSize << std::endl;
        // invalidate host caches
        vmaInvalidateAllocation(m_allocator, m_bitStreamBufferAllocation, encodeResult[0].bitstreamStartOffset, encodeResult[0].bitstreamSize);
        void* data;
        // map memory to host and write the bitstream into the file
        vmaMapMemory(m_allocator, m_bitStreamBufferAllocation, &data);
        m_outfile.write(reinterpret_cast<const char*>(data) + encodeResult[0].bitstreamStartOffset, encodeResult[0].bitstreamSize);
        vmaUnmapMemory(m_allocator, m_bitStreamBufferAllocation);

        m_frameCount++;
        return VK_SUCCESS;
    }

    void VHVideoEncoder::deinit()
    {
        if (!m_initialized) {
            return;
        }

        vkDestroyPipeline(m_device, m_computePipeline, nullptr);
        vkDestroyPipelineLayout(m_device, m_computePipelineLayout, nullptr);
        vkDestroyDescriptorPool(m_device, m_descriptorPool, nullptr);
        vkDestroyDescriptorSetLayout(m_device, m_computeDescriptorSetLayout, nullptr);

        vkDestroyVideoSessionParametersKHR(m_device, m_videoSessionParameters, nullptr);
        vkDestroyQueryPool(m_device, m_queryPool, nullptr);
        vmaDestroyBuffer(m_allocator, m_bitStreamBuffer, m_bitStreamBufferAllocation);
        vkDestroyImageView(m_device, m_yuvImageChromaView, nullptr);
        vmaDestroyImage(m_allocator, m_yuvImageChroma, m_yuvImageChromaAllocation);
        vkDestroyImageView(m_device, m_yuvImagePlane0View, nullptr);
        vkDestroyImageView(m_device, m_yuvImageView, nullptr);
        vmaDestroyImage(m_allocator, m_yuvImage, m_yuvImageAllocation);
        vkDestroyImageView(m_device, m_dpbImageView, nullptr);
        vmaDestroyImage(m_allocator, m_dpbImage, m_dpbImageAllocation);
        vkDestroyVideoSessionKHR(m_device, m_videoSession, nullptr);
        for (VmaAllocation& allocation : m_allocations) {
            vmaFreeMemory(m_allocator, allocation);
        }
        m_allocations.clear();
        m_outfile.close();

        m_initialized = false;
    }
};
