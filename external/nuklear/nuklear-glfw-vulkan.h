#ifndef NK_VULKAN_GLFW_H_
#define NK_VULKAN_GLFW_H_

#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_STANDARD_IO
#define NK_INCLUDE_STANDARD_VARARGS
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT
#define NK_INCLUDE_FONT_BAKING
#define NK_INCLUDE_DEFAULT_FONT

#include "nuklear.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

enum nk_glfw_init_state{
    NK_GLFW3_DEFAULT=0,
    NK_GLFW3_INSTALL_CALLBACKS
};

NK_API struct nk_context*   nk_glfw3_init(GLFWwindow *win, VkDevice logical_device, VkPhysicalDevice physical_device, VkQueue graphics_queue, uint32_t graphics_queue_index, VkFramebuffer* framebuffers, uint32_t framebuffers_len, VkFormat color_format, VkFormat depth_format, enum nk_glfw_init_state);
NK_API void                 nk_glfw3_shutdown(void);
NK_API void                 nk_glfw3_font_stash_begin(struct nk_font_atlas **atlas);
NK_API void                 nk_glfw3_font_stash_end(void);
NK_API void                 nk_glfw3_new_frame();
NK_API VkSemaphore          nk_glfw3_render(enum nk_anti_aliasing AA, uint32_t buffer_index, VkSemaphore wait_semaphore);

NK_API void                 nk_glfw3_device_destroy(void);
NK_API void                 nk_glfw3_device_create(VkDevice, VkPhysicalDevice, VkQueue graphics_queue, uint32_t graphics_queue_index, VkFramebuffer* framebuffers, uint32_t framebuffers_len, VkFormat, VkFormat);

NK_API void                 nk_glfw3_char_callback(GLFWwindow *win, unsigned int codepoint);
NK_API void                 nk_gflw3_scroll_callback(GLFWwindow *win, double xoff, double yoff);
NK_API void                 nk_glfw3_mouse_button_callback(GLFWwindow *win, int button, int action, int mods);

#endif

#ifdef NK_GLFW_VULKAN_IMPLEMENTATION

// ${NUKLEAR_SHADERS_START}
#include "nuklearshaders/nuklear.frag.h"
#include "nuklearshaders/nuklear.vert.h"
// ${NUKLEAR_SHADERS_END}

#ifndef NK_GLFW_TEXT_MAX
#define NK_GLFW_TEXT_MAX 256
#endif
#ifndef NK_GLFW_DOUBLE_CLICK_LO
#define NK_GLFW_DOUBLE_CLICK_LO 0.02
#endif
#ifndef NK_GLFW_DOUBLE_CLICK_HI
#define NK_GLFW_DOUBLE_CLICK_HI 0.2
#endif

#define VK_COLOR_COMPONENT_MASK_RGBA VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT

#define MAX_VERTEX_BUFFER 512 * 1024
#define MAX_INDEX_BUFFER 128 * 1024

struct Mat4f {
    float m[16];
};

struct nk_vulkan_adapter {
    struct nk_buffer cmds;
    struct nk_draw_null_texture null;
    VkDevice logical_device;
    VkPhysicalDevice physical_device;
    VkQueue graphics_queue;
    uint32_t graphics_queue_index;
    VkFramebuffer* framebuffers;
    uint32_t framebuffers_len;
    VkFormat color_format;
    VkFormat depth_format;
    VkSemaphore render_completed;
    VkSampler font_tex;
    VkImage font_image;
    VkImageView font_image_view;
    VkDeviceMemory font_memory;
    VkPipelineLayout pipeline_layout;
    VkRenderPass render_pass;
    VkPipeline pipeline;
    VkBuffer vertex_buffer;
    VkDeviceMemory vertex_memory;
    VkBuffer index_buffer;
    VkDeviceMemory index_memory;
    VkBuffer uniform_buffer;
    VkDeviceMemory uniform_memory;
    VkCommandPool command_pool;
    VkCommandBuffer* command_buffers; // currently always length 1
    VkDescriptorPool descriptor_pool;
    VkDescriptorSetLayout descriptor_set_layout;
    VkDescriptorSet descriptor_set;
    VkDescriptorSet image_descriptor_set[1000];
    size_t current_image_descriptor_set;
};

struct nk_glfw_vertex {
    float position[2];
    float uv[2];
    nk_byte col[4];
};

static struct nk_glfw {
    GLFWwindow *win;
    int width, height;
    int display_width, display_height;
    struct nk_vulkan_adapter adapter;
    struct nk_context ctx;
    struct nk_font_atlas atlas;
    struct nk_vec2 fb_scale;
    unsigned int text[NK_GLFW_TEXT_MAX];
    int text_len;
    struct nk_vec2 scroll;
    double last_button_click;
    int is_double_click_down;
    struct nk_vec2 double_click_pos;
} glfw;

struct nk_glfw_image_info {
    VkSampler sampler;
    VkImageView image_view;
};

VkPipelineShaderStageCreateInfo create_shader(struct nk_vulkan_adapter* adapter, unsigned char* spv_shader, uint32_t size, VkShaderStageFlagBits stage_bit) {
	VkShaderModuleCreateInfo create_info = {};
	create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	create_info.codeSize = size;
	create_info.pCode = (const uint32_t*)spv_shader;


    VkShaderModule module = VK_NULL_HANDLE;
	VkResult res = vkCreateShaderModule(adapter->logical_device, &create_info, NULL, &module); // == VK_SUCCESS);

	VkPipelineShaderStageCreateInfo shader_info = {};
	shader_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shader_info.stage = stage_bit;
	shader_info.module = module;
	shader_info.pName = "main";
    return shader_info;
}

void prepare_descriptor_pool(struct nk_vulkan_adapter* adapter) {
	VkDescriptorPoolSize pool_sizes[2] = {};
	pool_sizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	pool_sizes[0].descriptorCount = 1000;
	pool_sizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	pool_sizes[1].descriptorCount = 1000;

	VkDescriptorPoolCreateInfo pool_info = {};
	pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	pool_info.poolSizeCount = 2;
	pool_info.pPoolSizes = pool_sizes;
	pool_info.maxSets = 1000;

	VkResult res = vkCreateDescriptorPool(adapter->logical_device, &pool_info, VK_NULL_HANDLE, &adapter->descriptor_pool);
    assert( res == VK_SUCCESS);
}

