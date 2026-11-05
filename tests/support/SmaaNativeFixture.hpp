/** @file @brief Native unlit fixture recorder, using public attachment and coverage metadata. */
/// @brief Test-owned triangle positions and pixel-frequency cutout selection.
struct Push {
	std::array<Vec4,3> points{}; ///< Drawable-pixel triangle positions.
	Vec2 offset{}; ///< Subpixel phase.
	unsigned detail{}; ///< Alpha-cutout switch.
	unsigned unused{}; ///< Push-constant alignment padding.
};
/// @brief Callback-owned pipeline; VVE retires the owner after its last submission fence, before device destruction.
struct NativeFixture {
	VkDevice device{}; ///< Borrowed device, valid throughout callback ownership.
	VkPipelineLayout layout{}; ///< Push-constant layout for the test-owned shader.
	VkPipeline pipeline{}; ///< One pipeline for this engine's startup pass/coverage selection.
	Point offset{}; ///< Subpixel phase updated between ordinary frame calls.
	std::vector<Triangle> triangles{}; ///< Drawable-pixel inputs copied into each draw's push constants.
	bool detail{}; ///< Pixel-frequency cutout selection, updated only between frame calls.
	unsigned draws{}; ///< Number of actual geometry draws.
	unsigned samples{}; ///< Coverage receipt checked against the selected AA mode.
	unsigned attachments{}; ///< Attachment receipt checked against the selected backend.
	~NativeFixture();
	void create(const NativeDrawContext &);
	auto record(const NativeDrawContext &) -> std::expected<void, Error>;
};
/// @brief Release native owners while the fenced engine device remains valid.
NativeFixture::~NativeFixture() {
	if (device) {
		vkDestroyPipeline(device, pipeline, nullptr);
		vkDestroyPipelineLayout(device, layout, nullptr);
	}
}
/// @brief Build from the public samples, attachment count and formats, reusing the native test shader build rules.
void NativeFixture::create(const NativeDrawContext &c) {
	device = reinterpret_cast<VkDevice>(c.device);
	samples = c.samples;
	attachments = c.colorAttachmentCount;
	const VkPushConstantRange push{VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(Push)};
	const VkPipelineLayoutCreateInfo layoutInfo{.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.pushConstantRangeCount = 1, .pPushConstantRanges = &push};
	requireVk(vkCreatePipelineLayout(device, &layoutInfo, nullptr, &layout));
	std::array<VkShaderModule, 2> modules{};
	const auto shader = [&](std::span<const std::uint32_t> code, VkShaderModule &module) {
		const VkShaderModuleCreateInfo info{.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO, .codeSize = code.size_bytes(), .pCode = code.data()};
		requireVk(vkCreateShaderModule(device, &info, nullptr, &module));
	};
	try {
		shader(smaa_fixtureVertex, modules[0]);
		shader(c.pass == NativeDrawPass::gbuffer ? std::span<const std::uint32_t>{smaa_fixtureGbuffer} : std::span<const std::uint32_t>{smaa_fixtureForward}, modules[1]);
		const std::array<VkPipelineShaderStageCreateInfo, 2> stages{{
			{.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = VK_SHADER_STAGE_VERTEX_BIT, .module = modules[0], .pName = "fixtureVertex"},
			{.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = VK_SHADER_STAGE_FRAGMENT_BIT, .module = modules[1], .pName = c.pass == NativeDrawPass::gbuffer ? "fixtureGbuffer" : "fixtureForward"}}};
		const VkPipelineVertexInputStateCreateInfo vertices{.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
		const VkPipelineInputAssemblyStateCreateInfo assembly{.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO, .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST};
		const VkPipelineViewportStateCreateInfo viewport{.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO, .viewportCount = 1, .scissorCount = 1};
		const VkPipelineRasterizationStateCreateInfo raster{.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
			.polygonMode = VK_POLYGON_MODE_FILL, .cullMode = VK_CULL_MODE_NONE, .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE, .lineWidth = 1};
		const VkPipelineMultisampleStateCreateInfo multisample{.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO, .rasterizationSamples = VkSampleCountFlagBits(c.samples)};
		const VkPipelineDepthStencilStateCreateInfo depth{.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
			.depthTestEnable = VK_TRUE, .depthWriteEnable = VK_TRUE, .depthCompareOp = VK_COMPARE_OP_LESS};
		std::array<VkPipelineColorBlendAttachmentState, 3> colors{};
		for (auto &a : colors) { a.colorWriteMask = 15; }
		const VkPipelineColorBlendStateCreateInfo blend{.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
			.attachmentCount = c.colorAttachmentCount, .pAttachments = colors.data()};
		const std::array states{VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
		const VkPipelineDynamicStateCreateInfo dynamic{.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO, .dynamicStateCount = 2, .pDynamicStates = states.data()};
		std::array<VkFormat, 3> formats{};
		std::ranges::transform(c.colorFormats, formats.begin(), [](auto f) { return VkFormat(f); });
		const VkPipelineRenderingCreateInfo rendering{.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
			.colorAttachmentCount = c.colorAttachmentCount, .pColorAttachmentFormats = formats.data(), .depthAttachmentFormat = VkFormat(c.depthFormat)};
		const VkGraphicsPipelineCreateInfo info{.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO, .pNext = &rendering,
			.stageCount = 2, .pStages = stages.data(), .pVertexInputState = &vertices, .pInputAssemblyState = &assembly, .pViewportState = &viewport,
			.pRasterizationState = &raster, .pMultisampleState = &multisample, .pDepthStencilState = &depth, .pColorBlendState = &blend,
			.pDynamicState = &dynamic, .layout = layout};
		requireVk(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &info, nullptr, &pipeline));
	} catch (...) {
		for (auto m : modules) { vkDestroyShaderModule(device, m, nullptr); }
		throw;
	}
	for (auto m : modules) { vkDestroyShaderModule(device, m, nullptr); }
}
/// @brief Draw only the main opaque pass; G-buffer and forward use identical coverage and linear unlit RGB.
auto NativeFixture::record(const NativeDrawContext &c) -> std::expected<void, Error> try {
	if (c.prepare || (c.pass != NativeDrawPass::forward_opaque && c.pass != NativeDrawPass::gbuffer)) { return {}; }
	if (!device) { create(c); }
	Push push;
	push.offset = Vec2{float(offset[0]), float(offset[1])};
	push.detail = detail;
	const auto cmd = reinterpret_cast<VkCommandBuffer>(c.commandBuffer);
	const VkViewport viewport{float(c.viewportX), float(c.viewportY), float(c.width), float(c.height), 0, 1};
	const VkRect2D scissor{{int(c.viewportX), int(c.viewportY)}, {c.width, c.height}};
	vkCmdSetViewport(cmd, 0, 1, &viewport);
	vkCmdSetScissor(cmd, 0, 1, &scissor);
	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
	// Copy explicit geometry into each draw; the CPU integration never reads rendered pixels.
	for (const auto &triangle : triangles) {
		for (auto i : std::views::iota(0U, 3U)) {
			push.points[i] = Vec4{float(triangle[i][0]), float(triangle[i][1]), 0, 0};
		}
		vkCmdPushConstants(cmd, layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(push), &push);
		vkCmdDraw(cmd, 3, 1, 0, 0);
		++draws;
	}
	return {};
} catch (...) { return std::unexpected(Error::platform_error); }

