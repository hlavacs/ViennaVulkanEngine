#pragma once


namespace vvh {

	//---------------------------------------------------------------------------------------------

    struct RenCreateRenderPassInfo {
		const VkFormat& 	m_depthFormat;
		const VkDevice& 	m_device;
		const SwapChain& 	m_swapChain;
		const bool& 		m_clear;
		VkRenderPass& 		m_renderPass;
	};

	template<typename T = RenCreateRenderPassInfo>
	inline void RenCreateRenderPass(T&& info) {
        VkAttachmentDescription colorAttachment{};
        colorAttachment.format = info.m_swapChain.m_swapChainImageFormat;
        colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;      
        colorAttachment.loadOp = info.m_clear ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        colorAttachment.initialLayout = info.m_clear ? VK_IMAGE_LAYOUT_UNDEFINED : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        colorAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkAttachmentDescription depthAttachment{};
        depthAttachment.format = info.m_depthFormat;
        depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        depthAttachment.loadOp = info.m_clear ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
        depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAttachment.initialLayout = info.m_clear ? VK_IMAGE_LAYOUT_UNDEFINED : VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        std::array<VkAttachmentDescription, 2> attachments = {colorAttachment, depthAttachment};

        VkAttachmentReference colorAttachmentRef{};
        colorAttachmentRef.attachment = 0;
        colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkAttachmentReference depthAttachmentRef{};
        depthAttachmentRef.attachment = 1;
        depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &colorAttachmentRef;
        subpass.pDepthStencilAttachment = &depthAttachmentRef;

        VkSubpassDependency dependency{};
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass = 0;
        dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        dependency.srcAccessMask = 0;
        dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

        VkSubpassDependency dependency2{};
        dependency2.srcSubpass = 0;
        dependency2.dstSubpass = VK_SUBPASS_EXTERNAL;
        dependency2.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        dependency2.srcAccessMask = 0;
        dependency2.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        dependency2.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

        std::vector<VkSubpassDependency> dependencies = {dependency,dependency2};

        VkRenderPassCreateInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        renderPassInfo.pAttachments = attachments.data();
        renderPassInfo.subpassCount = 1;
        renderPassInfo.pSubpasses = &subpass;
        renderPassInfo.dependencyCount = dependencies.size();
        renderPassInfo.pDependencies = dependencies.data();

        if (vkCreateRenderPass(info.m_device, &renderPassInfo, nullptr, &info.m_renderPass) != VK_SUCCESS) {
            throw std::runtime_error("failed to create render pass!");
        }
    }

	//---------------------------------------------------------------------------------------------

	struct RenCreateDescriptorSetLayoutInfo {
		const VkDevice& 									m_device;
		const std::vector<VkDescriptorSetLayoutBinding>& 	m_bindings;
		VkDescriptorSetLayout& 								m_descriptorSetLayout;
	};

	template<typename T = RenCreateDescriptorSetLayoutInfo>
	inline void RenCreateDescriptorSetLayout(T&& info) {
		uint32_t i = 0;
		std::vector<VkDescriptorSetLayoutBinding> bindings = info.m_bindings;
		for( auto& uboLayoutBinding : bindings ) {
	        uboLayoutBinding.binding = i;
	        uboLayoutBinding.descriptorCount = 1;
	        uboLayoutBinding.pImmutableSamplers = nullptr;
            ++i;
		}

        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
        layoutInfo.pBindings = bindings.data();

        if (vkCreateDescriptorSetLayout(info.m_device, &layoutInfo, nullptr, &info.m_descriptorSetLayout) != VK_SUCCESS) {
            throw std::runtime_error("failed to create descriptor set layout 0!");
        }
    }

	//---------------------------------------------------------------------------------------------

  	struct RenCreateDescriptorPoolInfo {
		const VkDevice& 	m_device;
		const uint32_t& 	m_sizes;
		VkDescriptorPool& 	m_descriptorPool;
	};