void prepare_descriptor_set_layout(struct nk_vulkan_adapter* adapter) {
	VkDescriptorSetLayoutBinding bindings[2]={};
	bindings[0].binding = 0;
	bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	bindings[0].descriptorCount = 1;
	bindings[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	bindings[1].binding = 1;
	bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	bindings[1].descriptorCount = 1;
	bindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

	VkDescriptorSetLayoutCreateInfo descriptor_set_info = {};
	descriptor_set_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	descriptor_set_info.bindingCount = 2;
	descriptor_set_info.pBindings = bindings;

	VkResult res = vkCreateDescriptorSetLayout(adapter->logical_device, &descriptor_set_info, NULL, &adapter->descriptor_set_layout);
	assert( res == VK_SUCCESS); 
}

void prepare_descriptor_set(struct nk_vulkan_adapter* adapter, bool is_font = true) {
	VkDescriptorSetAllocateInfo allocate_info = {};
	allocate_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocate_info.descriptorPool = adapter->descriptor_pool;
	allocate_info.descriptorSetCount = 1;
	allocate_info.pSetLayouts = &adapter->descriptor_set_layout;
    
    VkResult res;
    if (is_font) {
        res = vkAllocateDescriptorSets(adapter->logical_device, &allocate_info, &adapter->descriptor_set);
    }
    else {
        res = vkAllocateDescriptorSets(adapter->logical_device, &allocate_info,
            &adapter->image_descriptor_set[adapter->current_image_descriptor_set]);
        adapter->current_image_descriptor_set++;
    }
    assert(res == VK_SUCCESS);
}

void update_write_descriptor_sets(struct nk_vulkan_adapter* adapter, struct nk_glfw_image_info *info = nullptr) {
	VkDescriptorBufferInfo buffer_info = {};
	buffer_info.buffer = adapter->uniform_buffer;
	buffer_info.offset = 0;
	buffer_info.range = sizeof(struct Mat4f);

	VkDescriptorImageInfo image_info = {};
	image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    if (info == nullptr) {
        image_info.imageView = adapter->font_image_view;
        image_info.sampler = adapter->font_tex;
    }
    else {
        image_info.imageView = info->image_view;
        image_info.sampler = info->sampler;
    }

	VkWriteDescriptorSet descriptor_writes[2] = {};
    memset(descriptor_writes, 0, sizeof(VkWriteDescriptorSet) * 2);
    descriptor_writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    if (info == nullptr)
        descriptor_writes[0].dstSet = adapter->descriptor_set;
    else
        descriptor_writes[0].dstSet = adapter->image_descriptor_set[adapter->current_image_descriptor_set - 1];
    descriptor_writes[0].dstBinding = 0;
    descriptor_writes[0].dstArrayElement = 0;
    descriptor_writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    descriptor_writes[0].descriptorCount = 1;
    descriptor_writes[0].pBufferInfo = &buffer_info;

    uint32_t descriptor_write_count = 1;
    if ((info == nullptr ? adapter->font_tex : info->sampler) != VK_NULL_HANDLE) {
        descriptor_write_count++;
        descriptor_writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        if (info == nullptr)
            descriptor_writes[1].dstSet = adapter->descriptor_set;
        else
            descriptor_writes[1].dstSet = adapter->image_descriptor_set[adapter->current_image_descriptor_set - 1];
        descriptor_writes[1].dstBinding = 1;
        descriptor_writes[1].dstArrayElement = 0;
        descriptor_writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptor_writes[1].descriptorCount = 1;
        descriptor_writes[1].pImageInfo = &image_info;
    }

    vkUpdateDescriptorSets(adapter->logical_device, descriptor_write_count, descriptor_writes, 0, VK_NULL_HANDLE);
}

void prepare_pipeline_layout(struct nk_vulkan_adapter* adapter) {
	VkPipelineLayoutCreateInfo pipeline_layout_info = {};
	pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipeline_layout_info.setLayoutCount = 1;
	pipeline_layout_info.pSetLayouts = &adapter->descriptor_set_layout;

	VkResult res = vkCreatePipelineLayout(adapter->logical_device, &pipeline_layout_info, NULL, &adapter->pipeline_layout);
    assert( res == VK_SUCCESS);
}

void prepare_pipeline(struct nk_vulkan_adapter* adapter) {
	VkPipelineInputAssemblyStateCreateInfo input_assembly_state = {};
	input_assembly_state.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	input_assembly_state.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	input_assembly_state.primitiveRestartEnable = VK_FALSE;

	VkPipelineRasterizationStateCreateInfo rasterization_state = {};
	rasterization_state.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterization_state.polygonMode = VK_POLYGON_MODE_FILL;
	rasterization_state.cullMode = VK_CULL_MODE_BACK_BIT;
	rasterization_state.frontFace = VK_FRONT_FACE_CLOCKWISE;
	rasterization_state.lineWidth = 1.0f;

	VkPipelineColorBlendAttachmentState attachment_state = {};
	attachment_state.blendEnable = VK_TRUE;
	attachment_state.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
	attachment_state.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	attachment_state.colorBlendOp = VK_BLEND_OP_ADD;
	attachment_state.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	attachment_state.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
	attachment_state.alphaBlendOp = VK_BLEND_OP_ADD;
	attachment_state.colorWriteMask = VK_COLOR_COMPONENT_MASK_RGBA;

	VkPipelineColorBlendStateCreateInfo color_blend_state = {};
	color_blend_state.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	color_blend_state.attachmentCount = 1;
	color_blend_state.pAttachments = &attachment_state;

	VkPipelineDepthStencilStateCreateInfo depth_stencil_state = {};
	depth_stencil_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depth_stencil_state.depthTestEnable = VK_TRUE;
	depth_stencil_state.depthWriteEnable = VK_TRUE;
	depth_stencil_state.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;

	VkPipelineViewportStateCreateInfo viewport_state = {};
	viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewport_state.viewportCount = 1;
	viewport_state.scissorCount = 1;

	VkPipelineMultisampleStateCreateInfo multisample_state = {};
	multisample_state.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisample_state.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

	VkDynamicState dynamic_states[2] = {
		VK_DYNAMIC_STATE_VIEWPORT,
		VK_DYNAMIC_STATE_SCISSOR
	};

	VkPipelineDynamicStateCreateInfo dynamic_state = {};
	dynamic_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamic_state.pDynamicStates = dynamic_states;
	dynamic_state.dynamicStateCount = 2;

	VkPipelineShaderStageCreateInfo shader_stages[2] = {
		create_shader(adapter, nuklearshaders_nuklear_vert_spv, nuklearshaders_nuklear_vert_spv_len, VK_SHADER_STAGE_VERTEX_BIT),
		create_shader(adapter, nuklearshaders_nuklear_frag_spv, nuklearshaders_nuklear_frag_spv_len, VK_SHADER_STAGE_FRAGMENT_BIT)
	};

	VkVertexInputBindingDescription vertex_input_info[1] = {};
	vertex_input_info[0].binding = 0;
	vertex_input_info[0].stride = sizeof(struct nk_glfw_vertex);
	vertex_input_info[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

	VkVertexInputAttributeDescription vertex_attribute_description[3] = {};
	vertex_attribute_description[0].binding = 0;
	vertex_attribute_description[0].location = 0;
	vertex_attribute_description[0].format = VK_FORMAT_R32G32_SFLOAT;
	vertex_attribute_description[0].offset = NK_OFFSETOF(struct nk_glfw_vertex, position);

	vertex_attribute_description[1].binding = 0;
	vertex_attribute_description[1].location = 1;
	vertex_attribute_description[1].format = VK_FORMAT_R32G32_SFLOAT;
	vertex_attribute_description[1].offset = NK_OFFSETOF(struct nk_glfw_vertex, uv);

	vertex_attribute_description[2].binding = 0;
	vertex_attribute_description[2].location = 2;
	vertex_attribute_description[2].format = VK_FORMAT_R8G8B8A8_UINT;
	vertex_attribute_description[2].offset = NK_OFFSETOF(struct nk_glfw_vertex, col);

	VkPipelineVertexInputStateCreateInfo vertex_input = {};
	vertex_input.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertex_input.vertexBindingDescriptionCount = 1;
	vertex_input.pVertexBindingDescriptions = vertex_input_info;
	vertex_input.vertexAttributeDescriptionCount = 3;
	vertex_input.pVertexAttributeDescriptions = vertex_attribute_description;

	VkGraphicsPipelineCreateInfo pipeline_info = {};
	pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipeline_info.flags = 0;
	pipeline_info.stageCount = 2;
	pipeline_info.pStages = shader_stages;
	pipeline_info.pVertexInputState = &vertex_input;
	pipeline_info.pInputAssemblyState = &input_assembly_state;
	pipeline_info.pViewportState = &viewport_state;
	pipeline_info.pRasterizationState = &rasterization_state;
	pipeline_info.pMultisampleState = &multisample_state;
	pipeline_info.pDepthStencilState = &depth_stencil_state;
	pipeline_info.pColorBlendState = &color_blend_state;
	pipeline_info.pDynamicState = &dynamic_state;
	pipeline_info.layout = adapter->pipeline_layout;
	pipeline_info.renderPass = adapter->render_pass;
	pipeline_info.basePipelineIndex = -1;
	pipeline_info.basePipelineHandle = VK_NULL_HANDLE;

    VkResult result = vkCreateGraphicsPipelines(adapter->logical_device, VK_NULL_HANDLE, 1, &pipeline_info, VK_NULL_HANDLE, &adapter->pipeline);

    vkDestroyShaderModule(adapter->logical_device, shader_stages[0].module, VK_NULL_HANDLE);
    vkDestroyShaderModule(adapter->logical_device, shader_stages[1].module, VK_NULL_HANDLE);
    assert(result == VK_SUCCESS);
}

void prepare_render_pass(struct nk_vulkan_adapter* adapter) {
	vh::vhRenderCreateRenderPass(
		adapter->logical_device,
		adapter->color_format,
		vh::vhDevFindDepthFormat(adapter->physical_device),
		VK_ATTACHMENT_LOAD_OP_LOAD, 
		&adapter->render_pass);
}

void prepare_command_buffers(struct nk_vulkan_adapter* adapter) {
	VkCommandPoolCreateInfo command_pool = {};
	command_pool.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	command_pool.queueFamilyIndex = adapter->graphics_queue_index;
	command_pool.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

	VkResult res = vkCreateCommandPool(adapter->logical_device, &command_pool, NULL, &adapter->command_pool);
    assert( res == VK_SUCCESS);

	VkCommandBufferAllocateInfo allocate_info = {};
	allocate_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocate_info.commandPool = adapter->command_pool;
	allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocate_info.commandBufferCount = 1;
	res = vkAllocateCommandBuffers(adapter->logical_device, &allocate_info, adapter->command_buffers);
    assert( res == VK_SUCCESS);
}

void prepare_semaphores(struct nk_vulkan_adapter *adapter) {
	VkSemaphoreCreateInfo semaphore_info = {};
	semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	VkResult res = vkCreateSemaphore(adapter->logical_device, &semaphore_info, VK_NULL_HANDLE, &adapter->render_completed);
    assert( res == VK_SUCCESS);
}

uint32_t find_memory_index(VkPhysicalDevice physical_device, uint32_t typeFilter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties mem_properties;
    vkGetPhysicalDeviceMemoryProperties(physical_device, &mem_properties);

    for (uint32_t i = 0; i < mem_properties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && (mem_properties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }

    assert(0);
    return 0;
}

void create_buffer_and_memory(struct nk_vulkan_adapter* adapter, VkBuffer* buffer, VkBufferUsageFlags usage, VkDeviceMemory* memory, VkDeviceSize size) {
	VkBufferCreateInfo buffer_info = {};
	buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	buffer_info.size = size;
	buffer_info.usage = usage;
	buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	VkResult res = vkCreateBuffer(adapter->logical_device, &buffer_info, VK_NULL_HANDLE, buffer); // == VK_SUCCESS);

    VkMemoryRequirements mem_reqs;
    vkGetBufferMemoryRequirements(adapter->logical_device, *buffer, &mem_reqs);

	VkMemoryAllocateInfo alloc_info = {};
	alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	alloc_info.allocationSize = mem_reqs.size;
	alloc_info.memoryTypeIndex = find_memory_index(adapter->physical_device, mem_reqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
	res = vkAllocateMemory(adapter->logical_device, &alloc_info, VK_NULL_HANDLE, memory);
    assert( res == VK_SUCCESS);
	res = vkBindBufferMemory(adapter->logical_device, *buffer, *memory, 0);
    assert( res == VK_SUCCESS);
}

NK_API void
nk_glfw3_device_create(VkDevice logical_device, VkPhysicalDevice physical_device, VkQueue graphics_queue, uint32_t graphics_queue_index, VkFramebuffer* framebuffers, uint32_t framebuffers_len, VkFormat color_format, VkFormat depth_format) {
    struct nk_vulkan_adapter *adapter = &glfw.adapter;
    nk_buffer_init_default(&adapter->cmds);
    adapter->logical_device = logical_device;
    adapter->physical_device = physical_device;
    adapter->graphics_queue = graphics_queue,
    adapter->graphics_queue_index = graphics_queue_index;
    adapter->framebuffers = framebuffers;
    adapter->framebuffers_len = framebuffers_len;
    adapter->color_format = color_format;
    adapter->depth_format = depth_format;
    adapter->command_buffers = (VkCommandBuffer*)malloc(framebuffers_len * sizeof(VkCommandBuffer));

    prepare_semaphores(adapter);
    prepare_render_pass(adapter);

    create_buffer_and_memory(adapter, &adapter->vertex_buffer, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, &adapter->vertex_memory, MAX_VERTEX_BUFFER);
    create_buffer_and_memory(adapter, &adapter->index_buffer, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, &adapter->index_memory, MAX_INDEX_BUFFER);
    create_buffer_and_memory(adapter, &adapter->uniform_buffer, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, &adapter->uniform_memory, sizeof(struct Mat4f));

    prepare_descriptor_pool(adapter);
    prepare_descriptor_set_layout(adapter);
    prepare_descriptor_set(adapter);
    prepare_pipeline_layout(adapter);
    prepare_pipeline(adapter);

    prepare_command_buffers(adapter);
}

NK_API void
nk_glfw3_device_destroy(void)
{
    struct nk_vulkan_adapter *adapter = &glfw.adapter;
    free(adapter->command_buffers);
    nk_buffer_free(&adapter->cmds);
}

NK_API void
nk_glfw3_char_callback(GLFWwindow *win, unsigned int codepoint)
{
    (void)win;
    if (glfw.text_len < NK_GLFW_TEXT_MAX)
        glfw.text[glfw.text_len++] = codepoint;
}

NK_API void
nk_gflw3_scroll_callback(GLFWwindow *win, double xoff, double yoff)
{
    (void)win; (void)xoff;
    glfw.scroll.x += (float)xoff;
    glfw.scroll.y += (float)yoff;
}

NK_API void
nk_glfw3_mouse_button_callback(GLFWwindow* window, int button, int action, int mods)
{
    double x, y;
    if (button != GLFW_MOUSE_BUTTON_LEFT) return;
    glfwGetCursorPos(window, &x, &y);
    if (action == GLFW_PRESS)  {
        double dt = glfwGetTime() - glfw.last_button_click;
        if (dt > NK_GLFW_DOUBLE_CLICK_LO && dt < NK_GLFW_DOUBLE_CLICK_HI) {
            glfw.is_double_click_down = nk_true;
            glfw.double_click_pos = nk_vec2((float) x, (float) y);
        }
        glfw.last_button_click = glfwGetTime();
    } else glfw.is_double_click_down = nk_false;
}

NK_INTERN void
nk_glfw3_clipbard_paste(nk_handle usr, struct nk_text_edit *edit)
{
    const char *text = glfwGetClipboardString(glfw.win);
    if (text) nk_textedit_paste(edit, text, nk_strlen(text));
    (void)usr;
}

NK_INTERN void
nk_glfw3_clipbard_copy(nk_handle usr, const char *text, int len)
{
    char *str = 0;
    (void)usr;
    if (!len) return;
    str = (char*)malloc((size_t)len+1);
    if (!str) return;
    memcpy(str, text, (size_t)len);
    str[len] = '\0';
    glfwSetClipboardString(glfw.win, str);
    free(str);
}

NK_API struct nk_context*
nk_glfw3_init(GLFWwindow *win, VkDevice logical_device, VkPhysicalDevice physical_device, VkQueue graphics_queue, uint32_t graphics_queue_index, VkFramebuffer* framebuffers, uint32_t framebuffers_len, VkFormat color_format, VkFormat depth_format, enum nk_glfw_init_state init_state)
{
    glfw.win = win;
    if (init_state == NK_GLFW3_INSTALL_CALLBACKS) {
        glfwSetScrollCallback(win, nk_gflw3_scroll_callback);
        glfwSetCharCallback(win, nk_glfw3_char_callback);
        glfwSetMouseButtonCallback(win, nk_glfw3_mouse_button_callback);
    }
    nk_init_default(&glfw.ctx, 0);
    glfw.ctx.clip.copy = nk_glfw3_clipbard_copy;
    glfw.ctx.clip.paste = nk_glfw3_clipbard_paste;
    glfw.ctx.clip.userdata = nk_handle_ptr(0);
    glfw.last_button_click = 0;
    nk_glfw3_device_create(logical_device, physical_device, graphics_queue, graphics_queue_index, framebuffers, framebuffers_len, color_format, depth_format);

    glfw.is_double_click_down = nk_false;
    glfw.double_click_pos = nk_vec2(0, 0);

    return &glfw.ctx;
}

NK_INTERN void
nk_glfw3_device_upload_atlas(const void *image, int width, int height)
{
    struct nk_vulkan_adapter *adapter = &glfw.adapter;

	VkImageCreateInfo image_info = {};
	image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	image_info.imageType = VK_IMAGE_TYPE_2D;
	image_info.format = VK_FORMAT_R8G8B8A8_UNORM;
	image_info.extent = { (uint32_t)width, (uint32_t)height, 1 };
	image_info.mipLevels = 1;
	image_info.arrayLayers = 1;
	image_info.samples = VK_SAMPLE_COUNT_1_BIT;
	image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
	image_info.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VkResult result = vkCreateImage(adapter->logical_device, &image_info, VK_NULL_HANDLE, &adapter->font_image);
    assert(result == VK_SUCCESS);

    VkMemoryRequirements mem_reqs;
    vkGetImageMemoryRequirements(adapter->logical_device, adapter->font_image, &mem_reqs);
	VkMemoryAllocateInfo alloc_info = {};
	alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	alloc_info.allocationSize = mem_reqs.size;
	alloc_info.memoryTypeIndex = find_memory_index(adapter->physical_device, mem_reqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	result = vkAllocateMemory(adapter->logical_device, &alloc_info, VK_NULL_HANDLE, &adapter->font_memory);
    assert( result == VK_SUCCESS);
	result = vkBindImageMemory(adapter->logical_device, adapter->font_image, adapter->font_memory, 0);
    assert( result == VK_SUCCESS);

    struct {
			VkDeviceMemory memory;
			VkBuffer buffer;
	} staging_buffer = {};

	VkBufferCreateInfo buffer_info = {};
	buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	buffer_info.size = alloc_info.allocationSize;
	buffer_info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
	buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	result = vkCreateBuffer(adapter->logical_device, &buffer_info, VK_NULL_HANDLE, &staging_buffer.buffer);
    assert( result == VK_SUCCESS);
    vkGetBufferMemoryRequirements(adapter->logical_device, staging_buffer.buffer, &mem_reqs);

    alloc_info.allocationSize = mem_reqs.size;
    alloc_info.memoryTypeIndex = find_memory_index(adapter->physical_device, mem_reqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
	result = vkAllocateMemory(adapter->logical_device, &alloc_info, VK_NULL_HANDLE, &staging_buffer.memory);
    assert( result == VK_SUCCESS);
	result = vkBindBufferMemory(adapter->logical_device, staging_buffer.buffer, staging_buffer.memory, 0);
    assert( result == VK_SUCCESS);

    uint8_t* data = 0;
	result = vkMapMemory(adapter->logical_device, staging_buffer.memory, 0, alloc_info.allocationSize, 0, (void**)&data);
    assert( result == VK_SUCCESS);
    memcpy(data, image, width * height * 4);
    vkUnmapMemory(adapter->logical_device, staging_buffer.memory);

    // use the same command buffer as for render as we are regenerating the buffer during render anyway
	VkCommandBufferBeginInfo begin_info = {};
	begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    // TODO: kill array
    VkCommandBuffer command_buffer = adapter->command_buffers[0];
	result = vkBeginCommandBuffer(command_buffer, &begin_info);
    assert( result == VK_SUCCESS);

	VkImageMemoryBarrier image_memory_barrier = {};
	image_memory_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	image_memory_barrier.image = adapter->font_image;
	image_memory_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	image_memory_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	image_memory_barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	image_memory_barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	image_memory_barrier.subresourceRange = {};
	image_memory_barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	image_memory_barrier.subresourceRange.levelCount = 1;
	image_memory_barrier.subresourceRange.layerCount = 1;

	image_memory_barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

	vkCmdPipelineBarrier(
		command_buffer,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        0,
        0, VK_NULL_HANDLE,
        0, VK_NULL_HANDLE,
        1,
        &image_memory_barrier
    );

	VkBufferImageCopy buffer_copy_region = {}; 
	buffer_copy_region.imageSubresource = {};
	buffer_copy_region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	buffer_copy_region.imageSubresource.mipLevel = 0;
	buffer_copy_region.imageSubresource.layerCount = 1;
	buffer_copy_region.imageExtent = { (uint32_t)width, (uint32_t)height, 1 };

    vkCmdCopyBufferToImage(
        command_buffer,
        staging_buffer.buffer,
        adapter->font_image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1,
        &buffer_copy_region
    );

	VkImageMemoryBarrier image_shader_memory_barrier = {};
	image_shader_memory_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	image_shader_memory_barrier.image = adapter->font_image;
	image_shader_memory_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	image_shader_memory_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	image_shader_memory_barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	image_shader_memory_barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	image_shader_memory_barrier.subresourceRange = {};
	image_shader_memory_barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	image_shader_memory_barrier.subresourceRange.levelCount = 1;
	image_shader_memory_barrier.subresourceRange.layerCount = 1;

	image_shader_memory_barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	image_shader_memory_barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

	vkCmdPipelineBarrier(
		command_buffer,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        0,
        0, VK_NULL_HANDLE,
        0, VK_NULL_HANDLE,
        1,
        &image_shader_memory_barrier
    );

	VkResult res = vkEndCommandBuffer(command_buffer);
    assert( res == VK_SUCCESS);

	VkSubmitInfo submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &command_buffer;

	res = vkQueueSubmit(adapter->graphics_queue, 1, &submitInfo, VK_NULL_HANDLE);
	assert( res == VK_SUCCESS);
	res = vkQueueWaitIdle(adapter->graphics_queue);
    assert( res == VK_SUCCESS);

    vkFreeMemory(adapter->logical_device, staging_buffer.memory, VK_NULL_HANDLE);
    vkDestroyBuffer(adapter->logical_device, staging_buffer.buffer, VK_NULL_HANDLE);

	VkImageViewCreateInfo image_view_info = {};
	image_view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	image_view_info.image = adapter->font_image;
	image_view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
	image_view_info.format = image_info.format;
	image_view_info.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

	res = vkCreateImageView(adapter->logical_device, &image_view_info, VK_NULL_HANDLE, &adapter->font_image_view);
    assert( res == VK_SUCCESS);

	VkSamplerCreateInfo sampler_info = {};
	sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	sampler_info.maxAnisotropy = 1.0;
	sampler_info.magFilter = VK_FILTER_LINEAR;
	sampler_info.minFilter = VK_FILTER_LINEAR;
	sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	sampler_info.mipLodBias = 0.0f;
	sampler_info.compareEnable = VK_FALSE;
	sampler_info.compareOp = VK_COMPARE_OP_ALWAYS;
	sampler_info.minLod = 0.0f;
	sampler_info.maxLod = 0.0f;
	sampler_info.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;

	res = vkCreateSampler(adapter->logical_device, &sampler_info, VK_NULL_HANDLE, &adapter->font_tex);
    assert( res == VK_SUCCESS);
}

NK_API void
nk_glfw3_font_stash_begin(struct nk_font_atlas **atlas)
{
    nk_font_atlas_init_default(&glfw.atlas);
    nk_font_atlas_begin(&glfw.atlas);
    *atlas = &glfw.atlas;
}

NK_API void
nk_glfw3_font_stash_end(void)
{
    struct nk_vulkan_adapter* dev = &glfw.adapter;

    const void *image; int w, h;
    image = nk_font_atlas_bake(&glfw.atlas, &w, &h, NK_FONT_ATLAS_RGBA32);
    nk_glfw3_device_upload_atlas(image, w, h);
    nk_font_atlas_end(&glfw.atlas, nk_handle_ptr((void*)dev->descriptor_set), &glfw.adapter.null);
    if (glfw.atlas.default_font) {
        nk_style_set_font(&glfw.ctx, &glfw.atlas.default_font->handle);
    }
    
    update_write_descriptor_sets(dev);
}

NK_API void*
nk_glfw3_add_image(VkSampler sampler, VkImageView image_view)
{
    struct nk_glfw_image_info info = {};
    info.sampler = sampler;
    info.image_view = image_view;
    struct nk_vulkan_adapter* dev = &glfw.adapter;

    prepare_descriptor_set(dev, false);
    update_write_descriptor_sets(dev, &info);
    return (void *)dev->image_descriptor_set[dev->current_image_descriptor_set - 1];
}

NK_API void
nk_glfw3_new_frame()
{
    int i;
    double x, y;
    struct nk_context *ctx = &glfw.ctx;
    struct GLFWwindow *win = glfw.win;

    glfwGetWindowSize(win, &glfw.width, &glfw.height);
    glfwGetFramebufferSize(win, &glfw.display_width, &glfw.display_height);
    glfw.fb_scale.x = (float)glfw.display_width/(float)glfw.width;
    glfw.fb_scale.y = (float)glfw.display_height/(float)glfw.height;

    nk_input_begin(ctx);
    for (i = 0; i < glfw.text_len; ++i)
        nk_input_unicode(ctx, glfw.text[i]);

#if NK_GLFW_GL3_MOUSE_GRABBING
    /* optional grabbing behavior */
    if (ctx->input.mouse.grab)
        glfwSetInputMode(glfw.win, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);
    else if (ctx->input.mouse.ungrab)
        glfwSetInputMode(glfw.win, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
#endif

    nk_input_key(ctx, NK_KEY_DEL, glfwGetKey(win, GLFW_KEY_DELETE) == GLFW_PRESS);
    nk_input_key(ctx, NK_KEY_ENTER, glfwGetKey(win, GLFW_KEY_ENTER) == GLFW_PRESS);
    nk_input_key(ctx, NK_KEY_TAB, glfwGetKey(win, GLFW_KEY_TAB) == GLFW_PRESS);
    nk_input_key(ctx, NK_KEY_BACKSPACE, glfwGetKey(win, GLFW_KEY_BACKSPACE) == GLFW_PRESS);
    nk_input_key(ctx, NK_KEY_UP, glfwGetKey(win, GLFW_KEY_UP) == GLFW_PRESS);
    nk_input_key(ctx, NK_KEY_DOWN, glfwGetKey(win, GLFW_KEY_DOWN) == GLFW_PRESS);
    nk_input_key(ctx, NK_KEY_TEXT_START, glfwGetKey(win, GLFW_KEY_HOME) == GLFW_PRESS);
    nk_input_key(ctx, NK_KEY_TEXT_END, glfwGetKey(win, GLFW_KEY_END) == GLFW_PRESS);
    nk_input_key(ctx, NK_KEY_SCROLL_START, glfwGetKey(win, GLFW_KEY_HOME) == GLFW_PRESS);
    nk_input_key(ctx, NK_KEY_SCROLL_END, glfwGetKey(win, GLFW_KEY_END) == GLFW_PRESS);
    nk_input_key(ctx, NK_KEY_SCROLL_DOWN, glfwGetKey(win, GLFW_KEY_PAGE_DOWN) == GLFW_PRESS);
    nk_input_key(ctx, NK_KEY_SCROLL_UP, glfwGetKey(win, GLFW_KEY_PAGE_UP) == GLFW_PRESS);
    nk_input_key(ctx, NK_KEY_SHIFT, glfwGetKey(win, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS||
                                    glfwGetKey(win, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS);

    if (glfwGetKey(win, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS ||
        glfwGetKey(win, GLFW_KEY_RIGHT_CONTROL) == GLFW_PRESS) {
        nk_input_key(ctx, NK_KEY_COPY, glfwGetKey(win, GLFW_KEY_C) == GLFW_PRESS);
        nk_input_key(ctx, NK_KEY_PASTE, glfwGetKey(win, GLFW_KEY_V) == GLFW_PRESS);
        nk_input_key(ctx, NK_KEY_CUT, glfwGetKey(win, GLFW_KEY_X) == GLFW_PRESS);
        nk_input_key(ctx, NK_KEY_TEXT_UNDO, glfwGetKey(win, GLFW_KEY_Z) == GLFW_PRESS);
        nk_input_key(ctx, NK_KEY_TEXT_REDO, glfwGetKey(win, GLFW_KEY_R) == GLFW_PRESS);
        nk_input_key(ctx, NK_KEY_TEXT_WORD_LEFT, glfwGetKey(win, GLFW_KEY_LEFT) == GLFW_PRESS);
        nk_input_key(ctx, NK_KEY_TEXT_WORD_RIGHT, glfwGetKey(win, GLFW_KEY_RIGHT) == GLFW_PRESS);
        nk_input_key(ctx, NK_KEY_TEXT_LINE_START, glfwGetKey(win, GLFW_KEY_B) == GLFW_PRESS);
        nk_input_key(ctx, NK_KEY_TEXT_LINE_END, glfwGetKey(win, GLFW_KEY_E) == GLFW_PRESS);
    } else {
        nk_input_key(ctx, NK_KEY_LEFT, glfwGetKey(win, GLFW_KEY_LEFT) == GLFW_PRESS);
        nk_input_key(ctx, NK_KEY_RIGHT, glfwGetKey(win, GLFW_KEY_RIGHT) == GLFW_PRESS);
        nk_input_key(ctx, NK_KEY_COPY, 0);
        nk_input_key(ctx, NK_KEY_PASTE, 0);
        nk_input_key(ctx, NK_KEY_CUT, 0);
        nk_input_key(ctx, NK_KEY_SHIFT, 0);
    }

    glfwGetCursorPos(win, &x, &y);
    nk_input_motion(ctx, (int)x, (int)y);
#if NK_GLFW_GL3_MOUSE_GRABBING
    if (ctx->input.mouse.grabbed) {
        glfwSetCursorPos(glfw.win, ctx->input.mouse.prev.x, ctx->input.mouse.prev.y);
        ctx->input.mouse.pos.x = ctx->input.mouse.prev.x;
        ctx->input.mouse.pos.y = ctx->input.mouse.prev.y;
    }
#endif
    nk_input_button(ctx, NK_BUTTON_LEFT, (int)x, (int)y, glfwGetMouseButton(win, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS);
    nk_input_button(ctx, NK_BUTTON_MIDDLE, (int)x, (int)y, glfwGetMouseButton(win, GLFW_MOUSE_BUTTON_MIDDLE) == GLFW_PRESS);
    nk_input_button(ctx, NK_BUTTON_RIGHT, (int)x, (int)y, glfwGetMouseButton(win, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS);
    nk_input_button(ctx, NK_BUTTON_DOUBLE, (int) glfw.double_click_pos.x, (int) glfw.double_click_pos.y, glfw.is_double_click_down);
    nk_input_scroll(ctx, glfw.scroll);
    nk_input_end(&glfw.ctx);
    glfw.text_len = 0;
    glfw.scroll = nk_vec2(0,0);
}

NK_API
VkSemaphore nk_glfw3_render(enum nk_anti_aliasing AA, uint32_t buffer_index, VkSemaphore wait_semaphore) {
    struct nk_vulkan_adapter *adapter = &glfw.adapter;
    struct GLFWwindow *win = glfw.win;
    struct nk_buffer vbuf, ebuf;

	struct Mat4f projection;
	projection = {
            2.0f, 0.0f, 0.0f, 0.0f,
            0.0f, -2.0f, 0.0f, 0.0f,
            0.0f, 0.0f, -1.0f, 0.0f,
            -1.0f, 1.0f, 0.0f, 1.0f
    };
    projection.m[0] /= glfw.width;
    projection.m[5] /= glfw.height;

    void* data;
    vkMapMemory(adapter->logical_device, adapter->uniform_memory, 0, sizeof(projection), 0, &data);
    memcpy(data, &projection, sizeof(projection));
    vkUnmapMemory(adapter->logical_device, adapter->uniform_memory);
    
	VkCommandBufferBeginInfo begin_info = {};
	begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

	VkRenderPassBeginInfo renderPassBeginInfo = {};
	renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderPassBeginInfo.renderPass = adapter->render_pass;
	renderPassBeginInfo.renderArea = {};
	renderPassBeginInfo.renderArea.offset = { 0,0 };
	renderPassBeginInfo.renderArea.extent = { (uint32_t)glfw.display_width, (uint32_t)glfw.display_height };
	renderPassBeginInfo.clearValueCount = 0;
	renderPassBeginInfo.pClearValues = VK_NULL_HANDLE;
	renderPassBeginInfo.framebuffer = adapter->framebuffers[buffer_index];

    VkCommandBuffer command_buffer = adapter->command_buffers[0];
    
	VkResult res = vkBeginCommandBuffer(command_buffer, &begin_info);
    assert( res == VK_SUCCESS);
    vkCmdBeginRenderPass(command_buffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
    
	VkViewport viewport = {};
	viewport.width = (float)glfw.display_width;
	viewport.height = (float)glfw.display_height;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;
    vkCmdSetViewport(command_buffer, 0, 1, &viewport);

    vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, adapter->pipeline);
    {
        /* convert from command queue into draw list and draw to screen */
        const struct nk_draw_command *cmd;
        void *vertices, *elements;

        /* load draw vertices & elements directly into vertex + element buffer */
        vkMapMemory(adapter->logical_device, adapter->vertex_memory, 0, MAX_VERTEX_BUFFER, 0, &vertices);
        vkMapMemory(adapter->logical_device, adapter->index_memory, 0, MAX_INDEX_BUFFER, 0, &elements);
        {
            /* fill convert configuration */
            struct nk_convert_config config;
            static const struct nk_draw_vertex_layout_element vertex_layout[] = {
                {NK_VERTEX_POSITION, NK_FORMAT_FLOAT, NK_OFFSETOF(struct nk_glfw_vertex, position)},
                {NK_VERTEX_TEXCOORD, NK_FORMAT_FLOAT, NK_OFFSETOF(struct nk_glfw_vertex, uv)},
                {NK_VERTEX_COLOR, NK_FORMAT_R8G8B8A8, NK_OFFSETOF(struct nk_glfw_vertex, col)},
                {NK_VERTEX_LAYOUT_END}
            };
            NK_MEMSET(&config, 0, sizeof(config));
            config.vertex_layout = vertex_layout;
            config.vertex_size = sizeof(struct nk_glfw_vertex);
            config.vertex_alignment = NK_ALIGNOF(struct nk_glfw_vertex);
            config.null = adapter->null;
            config.circle_segment_count = 22;
            config.curve_segment_count = 22;
            config.arc_segment_count = 22;
            config.global_alpha = 1.0f;
            config.shape_AA = AA;
            config.line_AA = AA;

            /* setup buffers to load vertices and elements */
            nk_buffer_init_fixed(&vbuf, vertices, (size_t)MAX_VERTEX_BUFFER);
            nk_buffer_init_fixed(&ebuf, elements, (size_t)MAX_INDEX_BUFFER);
            nk_convert(&glfw.ctx, &adapter->cmds, &vbuf, &ebuf, &config);
        }
        vkUnmapMemory(adapter->logical_device, adapter->vertex_memory);
        vkUnmapMemory(adapter->logical_device, adapter->index_memory);

        /* iterate over and execute each draw command */
        VkDeviceSize doffset = 0;
        vkCmdBindVertexBuffers(command_buffer, 0, 1, &adapter->vertex_buffer, &doffset);
        vkCmdBindIndexBuffer(command_buffer, adapter->index_buffer, 0, VK_INDEX_TYPE_UINT16);
        
        uint32_t index_offset = 0;
        nk_draw_foreach(cmd, &glfw.ctx, &adapter->cmds)
        {
            if (!cmd->elem_count) continue;

            vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, adapter->pipeline_layout, 0, 1, (VkDescriptorSet*)&cmd->texture.ptr, 0, VK_NULL_HANDLE);
			VkRect2D scissor = {};
			scissor.extent = { (uint32_t)(cmd->clip_rect.w * glfw.fb_scale.x), (uint32_t)(cmd->clip_rect.h * glfw.fb_scale.y)  };
			scissor.offset = { std::max((int)(cmd->clip_rect.x * glfw.fb_scale.x), (int)0), std::max((int)(cmd->clip_rect.y * glfw.fb_scale.y), (int)0) };
            vkCmdSetScissor(command_buffer, 0, 1, &scissor);
            vkCmdDrawIndexed(command_buffer, cmd->elem_count, 1, index_offset, 0, 0);
            index_offset += cmd->elem_count;
        }
        nk_clear(&glfw.ctx);
    }

    vkCmdEndRenderPass(command_buffer);
	res = vkEndCommandBuffer(command_buffer);
    assert( res == VK_SUCCESS);
    
    VkPipelineStageFlags wait_stages[1] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
	VkSubmitInfo submit_info = {};
	submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submit_info.commandBufferCount = 1;
	submit_info.pCommandBuffers = &command_buffer;
	submit_info.pWaitDstStageMask = wait_stages;
	submit_info.waitSemaphoreCount = 1;
	submit_info.pWaitSemaphores = &wait_semaphore;
	submit_info.signalSemaphoreCount = 1;
	submit_info.pSignalSemaphores = &adapter->render_completed;

	res = vkQueueSubmit(adapter->graphics_queue, 1, &submit_info, VK_NULL_HANDLE);
    assert( res == VK_SUCCESS);
	res = vkQueueWaitIdle(adapter->graphics_queue);
    assert( res == VK_SUCCESS);

    return adapter->render_completed;
}


NK_API
void nk_glfw3_shutdown(void)
{
    struct nk_vulkan_adapter *adapter = &glfw.adapter;
    vkFreeCommandBuffers(adapter->logical_device, adapter->command_pool, 1, adapter->command_buffers);
    vkDestroySemaphore(adapter->logical_device, adapter->render_completed, VK_NULL_HANDLE);

    vkFreeMemory(adapter->logical_device, adapter->vertex_memory, VK_NULL_HANDLE);
    vkFreeMemory(adapter->logical_device, adapter->index_memory, VK_NULL_HANDLE);
    vkFreeMemory(adapter->logical_device, adapter->uniform_memory, VK_NULL_HANDLE);
    vkFreeMemory(adapter->logical_device, adapter->font_memory, VK_NULL_HANDLE);

    vkDestroyBuffer(adapter->logical_device, adapter->vertex_buffer, VK_NULL_HANDLE);
    vkDestroyBuffer(adapter->logical_device, adapter->index_buffer, VK_NULL_HANDLE);
    vkDestroyBuffer(adapter->logical_device, adapter->uniform_buffer, VK_NULL_HANDLE);
    vkDestroyImage(adapter->logical_device, adapter->font_image, VK_NULL_HANDLE);

    vkDestroySampler(adapter->logical_device, adapter->font_tex, VK_NULL_HANDLE);
    vkDestroyImageView(adapter->logical_device, adapter->font_image_view, VK_NULL_HANDLE);

    vkDestroyPipelineLayout(adapter->logical_device, adapter->pipeline_layout, VK_NULL_HANDLE);
    vkDestroyRenderPass(adapter->logical_device, adapter->render_pass, VK_NULL_HANDLE);
    vkDestroyPipeline(adapter->logical_device, adapter->pipeline, VK_NULL_HANDLE);
    
    vkDestroyDescriptorSetLayout(adapter->logical_device, adapter->descriptor_set_layout, VK_NULL_HANDLE);
    vkDestroyDescriptorPool(adapter->logical_device, adapter->descriptor_pool, VK_NULL_HANDLE);
    vkDestroyCommandPool(adapter->logical_device, adapter->command_pool, VK_NULL_HANDLE);

    nk_font_atlas_clear(&glfw.atlas);
    nk_free(&glfw.ctx);
    nk_glfw3_device_destroy();
    memset(&glfw, 0, sizeof(glfw));
}

#endif