	template<typename T = RenCreateDescriptorPoolInfo>
	inline void RenCreateDescriptorPool(T&& info) {

		std::vector<VkDescriptorPoolSize> pool_sizes;
		pool_sizes.push_back({ VK_DESCRIPTOR_TYPE_SAMPLER, info.m_sizes });
		pool_sizes.push_back({ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, info.m_sizes });
		pool_sizes.push_back({ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, info.m_sizes });
		pool_sizes.push_back({ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, info.m_sizes });
		pool_sizes.push_back({ VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, info.m_sizes });
		pool_sizes.push_back({ VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, info.m_sizes });
		pool_sizes.push_back({ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, info.m_sizes });
		pool_sizes.push_back({ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, info.m_sizes });
		pool_sizes.push_back({ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, info.m_sizes });
		pool_sizes.push_back({ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, info.m_sizes });
		pool_sizes.push_back({ VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, info.m_sizes });

        VkDescriptorPoolCreateInfo pool_info = {};
        pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        pool_info.maxSets = info.m_sizes;
        pool_info.poolSizeCount = (uint32_t)std::size(pool_sizes);
        pool_info.pPoolSizes = pool_sizes.data();

		if (vkCreateDescriptorPool(info.m_device, &pool_info, nullptr, &info.m_descriptorPool) != VK_SUCCESS) {
            throw std::runtime_error("failed to create descriptor pool!");
        }
    }

	//---------------------------------------------------------------------------------------------

    struct RenCreateDescriptorSetInfo {
		const VkDevice&					m_device;
		const VkDescriptorSetLayout& 	m_descriptorSetLayouts;
		const VkDescriptorPool&			m_descriptorPool;
		DescriptorSet& 					m_descriptorSet;
	};

	template<typename T = RenCreateDescriptorSetInfo>
	inline void RenCreateDescriptorSet(T&& info) {

		info.m_descriptorSet.m_descriptorSetPerFrameInFlight.resize(MAX_FRAMES_IN_FLIGHT);
        std::vector<VkDescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, info.m_descriptorSetLayouts);
        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = info.m_descriptorPool;
        allocInfo.descriptorSetCount = (uint32_t)layouts.size();
        allocInfo.pSetLayouts = layouts.data();
        if (vkAllocateDescriptorSets(info.m_device, &allocInfo, info.m_descriptorSet.m_descriptorSetPerFrameInFlight.data()) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate descriptor sets!");
        }
    }

	//---------------------------------------------------------------------------------------------
    struct RenUpdateDescriptorSetInfo {
		const VkDevice& 		m_device;
		const Buffer& 			m_uniformBuffers;
		const size_t& 			m_binding;
		const VkDescriptorType& m_type;
		const size_t& 			m_size;
		DescriptorSet& 			m_descriptorSet;
	};

	template<typename T = RenUpdateDescriptorSetInfo>
	inline void RenUpdateDescriptorSet(T&& info) {

		size_t i = 0;
		for ( auto& ds : info.m_descriptorSet.m_descriptorSetPerFrameInFlight ) {
			VkDescriptorBufferInfo bufferInfo{};
			bufferInfo.buffer = info.m_uniformBuffers.m_uniformBuffers[i++];
			bufferInfo.offset = 0;
			bufferInfo.range = info.m_size;

			VkWriteDescriptorSet descriptorWrites{};

			descriptorWrites.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			descriptorWrites.dstSet = ds;
			descriptorWrites.dstBinding = (uint32_t)info.m_binding;
			descriptorWrites.dstArrayElement = 0;
			descriptorWrites.descriptorType = info.m_type;
			descriptorWrites.descriptorCount = 1;
			descriptorWrites.pBufferInfo = &bufferInfo;

			vkUpdateDescriptorSets(info.m_device, 1, &descriptorWrites, 0, nullptr);
		}
	}

	//---------------------------------------------------------------------------------------------

    struct RenUpdateDescriptorSetTextureInfo { 
		const VkDevice& 		m_device;
		const Image& 			m_texture;
		const size_t&			m_binding;
		const DescriptorSet& 	m_descriptorSet;
	};

	template<typename T = RenUpdateDescriptorSetTextureInfo>
	inline void RenUpdateDescriptorSetTexture(T&& info) {

		size_t i = 0;
	    for ( auto& ds : info.m_descriptorSet.m_descriptorSetPerFrameInFlight ) {

	        VkDescriptorImageInfo imageInfo{};
	        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	        imageInfo.imageView = info.m_texture.m_mapImageView;
	        imageInfo.sampler = info.m_texture.m_mapSampler;

	        VkWriteDescriptorSet descriptorWrites{};

	        descriptorWrites.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	        descriptorWrites.dstSet = ds;
	        descriptorWrites.dstBinding = info.m_binding;
	        descriptorWrites.dstArrayElement = 0;
	        descriptorWrites.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	        descriptorWrites.descriptorCount = 1;
	        descriptorWrites.pImageInfo = &imageInfo;

	        vkUpdateDescriptorSets(info.m_device, 1, &descriptorWrites, 0, nullptr);
		}
    }

	//---------------------------------------------------------------------------------------------

    struct RenCreateShaderModuleInfo {
		const VkDevice& 			m_device;
		const std::vector<char>& 	m_code;
	};

	template<typename T = RenCreateShaderModuleInfo>
	inline auto RenCreateShaderModule(T&& info) -> VkShaderModule {
        VkShaderModuleCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        createInfo.codeSize = info.m_code.size();
        createInfo.pCode = reinterpret_cast<const uint32_t*>(info.m_code.data());

        VkShaderModule shaderModule;
        if (vkCreateShaderModule(info.m_device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
            throw std::runtime_error("failed to create shader module!");
        }
        return shaderModule;
    }

	//---------------------------------------------------------------------------------------------

    struct RenCreateGraphicsPipelineInfo { 
		const VkDevice& 	m_device;
		const VkRenderPass& m_renderPass;
		const std::string& 	m_vertShaderPath;
		const std::string& 	m_fragShaderPath;
		const std::vector<VkVertexInputBindingDescription>& 	m_bindingDescription;
		const std::vector<VkVertexInputAttributeDescription>& 	m_attributeDescriptions;
		const std::vector<VkDescriptorSetLayout>& 				m_descriptorSetLayouts;
		const std::vector<int32_t>& 							m_specializationConstants;
		const std::vector<VkPushConstantRange>& 				m_pushConstantRanges;
		const std::vector<VkPipelineColorBlendAttachmentState>& m_blendAttachments;
  		Pipeline& m_graphicsPipeline;
	};

	template<typename T = RenCreateGraphicsPipelineInfo>
	inline void RenCreateGraphicsPipeline(T&& info) {

	    // Specialization constant setup
	    std::vector<VkSpecializationMapEntry> specializationEntries;
		for( uint32_t i=0; i<info.m_specializationConstants.size(); i++ ) {
			specializationEntries.push_back( 
				VkSpecializationMapEntry{.constantID = i, .offset = i * (uint32_t)sizeof(int32_t), .size = (uint32_t)sizeof(int32_t)} );
		}
		
	    VkSpecializationInfo specializationInfo{};
	    specializationInfo.mapEntryCount = (uint32_t)specializationEntries.size();
	    specializationInfo.pMapEntries = specializationEntries.data();
	    specializationInfo.dataSize = sizeof(int32_t) * info.m_specializationConstants.size();
	    specializationInfo.pData = info.m_specializationConstants.data();

		std::vector<VkPipelineShaderStageCreateInfo> shaderStages;

        VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
        auto vertShaderCode = vvh::ReadFile(info.m_vertShaderPath);
        VkShaderModule vertShaderModule = RenCreateShaderModule({info.m_device, vertShaderCode });
        vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
        vertShaderStageInfo.module = vertShaderModule;
        vertShaderStageInfo.pName = "main";
		vertShaderStageInfo.pSpecializationInfo = &specializationInfo;
		shaderStages.push_back(vertShaderStageInfo);

		VkShaderModule fragShaderModule{};
		if( !info.m_fragShaderPath.empty() ) {
	        VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
	        auto fragShaderCode = vvh::ReadFile(info.m_fragShaderPath);
	        fragShaderModule = RenCreateShaderModule({info.m_device, fragShaderCode });
	        fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	        fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	        fragShaderStageInfo.module = fragShaderModule;
	        fragShaderStageInfo.pName = "main";
			vertShaderStageInfo.pSpecializationInfo = &specializationInfo;
			shaderStages.push_back(fragShaderStageInfo);
		}

        VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
        vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

		if( !info.m_fragShaderPath.empty() ) {
	        vertexInputInfo.vertexBindingDescriptionCount = (uint32_t)info.m_bindingDescription.size();
	        vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(info.m_attributeDescriptions.size());
	        vertexInputInfo.pVertexBindingDescriptions = info.m_bindingDescription.data();
	        vertexInputInfo.pVertexAttributeDescriptions = info.m_attributeDescriptions.data();
		}

        VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
        inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        inputAssembly.primitiveRestartEnable = VK_FALSE;

        VkPipelineViewportStateCreateInfo viewportState{};
        viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportState.viewportCount = 1;
        viewportState.scissorCount = 1;

        VkPipelineRasterizationStateCreateInfo rasterizer{};
        rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterizer.depthClampEnable = VK_FALSE;
        rasterizer.rasterizerDiscardEnable = VK_FALSE;
        rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
        rasterizer.lineWidth = 1.0f;
        rasterizer.cullMode = VK_CULL_MODE_BACK_BIT; //VK_CULL_MODE_NONE
        rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        rasterizer.depthBiasEnable = VK_FALSE;

        VkPipelineMultisampleStateCreateInfo multisampling{};
        multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampling.sampleShadingEnable = VK_FALSE;
        multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        VkPipelineDepthStencilStateCreateInfo depthStencil{};
        depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        depthStencil.depthTestEnable = VK_TRUE;
        depthStencil.depthWriteEnable = VK_TRUE;
        depthStencil.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL ;
        depthStencil.depthBoundsTestEnable = VK_FALSE;
        depthStencil.stencilTestEnable = VK_FALSE;

        VkPipelineColorBlendAttachmentState colorBlendAttachment{};
        colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT 
            | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        colorBlendAttachment.blendEnable = VK_FALSE;

		auto blendAttachments = info.m_blendAttachments;
        if( blendAttachments.size() == 0) blendAttachments.push_back(colorBlendAttachment);

        VkPipelineColorBlendStateCreateInfo colorBlending{};
        colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlending.logicOpEnable = VK_FALSE;
        colorBlending.logicOp = VK_LOGIC_OP_COPY;
        colorBlending.attachmentCount = blendAttachments.size();
        colorBlending.pAttachments = blendAttachments.data();
        colorBlending.blendConstants[0] = 0.0f;
        colorBlending.blendConstants[1] = 0.0f;
        colorBlending.blendConstants[2] = 0.0f;
        colorBlending.blendConstants[3] = 0.0f;

        std::vector<VkDynamicState> dynamicStates = {
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR,
            VK_DYNAMIC_STATE_BLEND_CONSTANTS 
        };
        VkPipelineDynamicStateCreateInfo dynamicState{};
        dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
        dynamicState.pDynamicStates = dynamicStates.data();

        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = (uint32_t)info.m_descriptorSetLayouts.size();
        pipelineLayoutInfo.pSetLayouts = info.m_descriptorSetLayouts.data();
        pipelineLayoutInfo.pushConstantRangeCount = info.m_pushConstantRanges.size();
        pipelineLayoutInfo.pPushConstantRanges = info.m_pushConstantRanges.size() > 0 ? info.m_pushConstantRanges.data() : nullptr;

        if (vkCreatePipelineLayout(info.m_device, &pipelineLayoutInfo, nullptr, &info.m_graphicsPipeline.m_pipelineLayout) != VK_SUCCESS) {
            throw std::runtime_error("failed to create pipeline layout!");
        }

        VkGraphicsPipelineCreateInfo pipelineInfo{};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipelineInfo.stageCount = (uint32_t)shaderStages.size();
        pipelineInfo.pStages = shaderStages.data();
        pipelineInfo.pVertexInputState = &vertexInputInfo;
        pipelineInfo.pInputAssemblyState = &inputAssembly;
        pipelineInfo.pViewportState = &viewportState;
        pipelineInfo.pRasterizationState = &rasterizer;
        pipelineInfo.pMultisampleState = &multisampling;
        pipelineInfo.pDepthStencilState = &depthStencil;
        pipelineInfo.pColorBlendState = &colorBlending;
        pipelineInfo.pDynamicState = &dynamicState;
        pipelineInfo.layout = info.m_graphicsPipeline.m_pipelineLayout;
        pipelineInfo.renderPass = info.m_renderPass;
        pipelineInfo.subpass = 0;
        pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

        if (vkCreateGraphicsPipelines(info.m_device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &info.m_graphicsPipeline.m_pipeline) != VK_SUCCESS) {
            throw std::runtime_error("failed to create graphics pipeline!");
        }

		if(shaderStages.size() > 1) { vkDestroyShaderModule(info.m_device, fragShaderModule, nullptr); }
        vkDestroyShaderModule(info.m_device, vertShaderModule, nullptr);
    }

	//---------------------------------------------------------------------------------------------

    struct RenCreateFramebuffersInfo {
		const VkDevice& 	m_device;
		const DepthImage& 	m_depthImage;
		const VkRenderPass& m_renderPass;
		SwapChain& 			m_swapChain; 
	};

	template<typename T = RenCreateFramebuffersInfo>
	inline void RenCreateFramebuffers(T&& info) {
        info.m_swapChain.m_swapChainFramebuffers.resize(info.m_swapChain.m_swapChainImageViews.size());

        for (size_t i = 0; i < info.m_swapChain.m_swapChainImageViews.size(); i++) {
            std::array<VkImageView, 2> attachments = {
                info.m_swapChain.m_swapChainImageViews[i],
                info.m_depthImage.m_depthImageView
            };

            VkFramebufferCreateInfo framebufferInfo{};
            framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            framebufferInfo.renderPass = info.m_renderPass;
            framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
            framebufferInfo.pAttachments = attachments.data();
            framebufferInfo.width = info.m_swapChain.m_swapChainExtent.width;
            framebufferInfo.height = info.m_swapChain.m_swapChainExtent.height;
            framebufferInfo.layers = 1;

            if (vkCreateFramebuffer(info.m_device, &framebufferInfo, nullptr, &info.m_swapChain.m_swapChainFramebuffers[i]) != VK_SUCCESS) {
                throw std::runtime_error("failed to create framebuffer!");
            }
        }
    }

	//---------------------------------------------------------------------------------------------

	struct RenFindSupportedFormatInfo {
		const VkPhysicalDevice& 		m_physicalDevice;
		const std::vector<VkFormat>& 	m_candidates;
		const VkImageTiling& 			m_tiling;
		const VkFormatFeatureFlags& 	m_features;
	};

	template<typename T = RenFindSupportedFormatInfo>
	inline auto RenFindSupportedFormat(T&& info) -> VkFormat {

        for (VkFormat format : info.m_candidates) {
            VkFormatProperties props;
            vkGetPhysicalDeviceFormatProperties(info.m_physicalDevice, format, &props);

            if (info.m_tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & info.m_features) == info.m_features) {
                return format;
            } else if (info.m_tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & info.m_features) == info.m_features) {
                return format;
            }
        }

        throw std::runtime_error("failed to find supported format!");
    }


	//---------------------------------------------------------------------------------------------

    inline auto RenFindDepthFormat(VkPhysicalDevice physicalDevice) -> VkFormat {
        return RenFindSupportedFormat( {
			.m_physicalDevice = physicalDevice, 
            .m_candidates = {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT},
            .m_tiling = VK_IMAGE_TILING_OPTIMAL,
            .m_features = VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
		});
    }

	//---------------------------------------------------------------------------------------------

    struct RenCreateDepthResourcesInfo { 
		const VkPhysicalDevice& m_physicalDevice;
		const VkDevice& 		m_device;
		const VmaAllocator& 	m_vmaAllocator;
		const SwapChain& 		m_swapChain;
		DepthImage& 			m_depthImage;
	};

	template<typename T = RenCreateDepthResourcesInfo>
	inline void RenCreateDepthResources(T&& info) {
        const VkFormat depthFormat = RenFindDepthFormat(info.m_physicalDevice);

        ImgCreateImage2({
			info.m_physicalDevice, 
			info.m_device, 
			info.m_vmaAllocator, 
			info.m_swapChain.m_swapChainExtent.width, 
			info.m_swapChain.m_swapChainExtent.height, 
			depthFormat, 
			VK_IMAGE_TILING_OPTIMAL, 
			VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, 
			VK_IMAGE_LAYOUT_UNDEFINED, //  Do NOT CHANGE
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 
			info.m_depthImage.m_depthImage, 
			info.m_depthImage.m_depthImageAllocation
		});
        
		info.m_depthImage.m_depthImageView = ImgCreateImageView2( {
			.m_device = info.m_device, 
			.m_image  = info.m_depthImage.m_depthImage, 
			.m_format = depthFormat, 
			.m_aspects = VK_IMAGE_ASPECT_DEPTH_BIT
		});
    }

	//---------------------------------------------------------------------------------------------

	inline bool RenHasStencilComponent(VkFormat format) {
        return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
    }


} // namespace vh

