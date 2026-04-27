module;

#include <cctype>
#include <cmath>
#include <cstddef>
#include "FacadeMacros.hpp"
#include <cstdlib>
#include <cstring>
#include <SDL3/SDL_vulkan.h>
#include <vulkan/vulkan.h>
#include <vulkan/vk_enum_string_helper.h>

module VEEngine.V3;
import std;
import :Internal;

/**
 * @file
 * @brief v3 graphics-backend implementation.
 *
 * The backend currently models only frame-boundary lifecycle and task
 * registration, but it owns the seam where concrete Vulkan work will live.
 */
namespace vve::v3 {

   namespace {

      static_assert(std::is_standard_layout_v<ImportedVertex>,
                    "ImportedVertex must stay standard-layout so Vulkan vertex attributes can use offsetof.");
      static_assert(sizeof(ImportedVertex) <= static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max()),
                    "ImportedVertex is too large for VkVertexInputBindingDescription::stride.");

      constexpr std::uint32_t maxDrawDescriptorSetCopies = 256;

      /// @brief Converts ImportedVertex member offsets to Vulkan's 32-bit vertex attribute offset type.
      [[nodiscard]] constexpr std::uint32_t vertexOffset(std::size_t offset) {
         return static_cast<std::uint32_t>(offset);
      }

      /// @brief Converts a packed Vulkan version to major.minor.patch text.
      [[nodiscard]] std::string versionString(const std::uint32_t version) {
         return std::format("{}.{}.{}", VK_API_VERSION_MAJOR(version), VK_API_VERSION_MINOR(version),
                            VK_API_VERSION_PATCH(version));
      }

      /// @brief Reads an environment variable for diagnostics without mutating loader state.
      [[nodiscard]] std::string environmentValue(const char *name) {
         if (name == nullptr) {
            return {};
         }

         const char *value = std::getenv(name);
         return value == nullptr ? std::string{} : std::string(value);
      }

      /// @brief Returns whether an ASCII character should be trimmed from renderer ids.
      [[nodiscard]] bool isAsciiSpace(unsigned char character) {
         return std::isspace(character) != 0;
      }

      /// @brief Normalizes user-facing renderer ids for stable backend lookup.
      [[nodiscard]] std::string normalizeRendererId(std::string_view renderer_id) {
         while (!renderer_id.empty() && isAsciiSpace(static_cast<unsigned char>(renderer_id.front()))) {
            renderer_id.remove_prefix(1);
         }
         while (!renderer_id.empty() && isAsciiSpace(static_cast<unsigned char>(renderer_id.back()))) {
            renderer_id.remove_suffix(1);
         }

         std::string normalized{};
         normalized.reserve(renderer_id.size());
         for (const char character : renderer_id) {
            if (character == '-') {
               normalized.push_back('_');
            } else {
               normalized.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(character))));
            }
         }
         return normalized;
      }

      /// @brief Lowercases ASCII text for lightweight reflected-name matching.
      [[nodiscard]] std::string asciiLower(std::string_view text) {
         std::string lowered{};
         lowered.reserve(text.size());
         for (const char character : text) {
            lowered.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(character))));
         }
         return lowered;
      }

      /// @brief Returns whether reflected text contains a case-insensitive token.
      [[nodiscard]] bool containsAsciiToken(std::string_view text, std::string_view token) {
         return asciiLower(text).find(asciiLower(token)) != std::string::npos;
      }

      /// @brief Returns whether two reflected bindings identify the same descriptor slot.
      [[nodiscard]] bool sameDescriptorBinding(const PipelineDescriptorBindingDesc &left,
                                               const PipelineDescriptorBindingDesc &right) {
         return left.set == right.set && left.binding == right.binding && left.kind == right.kind;
      }

      /// @brief Returns whether a reflected uniform buffer represents material constants.
      [[nodiscard]] bool isMaterialConstantsBinding(const PipelineDescriptorBindingDesc &binding) {
         return binding.kind == DescriptorBindingKind::uniform_buffer &&
                (containsAsciiToken(binding.name, "material") || containsAsciiToken(binding.type_name, "Material"));
      }

      /// @brief Maps texture/sampler binding names to imported material texture semantics.
      [[nodiscard]] std::optional<TextureSemantic> textureSemanticForBinding(const PipelineDescriptorBindingDesc &binding) {
         if (containsAsciiToken(binding.name, "baseColor") || containsAsciiToken(binding.name, "diffuse")) {
            return TextureSemantic::base_color;
         }
         if (containsAsciiToken(binding.name, "normal")) {
            return TextureSemantic::normal;
         }
         if (containsAsciiToken(binding.name, "metallicRoughness") ||
             (containsAsciiToken(binding.name, "metallic") && containsAsciiToken(binding.name, "roughness"))) {
            return TextureSemantic::metallic_roughness;
         }
         if (containsAsciiToken(binding.name, "roughness")) {
            return TextureSemantic::roughness;
         }
         if (containsAsciiToken(binding.name, "metallic")) {
            return TextureSemantic::metallic;
         }
         if (containsAsciiToken(binding.name, "specular")) {
            return TextureSemantic::specular;
         }
         if (containsAsciiToken(binding.name, "emissive")) {
            return TextureSemantic::emissive;
         }
         if (containsAsciiToken(binding.name, "opacity") || containsAsciiToken(binding.name, "alpha")) {
            return TextureSemantic::opacity;
         }
         if (containsAsciiToken(binding.name, "occlusion") || containsAsciiToken(binding.name, "ambient")) {
            return TextureSemantic::ambient_occlusion;
         }

         return std::nullopt;
      }

      /// @brief Finds the material texture intended for a reflected texture or sampler binding.
      [[nodiscard]] const GpuMaterialTextureBinding *
      materialTextureForBinding(const DrawPacket &packet, const PipelineDescriptorBindingDesc &binding) {
         const auto semantic = textureSemanticForBinding(binding);
         if (!semantic.has_value()) {
            return nullptr;
         }

         for (const auto &texture : packet.material_textures) {
            if (texture.semantic == *semantic && texture.image.value.isValid() && texture.sampler.value.isValid()) {
               return std::addressof(texture);
            }
         }

         return nullptr;
      }

      /// @brief Builds a cache key for one material/texture descriptor-set payload.
      [[nodiscard]] std::string drawDescriptorSetKey(const DrawPacket &packet) {
         auto key = std::string{"material:"};
         key += std::to_string(packet.material.value.value());
         key.push_back(':');
         key += std::to_string(packet.material_constants_buffer.value.value());
         for (const auto &texture : packet.material_textures) {
            key.push_back('|');
            key += std::to_string(static_cast<std::uint32_t>(texture.semantic));
            key.push_back(':');
            key += std::to_string(texture.binding);
            key.push_back(':');
            key += std::to_string(texture.uv_set);
            key.push_back(':');
            key += std::to_string(texture.image.value.value());
            key.push_back(':');
            key += std::to_string(texture.sampler.value.value());
         }
         return key;
      }

      /// @brief Stable renderer handle derived from the canonical backend id.
      [[nodiscard]] RendererHandle rendererHandle(std::string_view renderer_id) {
         return RendererHandle{.value = vve::Handle::fromHash(std::format("vulkan.renderer.{}", renderer_id))};
      }

      /// @brief Canonical renderer descriptors currently known to the Vulkan backend.
      [[nodiscard]] const std::map<std::string_view, RendererDesc> &rendererRegistry() {
         static const std::map<std::string_view, RendererDesc> renderers{
             {"forward",
              RendererDesc{.handle = rendererHandle("forward"),
                           .id = "forward",
                           .display_name = "Forward Renderer",
                           .api = vve::GraphicsApi::vulkan,
                           .kind = vve::RendererKind::forward_renderer,
                           .main_kernel = RenderKernelId::forward_opaque}},
             {"deferred",
              RendererDesc{.handle = rendererHandle("deferred"),
                           .id = "deferred",
                           .display_name = "Deferred Renderer",
                           .api = vve::GraphicsApi::vulkan,
                           .kind = vve::RendererKind::deferred_renderer,
                           .main_kernel = RenderKernelId::deferred_gbuffer}},
             {"path_tracing",
              RendererDesc{.handle = rendererHandle("path_tracing"),
                           .id = "path_tracing",
                           .display_name = "Path Tracing Renderer",
                           .api = vve::GraphicsApi::vulkan,
                           .kind = vve::RendererKind::path_tracing,
                           .main_kernel = RenderKernelId::path_trace}},
         };
         return renderers;
      }

      /// @brief Accepted aliases for renderer ids exposed to applications and launch scripts.
      [[nodiscard]] const std::map<std::string_view, std::string_view> &rendererAliasMap() {
         static const std::map<std::string_view, std::string_view> aliases{
             {"forward", "forward"},
             {"forward_renderer", "forward"},
             {"deferred", "deferred"},
             {"deferred_renderer", "deferred"},
             {"path_tracer", "path_tracing"},
             {"path_tracing", "path_tracing"},
             {"path_tracing_renderer", "path_tracing"},
         };
         return aliases;
      }

      /// @brief Maps Slang reflection binding labels to backend descriptor categories.
      [[nodiscard]] const std::map<std::string_view, DescriptorBindingKind> &descriptorBindingKindMap() {
         static const std::map<std::string_view, DescriptorBindingKind> kinds{
             {"acceleration_structure", DescriptorBindingKind::acceleration_structure},
             {"combined_texture_sampler", DescriptorBindingKind::sampled_image},
             {"constant_buffer", DescriptorBindingKind::uniform_buffer},
             {"descriptor_table_slot", DescriptorBindingKind::parameter_block},
             {"input_render_target", DescriptorBindingKind::input_attachment},
             {"parameter_block", DescriptorBindingKind::parameter_block},
             {"push_constant", DescriptorBindingKind::push_constant},
             {"push_constant_buffer", DescriptorBindingKind::push_constant},
             {"raw_buffer", DescriptorBindingKind::storage_buffer},
             {"ray_tracing_acceleration_structure", DescriptorBindingKind::acceleration_structure},
             {"register_space", DescriptorBindingKind::parameter_block},
             {"sampler", DescriptorBindingKind::sampler},
             {"sampler_state", DescriptorBindingKind::sampler},
             {"shader_resource", DescriptorBindingKind::sampled_image},
             {"sub_element_register_space", DescriptorBindingKind::parameter_block},
             {"texture", DescriptorBindingKind::sampled_image},
             {"typed_buffer", DescriptorBindingKind::storage_buffer},
             {"unordered_access", DescriptorBindingKind::storage_buffer},
         };
         return kinds;
      }

      /// @brief Converts a reflected binding label to the backend category enum.
      [[nodiscard]] DescriptorBindingKind descriptorBindingKind(std::string_view reflected_kind) {
         return vve::detail::mapValueOr(descriptorBindingKindMap(), reflected_kind, DescriptorBindingKind::unknown);
      }

      /// @brief Refines Slang parameter-block entries into concrete Vulkan descriptor categories.
      [[nodiscard]] DescriptorBindingKind descriptorBindingKind(const ShaderParameter &parameter) {
         const auto reflected_kind = descriptorBindingKind(parameter.binding_kind);
         if (reflected_kind != DescriptorBindingKind::parameter_block) {
            return reflected_kind;
         }

         const std::string_view type_name{parameter.type_name};
         if (type_name.find("ConstantBuffer") != std::string_view::npos) {
            return DescriptorBindingKind::uniform_buffer;
         }
         if (type_name.find("Sampler") != std::string_view::npos) {
            return DescriptorBindingKind::sampler;
         }
         if (type_name.find("Texture") != std::string_view::npos) {
            return DescriptorBindingKind::sampled_image;
         }
         if (type_name.find("Buffer") != std::string_view::npos) {
            return DescriptorBindingKind::storage_buffer;
         }

         return reflected_kind;
      }

      /// @brief Maps a user renderer id to a canonical registry key.
      [[nodiscard]] std::optional<std::string_view> canonicalRendererId(std::string_view renderer_id) {
         const auto normalized = normalizeRendererId(renderer_id);
         const std::string_view normalized_view{normalized};
         const auto alias = rendererAliasMap().find(normalized_view);
         if (alias == rendererAliasMap().end()) {
            return std::nullopt;
         }
         return alias->second;
      }

      /// @brief Appends stage entry points from shader reflection to a pipeline layout.
      [[nodiscard]] std::expected<void, vve::Error> appendPipelineStages(PipelineLayoutDesc &layout,
                                                                         const ShaderMetadata &shader) {
         if (!shader.handle.value.isValid() || shader.binaries.empty() || shader.stages.empty()) {
            return std::unexpected(vve::Error::invalid_argument);
         }

         layout.shader_stages.reserve(shader.binaries.size());
         for (const auto &binary : shader.binaries) {
            if (binary.entry_point.empty() || binary.spirv_words.empty()) {
               return std::unexpected(vve::Error::invalid_argument);
            }

            layout.shader_stages.push_back(PipelineShaderStageDesc{
                .stage = binary.stage,
                .entry_point = binary.entry_point,
                .spirv_word_count = binary.spirv_words.size()});
         }

         std::ranges::sort(layout.shader_stages, {}, &PipelineShaderStageDesc::stage);
         return {};
      }

      /// @brief Appends reflected descriptor bindings grouped by descriptor set.
      void appendPipelineBindings(PipelineLayoutDesc &layout, const ShaderMetadata &shader) {
         std::map<std::uint32_t, PipelineDescriptorSetLayoutDesc> descriptor_sets{};
         for (const auto &parameter : shader.parameters) {
            const auto kind = descriptorBindingKind(parameter);
            if (kind == DescriptorBindingKind::push_constant) {
               layout.push_constants.push_back(PipelinePushConstantRangeDesc{
                   .name = parameter.name,
                   .type_name = parameter.type_name,
                   .visible_stages = shader.stages});
               continue;
            }

            auto &set_layout = descriptor_sets[parameter.set];
            set_layout.set = parameter.set;
            set_layout.bindings.push_back(PipelineDescriptorBindingDesc{
                .set = parameter.set,
                .binding = parameter.binding,
                .kind = kind,
                .name = parameter.name,
                .type_name = parameter.type_name,
                .visible_stages = shader.stages});
         }

         layout.descriptor_sets.reserve(descriptor_sets.size());
         for (auto &[set, set_layout] : descriptor_sets) {
            (void)set;
            std::ranges::sort(set_layout.bindings, [](const auto &lhs, const auto &rhs) {
               return std::tie(lhs.binding, lhs.name, lhs.type_name) < std::tie(rhs.binding, rhs.name, rhs.type_name);
            });
            layout.descriptor_sets.push_back(std::move(set_layout));
         }

         std::ranges::sort(layout.push_constants, {}, &PipelinePushConstantRangeDesc::name);
      }

      /// @brief Builds the validated backend-facing layout plan for a renderer/shader pair.
      [[nodiscard]] std::expected<PipelineLayoutDesc, vve::Error>
      buildPipelineLayout(const RendererDesc &renderer, const ShaderMetadata &shader) {
         if (!renderer.handle.value.isValid() || renderer.api != vve::GraphicsApi::vulkan ||
             renderer.id.empty() || shader.intended_renderer != vve::rendererKindName(renderer.kind)) {
            return std::unexpected(vve::Error::invalid_argument);
         }

         PipelineLayoutDesc layout{.renderer = renderer.handle,
                                   .renderer_id = renderer.id,
                                   .shader_program = shader.handle};
         if (auto stage_result = appendPipelineStages(layout, shader); !stage_result) {
            return std::unexpected(stage_result.error());
         }

         appendPipelineBindings(layout, shader);
         return layout;
      }

      /// @brief Backend-owned shader module plus the entry-point metadata required by VkPipeline creation.
      struct VulkanShaderModule {
         VkShaderModule module{VK_NULL_HANDLE};
         ShaderStage stage{ShaderStage::vertex};
         std::string entry_point{};
      };

      /// @brief Backend-owned fallback buffer used to satisfy reflected descriptor bindings.
      struct VulkanDescriptorBufferResource {
         VkBuffer buffer{VK_NULL_HANDLE};
         VkDeviceMemory memory{VK_NULL_HANDLE};
         VkDeviceSize size{0};
         VkDescriptorType descriptor_type{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER};
         PipelineDescriptorBindingDesc binding{};
      };

      /// @brief Backend-owned fallback sampled image used before real texture upload exists.
      struct VulkanDescriptorImageResource {
         VkImage image{VK_NULL_HANDLE};
         VkDeviceMemory memory{VK_NULL_HANDLE};
         VkImageView image_view{VK_NULL_HANDLE};
         VkImageLayout layout{VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
         PipelineDescriptorBindingDesc binding{};
      };

      /// @brief Backend-owned fallback sampler used before real material samplers are available.
      struct VulkanDescriptorSamplerResource {
         VkSampler sampler{VK_NULL_HANDLE};
         PipelineDescriptorBindingDesc binding{};
      };

      /// @brief Backend-owned Vulkan objects created for one reflected pipeline layout.
      struct VulkanPipelineResources {
         PipelineBackendResources summary{};
         std::vector<VulkanShaderModule> shader_modules{};
         std::vector<VkDescriptorSetLayout> descriptor_set_layouts{};
         std::vector<std::vector<PipelineDescriptorBindingDesc>> descriptor_bindings{};
         VkDescriptorPool descriptor_pool{VK_NULL_HANDLE};
         std::vector<VkDescriptorSet> descriptor_sets{};
         std::vector<VulkanDescriptorBufferResource> descriptor_buffers{};
         std::vector<VulkanDescriptorImageResource> descriptor_images{};
         std::vector<VulkanDescriptorSamplerResource> descriptor_samplers{};
         std::unordered_map<std::string, std::vector<VkDescriptorSet>> draw_descriptor_sets{};
         VkPipelineLayout pipeline_layout{VK_NULL_HANDLE};
      };

      /// @brief Backend-owned state for one created VkPipeline.
      struct VulkanGraphicsPipelineResources {
         GraphicsPipelineResources summary{};
         VkRenderPass render_pass{VK_NULL_HANDLE};
         VkPipeline pipeline{VK_NULL_HANDLE};
      };

      /// @brief Backend-owned Vulkan buffer and its bound device memory.
      struct VulkanGpuBufferResources {
         GpuBufferResources summary{};
         VkBuffer buffer{VK_NULL_HANDLE};
         VkDeviceMemory memory{VK_NULL_HANDLE};
      };

      /// @brief Backend-owned Vulkan sampled image, view, sampler, and memory.
      struct VulkanGpuImageResources {
         GpuTextureResources summary{};
         VkImage image{VK_NULL_HANDLE};
         VkDeviceMemory memory{VK_NULL_HANDLE};
         VkImageView image_view{VK_NULL_HANDLE};
         VkSampler sampler{VK_NULL_HANDLE};
         VkImageLayout layout{VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
      };

      /// @brief Per-frame synchronization objects used by one window swapchain.
      struct VulkanSwapchainFrameSync {
         VkSemaphore image_available{VK_NULL_HANDLE};
         VkSemaphore render_finished{VK_NULL_HANDLE};
         VkFence render_fence{VK_NULL_HANDLE};
      };

      /// @brief Backend-owned Vulkan presentation resources for one SDL window.
      struct VulkanWindowSwapchainResources {
         WindowSwapchainResources summary{};
         VkSurfaceKHR surface{VK_NULL_HANDLE};
         VkSwapchainKHR swapchain{VK_NULL_HANDLE};
         VkFormat format{VK_FORMAT_UNDEFINED};
         VkPresentModeKHR present_mode{VK_PRESENT_MODE_FIFO_KHR};
         VkExtent2D extent{};
         std::vector<VkImage> images{};
         std::vector<VkImageView> image_views{};
         VkFormat depth_format{VK_FORMAT_UNDEFINED};
         VkImage depth_image{VK_NULL_HANDLE};
         VkDeviceMemory depth_memory{VK_NULL_HANDLE};
         VkImageView depth_image_view{VK_NULL_HANDLE};
         VkRenderPass clear_render_pass{VK_NULL_HANDLE};
         std::vector<VkFramebuffer> framebuffers{};
         std::vector<VkCommandBuffer> command_buffers{};
         std::vector<VulkanSwapchainFrameSync> frame_sync{};
         std::uint32_t current_frame_slot{0};
         std::uint32_t active_frame_slot{0};
         std::uint32_t active_image_index{0};
         bool frame_acquired{false};
         bool command_recorded{false};
         bool frame_submitted{false};
      };

      /// @brief Physical device and queue family selected for backend object creation.
      struct SelectedPhysicalDevice {
         VkPhysicalDevice device{VK_NULL_HANDLE};
         std::uint32_t graphics_queue_family{0};
      };

      /// @brief Builds a stable resource-bundle handle for one renderer/shader pair.
      [[nodiscard]] PipelineResourceHandle pipelineResourceHandle(const PipelineLayoutDesc &layout) {
         auto seed = std::string{"vulkan.pipeline_resources:"};
         seed += layout.renderer_id;
         seed.push_back(':');
         seed += std::to_string(layout.shader_program.value.value());
         return PipelineResourceHandle{.value = vve::Handle::fromHash(seed)};
      }

      /// @brief Builds a stable graphics-pipeline handle from renderer-selected state.
      [[nodiscard]] GraphicsPipelineHandle graphicsPipelineHandle(const GraphicsPipelineDesc &desc) {
         auto seed = std::string{"vulkan.graphics_pipeline:"};
         seed += desc.renderer_id;
         seed.push_back(':');
         seed += std::to_string(desc.backend_resources.value.value());
         seed.push_back(':');
         seed += std::to_string(static_cast<std::uint32_t>(desc.main_kernel));
         seed.push_back(':');
         seed += std::to_string(desc.color_attachment_count);
         seed.push_back(':');
         seed += std::to_string(static_cast<std::uint32_t>(desc.color_format));
         seed.push_back(':');
         seed += std::to_string(static_cast<std::uint32_t>(desc.depth_format));
         return GraphicsPipelineHandle{.value = vve::Handle::fromHash(seed)};
      }

      /// @brief Builds a stable GPU-buffer handle from owner, usage, and uploaded generation.
      [[nodiscard]] GpuBufferHandle gpuBufferHandle(vve::Handle owner, ResourceKind owner_kind,
                                                    GpuBufferUsage usage, std::size_t byte_size,
                                                    std::uint32_t generation) {
         auto seed = std::string{"vulkan.buffer:"};
         seed += std::to_string(owner.value());
         seed.push_back(':');
         seed += std::to_string(static_cast<std::uint32_t>(owner_kind));
         seed.push_back(':');
         seed += std::to_string(static_cast<std::uint32_t>(usage));
         seed.push_back(':');
         seed += std::to_string(byte_size);
         seed.push_back(':');
         seed += std::to_string(generation);
         return GpuBufferHandle{.value = vve::Handle::fromHash(seed)};
      }

      /// @brief Builds a stable GPU-image handle from owner, format, dimensions, and uploaded generation.
      [[nodiscard]] GpuImageHandle gpuImageHandle(vve::Handle owner, ResourceKind owner_kind, GpuImageUsage usage,
                                                  GpuImageFormat format, std::uint32_t width,
                                                  std::uint32_t height, std::uint32_t generation) {
         auto seed = std::string{"vulkan.image:"};
         seed += std::to_string(owner.value());
         seed.push_back(':');
         seed += std::to_string(static_cast<std::uint32_t>(owner_kind));
         seed.push_back(':');
         seed += std::to_string(static_cast<std::uint32_t>(usage));
         seed.push_back(':');
         seed += std::to_string(static_cast<std::uint32_t>(format));
         seed.push_back(':');
         seed += std::to_string(width);
         seed.push_back(':');
         seed += std::to_string(height);
         seed.push_back(':');
         seed += std::to_string(generation);
         return GpuImageHandle{.value = vve::Handle::fromHash(seed)};
      }

      /// @brief Builds a stable sampler handle paired with one image upload generation.
      [[nodiscard]] GpuSamplerHandle gpuSamplerHandle(vve::Handle owner, GpuImageHandle image,
                                                      std::uint32_t generation) {
         auto seed = std::string{"vulkan.sampler:"};
         seed += std::to_string(owner.value());
         seed.push_back(':');
         seed += std::to_string(image.value.value());
         seed.push_back(':');
         seed += std::to_string(generation);
         return GpuSamplerHandle{.value = vve::Handle::fromHash(seed)};
      }

      /// @brief Builds a stable swapchain-resource handle for one window.
      [[nodiscard]] SwapchainHandle swapchainHandle(const NativeWindowHandle &window) {
         auto seed = std::string{"vulkan.swapchain:"};
         seed += window.window_id;
         seed.push_back(':');
         seed += std::to_string(window.window.value.value());
         return SwapchainHandle{.value = vve::Handle::fromHash(seed)};
      }

      /// @brief Returns whether a pipeline description matches the binding it was requested from.
      [[nodiscard]] bool graphicsPipelineDescMatchesBinding(const RendererPipelineBinding &binding,
                                                            const GraphicsPipelineDesc &desc) {
         return binding.ready_for_pipeline_creation && binding.renderer.value == desc.renderer.value &&
                binding.renderer_id == desc.renderer_id &&
                binding.backend_resources.value == desc.backend_resources.value &&
                binding.main_kernel == desc.main_kernel && desc.color_attachment_count > 0;
      }

      /// @brief Validates renderer-specific graphics pipeline state before later VkPipeline creation.
      [[nodiscard]] bool graphicsPipelineDescMatchesRenderer(const GraphicsPipelineDesc &desc) {
         if (desc.renderer_id == "forward") {
            return desc.main_kernel == RenderKernelId::forward_opaque &&
                   desc.topology == GraphicsPrimitiveTopology::triangle_list &&
                   desc.cull_mode == GraphicsCullMode::back && desc.depth_test_enabled &&
                   desc.depth_write_enabled && desc.depth_format != GraphicsDepthFormat::none &&
                   desc.color_attachment_count == 1 && desc.vertex_binding_count == 1 &&
                   desc.vertex_attribute_count >= 5;
         }

         if (desc.renderer_id == "deferred") {
            return desc.main_kernel == RenderKernelId::deferred_gbuffer &&
                   desc.topology == GraphicsPrimitiveTopology::triangle_list &&
                   desc.depth_test_enabled && desc.depth_write_enabled &&
                   desc.depth_format != GraphicsDepthFormat::none && desc.color_attachment_count >= 3 &&
                   desc.vertex_binding_count == 1 && desc.vertex_attribute_count >= 5;
         }

         if (desc.renderer_id == "path_tracing") {
            return desc.main_kernel == RenderKernelId::path_trace &&
                   desc.topology == GraphicsPrimitiveTopology::triangle_list &&
                   desc.cull_mode == GraphicsCullMode::none && !desc.depth_test_enabled &&
                   !desc.depth_write_enabled && desc.depth_format == GraphicsDepthFormat::none &&
                   desc.color_attachment_count == 1 && desc.vertex_binding_count == 0 &&
                   desc.vertex_attribute_count == 0;
         }

         return false;
      }

      /// @brief Converts reflected shader stages to Vulkan shader-stage flags.
      [[nodiscard]] VkShaderStageFlags shaderStageFlags(const std::vector<ShaderStage> &stages) {
         VkShaderStageFlags flags = 0;
         for (const auto stage : stages) {
            switch (stage) {
            case ShaderStage::vertex:
               flags |= VK_SHADER_STAGE_VERTEX_BIT;
               break;
            case ShaderStage::fragment:
               flags |= VK_SHADER_STAGE_FRAGMENT_BIT;
               break;
            case ShaderStage::compute:
               flags |= VK_SHADER_STAGE_COMPUTE_BIT;
               break;
            }
         }

         return flags;
      }

      /// @brief Converts one reflected shader stage to a Vulkan graphics-pipeline stage bit.
      [[nodiscard]] std::optional<VkShaderStageFlagBits> shaderStageFlagBit(ShaderStage stage) {
         switch (stage) {
         case ShaderStage::vertex:
            return VK_SHADER_STAGE_VERTEX_BIT;
         case ShaderStage::fragment:
            return VK_SHADER_STAGE_FRAGMENT_BIT;
         case ShaderStage::compute:
            return std::nullopt;
         }

         return std::nullopt;
      }

      /// @brief Converts a reflected descriptor category to a Vulkan descriptor type.
      [[nodiscard]] std::optional<VkDescriptorType> descriptorType(DescriptorBindingKind kind) {
         switch (kind) {
         case DescriptorBindingKind::uniform_buffer:
            return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
         case DescriptorBindingKind::sampled_image:
            return VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
         case DescriptorBindingKind::sampler:
            return VK_DESCRIPTOR_TYPE_SAMPLER;
         case DescriptorBindingKind::storage_buffer:
            return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
         case DescriptorBindingKind::input_attachment:
            return VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
         case DescriptorBindingKind::acceleration_structure:
#ifdef VK_KHR_acceleration_structure
            return VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
#else
            return std::nullopt;
#endif
         case DescriptorBindingKind::unknown:
         case DescriptorBindingKind::parameter_block:
         case DescriptorBindingKind::push_constant:
            return std::nullopt;
         }

         return std::nullopt;
      }

      /// @brief CPU-side layout matching rasterizer.slang FrameConstants.
      struct VulkanFrameConstants {
         std::array<float, 16> model{};
         std::array<float, 16> view{};
         std::array<float, 16> projection{};
      };

      static_assert(sizeof(VulkanFrameConstants) == sizeof(float) * 48U);

      /// @brief Returns whether a reflected uniform buffer represents per-frame camera data.
      [[nodiscard]] bool isFrameConstantsBinding(const PipelineDescriptorBindingDesc &binding) {
         return binding.kind == DescriptorBindingKind::uniform_buffer &&
                (containsAsciiToken(binding.name, "frame") || containsAsciiToken(binding.type_name, "Frame"));
      }

      /// @brief Builds a column-major identity matrix payload for Slang float4x4 constants.
      [[nodiscard]] std::array<float, 16> identityMatrixPayload() {
         return std::array<float, 16>{1.0F, 0.0F, 0.0F, 0.0F,
                                      0.0F, 1.0F, 0.0F, 0.0F,
                                      0.0F, 0.0F, 1.0F, 0.0F,
                                      0.0F, 0.0F, 0.0F, 1.0F};
      }

      /// @brief Builds a column-major translation matrix payload.
      [[nodiscard]] std::array<float, 16> translationMatrixPayload(float x, float y, float z) {
         return std::array<float, 16>{1.0F, 0.0F, 0.0F, 0.0F,
                                      0.0F, 1.0F, 0.0F, 0.0F,
                                      0.0F, 0.0F, 1.0F, 0.0F,
                                      x,    y,    z,    1.0F};
      }

      /// @brief Builds a Vulkan clip-space perspective projection matrix payload.
      [[nodiscard]] std::array<float, 16> perspectiveMatrixPayload(float aspect_ratio) {
         constexpr float pi = 3.14159265358979323846F;
         constexpr float vertical_field_of_view = 60.0F * pi / 180.0F;
         constexpr float near_plane = 0.1F;
         constexpr float far_plane = 10000.0F;
         const float f = 1.0F / std::tan(vertical_field_of_view * 0.5F);
         const float safe_aspect = std::max(aspect_ratio, 0.001F);

         return std::array<float, 16>{f / safe_aspect, 0.0F, 0.0F, 0.0F,
                                      0.0F, -f, 0.0F, 0.0F,
                                      0.0F, 0.0F, far_plane / (near_plane - far_plane), -1.0F,
                                      0.0F, 0.0F, (far_plane * near_plane) / (near_plane - far_plane), 0.0F};
      }

      /// @brief Builds the default camera constants for the current swapchain extent.
      [[nodiscard]] VulkanFrameConstants frameConstantsForExtent(VkExtent2D extent) {
         const auto width = static_cast<float>(std::max(extent.width, 1U));
         const auto height = static_cast<float>(std::max(extent.height, 1U));
         return VulkanFrameConstants{.model = identityMatrixPayload(),
                                     .view = translationMatrixPayload(0.0F, -1.5F, -6.0F),
                                     .projection = perspectiveMatrixPayload(width / height)};
      }

      /// @brief Copies frame constants into a byte span when the descriptor buffer is large enough.
      [[nodiscard]] bool writeFrameConstants(std::span<std::byte> bytes, const VulkanFrameConstants &constants) {
         if (bytes.size() < sizeof(constants)) {
            return false;
         }

         std::memcpy(bytes.data(), &constants, sizeof(constants));
         return true;
      }

      /// @brief Builds conservative fallback bytes for reflected buffer descriptors.
      [[nodiscard]] std::vector<std::byte> fallbackDescriptorBufferBytes(const PipelineDescriptorBindingDesc &binding) {
         std::vector<std::byte> bytes(1024, std::byte{0});
         const auto write_float = [&bytes](std::size_t offset, float value) {
            if (offset + sizeof(value) <= bytes.size()) {
               std::memcpy(bytes.data() + offset, &value, sizeof(value));
            }
         };
         const auto write_uint = [&bytes](std::size_t offset, std::uint32_t value) {
            if (offset + sizeof(value) <= bytes.size()) {
               std::memcpy(bytes.data() + offset, &value, sizeof(value));
            }
         };
         const auto write_identity = [&write_float](std::size_t offset) {
            for (std::size_t row = 0; row < 4; ++row) {
               for (std::size_t column = 0; column < 4; ++column) {
                  write_float(offset + ((row * 4U + column) * sizeof(float)), row == column ? 1.0F : 0.0F);
               }
            }
         };

         if (isFrameConstantsBinding(binding)) {
            [[maybe_unused]] const bool frame_written =
                writeFrameConstants(bytes, frameConstantsForExtent(VkExtent2D{.width = 1, .height = 1}));
         }
         if (containsAsciiToken(binding.name, "frame") && !isFrameConstantsBinding(binding)) {
            write_identity(0);
            write_identity(64);
            write_identity(128);
         }
         if (containsAsciiToken(binding.name, "material")) {
            write_float(0, 1.0F);
            write_float(4, 1.0F);
            write_float(8, 1.0F);
            write_float(12, 1.0F);
            write_float(16, 1.0F);
         }
         if (containsAsciiToken(binding.name, "lighting")) {
            write_uint(0, 0U);
            write_float(4, 0.25F);
            write_float(8, 0.25F);
            write_float(12, 0.25F);
            write_float(16, 0.25F);
            write_float(20, 0.25F);
            write_float(24, 0.25F);
         }

         return bytes;
      }

      /// @brief Chooses a clear color for fallback textures by reflected binding name.
      [[nodiscard]] VkClearColorValue fallbackTextureClearColor(const PipelineDescriptorBindingDesc &binding) {
         if (containsAsciiToken(binding.name, "normal")) {
            return VkClearColorValue{.float32 = {0.5F, 0.5F, 1.0F, 1.0F}};
         }

         return VkClearColorValue{.float32 = {1.0F, 1.0F, 1.0F, 1.0F}};
      }

      /// @brief Converts renderer-selected primitive topology to Vulkan pipeline state.
      [[nodiscard]] VkPrimitiveTopology primitiveTopology(GraphicsPrimitiveTopology topology) {
         switch (topology) {
         case GraphicsPrimitiveTopology::triangle_list:
            return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
         case GraphicsPrimitiveTopology::triangle_strip:
            return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
         }

         return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
      }

      /// @brief Converts renderer-selected culling to Vulkan pipeline state.
      [[nodiscard]] VkCullModeFlags cullMode(GraphicsCullMode mode) {
         switch (mode) {
         case GraphicsCullMode::none:
            return VK_CULL_MODE_NONE;
         case GraphicsCullMode::front:
            return VK_CULL_MODE_FRONT_BIT;
         case GraphicsCullMode::back:
            return VK_CULL_MODE_BACK_BIT;
         }

         return VK_CULL_MODE_NONE;
      }

      /// @brief Converts renderer-selected front-face winding to Vulkan pipeline state.
      [[nodiscard]] VkFrontFace frontFace(GraphicsFrontFace winding) {
         switch (winding) {
         case GraphicsFrontFace::counter_clockwise:
            return VK_FRONT_FACE_COUNTER_CLOCKWISE;
         case GraphicsFrontFace::clockwise:
            return VK_FRONT_FACE_CLOCKWISE;
         }

         return VK_FRONT_FACE_COUNTER_CLOCKWISE;
      }

      /// @brief Converts renderer-selected depth comparison to Vulkan pipeline state.
      [[nodiscard]] VkCompareOp depthCompareOp(GraphicsDepthCompareOp compare) {
         switch (compare) {
         case GraphicsDepthCompareOp::always:
            return VK_COMPARE_OP_ALWAYS;
         case GraphicsDepthCompareOp::less:
            return VK_COMPARE_OP_LESS;
         case GraphicsDepthCompareOp::less_equal:
            return VK_COMPARE_OP_LESS_OR_EQUAL;
         }

         return VK_COMPARE_OP_ALWAYS;
      }

      /// @brief Converts placeholder color formats to Vulkan formats.
      [[nodiscard]] VkFormat colorFormat(GraphicsColorFormat format) {
         switch (format) {
         case GraphicsColorFormat::bgra8_srgb:
            return VK_FORMAT_B8G8R8A8_SRGB;
         case GraphicsColorFormat::rgba8_srgb:
            return VK_FORMAT_R8G8B8A8_SRGB;
         case GraphicsColorFormat::rgba16_float:
            return VK_FORMAT_R16G16B16A16_SFLOAT;
         }

         return VK_FORMAT_B8G8R8A8_SRGB;
      }

      /// @brief Converts placeholder depth formats to Vulkan formats.
      [[nodiscard]] VkFormat depthFormat(GraphicsDepthFormat format) {
         switch (format) {
         case GraphicsDepthFormat::none:
            return VK_FORMAT_UNDEFINED;
         case GraphicsDepthFormat::depth32_float:
            return VK_FORMAT_D32_SFLOAT;
         }

         return VK_FORMAT_UNDEFINED;
      }

      /// @brief Converts backend-neutral image formats to Vulkan formats.
      [[nodiscard]] std::optional<VkFormat> imageFormat(GpuImageFormat format) {
         switch (format) {
         case GpuImageFormat::rgba8_unorm:
            return VK_FORMAT_R8G8B8A8_UNORM;
         case GpuImageFormat::rgba8_srgb:
            return VK_FORMAT_R8G8B8A8_SRGB;
         case GpuImageFormat::bgra8_srgb:
            return VK_FORMAT_B8G8R8A8_SRGB;
         case GpuImageFormat::rgba16_float:
            return VK_FORMAT_R16G16B16A16_SFLOAT;
         case GpuImageFormat::depth32_float:
         case GpuImageFormat::unknown:
            return std::nullopt;
         }

         return std::nullopt;
      }

      /// @brief Returns the expected bytes per texel for upload validation.
      [[nodiscard]] std::optional<std::size_t> imageFormatBytesPerTexel(GpuImageFormat format) {
         switch (format) {
         case GpuImageFormat::rgba8_unorm:
         case GpuImageFormat::rgba8_srgb:
         case GpuImageFormat::bgra8_srgb:
            return 4U;
         case GpuImageFormat::rgba16_float:
            return 8U;
         case GpuImageFormat::depth32_float:
         case GpuImageFormat::unknown:
            return std::nullopt;
         }

         return std::nullopt;
      }

      /// @brief Converts backend-neutral buffer usage to Vulkan buffer usage flags.
      [[nodiscard]] std::optional<VkBufferUsageFlags> bufferUsageFlags(GpuBufferUsage usage) {
         switch (usage) {
         case GpuBufferUsage::vertex:
            return VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
         case GpuBufferUsage::index:
            return VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
         case GpuBufferUsage::uniform:
            return VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
         case GpuBufferUsage::storage:
            return VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
         case GpuBufferUsage::staging:
            return VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
         case GpuBufferUsage::unknown:
            return std::nullopt;
         }

         return std::nullopt;
      }

      /// @brief Adds a unique extension name to a mutable extension list.
      void appendUniqueExtension(std::vector<std::string> &extensions, std::string_view extension) {
         if (extension.empty()) {
            return;
         }

         if (!std::ranges::contains(extensions, extension)) {
            extensions.emplace_back(extension);
         }
      }

      /// @brief Chooses the surface format used by Stage 11 swapchains.
      [[nodiscard]] VkSurfaceFormatKHR chooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR> &formats) {
         const auto preferred = std::ranges::find_if(formats, [](const VkSurfaceFormatKHR &format) {
            return format.format == VK_FORMAT_B8G8R8A8_SRGB &&
                   format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
         });
         if (preferred != formats.end()) {
            return *preferred;
         }

         return formats.empty() ? VkSurfaceFormatKHR{.format = VK_FORMAT_B8G8R8A8_SRGB,
                                                     .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR}
                                : formats.front();
      }

      /// @brief Chooses a presentation mode that is available on every Vulkan implementation.
      [[nodiscard]] VkPresentModeKHR choosePresentMode(const std::vector<VkPresentModeKHR> &modes) {
         if (std::ranges::contains(modes, VK_PRESENT_MODE_MAILBOX_KHR)) {
            return VK_PRESENT_MODE_MAILBOX_KHR;
         }
         if (std::ranges::contains(modes, VK_PRESENT_MODE_FIFO_KHR)) {
            return VK_PRESENT_MODE_FIFO_KHR;
         }

         return modes.empty() ? VK_PRESENT_MODE_FIFO_KHR : modes.front();
      }

      /// @brief Chooses the swapchain extent clamped to surface capabilities.
      [[nodiscard]] VkExtent2D chooseSwapchainExtent(const VkSurfaceCapabilitiesKHR &capabilities,
                                                     const NativeWindowHandle &window) {
         if (capabilities.currentExtent.width != std::numeric_limits<std::uint32_t>::max()) {
            return capabilities.currentExtent;
         }

         const auto clamp_extent = [](std::uint32_t value, std::uint32_t minimum, std::uint32_t maximum) {
            return std::min(std::max(value, minimum), maximum);
         };
         return VkExtent2D{.width = clamp_extent(std::max(window.width, 1U), capabilities.minImageExtent.width,
                                                 capabilities.maxImageExtent.width),
                           .height = clamp_extent(std::max(window.height, 1U), capabilities.minImageExtent.height,
                                                  capabilities.maxImageExtent.height)};
      }

      /// @brief Chooses a composite-alpha mode accepted by the surface.
      [[nodiscard]] VkCompositeAlphaFlagBitsKHR chooseCompositeAlpha(VkCompositeAlphaFlagsKHR supported) {
         if ((supported & VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR) != 0) {
            return VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
         }
         if ((supported & VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR) != 0) {
            return VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR;
         }
         if ((supported & VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR) != 0) {
            return VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR;
         }

         return VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR;
      }

      /// @brief Returns the number of conservative frame-sync slots used by one window.
      [[nodiscard]] std::uint32_t frameSyncSlotCount(std::size_t image_count) {
         return static_cast<std::uint32_t>(std::max<std::size_t>(1U, std::min<std::size_t>(2U, image_count)));
      }

      /// @brief Builds a stable clear color so different windows are visually distinguishable.
      [[nodiscard]] VkClearColorValue windowClearColor(const WindowSwapchainResources &summary) {
         const auto bits = summary.handle.value.value();
         const auto channel = [bits](std::uint32_t shift) {
            const auto value = static_cast<std::uint8_t>((bits >> shift) & 0xFFU);
            return 0.12F + (static_cast<float>(value) / 255.0F) * 0.32F;
         };

         return VkClearColorValue{.float32 = {channel(0U), channel(8U), channel(16U), 1.0F}};
      }

      [[nodiscard]] bool hasDeviceExtension(VkPhysicalDevice device, const char *name);

      /// @brief Returns whether a physical device supports a queue family with graphics work.
      [[nodiscard]] std::optional<std::uint32_t> graphicsQueueFamily(VkInstance instance, VkPhysicalDevice device,
                                                                     bool require_presentation) {
         std::uint32_t queue_family_count = 0;
         vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, nullptr);
         if (queue_family_count == 0) {
            return std::nullopt;
         }

         std::vector<VkQueueFamilyProperties> queue_families(queue_family_count);
         vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, queue_families.data());
         for (std::uint32_t index = 0; index < queue_family_count; ++index) {
            if ((queue_families[index].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0 &&
                queue_families[index].queueCount > 0 &&
                (!require_presentation || SDL_Vulkan_GetPresentationSupport(instance, device, index))) {
               return index;
            }
         }

         return std::nullopt;
      }

      /// @brief Selects the first physical device that can create graphics-capable resources.
      [[nodiscard]] std::expected<SelectedPhysicalDevice, vve::Error>
      selectPhysicalDevice(VkInstance instance, bool require_presentation) {
         std::uint32_t device_count = 0;
         VkResult result = vkEnumeratePhysicalDevices(instance, &device_count, nullptr);
         if (result != VK_SUCCESS || device_count == 0) {
            std::cerr << "[VulkanGraphicsBackend] no Vulkan physical device available for backend resources\n";
            return std::unexpected(vve::Error::internal_error);
         }

         std::vector<VkPhysicalDevice> devices(device_count);
         result = vkEnumeratePhysicalDevices(instance, &device_count, devices.data());
         if (result != VK_SUCCESS && result != VK_INCOMPLETE) {
            std::cerr << "[VulkanGraphicsBackend] vkEnumeratePhysicalDevices failed: " << string_VkResult(result)
                      << '\n';
            return std::unexpected(vve::Error::internal_error);
         }

         devices.resize(device_count);
         for (const auto device : devices) {
            if (require_presentation && !hasDeviceExtension(device, VK_KHR_SWAPCHAIN_EXTENSION_NAME)) {
               continue;
            }

            if (const auto queue_family = graphicsQueueFamily(instance, device, require_presentation)) {
               return SelectedPhysicalDevice{.device = device, .graphics_queue_family = *queue_family};
            }
         }

         std::cerr << "[VulkanGraphicsBackend] no compatible Vulkan graphics/present queue family found\n";
         return std::unexpected(vve::Error::internal_error);
      }

      /// @brief Returns device extensions required for the selected physical device.
      [[nodiscard]] std::expected<std::vector<const char *>, vve::Error>
      requiredDeviceExtensions(VkPhysicalDevice device, bool require_swapchain) {
         std::vector<const char *> extensions{};
         if (require_swapchain) {
            if (!hasDeviceExtension(device, VK_KHR_SWAPCHAIN_EXTENSION_NAME)) {
               std::cerr << "[VulkanGraphicsBackend] selected device does not support "
                         << VK_KHR_SWAPCHAIN_EXTENSION_NAME << '\n';
               return std::unexpected(vve::Error::internal_error);
            }
            extensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
         }
#ifdef VK_KHR_portability_subset
         if (hasDeviceExtension(device, VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME)) {
            extensions.push_back(VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME);
         }
#endif
         return extensions;
      }

      /// @brief Enumerates available instance extensions.
      [[nodiscard]] std::expected<std::vector<VkExtensionProperties>, vve::Error> enumerateInstanceExtensions() {
         std::uint32_t extension_count = 0;
         VkResult result = vkEnumerateInstanceExtensionProperties(nullptr, &extension_count, nullptr);
         if (result != VK_SUCCESS) {
            std::cerr << "[VulkanGraphicsBackend] vkEnumerateInstanceExtensionProperties failed: "
                      << string_VkResult(result) << '\n';
            return std::unexpected(vve::Error::internal_error);
         }

         std::vector<VkExtensionProperties> extensions(extension_count);
         if (extension_count == 0) {
            return extensions;
         }

         result = vkEnumerateInstanceExtensionProperties(nullptr, &extension_count, extensions.data());
         if (result != VK_SUCCESS && result != VK_INCOMPLETE) {
            std::cerr << "[VulkanGraphicsBackend] vkEnumerateInstanceExtensionProperties failed: "
                      << string_VkResult(result) << '\n';
            return std::unexpected(vve::Error::internal_error);
         }

         extensions.resize(extension_count);
         return extensions;
      }

      /// @brief Returns whether an extension name is present in an enumerated extension list.
      [[nodiscard]] bool hasExtension(const std::vector<VkExtensionProperties> &extensions, const char *name) {
         return std::ranges::any_of(extensions, [name](const VkExtensionProperties &extension) {
            return std::string_view(extension.extensionName) == name;
         });
      }

      /// @brief Creates a Vulkan instance with backend and window-system extension requirements.
      [[nodiscard]] std::expected<VkInstance, vve::Error>
      createDiagnosticInstance(const std::vector<std::string> &required_instance_extensions = {}) {
         const auto extensions = enumerateInstanceExtensions();
         if (!extensions) {
            return std::unexpected(extensions.error());
         }

         std::vector<std::string> enabled_extension_names = required_instance_extensions;
         VkInstanceCreateFlags create_flags = 0;
         if (hasExtension(*extensions, VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME)) {
            appendUniqueExtension(enabled_extension_names, VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
         }
         if (hasExtension(*extensions, VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME)) {
            appendUniqueExtension(enabled_extension_names, VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
            create_flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
         }
         for (const auto &extension_name : enabled_extension_names) {
            if (!hasExtension(*extensions, extension_name.c_str())) {
               std::cerr << "[VulkanGraphicsBackend] required instance extension is unavailable: "
                         << extension_name << '\n';
               return std::unexpected(vve::Error::internal_error);
            }
         }

         std::vector<const char *> enabled_extensions{};
         enabled_extensions.reserve(enabled_extension_names.size());
         for (const auto &extension_name : enabled_extension_names) {
            enabled_extensions.push_back(extension_name.c_str());
         }

         const VkApplicationInfo app_info{.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
                                          .pNext = nullptr,
                                          .pApplicationName = "Vienna Vulkan Engine",
                                          .applicationVersion = VK_MAKE_VERSION(3, 0, 0),
                                          .pEngineName = "Vienna Vulkan Engine",
                                          .engineVersion = VK_MAKE_VERSION(3, 0, 0),
                                          .apiVersion = VK_API_VERSION_1_0};
         const VkInstanceCreateInfo create_info{.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
                                                .pNext = nullptr,
                                                .flags = create_flags,
                                                .pApplicationInfo = &app_info,
                                                .enabledLayerCount = 0,
                                                .ppEnabledLayerNames = nullptr,
                                                .enabledExtensionCount =
                                                    static_cast<std::uint32_t>(enabled_extensions.size()),
                                                .ppEnabledExtensionNames = enabled_extensions.data()};

         VkInstance instance = VK_NULL_HANDLE;
         const VkResult result = vkCreateInstance(&create_info, nullptr, &instance);
         if (result != VK_SUCCESS) {
            std::cerr << "[VulkanGraphicsBackend] vkCreateInstance failed: " << string_VkResult(result) << '\n';
            return std::unexpected(vve::Error::internal_error);
         }

         if (!required_instance_extensions.empty()) {
            std::clog << "[VulkanGraphicsBackend] presentation_extensions="
                      << required_instance_extensions.size() << '\n';
         }
         std::clog << "[VulkanGraphicsBackend] portability_enumeration="
                   << ((create_flags & VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR) != 0 ? "enabled" : "disabled")
                   << '\n';
         return instance;
      }

      /// @brief Returns whether a physical-device extension is advertised.
      [[nodiscard]] bool hasDeviceExtension(VkPhysicalDevice device, const char *name) {
         std::uint32_t extension_count = 0;
         if (vkEnumerateDeviceExtensionProperties(device, nullptr, &extension_count, nullptr) != VK_SUCCESS) {
            return false;
         }

         std::vector<VkExtensionProperties> extensions(extension_count);
         if (extension_count == 0) {
            return false;
         }

         if (vkEnumerateDeviceExtensionProperties(device, nullptr, &extension_count, extensions.data()) != VK_SUCCESS) {
            return false;
         }

         extensions.resize(extension_count);
         return hasExtension(extensions, name);
      }

      /// @brief Logs physical-device and driver metadata exposed by the active Vulkan loader and ICD selection.
      [[nodiscard]] std::expected<void, vve::Error> logPhysicalDevices(VkInstance instance) {
         std::uint32_t device_count = 0;
         VkResult result = vkEnumeratePhysicalDevices(instance, &device_count, nullptr);
         if (result != VK_SUCCESS) {
            std::cerr << "[VulkanGraphicsBackend] vkEnumeratePhysicalDevices failed: " << string_VkResult(result)
                      << '\n';
            return std::unexpected(vve::Error::internal_error);
         }

         std::clog << "[VulkanGraphicsBackend] physical_devices=" << device_count << '\n';
         if (device_count == 0) {
            return std::unexpected(vve::Error::internal_error);
         }

         std::vector<VkPhysicalDevice> devices(device_count);
         result = vkEnumeratePhysicalDevices(instance, &device_count, devices.data());
         if (result != VK_SUCCESS && result != VK_INCOMPLETE) {
            std::cerr << "[VulkanGraphicsBackend] vkEnumeratePhysicalDevices failed: " << string_VkResult(result)
                      << '\n';
            return std::unexpected(vve::Error::internal_error);
         }

         devices.resize(device_count);
         const auto get_properties_2 =
             reinterpret_cast<PFN_vkGetPhysicalDeviceProperties2>(vkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceProperties2"));
         const auto get_properties_2_khr = reinterpret_cast<PFN_vkGetPhysicalDeviceProperties2KHR>(
             vkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceProperties2KHR"));

         for (std::size_t device_index = 0; device_index < devices.size(); ++device_index) {
            VkPhysicalDeviceProperties properties{};
            vkGetPhysicalDeviceProperties(devices[device_index], &properties);

            VkPhysicalDeviceDriverProperties driver_properties{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES,
                                                              .pNext = nullptr};
            VkPhysicalDeviceProperties2 properties_2{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
                                                     .pNext = &driver_properties,
                                                     .properties = {}};
            const bool can_query_driver_properties =
                properties.apiVersion >= VK_API_VERSION_1_2 ||
                hasDeviceExtension(devices[device_index], VK_KHR_DRIVER_PROPERTIES_EXTENSION_NAME);
            if (can_query_driver_properties && get_properties_2 != nullptr) {
               get_properties_2(devices[device_index], &properties_2);
            } else if (can_query_driver_properties && get_properties_2_khr != nullptr) {
               get_properties_2_khr(devices[device_index], &properties_2);
            } else {
               properties_2.properties = properties;
            }

            std::clog << "[VulkanGraphicsBackend] device[" << device_index << "] name=\""
                      << properties.deviceName << "\" type=" << string_VkPhysicalDeviceType(properties.deviceType)
                      << " api=" << versionString(properties.apiVersion)
                      << " driver_version=" << versionString(properties.driverVersion);
            if (can_query_driver_properties && (get_properties_2 != nullptr || get_properties_2_khr != nullptr)) {
               std::clog << " driver_id=" << string_VkDriverId(driver_properties.driverID)
                         << " driver_name=\"" << driver_properties.driverName << "\""
                         << " driver_info=\"" << driver_properties.driverInfo << "\"";
            }
            std::clog << '\n';
         }

         return {};
      }

      /// @brief Runs the startup Vulkan loader and physical-device diagnostic pass.
      [[nodiscard]] std::expected<void, vve::Error> logVulkanStartupDiagnostics() {
         const auto selected_icd = environmentValue("VVE_VULKAN_ICD");
         const auto vk_icd_filenames = environmentValue("VK_ICD_FILENAMES");
         std::clog << "[VulkanGraphicsBackend] VVE_VULKAN_ICD="
                   << (selected_icd.empty() ? "<default>" : selected_icd) << '\n';
         std::clog << "[VulkanGraphicsBackend] VK_ICD_FILENAMES="
                   << (vk_icd_filenames.empty() ? "<loader default>" : vk_icd_filenames) << '\n';

         const auto instance = createDiagnosticInstance();
         if (!instance) {
            return std::unexpected(instance.error());
         }

         const auto device_result = logPhysicalDevices(*instance);
         vkDestroyInstance(*instance, nullptr);
         if (!device_result) {
            return std::unexpected(device_result.error());
         }

         return {};
      }

   } // namespace

   /**
    * @brief Concrete Vulkan backend implementation used by v3.
    */
   class VulkanGraphicsBackendImplementation {
   public:
      ~VulkanGraphicsBackendImplementation() { destroy(); }

      /// @brief Returns the backend name for diagnostics.
      [[nodiscard]] std::string_view name() const noexcept { return "VulkanGraphicsBackend"; }

      /// @brief Returns the public graphics API implemented by this backend.
      [[nodiscard]] vve::GraphicsApi api() const noexcept { return vve::GraphicsApi::vulkan; }

      /// @brief Initializes backend-owned state.
      [[nodiscard]] std::expected<void, vve::Error> init() {
         return init(std::vector<std::string>{});
      }

      /// @brief Initializes backend-owned state with platform-required instance extensions.
      [[nodiscard]] std::expected<void, vve::Error> init(const std::vector<std::string> &instance_extensions) {
         if (initialized_) {
            return {};
         }

         const auto selected_icd = environmentValue("VVE_VULKAN_ICD");
         const auto vk_icd_filenames = environmentValue("VK_ICD_FILENAMES");
         std::clog << "[VulkanGraphicsBackend] VVE_VULKAN_ICD="
                   << (selected_icd.empty() ? "<default>" : selected_icd) << '\n';
         std::clog << "[VulkanGraphicsBackend] VK_ICD_FILENAMES="
                   << (vk_icd_filenames.empty() ? "<loader default>" : vk_icd_filenames) << '\n';

         const auto instance = createDiagnosticInstance(instance_extensions);
         if (!instance) {
            return std::unexpected(instance.error());
         }
         instance_ = *instance;

         if (auto device_result = logPhysicalDevices(instance_); !device_result) {
            destroy();
            return std::unexpected(device_result.error());
         }

         const bool require_presentation = !instance_extensions.empty();
         const auto selected_device = selectPhysicalDevice(instance_, require_presentation);
         if (!selected_device) {
            destroy();
            return std::unexpected(selected_device.error());
         }

         physical_device_ = selected_device->device;
         graphics_queue_family_ = selected_device->graphics_queue_family;
         presentation_enabled_ = require_presentation;
         if (auto logical_device = createLogicalDevice(); !logical_device) {
            destroy();
            return std::unexpected(logical_device.error());
         }
         if (auto pipeline_cache = createPipelineCache(); !pipeline_cache) {
            destroy();
            return std::unexpected(pipeline_cache.error());
         }
         if (auto command_pool = createCommandPool(); !command_pool) {
            destroy();
            return std::unexpected(command_pool.error());
         }

         initialized_ = true;
         return {};
      }

      /// @brief Returns all renderer descriptors supported by this Vulkan backend.
      [[nodiscard]] std::vector<RendererDesc> supportedRenderers() const {
         std::vector<RendererDesc> renderers{};
         renderers.reserve(rendererRegistry().size());
         for (const auto &[id, renderer] : rendererRegistry()) {
            (void)id;
            renderers.push_back(renderer);
         }
         return renderers;
      }

      /// @brief Creates or resolves a backend renderer descriptor by id.
      [[nodiscard]] std::expected<RendererDesc, vve::Error> createRenderer(std::string_view renderer_id) const {
         const auto canonical_id = canonicalRendererId(renderer_id);
         if (!canonical_id.has_value()) {
            return std::unexpected(vve::Error::invalid_argument);
         }

         const auto renderer = rendererRegistry().find(*canonical_id);
         if (renderer == rendererRegistry().end()) {
            return std::unexpected(vve::Error::internal_error);
         }

         return renderer->second;
      }

      /// @brief Builds a backend-facing pipeline layout description from shader reflection.
      [[nodiscard]] std::expected<PipelineLayoutDesc, vve::Error>
      createPipelineLayout(const RendererDesc &renderer, const ShaderMetadata &shader) const {
         return buildPipelineLayout(renderer, shader);
      }

      /// @brief Creates backend-owned Vulkan objects for a reflected pipeline layout.
      [[nodiscard]] std::expected<PipelineBackendResources, vve::Error>
      createPipelineResources(const PipelineLayoutDesc &layout, const ShaderMetadata &shader) {
         if (!initialized_ || device_ == VK_NULL_HANDLE) {
            return std::unexpected(vve::Error::not_initialized);
         }
         if (layout.shader_program.value != shader.handle.value || !layout.shader_program.value.isValid()) {
            return std::unexpected(vve::Error::invalid_argument);
         }
         if (!layout.push_constants.empty()) {
            std::cerr << "[VulkanGraphicsBackend] push constants require reflected byte ranges before Vulkan creation\n";
            return std::unexpected(vve::Error::invalid_argument);
         }

         const auto handle = pipelineResourceHandle(layout);
         const auto existing = pipeline_resources_.find(handle.value.value());
         if (existing != pipeline_resources_.end()) {
            return existing->second.summary;
         }

         VulkanPipelineResources resources{};
         resources.summary = PipelineBackendResources{.handle = handle,
                                                      .renderer = layout.renderer,
                                                      .shader_program = layout.shader_program};

         if (auto shader_modules = createShaderModules(resources, shader); !shader_modules) {
            destroyPipelineResources(resources);
            return std::unexpected(shader_modules.error());
         }

         if (auto descriptor_sets = createDescriptorSetLayouts(resources, layout); !descriptor_sets) {
            destroyPipelineResources(resources);
            return std::unexpected(descriptor_sets.error());
         }

         if (auto pipeline_layout = createVulkanPipelineLayout(resources); !pipeline_layout) {
            destroyPipelineResources(resources);
            return std::unexpected(pipeline_layout.error());
         }
         if (auto descriptor_pool = createDescriptorPoolAndSets(resources); !descriptor_pool) {
            destroyPipelineResources(resources);
            return std::unexpected(descriptor_pool.error());
         }
         if (auto descriptor_updates = updateFallbackDescriptorSets(resources); !descriptor_updates) {
            destroyPipelineResources(resources);
            return std::unexpected(descriptor_updates.error());
         }

         resources.summary.shader_module_count = resources.shader_modules.size();
         resources.summary.descriptor_set_layout_count = resources.descriptor_set_layouts.size();
         resources.summary.pipeline_layout_created = resources.pipeline_layout != VK_NULL_HANDLE;
         const auto summary = resources.summary;
         pipeline_resources_.emplace(handle.value.value(), std::move(resources));
         return summary;
      }

      /// @brief Returns backend resource metadata for an already-created pipeline resource bundle.
      [[nodiscard]] std::expected<std::optional<PipelineBackendResources>, vve::Error>
      pipelineResources(PipelineResourceHandle resources) const {
         if (!resources.value.isValid()) {
            return std::unexpected(vve::Error::invalid_argument);
         }

         const auto resource_it = pipeline_resources_.find(resources.value.value());
         if (resource_it == pipeline_resources_.end()) {
            return std::optional<PipelineBackendResources>{};
         }

         return resource_it->second.summary;
      }

      /// @brief Creates backend-owned graphics pipeline preparation data.
      [[nodiscard]] std::expected<GraphicsPipelineResources, vve::Error>
      createGraphicsPipelineResources(const RendererPipelineBinding &binding, const GraphicsPipelineDesc &desc) {
         if (!initialized_ || device_ == VK_NULL_HANDLE || pipeline_cache_ == VK_NULL_HANDLE) {
            return std::unexpected(vve::Error::not_initialized);
         }
         if (!graphicsPipelineDescMatchesBinding(binding, desc) || !graphicsPipelineDescMatchesRenderer(desc)) {
            return std::unexpected(vve::Error::invalid_argument);
         }

         const auto pipeline_resource = pipeline_resources_.find(binding.backend_resources.value.value());
         if (pipeline_resource == pipeline_resources_.end() ||
             !pipeline_resource->second.summary.pipeline_layout_created) {
            return std::unexpected(vve::Error::invalid_argument);
         }

         const auto handle = graphicsPipelineHandle(desc);
         const auto existing = graphics_pipelines_.find(handle.value.value());
         if (existing != graphics_pipelines_.end()) {
            return existing->second.summary;
         }

         VulkanGraphicsPipelineResources resources{};
         resources.summary = GraphicsPipelineResources{.handle = handle,
                                                       .renderer = desc.renderer,
                                                       .backend_resources = desc.backend_resources,
                                                       .main_kernel = desc.main_kernel,
                                                       .color_attachment_count = desc.color_attachment_count,
                                                       .depth_enabled =
                                                           desc.depth_format != GraphicsDepthFormat::none,
                                                       .pipeline_cache_ready = pipeline_cache_ != VK_NULL_HANDLE,
                                                       .vulkan_pipeline_created = false};
         if (auto render_pass = createPlaceholderRenderPass(resources, desc); !render_pass) {
            destroyGraphicsPipelineResources(resources);
            return std::unexpected(render_pass.error());
         }
         if (auto pipeline = createVulkanGraphicsPipeline(resources, pipeline_resource->second, desc); !pipeline) {
            destroyGraphicsPipelineResources(resources);
            return std::unexpected(pipeline.error());
         }

         resources.summary.vulkan_pipeline_created = resources.pipeline != VK_NULL_HANDLE;
         const auto summary = resources.summary;
         graphics_pipelines_.emplace(handle.value.value(), std::move(resources));
         return summary;
      }

      /// @brief Returns backend graphics pipeline metadata for an already-prepared pipeline.
      [[nodiscard]] std::expected<std::optional<GraphicsPipelineResources>, vve::Error>
      graphicsPipelineResources(GraphicsPipelineHandle pipeline) const {
         if (!pipeline.value.isValid()) {
            return std::unexpected(vve::Error::invalid_argument);
         }

         const auto pipeline_it = graphics_pipelines_.find(pipeline.value.value());
         if (pipeline_it == graphics_pipelines_.end()) {
            return std::optional<GraphicsPipelineResources>{};
         }

         return pipeline_it->second.summary;
      }

      /// @brief Creates a host-visible Vulkan buffer and copies CPU bytes into it.
      [[nodiscard]] std::expected<GpuBufferResources, vve::Error>
      createBuffer(vve::Handle owner, ResourceKind owner_kind, GpuBufferUsage usage,
                   std::span<const std::byte> bytes, std::uint32_t generation) {
         if (!initialized_ || physical_device_ == VK_NULL_HANDLE || device_ == VK_NULL_HANDLE) {
            return std::unexpected(vve::Error::not_initialized);
         }
         if (!owner.isValid() || bytes.empty() || generation == 0) {
            return std::unexpected(vve::Error::invalid_argument);
         }

         const auto vk_usage = bufferUsageFlags(usage);
         if (!vk_usage.has_value()) {
            return std::unexpected(vve::Error::invalid_argument);
         }

         const auto handle = gpuBufferHandle(owner, owner_kind, usage, bytes.size(), generation);
         const auto existing = buffers_.find(handle.value.value());
         if (existing != buffers_.end()) {
            return existing->second.summary;
         }

         VulkanGpuBufferResources resources{};
         resources.summary = GpuBufferResources{.handle = handle,
                                                .owner = owner,
                                                .owner_kind = owner_kind,
                                                .usage = usage,
                                                .byte_size = bytes.size(),
                                                .generation = generation};

         if (auto result = createVulkanBuffer(resources, *vk_usage, bytes); !result) {
            destroyBufferResources(resources);
            return std::unexpected(result.error());
         }

         resources.summary.buffer_created = resources.buffer != VK_NULL_HANDLE;
         resources.summary.memory_bound = resources.memory != VK_NULL_HANDLE;
         const auto summary = resources.summary;
         buffers_.emplace(handle.value.value(), std::move(resources));
         return summary;
      }

      /// @brief Returns backend buffer metadata for an already-created GPU buffer.
      [[nodiscard]] std::expected<std::optional<GpuBufferResources>, vve::Error>
      bufferResources(GpuBufferHandle buffer) const {
         if (!buffer.value.isValid()) {
            return std::unexpected(vve::Error::invalid_argument);
         }

         const auto buffer_it = buffers_.find(buffer.value.value());
         if (buffer_it == buffers_.end()) {
            return std::optional<GpuBufferResources>{};
         }

         return buffer_it->second.summary;
      }

      /// @brief Destroys a backend-owned GPU buffer.
      [[nodiscard]] std::expected<void, vve::Error> destroyBuffer(GpuBufferHandle buffer) {
         if (!buffer.value.isValid()) {
            return std::unexpected(vve::Error::invalid_argument);
         }

         const auto buffer_it = buffers_.find(buffer.value.value());
         if (buffer_it == buffers_.end()) {
            return std::unexpected(vve::Error::invalid_argument);
         }

         destroyBufferResources(buffer_it->second);
         buffers_.erase(buffer_it);
         return {};
      }

      /// @brief Creates a device-local sampled image and uploads RGBA pixel bytes through a staging buffer.
      [[nodiscard]] std::expected<GpuTextureResources, vve::Error>
      createSampledImage(vve::Handle owner, ResourceKind owner_kind, GpuImageFormat format,
                         std::uint32_t width, std::uint32_t height,
                         std::span<const std::byte> rgba_pixels, std::uint32_t generation) {
         if (!initialized_ || physical_device_ == VK_NULL_HANDLE || device_ == VK_NULL_HANDLE ||
             command_pool_ == VK_NULL_HANDLE || graphics_queue_ == VK_NULL_HANDLE) {
            return std::unexpected(vve::Error::not_initialized);
         }
         if (!owner.isValid() || owner_kind != ResourceKind::texture || width == 0 || height == 0 ||
             rgba_pixels.empty() || generation == 0) {
            return std::unexpected(vve::Error::invalid_argument);
         }

         const auto vk_format = imageFormat(format);
         const auto bytes_per_texel = imageFormatBytesPerTexel(format);
         if (!vk_format.has_value() || !bytes_per_texel.has_value()) {
            return std::unexpected(vve::Error::invalid_argument);
         }
         const auto expected_bytes = static_cast<std::size_t>(width) * static_cast<std::size_t>(height) *
                                     *bytes_per_texel;
         if (rgba_pixels.size() != expected_bytes) {
            return std::unexpected(vve::Error::invalid_argument);
         }

         const auto image_handle =
             gpuImageHandle(owner, owner_kind, GpuImageUsage::sampled, format, width, height, generation);
         const auto existing = images_.find(image_handle.value.value());
         if (existing != images_.end()) {
            return existing->second.summary;
         }

         VulkanGpuImageResources resources{};
         resources.summary = GpuTextureResources{.texture = TextureHandle{.value = owner},
                                                 .image = image_handle,
                                                 .sampler = gpuSamplerHandle(owner, image_handle, generation),
                                                 .usage = GpuImageUsage::sampled,
                                                 .format = format,
                                                 .width = width,
                                                 .height = height,
                                                 .mip_levels = 1,
                                                 .array_layers = 1,
                                                 .generation = generation};

         if (auto create_result = createVulkanSampledImage(resources, *vk_format, rgba_pixels); !create_result) {
            destroyImageResources(resources);
            return std::unexpected(create_result.error());
         }

         resources.summary.image_created = resources.image != VK_NULL_HANDLE;
         resources.summary.image_view_created = resources.image_view != VK_NULL_HANDLE;
         resources.summary.sampler_created = resources.sampler != VK_NULL_HANDLE;
         resources.summary.resident = resources.summary.image_created && resources.summary.image_view_created &&
                                      resources.summary.sampler_created;
         const auto summary = resources.summary;
         images_.emplace(image_handle.value.value(), std::move(resources));
         return summary;
      }

      /// @brief Returns backend sampled image metadata for an already-created GPU image.
      [[nodiscard]] std::expected<std::optional<GpuTextureResources>, vve::Error>
      imageResources(GpuImageHandle image) const {
         if (!image.value.isValid()) {
            return std::unexpected(vve::Error::invalid_argument);
         }

         const auto image_it = images_.find(image.value.value());
         if (image_it == images_.end()) {
            return std::optional<GpuTextureResources>{};
         }

         return image_it->second.summary;
      }

      /// @brief Destroys a backend-owned sampled image.
      [[nodiscard]] std::expected<void, vve::Error> destroyImage(GpuImageHandle image) {
         if (!image.value.isValid()) {
            return std::unexpected(vve::Error::invalid_argument);
         }

         const auto image_it = images_.find(image.value.value());
         if (image_it == images_.end()) {
            return std::unexpected(vve::Error::invalid_argument);
         }

         destroyImageResources(image_it->second);
         images_.erase(image_it);
         return {};
      }

      /// @brief Creates surface, swapchain, and image views for one native window.
      [[nodiscard]] std::expected<WindowSwapchainResources, vve::Error>
      createWindowSwapchain(const NativeWindowHandle &window) {
         if (!initialized_ || !presentation_enabled_ || instance_ == VK_NULL_HANDLE || device_ == VK_NULL_HANDLE) {
            return std::unexpected(vve::Error::not_initialized);
         }
         if (!window.window.value.isValid() || window.window_id.empty() || window.native_window == nullptr ||
             !window.vulkan_capable || window.width == 0 || window.height == 0) {
            return std::unexpected(vve::Error::invalid_argument);
         }

         const auto handle = swapchainHandle(window);
         const auto existing = window_swapchains_.find(handle.value.value());
         if (existing != window_swapchains_.end()) {
            return existing->second.summary;
         }

         VulkanWindowSwapchainResources resources{};
         resources.summary = WindowSwapchainResources{.handle = handle,
                                                      .window = window.window,
                                                      .window_id = window.window_id,
                                                      .width = window.width,
                                                      .height = window.height};

         if (auto surface = createWindowSurface(resources, window); !surface) {
            destroyWindowSwapchainResources(resources);
            return std::unexpected(surface.error());
         }
         if (auto swapchain = createVulkanSwapchain(resources, window); !swapchain) {
            destroyWindowSwapchainResources(resources);
            return std::unexpected(swapchain.error());
         }
         if (auto image_views = createSwapchainImageViews(resources); !image_views) {
            destroyWindowSwapchainResources(resources);
            return std::unexpected(image_views.error());
         }
         if (auto depth_resources = createSwapchainDepthResources(resources); !depth_resources) {
            destroyWindowSwapchainResources(resources);
            return std::unexpected(depth_resources.error());
         }
         if (auto render_pass = createSwapchainClearRenderPass(resources); !render_pass) {
            destroyWindowSwapchainResources(resources);
            return std::unexpected(render_pass.error());
         }
         if (auto framebuffers = createSwapchainFramebuffers(resources); !framebuffers) {
            destroyWindowSwapchainResources(resources);
            return std::unexpected(framebuffers.error());
         }
         if (auto command_buffers = allocateSwapchainCommandBuffers(resources); !command_buffers) {
            destroyWindowSwapchainResources(resources);
            return std::unexpected(command_buffers.error());
         }
         if (auto frame_sync = createSwapchainFrameSync(resources); !frame_sync) {
            destroyWindowSwapchainResources(resources);
            return std::unexpected(frame_sync.error());
         }

         resources.summary.surface_created = resources.surface != VK_NULL_HANDLE;
         resources.summary.swapchain_created = resources.swapchain != VK_NULL_HANDLE;
         resources.summary.image_count = static_cast<std::uint32_t>(resources.images.size());
         resources.summary.image_view_count = static_cast<std::uint32_t>(resources.image_views.size());
         resources.summary.depth_image_count = resources.depth_image == VK_NULL_HANDLE ? 0U : 1U;
         resources.summary.depth_image_view_count = resources.depth_image_view == VK_NULL_HANDLE ? 0U : 1U;
         resources.summary.framebuffer_count = static_cast<std::uint32_t>(resources.framebuffers.size());
         resources.summary.frames_in_flight = static_cast<std::uint32_t>(resources.frame_sync.size());
         resources.summary.width = resources.extent.width;
         resources.summary.height = resources.extent.height;
         resources.summary.surface_format = string_VkFormat(resources.format);
         resources.summary.depth_format = string_VkFormat(resources.depth_format);
         resources.summary.present_mode = string_VkPresentModeKHR(resources.present_mode);
         resources.summary.depth_image_created = resources.depth_image != VK_NULL_HANDLE;
         resources.summary.depth_image_view_created = resources.depth_image_view != VK_NULL_HANDLE;
         resources.summary.current_image_index = resources.active_image_index;
         resources.summary.frame_acquired = resources.frame_acquired;
         resources.summary.swapchain_dirty = false;
         const auto summary = resources.summary;
         window_swapchains_.emplace(handle.value.value(), std::move(resources));
         return summary;
      }

      /// @brief Recreates surface, swapchain, framebuffers, and frame sync for one resized native window.
      [[nodiscard]] std::expected<WindowSwapchainResources, vve::Error>
      recreateWindowSwapchain(const NativeWindowHandle &window) {
         if (!initialized_ || !presentation_enabled_ || instance_ == VK_NULL_HANDLE || device_ == VK_NULL_HANDLE) {
            return std::unexpected(vve::Error::not_initialized);
         }
         if (!window.window.value.isValid() || window.window_id.empty() || window.native_window == nullptr ||
             !window.vulkan_capable || window.width == 0 || window.height == 0) {
            return std::unexpected(vve::Error::invalid_argument);
         }

         const auto handle = swapchainHandle(window);
         const auto existing = window_swapchains_.find(handle.value.value());
         if (existing != window_swapchains_.end()) {
            if (const VkResult idle_result = vkDeviceWaitIdle(device_); idle_result != VK_SUCCESS) {
               std::cerr << "[VulkanGraphicsBackend] vkDeviceWaitIdle failed before swapchain recreation for '"
                         << window.window_id << "': " << string_VkResult(idle_result) << '\n';
               return std::unexpected(vve::Error::internal_error);
            }
            destroyWindowSwapchainResources(existing->second);
            window_swapchains_.erase(existing);
         }

         return createWindowSwapchain(window);
      }

      /// @brief Returns backend swapchain metadata for an already-created window swapchain.
      [[nodiscard]] std::expected<std::optional<WindowSwapchainResources>, vve::Error>
      windowSwapchain(SwapchainHandle swapchain) const {
         if (!swapchain.value.isValid()) {
            return std::unexpected(vve::Error::invalid_argument);
         }

         const auto swapchain_it = window_swapchains_.find(swapchain.value.value());
         if (swapchain_it == window_swapchains_.end()) {
            return std::optional<WindowSwapchainResources>{};
         }

         return swapchain_it->second.summary;
      }

      /// @brief Records clear work for one window's currently acquired swapchain image.
      [[nodiscard]] std::expected<void, vve::Error> recordWindowFrame(SwapchainHandle swapchain) {
         return recordWindowFrame(swapchain, WindowDrawPacketList{});
      }

      /// @brief Records draw work for one window's currently acquired swapchain image.
      [[nodiscard]] std::expected<void, vve::Error>
      recordWindowFrame(SwapchainHandle swapchain, const WindowDrawPacketList &draw_packets) {
         if (!initialized_) {
            return std::unexpected(vve::Error::not_initialized);
         }
         if (!swapchain.value.isValid()) {
            return std::unexpected(vve::Error::invalid_argument);
         }

         const auto swapchain_it = window_swapchains_.find(swapchain.value.value());
         if (swapchain_it == window_swapchains_.end()) {
            return std::unexpected(vve::Error::invalid_argument);
         }

         return recordWindowFrame(swapchain_it->second, draw_packets);
      }

      /// @brief Submits recorded clear work for one window's currently acquired swapchain image.
      [[nodiscard]] std::expected<void, vve::Error> submitWindowFrame(SwapchainHandle swapchain) {
         if (!initialized_) {
            return std::unexpected(vve::Error::not_initialized);
         }
         if (!swapchain.value.isValid()) {
            return std::unexpected(vve::Error::invalid_argument);
         }

         const auto swapchain_it = window_swapchains_.find(swapchain.value.value());
         if (swapchain_it == window_swapchains_.end()) {
            return std::unexpected(vve::Error::invalid_argument);
         }

         return submitWindowFrame(swapchain_it->second);
      }

      /// @brief Performs begin-frame backend work.
      [[nodiscard]] std::expected<void, vve::Error> beginFrame(const FrameContext &) {
         if (!initialized_) {
            return std::unexpected(vve::Error::not_initialized);
         }

         for (auto &[handle, resources] : window_swapchains_) {
            (void)handle;
            if (auto frame_result = acquireWindowFrame(resources); !frame_result) {
               return std::unexpected(frame_result.error());
            }
         }

         return {};
      }

      /// @brief Performs end-frame backend work.
      [[nodiscard]] std::expected<void, vve::Error> endFrame(const FrameContext &) {
         if (!initialized_) {
            return std::unexpected(vve::Error::not_initialized);
         }

         for (auto &[handle, resources] : window_swapchains_) {
            (void)handle;
            if (auto present_result = presentWindowFrame(resources); !present_result) {
               return std::unexpected(present_result.error());
            }
         }

         return {};
      }

      /// @brief Registers the backend's built-in frame-boundary tasks.
      void registerTasks(TaskGraphBuilder &builder) {
         [[maybe_unused]] const auto begin_frame_task = builder.addTask(
             "task.begin_frame", TaskKernelId::begin_frame,
             detail::requireFrame([this](const FrameContext &frame_context) { return beginFrame(frame_context); }),
             {}, {}, "Begin Frame", TaskPhase::begin_frame);
         [[maybe_unused]] const auto end_frame_task = builder.addTask(
             "task.end_frame", TaskKernelId::end_frame,
             detail::requireFrame([this](const FrameContext &frame_context) { return endFrame(frame_context); }), {},
             {}, "End Frame", TaskPhase::end_frame);
      }

   private:
      [[nodiscard]] std::expected<void, vve::Error> createLogicalDevice() {
         const float queue_priority = 1.0F;
         const VkDeviceQueueCreateInfo queue_create_info{.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                                                         .pNext = nullptr,
                                                         .flags = 0,
                                                         .queueFamilyIndex = graphics_queue_family_,
                                                         .queueCount = 1,
                                                         .pQueuePriorities = &queue_priority};
         const auto enabled_extensions = requiredDeviceExtensions(physical_device_, presentation_enabled_);
         if (!enabled_extensions) {
            return std::unexpected(enabled_extensions.error());
         }
         const VkDeviceCreateInfo create_info{.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
                                              .pNext = nullptr,
                                              .flags = 0,
                                              .queueCreateInfoCount = 1,
                                              .pQueueCreateInfos = &queue_create_info,
                                              .enabledLayerCount = 0,
                                              .ppEnabledLayerNames = nullptr,
                                              .enabledExtensionCount =
                                                  static_cast<std::uint32_t>(enabled_extensions->size()),
                                              .ppEnabledExtensionNames = enabled_extensions->data(),
                                              .pEnabledFeatures = nullptr};

         const VkResult result = vkCreateDevice(physical_device_, &create_info, nullptr, &device_);
         if (result != VK_SUCCESS) {
            std::cerr << "[VulkanGraphicsBackend] vkCreateDevice failed: " << string_VkResult(result) << '\n';
            return std::unexpected(vve::Error::internal_error);
         }

         vkGetDeviceQueue(device_, graphics_queue_family_, 0, &graphics_queue_);
         return {};
      }

      [[nodiscard]] std::expected<void, vve::Error> createPipelineCache() {
         const VkPipelineCacheCreateInfo create_info{.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO,
                                                     .pNext = nullptr,
                                                     .flags = 0,
                                                     .initialDataSize = 0,
                                                     .pInitialData = nullptr};
         const VkResult result = vkCreatePipelineCache(device_, &create_info, nullptr, &pipeline_cache_);
         if (result != VK_SUCCESS) {
            std::cerr << "[VulkanGraphicsBackend] vkCreatePipelineCache failed: " << string_VkResult(result)
                      << '\n';
            return std::unexpected(vve::Error::internal_error);
         }

         return {};
      }

      [[nodiscard]] std::expected<void, vve::Error> createCommandPool() {
         const VkCommandPoolCreateInfo create_info{.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
                                                   .pNext = nullptr,
                                                   .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
                                                   .queueFamilyIndex = graphics_queue_family_};
         const VkResult result = vkCreateCommandPool(device_, &create_info, nullptr, &command_pool_);
         if (result != VK_SUCCESS) {
            std::cerr << "[VulkanGraphicsBackend] vkCreateCommandPool failed: " << string_VkResult(result) << '\n';
            return std::unexpected(vve::Error::internal_error);
         }

         return {};
      }

      [[nodiscard]] std::expected<std::uint32_t, vve::Error>
      findMemoryType(std::uint32_t type_bits, VkMemoryPropertyFlags required_properties) const {
         VkPhysicalDeviceMemoryProperties memory_properties{};
         vkGetPhysicalDeviceMemoryProperties(physical_device_, &memory_properties);
         for (std::uint32_t memory_type = 0; memory_type < memory_properties.memoryTypeCount; ++memory_type) {
            const bool type_supported = (type_bits & (1U << memory_type)) != 0;
            const auto property_flags = memory_properties.memoryTypes[memory_type].propertyFlags;
            if (type_supported && (property_flags & required_properties) == required_properties) {
               return memory_type;
            }
         }

         return std::unexpected(vve::Error::internal_error);
      }

      [[nodiscard]] std::expected<void, vve::Error>
      createVulkanBuffer(VulkanGpuBufferResources &resources, VkBufferUsageFlags usage,
                         std::span<const std::byte> bytes) {
         const VkDeviceSize buffer_size = static_cast<VkDeviceSize>(bytes.size());
         const VkBufferCreateInfo buffer_info{.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                                              .pNext = nullptr,
                                              .flags = 0,
                                              .size = buffer_size,
                                              .usage = usage,
                                              .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
                                              .queueFamilyIndexCount = 0,
                                              .pQueueFamilyIndices = nullptr};

         VkResult result = vkCreateBuffer(device_, &buffer_info, nullptr, &resources.buffer);
         if (result != VK_SUCCESS) {
            std::cerr << "[VulkanGraphicsBackend] vkCreateBuffer failed: " << string_VkResult(result) << '\n';
            return std::unexpected(vve::Error::internal_error);
         }

         VkMemoryRequirements requirements{};
         vkGetBufferMemoryRequirements(device_, resources.buffer, &requirements);
         const auto memory_type = findMemoryType(requirements.memoryTypeBits,
                                                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                                     VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
         if (!memory_type) {
            return std::unexpected(memory_type.error());
         }

         const VkMemoryAllocateInfo allocate_info{.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
                                                  .pNext = nullptr,
                                                  .allocationSize = requirements.size,
                                                  .memoryTypeIndex = *memory_type};
         result = vkAllocateMemory(device_, &allocate_info, nullptr, &resources.memory);
         if (result != VK_SUCCESS) {
            std::cerr << "[VulkanGraphicsBackend] vkAllocateMemory(buffer) failed: " << string_VkResult(result)
                      << '\n';
            return std::unexpected(vve::Error::internal_error);
         }

         result = vkBindBufferMemory(device_, resources.buffer, resources.memory, 0);
         if (result != VK_SUCCESS) {
            std::cerr << "[VulkanGraphicsBackend] vkBindBufferMemory failed: " << string_VkResult(result) << '\n';
            return std::unexpected(vve::Error::internal_error);
         }

         void *mapped = nullptr;
         result = vkMapMemory(device_, resources.memory, 0, buffer_size, 0, &mapped);
         if (result != VK_SUCCESS || mapped == nullptr) {
            std::cerr << "[VulkanGraphicsBackend] vkMapMemory(buffer) failed: " << string_VkResult(result) << '\n';
            return std::unexpected(vve::Error::internal_error);
         }

         std::memcpy(mapped, bytes.data(), bytes.size());
         vkUnmapMemory(device_, resources.memory);
         return {};
      }

      [[nodiscard]] std::expected<void, vve::Error>
      createVulkanSampledImage(VulkanGpuImageResources &resources, VkFormat format,
                               std::span<const std::byte> bytes) {
         VulkanGpuBufferResources staging_buffer{};
         if (auto staging_result = createVulkanBuffer(staging_buffer, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, bytes);
             !staging_result) {
            destroyBufferResources(staging_buffer);
            return std::unexpected(staging_result.error());
         }

         const VkImageCreateInfo image_info{.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
                                            .pNext = nullptr,
                                            .flags = 0,
                                            .imageType = VK_IMAGE_TYPE_2D,
                                            .format = format,
                                            .extent = {.width = resources.summary.width,
                                                       .height = resources.summary.height,
                                                       .depth = 1},
                                            .mipLevels = resources.summary.mip_levels,
                                            .arrayLayers = resources.summary.array_layers,
                                            .samples = VK_SAMPLE_COUNT_1_BIT,
                                            .tiling = VK_IMAGE_TILING_OPTIMAL,
                                            .usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                                            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
                                            .queueFamilyIndexCount = 0,
                                            .pQueueFamilyIndices = nullptr,
                                            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED};
         VkResult result = vkCreateImage(device_, &image_info, nullptr, &resources.image);
         if (result != VK_SUCCESS) {
            destroyBufferResources(staging_buffer);
            std::cerr << "[VulkanGraphicsBackend] vkCreateImage(sampled texture) failed: "
                      << string_VkResult(result) << '\n';
            return std::unexpected(vve::Error::internal_error);
         }

         VkMemoryRequirements requirements{};
         vkGetImageMemoryRequirements(device_, resources.image, &requirements);
         const auto memory_type = findMemoryType(requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
         if (!memory_type) {
            destroyBufferResources(staging_buffer);
            return std::unexpected(memory_type.error());
         }

         const VkMemoryAllocateInfo allocate_info{.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
                                                  .pNext = nullptr,
                                                  .allocationSize = requirements.size,
                                                  .memoryTypeIndex = *memory_type};
         result = vkAllocateMemory(device_, &allocate_info, nullptr, &resources.memory);
         if (result != VK_SUCCESS) {
            destroyBufferResources(staging_buffer);
            std::cerr << "[VulkanGraphicsBackend] vkAllocateMemory(sampled texture) failed: "
                      << string_VkResult(result) << '\n';
            return std::unexpected(vve::Error::internal_error);
         }

         result = vkBindImageMemory(device_, resources.image, resources.memory, 0);
         if (result != VK_SUCCESS) {
            destroyBufferResources(staging_buffer);
            std::cerr << "[VulkanGraphicsBackend] vkBindImageMemory(sampled texture) failed: "
                      << string_VkResult(result) << '\n';
            return std::unexpected(vve::Error::internal_error);
         }

         if (auto upload_result = uploadSampledImage(resources, staging_buffer.buffer); !upload_result) {
            destroyBufferResources(staging_buffer);
            return std::unexpected(upload_result.error());
         }
         destroyBufferResources(staging_buffer);

         const VkImageViewCreateInfo view_info{.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                                               .pNext = nullptr,
                                               .flags = 0,
                                               .image = resources.image,
                                               .viewType = VK_IMAGE_VIEW_TYPE_2D,
                                               .format = format,
                                               .components = {.r = VK_COMPONENT_SWIZZLE_IDENTITY,
                                                              .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                                                              .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                                                              .a = VK_COMPONENT_SWIZZLE_IDENTITY},
                                               .subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                                                                    .baseMipLevel = 0,
                                                                    .levelCount = resources.summary.mip_levels,
                                                                    .baseArrayLayer = 0,
                                                                    .layerCount = resources.summary.array_layers}};
         result = vkCreateImageView(device_, &view_info, nullptr, &resources.image_view);
         if (result != VK_SUCCESS) {
            std::cerr << "[VulkanGraphicsBackend] vkCreateImageView(sampled texture) failed: "
                      << string_VkResult(result) << '\n';
            return std::unexpected(vve::Error::internal_error);
         }

         const VkSamplerCreateInfo sampler_info{.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
                                                .pNext = nullptr,
                                                .flags = 0,
                                                .magFilter = VK_FILTER_LINEAR,
                                                .minFilter = VK_FILTER_LINEAR,
                                                .mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST,
                                                .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
                                                .addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
                                                .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
                                                .mipLodBias = 0.0F,
                                                .anisotropyEnable = VK_FALSE,
                                                .maxAnisotropy = 1.0F,
                                                .compareEnable = VK_FALSE,
                                                .compareOp = VK_COMPARE_OP_ALWAYS,
                                                .minLod = 0.0F,
                                                .maxLod = 0.0F,
                                                .borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE,
                                                .unnormalizedCoordinates = VK_FALSE};
         result = vkCreateSampler(device_, &sampler_info, nullptr, &resources.sampler);
         if (result != VK_SUCCESS) {
            std::cerr << "[VulkanGraphicsBackend] vkCreateSampler(sampled texture) failed: "
                      << string_VkResult(result) << '\n';
            return std::unexpected(vve::Error::internal_error);
         }

         resources.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
         return {};
      }

      [[nodiscard]] std::expected<void, vve::Error>
      uploadSampledImage(const VulkanGpuImageResources &resources, VkBuffer staging_buffer) {
         if (resources.image == VK_NULL_HANDLE || staging_buffer == VK_NULL_HANDLE ||
             command_pool_ == VK_NULL_HANDLE || graphics_queue_ == VK_NULL_HANDLE) {
            return std::unexpected(vve::Error::invalid_argument);
         }

         VkCommandBuffer command_buffer = VK_NULL_HANDLE;
         const VkCommandBufferAllocateInfo allocate_info{
             .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
             .pNext = nullptr,
             .commandPool = command_pool_,
             .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
             .commandBufferCount = 1};
         VkResult result = vkAllocateCommandBuffers(device_, &allocate_info, &command_buffer);
         if (result != VK_SUCCESS) {
            std::cerr << "[VulkanGraphicsBackend] vkAllocateCommandBuffers(sampled texture) failed: "
                      << string_VkResult(result) << '\n';
            return std::unexpected(vve::Error::internal_error);
         }

         const VkCommandBufferBeginInfo begin_info{.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                                                   .pNext = nullptr,
                                                   .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
                                                   .pInheritanceInfo = nullptr};
         result = vkBeginCommandBuffer(command_buffer, &begin_info);
         if (result != VK_SUCCESS) {
            vkFreeCommandBuffers(device_, command_pool_, 1, &command_buffer);
            std::cerr << "[VulkanGraphicsBackend] vkBeginCommandBuffer(sampled texture) failed: "
                      << string_VkResult(result) << '\n';
            return std::unexpected(vve::Error::internal_error);
         }

         const VkImageSubresourceRange range{.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                                             .baseMipLevel = 0,
                                             .levelCount = resources.summary.mip_levels,
                                             .baseArrayLayer = 0,
                                             .layerCount = resources.summary.array_layers};
         const VkImageMemoryBarrier to_transfer{.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                                                .pNext = nullptr,
                                                .srcAccessMask = 0,
                                                .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
                                                .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                                                .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                                                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                                                .image = resources.image,
                                                .subresourceRange = range};
         vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
                              0, nullptr, 0, nullptr, 1, &to_transfer);

         const VkBufferImageCopy copy_region{.bufferOffset = 0,
                                             .bufferRowLength = 0,
                                             .bufferImageHeight = 0,
                                             .imageSubresource = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                                                                  .mipLevel = 0,
                                                                  .baseArrayLayer = 0,
                                                                  .layerCount = resources.summary.array_layers},
                                             .imageOffset = {.x = 0, .y = 0, .z = 0},
                                             .imageExtent = {.width = resources.summary.width,
                                                             .height = resources.summary.height,
                                                             .depth = 1}};
         vkCmdCopyBufferToImage(command_buffer, staging_buffer, resources.image,
                                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy_region);

         const VkImageMemoryBarrier to_shader_read{.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                                                   .pNext = nullptr,
                                                   .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
                                                   .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
                                                   .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                                   .newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                                   .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                                                   .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                                                   .image = resources.image,
                                                   .subresourceRange = range};
         vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                              VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
                              0, nullptr, 0, nullptr, 1, &to_shader_read);

         result = vkEndCommandBuffer(command_buffer);
         if (result != VK_SUCCESS) {
            vkFreeCommandBuffers(device_, command_pool_, 1, &command_buffer);
            std::cerr << "[VulkanGraphicsBackend] vkEndCommandBuffer(sampled texture) failed: "
                      << string_VkResult(result) << '\n';
            return std::unexpected(vve::Error::internal_error);
         }

         const VkSubmitInfo submit_info{.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                                        .pNext = nullptr,
                                        .waitSemaphoreCount = 0,
                                        .pWaitSemaphores = nullptr,
                                        .pWaitDstStageMask = nullptr,
                                        .commandBufferCount = 1,
                                        .pCommandBuffers = &command_buffer,
                                        .signalSemaphoreCount = 0,
                                        .pSignalSemaphores = nullptr};
         result = vkQueueSubmit(graphics_queue_, 1, &submit_info, VK_NULL_HANDLE);
         if (result == VK_SUCCESS) {
            result = vkQueueWaitIdle(graphics_queue_);
         }
         vkFreeCommandBuffers(device_, command_pool_, 1, &command_buffer);
         if (result != VK_SUCCESS) {
            std::cerr << "[VulkanGraphicsBackend] sampled texture upload failed: " << string_VkResult(result)
                      << '\n';
            return std::unexpected(vve::Error::internal_error);
         }

         return {};
      }

      [[nodiscard]] std::expected<void, vve::Error> createShaderModules(VulkanPipelineResources &resources,
                                                                        const ShaderMetadata &shader) {
         resources.shader_modules.reserve(shader.binaries.size());
         for (const auto &binary : shader.binaries) {
            if (binary.spirv_words.empty()) {
               return std::unexpected(vve::Error::invalid_argument);
            }

            const VkShaderModuleCreateInfo create_info{.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
                                                       .pNext = nullptr,
                                                       .flags = 0,
                                                       .codeSize = binary.spirv_words.size() *
                                                                   sizeof(binary.spirv_words.front()),
                                                       .pCode = binary.spirv_words.data()};
            VkShaderModule shader_module = VK_NULL_HANDLE;
            const VkResult result = vkCreateShaderModule(device_, &create_info, nullptr, &shader_module);
            if (result != VK_SUCCESS) {
               std::cerr << "[VulkanGraphicsBackend] vkCreateShaderModule failed: " << string_VkResult(result)
                         << '\n';
               return std::unexpected(vve::Error::internal_error);
            }

            // Slang emits one SPIR-V module per requested entry point here, and
            // the Vulkan OpEntryPoint name inside each generated module is "main".
            resources.shader_modules.push_back(VulkanShaderModule{.module = shader_module,
                                                                  .stage = binary.stage,
                                                                  .entry_point = "main"});
         }

         return {};
      }

      [[nodiscard]] std::expected<void, vve::Error>
      createDescriptorSetLayouts(VulkanPipelineResources &resources, const PipelineLayoutDesc &layout) {
         resources.descriptor_set_layouts.reserve(layout.descriptor_sets.size());
         resources.descriptor_bindings.reserve(layout.descriptor_sets.size());
         for (const auto &descriptor_set : layout.descriptor_sets) {
            std::map<std::uint32_t, VkDescriptorSetLayoutBinding> bindings_by_index{};
            std::map<std::uint32_t, PipelineDescriptorBindingDesc> binding_descs_by_index{};
            for (const auto &binding : descriptor_set.bindings) {
               const auto type = descriptorType(binding.kind);
               if (!type.has_value()) {
                  continue;
               }

               auto stage_flags = shaderStageFlags(binding.visible_stages);
               if (stage_flags == 0) {
                  std::vector<ShaderStage> layout_stages{};
                  layout_stages.reserve(layout.shader_stages.size());
                  for (const auto &shader_stage : layout.shader_stages) {
                     layout_stages.push_back(shader_stage.stage);
                  }
                  stage_flags = shaderStageFlags(layout_stages);
               }

               VkDescriptorSetLayoutBinding vk_binding{.binding = binding.binding,
                                                       .descriptorType = *type,
                                                       .descriptorCount = 1,
                                                       .stageFlags = stage_flags,
                                                       .pImmutableSamplers = nullptr};
               if (auto existing = bindings_by_index.find(binding.binding); existing != bindings_by_index.end()) {
                  if (existing->second.descriptorType != vk_binding.descriptorType) {
                     std::cerr << "[VulkanGraphicsBackend] conflicting descriptor types for set "
                               << descriptor_set.set << " binding " << binding.binding << '\n';
                     return std::unexpected(vve::Error::invalid_argument);
                  }
                  existing->second.stageFlags |= vk_binding.stageFlags;
                  continue;
               }

               bindings_by_index.emplace(binding.binding, vk_binding);
               binding_descs_by_index.emplace(binding.binding, binding);
            }

            std::vector<VkDescriptorSetLayoutBinding> bindings{};
            std::vector<PipelineDescriptorBindingDesc> binding_descs{};
            bindings.reserve(bindings_by_index.size());
            for (const auto &[binding_index, binding] : bindings_by_index) {
               (void)binding_index;
               bindings.push_back(binding);
            }
            binding_descs.reserve(binding_descs_by_index.size());
            for (const auto &[binding_index, binding_desc] : binding_descs_by_index) {
               (void)binding_index;
               binding_descs.push_back(binding_desc);
            }

            const VkDescriptorSetLayoutCreateInfo create_info{
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
                .pNext = nullptr,
                .flags = 0,
                .bindingCount = static_cast<std::uint32_t>(bindings.size()),
                .pBindings = bindings.data()};
            VkDescriptorSetLayout descriptor_set_layout = VK_NULL_HANDLE;
            const VkResult result = vkCreateDescriptorSetLayout(device_, &create_info, nullptr, &descriptor_set_layout);
            if (result != VK_SUCCESS) {
               std::cerr << "[VulkanGraphicsBackend] vkCreateDescriptorSetLayout failed: " << string_VkResult(result)
                         << '\n';
               return std::unexpected(vve::Error::internal_error);
            }

            resources.descriptor_set_layouts.push_back(descriptor_set_layout);
            resources.descriptor_bindings.push_back(std::move(binding_descs));
         }

         return {};
      }

      [[nodiscard]] std::expected<void, vve::Error>
      createDescriptorPoolAndSets(VulkanPipelineResources &resources) {
         if (resources.descriptor_set_layouts.empty()) {
            return {};
         }

         std::map<VkDescriptorType, std::uint32_t> descriptor_counts{};
         for (const auto &descriptor_set : resources.descriptor_bindings) {
            for (const auto &binding : descriptor_set) {
               const auto type = descriptorType(binding.kind);
               if (type.has_value()) {
                  ++descriptor_counts[*type];
               }
            }
         }
         if (descriptor_counts.empty()) {
            return {};
         }

         std::vector<VkDescriptorPoolSize> pool_sizes{};
         pool_sizes.reserve(descriptor_counts.size());
         const auto descriptor_copy_capacity = maxDrawDescriptorSetCopies + 1U;
         for (const auto &[type, count] : descriptor_counts) {
            pool_sizes.push_back(VkDescriptorPoolSize{.type = type,
                                                      .descriptorCount = count * descriptor_copy_capacity});
         }

         const VkDescriptorPoolCreateInfo pool_info{.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
                                                    .pNext = nullptr,
                                                    .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
                                                    .maxSets = static_cast<std::uint32_t>(
                                                        resources.descriptor_set_layouts.size()) *
                                                               descriptor_copy_capacity,
                                                    .poolSizeCount =
                                                        static_cast<std::uint32_t>(pool_sizes.size()),
                                                    .pPoolSizes = pool_sizes.data()};
         VkResult result = vkCreateDescriptorPool(device_, &pool_info, nullptr, &resources.descriptor_pool);
         if (result != VK_SUCCESS) {
            std::cerr << "[VulkanGraphicsBackend] vkCreateDescriptorPool failed: " << string_VkResult(result)
                      << '\n';
            return std::unexpected(vve::Error::internal_error);
         }

         resources.descriptor_sets.resize(resources.descriptor_set_layouts.size(), VK_NULL_HANDLE);
         const VkDescriptorSetAllocateInfo allocate_info{.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
                                                         .pNext = nullptr,
                                                         .descriptorPool = resources.descriptor_pool,
                                                         .descriptorSetCount =
                                                             static_cast<std::uint32_t>(resources.descriptor_sets.size()),
                                                         .pSetLayouts = resources.descriptor_set_layouts.data()};
         result = vkAllocateDescriptorSets(device_, &allocate_info, resources.descriptor_sets.data());
         if (result != VK_SUCCESS) {
            std::cerr << "[VulkanGraphicsBackend] vkAllocateDescriptorSets failed: " << string_VkResult(result)
                      << '\n';
            return std::unexpected(vve::Error::internal_error);
         }

         return {};
      }

      [[nodiscard]] std::expected<std::size_t, vve::Error>
      createFallbackDescriptorBuffer(VulkanPipelineResources &resources, VkDescriptorType descriptor_type,
                                     const PipelineDescriptorBindingDesc &binding) {
         VkBufferUsageFlags usage = 0;
         if (descriptor_type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER) {
            usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
         } else if (descriptor_type == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER) {
            usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
         } else {
            return std::unexpected(vve::Error::invalid_argument);
         }

         const auto bytes = fallbackDescriptorBufferBytes(binding);
         VulkanGpuBufferResources buffer_resource{};
         if (auto create_result = createVulkanBuffer(buffer_resource, usage, std::span<const std::byte>{bytes});
             !create_result) {
            destroyBufferResources(buffer_resource);
            return std::unexpected(create_result.error());
         }

         const auto index = resources.descriptor_buffers.size();
         resources.descriptor_buffers.push_back(VulkanDescriptorBufferResource{
             .buffer = buffer_resource.buffer,
             .memory = buffer_resource.memory,
             .size = static_cast<VkDeviceSize>(bytes.size()),
             .descriptor_type = descriptor_type,
             .binding = binding});
         buffer_resource.buffer = VK_NULL_HANDLE;
         buffer_resource.memory = VK_NULL_HANDLE;
         return index;
      }

      [[nodiscard]] std::expected<void, vve::Error>
      updateDescriptorBufferBytes(const VulkanDescriptorBufferResource &resource, std::span<const std::byte> bytes) {
         if (resource.memory == VK_NULL_HANDLE || resource.size < bytes.size()) {
            return std::unexpected(vve::Error::invalid_argument);
         }

         void *mapped = nullptr;
         const VkResult result =
             vkMapMemory(device_, resource.memory, 0, static_cast<VkDeviceSize>(bytes.size()), 0, &mapped);
         if (result != VK_SUCCESS || mapped == nullptr) {
            std::cerr << "[VulkanGraphicsBackend] vkMapMemory(frame constants) failed: "
                      << string_VkResult(result) << '\n';
            return std::unexpected(vve::Error::internal_error);
         }

         std::memcpy(mapped, bytes.data(), bytes.size());
         vkUnmapMemory(device_, resource.memory);
         return {};
      }

      [[nodiscard]] std::expected<void, vve::Error>
      updateFrameDescriptorBuffers(VulkanPipelineResources &resources, VkExtent2D extent) {
         const auto constants = frameConstantsForExtent(extent);
         std::array<std::byte, sizeof(constants)> bytes{};
         std::memcpy(bytes.data(), &constants, sizeof(constants));

         for (const auto &buffer : resources.descriptor_buffers) {
            if (buffer.descriptor_type != VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER ||
                !isFrameConstantsBinding(buffer.binding)) {
               continue;
            }

            if (auto update_result = updateDescriptorBufferBytes(buffer, bytes); !update_result) {
               return std::unexpected(update_result.error());
            }
         }

         return {};
      }

      [[nodiscard]] std::expected<void, vve::Error>
      initializeFallbackImage(VkImage image, VkClearColorValue clear_color) {
         if (command_pool_ == VK_NULL_HANDLE || graphics_queue_ == VK_NULL_HANDLE || image == VK_NULL_HANDLE) {
            return std::unexpected(vve::Error::invalid_argument);
         }

         VkCommandBuffer command_buffer = VK_NULL_HANDLE;
         const VkCommandBufferAllocateInfo allocate_info{
             .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
             .pNext = nullptr,
             .commandPool = command_pool_,
             .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
             .commandBufferCount = 1};
         VkResult result = vkAllocateCommandBuffers(device_, &allocate_info, &command_buffer);
         if (result != VK_SUCCESS) {
            std::cerr << "[VulkanGraphicsBackend] vkAllocateCommandBuffers(fallback image) failed: "
                      << string_VkResult(result) << '\n';
            return std::unexpected(vve::Error::internal_error);
         }

         const VkCommandBufferBeginInfo begin_info{.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                                                   .pNext = nullptr,
                                                   .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
                                                   .pInheritanceInfo = nullptr};
         result = vkBeginCommandBuffer(command_buffer, &begin_info);
         if (result != VK_SUCCESS) {
            vkFreeCommandBuffers(device_, command_pool_, 1, &command_buffer);
            std::cerr << "[VulkanGraphicsBackend] vkBeginCommandBuffer(fallback image) failed: "
                      << string_VkResult(result) << '\n';
            return std::unexpected(vve::Error::internal_error);
         }

         const VkImageSubresourceRange range{.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                                             .baseMipLevel = 0,
                                             .levelCount = 1,
                                             .baseArrayLayer = 0,
                                             .layerCount = 1};
         const VkImageMemoryBarrier to_transfer{.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                                                .pNext = nullptr,
                                                .srcAccessMask = 0,
                                                .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
                                                .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                                                .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                                                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                                                .image = image,
                                                .subresourceRange = range};
         vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
                              0, nullptr, 0, nullptr, 1, &to_transfer);
         vkCmdClearColorImage(command_buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clear_color, 1, &range);
         const VkImageMemoryBarrier to_shader_read{.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                                                   .pNext = nullptr,
                                                   .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
                                                   .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
                                                   .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                                   .newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                                   .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                                                   .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                                                   .image = image,
                                                   .subresourceRange = range};
         vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                              VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
                              0, nullptr, 0, nullptr, 1, &to_shader_read);

         result = vkEndCommandBuffer(command_buffer);
         if (result != VK_SUCCESS) {
            vkFreeCommandBuffers(device_, command_pool_, 1, &command_buffer);
            std::cerr << "[VulkanGraphicsBackend] vkEndCommandBuffer(fallback image) failed: "
                      << string_VkResult(result) << '\n';
            return std::unexpected(vve::Error::internal_error);
         }

         const VkSubmitInfo submit_info{.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                                        .pNext = nullptr,
                                        .waitSemaphoreCount = 0,
                                        .pWaitSemaphores = nullptr,
                                        .pWaitDstStageMask = nullptr,
                                        .commandBufferCount = 1,
                                        .pCommandBuffers = &command_buffer,
                                        .signalSemaphoreCount = 0,
                                        .pSignalSemaphores = nullptr};
         result = vkQueueSubmit(graphics_queue_, 1, &submit_info, VK_NULL_HANDLE);
         if (result == VK_SUCCESS) {
            result = vkQueueWaitIdle(graphics_queue_);
         }
         vkFreeCommandBuffers(device_, command_pool_, 1, &command_buffer);
         if (result != VK_SUCCESS) {
            std::cerr << "[VulkanGraphicsBackend] fallback image initialization failed: "
                      << string_VkResult(result) << '\n';
            return std::unexpected(vve::Error::internal_error);
         }

         return {};
      }

      [[nodiscard]] std::expected<std::size_t, vve::Error>
      createFallbackSampledImage(VulkanPipelineResources &resources,
                                 const PipelineDescriptorBindingDesc &binding) {
         VulkanDescriptorImageResource image_resource{};
         image_resource.binding = binding;
         const VkImageCreateInfo image_info{.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
                                            .pNext = nullptr,
                                            .flags = 0,
                                            .imageType = VK_IMAGE_TYPE_2D,
                                            .format = VK_FORMAT_R8G8B8A8_UNORM,
                                            .extent = {.width = 1, .height = 1, .depth = 1},
                                            .mipLevels = 1,
                                            .arrayLayers = 1,
                                            .samples = VK_SAMPLE_COUNT_1_BIT,
                                            .tiling = VK_IMAGE_TILING_OPTIMAL,
                                            .usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                                            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
                                            .queueFamilyIndexCount = 0,
                                            .pQueueFamilyIndices = nullptr,
                                            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED};
         VkResult result = vkCreateImage(device_, &image_info, nullptr, &image_resource.image);
         if (result != VK_SUCCESS) {
            std::cerr << "[VulkanGraphicsBackend] vkCreateImage(fallback texture) failed: "
                      << string_VkResult(result) << '\n';
            return std::unexpected(vve::Error::internal_error);
         }

         VkMemoryRequirements requirements{};
         vkGetImageMemoryRequirements(device_, image_resource.image, &requirements);
         const auto memory_type = findMemoryType(requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
         if (!memory_type) {
            vkDestroyImage(device_, image_resource.image, nullptr);
            return std::unexpected(memory_type.error());
         }

         const VkMemoryAllocateInfo allocate_info{.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
                                                  .pNext = nullptr,
                                                  .allocationSize = requirements.size,
                                                  .memoryTypeIndex = *memory_type};
         result = vkAllocateMemory(device_, &allocate_info, nullptr, &image_resource.memory);
         if (result != VK_SUCCESS) {
            vkDestroyImage(device_, image_resource.image, nullptr);
            std::cerr << "[VulkanGraphicsBackend] vkAllocateMemory(fallback texture) failed: "
                      << string_VkResult(result) << '\n';
            return std::unexpected(vve::Error::internal_error);
         }

         result = vkBindImageMemory(device_, image_resource.image, image_resource.memory, 0);
         if (result != VK_SUCCESS) {
            vkFreeMemory(device_, image_resource.memory, nullptr);
            vkDestroyImage(device_, image_resource.image, nullptr);
            std::cerr << "[VulkanGraphicsBackend] vkBindImageMemory(fallback texture) failed: "
                      << string_VkResult(result) << '\n';
            return std::unexpected(vve::Error::internal_error);
         }

         const VkImageViewCreateInfo view_info{.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                                               .pNext = nullptr,
                                               .flags = 0,
                                               .image = image_resource.image,
                                               .viewType = VK_IMAGE_VIEW_TYPE_2D,
                                               .format = VK_FORMAT_R8G8B8A8_UNORM,
                                               .components = {.r = VK_COMPONENT_SWIZZLE_IDENTITY,
                                                              .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                                                              .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                                                              .a = VK_COMPONENT_SWIZZLE_IDENTITY},
                                               .subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                                                                    .baseMipLevel = 0,
                                                                    .levelCount = 1,
                                                                    .baseArrayLayer = 0,
                                                                    .layerCount = 1}};
         result = vkCreateImageView(device_, &view_info, nullptr, &image_resource.image_view);
         if (result != VK_SUCCESS) {
            vkFreeMemory(device_, image_resource.memory, nullptr);
            vkDestroyImage(device_, image_resource.image, nullptr);
            std::cerr << "[VulkanGraphicsBackend] vkCreateImageView(fallback texture) failed: "
                      << string_VkResult(result) << '\n';
            return std::unexpected(vve::Error::internal_error);
         }

         if (auto init_result = initializeFallbackImage(image_resource.image, fallbackTextureClearColor(binding));
             !init_result) {
            vkDestroyImageView(device_, image_resource.image_view, nullptr);
            vkFreeMemory(device_, image_resource.memory, nullptr);
            vkDestroyImage(device_, image_resource.image, nullptr);
            return std::unexpected(init_result.error());
         }

         const auto index = resources.descriptor_images.size();
         resources.descriptor_images.push_back(image_resource);
         return index;
      }

      [[nodiscard]] std::expected<std::size_t, vve::Error>
      createFallbackSampler(VulkanPipelineResources &resources, const PipelineDescriptorBindingDesc &binding) {
         const VkSamplerCreateInfo sampler_info{.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
                                                .pNext = nullptr,
                                                .flags = 0,
                                                .magFilter = VK_FILTER_LINEAR,
                                                .minFilter = VK_FILTER_LINEAR,
                                                .mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST,
                                                .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
                                                .addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
                                                .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
                                                .mipLodBias = 0.0F,
                                                .anisotropyEnable = VK_FALSE,
                                                .maxAnisotropy = 1.0F,
                                                .compareEnable = VK_FALSE,
                                                .compareOp = VK_COMPARE_OP_ALWAYS,
                                                .minLod = 0.0F,
                                                .maxLod = 0.0F,
                                                .borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE,
                                                .unnormalizedCoordinates = VK_FALSE};
         VkSampler sampler = VK_NULL_HANDLE;
         const VkResult result = vkCreateSampler(device_, &sampler_info, nullptr, &sampler);
         if (result != VK_SUCCESS) {
            std::cerr << "[VulkanGraphicsBackend] vkCreateSampler(fallback) failed: " << string_VkResult(result)
                      << '\n';
            return std::unexpected(vve::Error::internal_error);
         }

         const auto index = resources.descriptor_samplers.size();
         resources.descriptor_samplers.push_back(VulkanDescriptorSamplerResource{.sampler = sampler,
                                                                                 .binding = binding});
         return index;
      }

      [[nodiscard]] std::expected<void, vve::Error>
      updateFallbackDescriptorSets(VulkanPipelineResources &resources) {
         if (resources.descriptor_sets.empty()) {
            return {};
         }

         std::size_t descriptor_count = 0;
         for (const auto &descriptor_set : resources.descriptor_bindings) {
            descriptor_count += descriptor_set.size();
         }

         std::vector<VkDescriptorBufferInfo> buffer_infos{};
         std::vector<VkDescriptorImageInfo> image_infos{};
         std::vector<VkWriteDescriptorSet> writes{};
         buffer_infos.reserve(descriptor_count);
         image_infos.reserve(descriptor_count);
         writes.reserve(descriptor_count);

         for (std::size_t set_index = 0; set_index < resources.descriptor_bindings.size(); ++set_index) {
            if (set_index >= resources.descriptor_sets.size() ||
                resources.descriptor_sets[set_index] == VK_NULL_HANDLE) {
               return std::unexpected(vve::Error::invalid_argument);
            }

            for (const auto &binding : resources.descriptor_bindings[set_index]) {
               const auto type = descriptorType(binding.kind);
               if (!type.has_value()) {
                  continue;
               }

               VkDescriptorBufferInfo *buffer_info = nullptr;
               VkDescriptorImageInfo *image_info = nullptr;
               if (*type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER || *type == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER) {
                  const auto buffer_index = createFallbackDescriptorBuffer(resources, *type, binding);
                  if (!buffer_index) {
                     return std::unexpected(buffer_index.error());
                  }
                  const auto &buffer = resources.descriptor_buffers[*buffer_index];
                  buffer_infos.push_back(VkDescriptorBufferInfo{.buffer = buffer.buffer,
                                                                .offset = 0,
                                                                .range = buffer.size});
                  buffer_info = &buffer_infos.back();
               } else if (*type == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE) {
                  const auto image_index = createFallbackSampledImage(resources, binding);
                  if (!image_index) {
                     return std::unexpected(image_index.error());
                  }
                  const auto &image = resources.descriptor_images[*image_index];
                  image_infos.push_back(VkDescriptorImageInfo{.sampler = VK_NULL_HANDLE,
                                                              .imageView = image.image_view,
                                                              .imageLayout = image.layout});
                  image_info = &image_infos.back();
               } else if (*type == VK_DESCRIPTOR_TYPE_SAMPLER) {
                  const auto sampler_index = createFallbackSampler(resources, binding);
                  if (!sampler_index) {
                     return std::unexpected(sampler_index.error());
                  }
                  image_infos.push_back(VkDescriptorImageInfo{.sampler = resources.descriptor_samplers[*sampler_index].sampler,
                                                              .imageView = VK_NULL_HANDLE,
                                                              .imageLayout = VK_IMAGE_LAYOUT_UNDEFINED});
                  image_info = &image_infos.back();
               } else {
                  std::cerr << "[VulkanGraphicsBackend] unsupported fallback descriptor type for binding "
                            << binding.binding << '\n';
                  return std::unexpected(vve::Error::invalid_argument);
               }

               writes.push_back(VkWriteDescriptorSet{.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                                                     .pNext = nullptr,
                                                     .dstSet = resources.descriptor_sets[set_index],
                                                     .dstBinding = binding.binding,
                                                     .dstArrayElement = 0,
                                                     .descriptorCount = 1,
                                                     .descriptorType = *type,
                                                     .pImageInfo = image_info,
                                                     .pBufferInfo = buffer_info,
                                                     .pTexelBufferView = nullptr});
            }
         }

         if (!writes.empty()) {
            vkUpdateDescriptorSets(device_, static_cast<std::uint32_t>(writes.size()), writes.data(), 0, nullptr);
         }
         return {};
      }

      [[nodiscard]] const VulkanDescriptorBufferResource *
      fallbackDescriptorBuffer(const VulkanPipelineResources &resources,
                               const PipelineDescriptorBindingDesc &binding,
                               VkDescriptorType descriptor_type) const {
         for (const auto &buffer : resources.descriptor_buffers) {
            if (buffer.descriptor_type == descriptor_type && sameDescriptorBinding(buffer.binding, binding) &&
                buffer.buffer != VK_NULL_HANDLE) {
               return std::addressof(buffer);
            }
         }

         return nullptr;
      }

      [[nodiscard]] const VulkanDescriptorImageResource *
      fallbackDescriptorImage(const VulkanPipelineResources &resources,
                              const PipelineDescriptorBindingDesc &binding) const {
         for (const auto &image : resources.descriptor_images) {
            if (sameDescriptorBinding(image.binding, binding) && image.image_view != VK_NULL_HANDLE) {
               return std::addressof(image);
            }
         }

         return nullptr;
      }

      [[nodiscard]] const VulkanDescriptorSamplerResource *
      fallbackDescriptorSampler(const VulkanPipelineResources &resources,
                                const PipelineDescriptorBindingDesc &binding) const {
         for (const auto &sampler : resources.descriptor_samplers) {
            if (sameDescriptorBinding(sampler.binding, binding) && sampler.sampler != VK_NULL_HANDLE) {
               return std::addressof(sampler);
            }
         }

         return nullptr;
      }

      [[nodiscard]] const VulkanGpuBufferResources *
      uploadedBuffer(GpuBufferHandle handle, GpuBufferUsage expected_usage) const {
         if (!handle.value.isValid()) {
            return nullptr;
         }

         const auto buffer = buffers_.find(handle.value.value());
         if (buffer == buffers_.end() || buffer->second.buffer == VK_NULL_HANDLE ||
             buffer->second.summary.usage != expected_usage) {
            return nullptr;
         }

         return std::addressof(buffer->second);
      }

      [[nodiscard]] const VulkanGpuImageResources *
      uploadedMaterialImage(const GpuMaterialTextureBinding *binding) const {
         if (binding == nullptr || !binding->image.value.isValid() || !binding->sampler.value.isValid()) {
            return nullptr;
         }

         const auto image = images_.find(binding->image.value.value());
         if (image == images_.end() || image->second.image_view == VK_NULL_HANDLE ||
             image->second.sampler == VK_NULL_HANDLE || !image->second.summary.resident ||
             image->second.summary.sampler.value != binding->sampler.value) {
            return nullptr;
         }

         return std::addressof(image->second);
      }

      [[nodiscard]] std::expected<void, vve::Error>
      writeDrawDescriptorSets(VulkanPipelineResources &resources, const std::vector<VkDescriptorSet> &descriptor_sets,
                              const DrawPacket &packet) {
         std::size_t descriptor_count = 0;
         for (const auto &descriptor_set : resources.descriptor_bindings) {
            descriptor_count += descriptor_set.size();
         }

         std::vector<VkDescriptorBufferInfo> buffer_infos{};
         std::vector<VkDescriptorImageInfo> image_infos{};
         std::vector<VkWriteDescriptorSet> writes{};
         buffer_infos.reserve(descriptor_count);
         image_infos.reserve(descriptor_count);
         writes.reserve(descriptor_count);

         for (std::size_t set_index = 0; set_index < resources.descriptor_bindings.size(); ++set_index) {
            if (set_index >= descriptor_sets.size() || descriptor_sets[set_index] == VK_NULL_HANDLE) {
               return std::unexpected(vve::Error::invalid_argument);
            }

            for (const auto &binding : resources.descriptor_bindings[set_index]) {
               const auto type = descriptorType(binding.kind);
               if (!type.has_value()) {
                  continue;
               }

               VkDescriptorBufferInfo *buffer_info = nullptr;
               VkDescriptorImageInfo *image_info = nullptr;
               if (*type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER || *type == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER) {
                  const auto *uploaded_material_buffer =
                      *type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER && isMaterialConstantsBinding(binding)
                          ? uploadedBuffer(packet.material_constants_buffer, GpuBufferUsage::uniform)
                          : nullptr;
                  if (uploaded_material_buffer != nullptr) {
                     buffer_infos.push_back(VkDescriptorBufferInfo{
                         .buffer = uploaded_material_buffer->buffer,
                         .offset = 0,
                         .range = static_cast<VkDeviceSize>(uploaded_material_buffer->summary.byte_size)});
                  } else {
                     const auto *fallback_buffer = fallbackDescriptorBuffer(resources, binding, *type);
                     if (fallback_buffer == nullptr) {
                        return std::unexpected(vve::Error::invalid_argument);
                     }
                     buffer_infos.push_back(VkDescriptorBufferInfo{.buffer = fallback_buffer->buffer,
                                                                   .offset = 0,
                                                                   .range = fallback_buffer->size});
                  }
                  buffer_info = &buffer_infos.back();
               } else if (*type == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE) {
                  const auto *uploaded_image = uploadedMaterialImage(materialTextureForBinding(packet, binding));
                  if (uploaded_image != nullptr) {
                     image_infos.push_back(VkDescriptorImageInfo{.sampler = VK_NULL_HANDLE,
                                                                 .imageView = uploaded_image->image_view,
                                                                 .imageLayout = uploaded_image->layout});
                  } else {
                     const auto *fallback_image = fallbackDescriptorImage(resources, binding);
                     if (fallback_image == nullptr) {
                        return std::unexpected(vve::Error::invalid_argument);
                     }
                     image_infos.push_back(VkDescriptorImageInfo{.sampler = VK_NULL_HANDLE,
                                                                 .imageView = fallback_image->image_view,
                                                                 .imageLayout = fallback_image->layout});
                  }
                  image_info = &image_infos.back();
               } else if (*type == VK_DESCRIPTOR_TYPE_SAMPLER) {
                  const auto *uploaded_image = uploadedMaterialImage(materialTextureForBinding(packet, binding));
                  if (uploaded_image != nullptr) {
                     image_infos.push_back(VkDescriptorImageInfo{.sampler = uploaded_image->sampler,
                                                                 .imageView = VK_NULL_HANDLE,
                                                                 .imageLayout = VK_IMAGE_LAYOUT_UNDEFINED});
                  } else {
                     const auto *fallback_sampler = fallbackDescriptorSampler(resources, binding);
                     if (fallback_sampler == nullptr) {
                        return std::unexpected(vve::Error::invalid_argument);
                     }
                     image_infos.push_back(VkDescriptorImageInfo{.sampler = fallback_sampler->sampler,
                                                                 .imageView = VK_NULL_HANDLE,
                                                                 .imageLayout = VK_IMAGE_LAYOUT_UNDEFINED});
                  }
                  image_info = &image_infos.back();
               } else {
                  return std::unexpected(vve::Error::invalid_argument);
               }

               writes.push_back(VkWriteDescriptorSet{.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                                                     .pNext = nullptr,
                                                     .dstSet = descriptor_sets[set_index],
                                                     .dstBinding = binding.binding,
                                                     .dstArrayElement = 0,
                                                     .descriptorCount = 1,
                                                     .descriptorType = *type,
                                                     .pImageInfo = image_info,
                                                     .pBufferInfo = buffer_info,
                                                     .pTexelBufferView = nullptr});
            }
         }

         if (!writes.empty()) {
            vkUpdateDescriptorSets(device_, static_cast<std::uint32_t>(writes.size()), writes.data(), 0, nullptr);
         }
         return {};
      }

      [[nodiscard]] std::expected<const std::vector<VkDescriptorSet> *, vve::Error>
      descriptorSetsForDraw(VulkanPipelineResources &resources, const DrawPacket &packet) {
         if (resources.descriptor_sets.empty()) {
            return std::addressof(resources.descriptor_sets);
         }
         if (resources.descriptor_pool == VK_NULL_HANDLE || resources.descriptor_set_layouts.empty()) {
            return std::unexpected(vve::Error::invalid_argument);
         }

         const auto key = drawDescriptorSetKey(packet);
         if (const auto existing = resources.draw_descriptor_sets.find(key);
             existing != resources.draw_descriptor_sets.end()) {
            return std::addressof(existing->second);
         }
         if (resources.draw_descriptor_sets.size() >= maxDrawDescriptorSetCopies) {
            return std::unexpected(vve::Error::internal_error);
         }

         std::vector<VkDescriptorSet> descriptor_sets(resources.descriptor_set_layouts.size(), VK_NULL_HANDLE);
         const VkDescriptorSetAllocateInfo allocate_info{.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
                                                        .pNext = nullptr,
                                                        .descriptorPool = resources.descriptor_pool,
                                                        .descriptorSetCount =
                                                            static_cast<std::uint32_t>(descriptor_sets.size()),
                                                        .pSetLayouts = resources.descriptor_set_layouts.data()};
         const VkResult result = vkAllocateDescriptorSets(device_, &allocate_info, descriptor_sets.data());
         if (result != VK_SUCCESS) {
            std::cerr << "[VulkanGraphicsBackend] vkAllocateDescriptorSets(draw material) failed: "
                      << string_VkResult(result) << '\n';
            return std::unexpected(vve::Error::internal_error);
         }

         if (auto write_result = writeDrawDescriptorSets(resources, descriptor_sets, packet); !write_result) {
            [[maybe_unused]] const auto free_result = vkFreeDescriptorSets(
                device_, resources.descriptor_pool, static_cast<std::uint32_t>(descriptor_sets.size()),
                descriptor_sets.data());
            return std::unexpected(write_result.error());
         }

         auto [descriptor_set, inserted] = resources.draw_descriptor_sets.emplace(key, std::move(descriptor_sets));
         (void)inserted;
         return std::addressof(descriptor_set->second);
      }

      [[nodiscard]] std::expected<void, vve::Error> createVulkanPipelineLayout(VulkanPipelineResources &resources) {
         const VkPipelineLayoutCreateInfo create_info{.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
                                                      .pNext = nullptr,
                                                      .flags = 0,
                                                      .setLayoutCount =
                                                          static_cast<std::uint32_t>(resources.descriptor_set_layouts.size()),
                                                      .pSetLayouts = resources.descriptor_set_layouts.data(),
                                                      .pushConstantRangeCount = 0,
                                                      .pPushConstantRanges = nullptr};
         const VkResult result = vkCreatePipelineLayout(device_, &create_info, nullptr, &resources.pipeline_layout);
         if (result != VK_SUCCESS) {
            std::cerr << "[VulkanGraphicsBackend] vkCreatePipelineLayout failed: " << string_VkResult(result)
                      << '\n';
            return std::unexpected(vve::Error::internal_error);
         }

         return {};
      }

      [[nodiscard]] std::expected<void, vve::Error>
      createPlaceholderRenderPass(VulkanGraphicsPipelineResources &resources, const GraphicsPipelineDesc &desc) {
         if (desc.color_attachment_count == 0) {
            return std::unexpected(vve::Error::invalid_argument);
         }

         std::vector<VkAttachmentDescription> attachments{};
         std::vector<VkAttachmentReference> color_references{};
         attachments.reserve(desc.color_attachment_count + (desc.depth_format == GraphicsDepthFormat::none ? 0U : 1U));
         color_references.reserve(desc.color_attachment_count);

         for (std::uint32_t color_index = 0; color_index < desc.color_attachment_count; ++color_index) {
            attachments.push_back(VkAttachmentDescription{.flags = 0,
                                                          .format = colorFormat(desc.color_format),
                                                          .samples = VK_SAMPLE_COUNT_1_BIT,
                                                          .loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                                                          .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
                                                          .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                                                          .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
                                                          .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                                                          .finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL});
            color_references.push_back(VkAttachmentReference{
                .attachment = color_index,
                .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL});
         }

         VkAttachmentReference depth_reference{.attachment = VK_ATTACHMENT_UNUSED,
                                               .layout = VK_IMAGE_LAYOUT_UNDEFINED};
         if (desc.depth_format != GraphicsDepthFormat::none) {
            depth_reference = VkAttachmentReference{
                .attachment = static_cast<std::uint32_t>(attachments.size()),
                .layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};
            attachments.push_back(VkAttachmentDescription{.flags = 0,
                                                          .format = depthFormat(desc.depth_format),
                                                          .samples = VK_SAMPLE_COUNT_1_BIT,
                                                          .loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                                                          .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
                                                          .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                                                          .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
                                                          .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                                                          .finalLayout =
                                                              VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL});
         }

         const VkSubpassDescription subpass{.flags = 0,
                                            .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
                                            .inputAttachmentCount = 0,
                                            .pInputAttachments = nullptr,
                                            .colorAttachmentCount =
                                                static_cast<std::uint32_t>(color_references.size()),
                                            .pColorAttachments = color_references.data(),
                                            .pResolveAttachments = nullptr,
                                            .pDepthStencilAttachment =
                                                desc.depth_format == GraphicsDepthFormat::none ? nullptr
                                                                                               : &depth_reference,
                                            .preserveAttachmentCount = 0,
                                            .pPreserveAttachments = nullptr};
         const VkRenderPassCreateInfo create_info{.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
                                                  .pNext = nullptr,
                                                  .flags = 0,
                                                  .attachmentCount =
                                                      static_cast<std::uint32_t>(attachments.size()),
                                                  .pAttachments = attachments.data(),
                                                  .subpassCount = 1,
                                                  .pSubpasses = &subpass,
                                                  .dependencyCount = 0,
                                                  .pDependencies = nullptr};
         const VkResult result = vkCreateRenderPass(device_, &create_info, nullptr, &resources.render_pass);
         if (result != VK_SUCCESS) {
            std::cerr << "[VulkanGraphicsBackend] vkCreateRenderPass failed: " << string_VkResult(result) << '\n';
            return std::unexpected(vve::Error::internal_error);
         }

         return {};
      }

      [[nodiscard]] std::expected<void, vve::Error>
      createVulkanGraphicsPipeline(VulkanGraphicsPipelineResources &resources,
                                   const VulkanPipelineResources &pipeline_resources,
                                   const GraphicsPipelineDesc &desc) {
         if (resources.render_pass == VK_NULL_HANDLE || pipeline_resources.pipeline_layout == VK_NULL_HANDLE) {
            return std::unexpected(vve::Error::invalid_argument);
         }

         std::vector<VkPipelineShaderStageCreateInfo> shader_stages{};
         shader_stages.reserve(pipeline_resources.shader_modules.size());
         bool has_vertex_stage = false;
         bool has_fragment_stage = false;
         for (const auto &shader_module : pipeline_resources.shader_modules) {
            const auto stage = shaderStageFlagBit(shader_module.stage);
            if (!stage.has_value()) {
               continue;
            }
            if (shader_module.module == VK_NULL_HANDLE || shader_module.entry_point.empty()) {
               return std::unexpected(vve::Error::invalid_argument);
            }

            has_vertex_stage = has_vertex_stage || *stage == VK_SHADER_STAGE_VERTEX_BIT;
            has_fragment_stage = has_fragment_stage || *stage == VK_SHADER_STAGE_FRAGMENT_BIT;
            shader_stages.push_back(VkPipelineShaderStageCreateInfo{.sType =
                                                                         VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                                                                     .pNext = nullptr,
                                                                     .flags = 0,
                                                                     .stage = *stage,
                                                                     .module = shader_module.module,
                                                                     .pName = shader_module.entry_point.c_str(),
                                                                     .pSpecializationInfo = nullptr});
         }
         if (!has_vertex_stage || !has_fragment_stage || shader_stages.empty()) {
            return std::unexpected(vve::Error::invalid_argument);
         }

         std::vector<VkVertexInputBindingDescription> vertex_bindings{};
         std::vector<VkVertexInputAttributeDescription> vertex_attributes{};
         if (desc.vertex_binding_count > 0) {
            if (desc.vertex_binding_count != 1 || desc.vertex_attribute_count < 5) {
               return std::unexpected(vve::Error::invalid_argument);
            }

            vertex_bindings.push_back(VkVertexInputBindingDescription{.binding = 0,
                                                                      .stride = static_cast<std::uint32_t>(
                                                                          sizeof(ImportedVertex)),
                                                                      .inputRate = VK_VERTEX_INPUT_RATE_VERTEX});
            vertex_attributes = {
                VkVertexInputAttributeDescription{.location = 0,
                                                  .binding = 0,
                                                  .format = VK_FORMAT_R32G32B32_SFLOAT,
                                                  .offset = vertexOffset(offsetof(ImportedVertex, position))},
                VkVertexInputAttributeDescription{.location = 1,
                                                  .binding = 0,
                                                  .format = VK_FORMAT_R32G32B32_SFLOAT,
                                                  .offset = vertexOffset(offsetof(ImportedVertex, normal))},
                VkVertexInputAttributeDescription{.location = 2,
                                                  .binding = 0,
                                                  .format = VK_FORMAT_R32G32B32_SFLOAT,
                                                  .offset = vertexOffset(offsetof(ImportedVertex, tangent))},
                VkVertexInputAttributeDescription{.location = 3,
                                                  .binding = 0,
                                                  .format = VK_FORMAT_R32G32_SFLOAT,
                                                  .offset = vertexOffset(offsetof(ImportedVertex, texcoord0))},
                VkVertexInputAttributeDescription{.location = 4,
                                                  .binding = 0,
                                                  .format = VK_FORMAT_R32G32B32A32_SFLOAT,
                                                  .offset = vertexOffset(offsetof(ImportedVertex, color0))}};
         }

         const VkPipelineVertexInputStateCreateInfo vertex_input{
             .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
             .pNext = nullptr,
             .flags = 0,
             .vertexBindingDescriptionCount = static_cast<std::uint32_t>(vertex_bindings.size()),
             .pVertexBindingDescriptions = vertex_bindings.data(),
             .vertexAttributeDescriptionCount = static_cast<std::uint32_t>(vertex_attributes.size()),
             .pVertexAttributeDescriptions = vertex_attributes.data()};
         const VkPipelineInputAssemblyStateCreateInfo input_assembly{
             .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
             .pNext = nullptr,
             .flags = 0,
             .topology = primitiveTopology(desc.topology),
             .primitiveRestartEnable = VK_FALSE};
         const VkPipelineViewportStateCreateInfo viewport_state{.sType =
                                                                   VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
                                                               .pNext = nullptr,
                                                               .flags = 0,
                                                               .viewportCount = 1,
                                                               .pViewports = nullptr,
                                                               .scissorCount = 1,
                                                               .pScissors = nullptr};
         const VkPipelineRasterizationStateCreateInfo rasterization{
             .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
             .pNext = nullptr,
             .flags = 0,
             .depthClampEnable = VK_FALSE,
             .rasterizerDiscardEnable = VK_FALSE,
             .polygonMode = VK_POLYGON_MODE_FILL,
             .cullMode = cullMode(desc.cull_mode),
             .frontFace = frontFace(desc.front_face),
             .depthBiasEnable = VK_FALSE,
             .depthBiasConstantFactor = 0.0F,
             .depthBiasClamp = 0.0F,
             .depthBiasSlopeFactor = 0.0F,
             .lineWidth = 1.0F};
         const VkPipelineMultisampleStateCreateInfo multisample{
             .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
             .pNext = nullptr,
             .flags = 0,
             .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
             .sampleShadingEnable = VK_FALSE,
             .minSampleShading = 1.0F,
             .pSampleMask = nullptr,
             .alphaToCoverageEnable = VK_FALSE,
             .alphaToOneEnable = VK_FALSE};
         const VkPipelineDepthStencilStateCreateInfo depth_stencil{
             .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
             .pNext = nullptr,
             .flags = 0,
             .depthTestEnable = desc.depth_test_enabled ? VK_TRUE : VK_FALSE,
             .depthWriteEnable = desc.depth_write_enabled ? VK_TRUE : VK_FALSE,
             .depthCompareOp = depthCompareOp(desc.depth_compare),
             .depthBoundsTestEnable = VK_FALSE,
             .stencilTestEnable = VK_FALSE,
             .front = {},
             .back = {},
             .minDepthBounds = 0.0F,
             .maxDepthBounds = 1.0F};
         std::vector<VkPipelineColorBlendAttachmentState> color_blend_attachments{};
         color_blend_attachments.resize(desc.color_attachment_count,
                                        VkPipelineColorBlendAttachmentState{
                                            .blendEnable = desc.blending_enabled ? VK_TRUE : VK_FALSE,
                                            .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
                                            .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
                                            .colorBlendOp = VK_BLEND_OP_ADD,
                                            .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
                                            .dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
                                            .alphaBlendOp = VK_BLEND_OP_ADD,
                                            .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                                              VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT});
         const VkPipelineColorBlendStateCreateInfo color_blend{
             .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
             .pNext = nullptr,
             .flags = 0,
             .logicOpEnable = VK_FALSE,
             .logicOp = VK_LOGIC_OP_COPY,
             .attachmentCount = static_cast<std::uint32_t>(color_blend_attachments.size()),
             .pAttachments = color_blend_attachments.data(),
             .blendConstants = {0.0F, 0.0F, 0.0F, 0.0F}};
         const std::array dynamic_states{VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
         const VkPipelineDynamicStateCreateInfo dynamic_state{.sType =
                                                                 VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
                                                             .pNext = nullptr,
                                                             .flags = 0,
                                                             .dynamicStateCount =
                                                                 static_cast<std::uint32_t>(dynamic_states.size()),
                                                             .pDynamicStates = dynamic_states.data()};
         const VkGraphicsPipelineCreateInfo create_info{.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
                                                        .pNext = nullptr,
                                                        .flags = 0,
                                                        .stageCount =
                                                            static_cast<std::uint32_t>(shader_stages.size()),
                                                        .pStages = shader_stages.data(),
                                                        .pVertexInputState = &vertex_input,
                                                        .pInputAssemblyState = &input_assembly,
                                                        .pTessellationState = nullptr,
                                                        .pViewportState = &viewport_state,
                                                        .pRasterizationState = &rasterization,
                                                        .pMultisampleState = &multisample,
                                                        .pDepthStencilState = &depth_stencil,
                                                        .pColorBlendState = &color_blend,
                                                        .pDynamicState = &dynamic_state,
                                                        .layout = pipeline_resources.pipeline_layout,
                                                        .renderPass = resources.render_pass,
                                                        .subpass = 0,
                                                        .basePipelineHandle = VK_NULL_HANDLE,
                                                        .basePipelineIndex = -1};
         const VkResult result =
             vkCreateGraphicsPipelines(device_, pipeline_cache_, 1, &create_info, nullptr, &resources.pipeline);
         if (result != VK_SUCCESS) {
            std::cerr << "[VulkanGraphicsBackend] vkCreateGraphicsPipelines failed: " << string_VkResult(result)
                      << '\n';
            return std::unexpected(vve::Error::internal_error);
         }

         return {};
      }

      [[nodiscard]] std::expected<void, vve::Error>
      createWindowSurface(VulkanWindowSwapchainResources &resources, const NativeWindowHandle &window) {
         VkSurfaceKHR surface = VK_NULL_HANDLE;
         if (!SDL_Vulkan_CreateSurface(static_cast<SDL_Window *>(window.native_window), instance_, nullptr, &surface)) {
            std::cerr << "[VulkanGraphicsBackend] SDL_Vulkan_CreateSurface failed for '" << window.window_id
                      << "': " << SDL_GetError() << '\n';
            return std::unexpected(vve::Error::internal_error);
         }

         VkBool32 present_supported = VK_FALSE;
         const VkResult present_result =
             vkGetPhysicalDeviceSurfaceSupportKHR(physical_device_, graphics_queue_family_, surface, &present_supported);
         if (present_result != VK_SUCCESS || present_supported != VK_TRUE) {
            std::cerr << "[VulkanGraphicsBackend] selected queue cannot present to window '" << window.window_id
                      << "' surface: " << string_VkResult(present_result) << '\n';
            SDL_Vulkan_DestroySurface(instance_, surface, nullptr);
            return std::unexpected(vve::Error::internal_error);
         }

         resources.surface = surface;
         return {};
      }

      [[nodiscard]] std::expected<std::vector<VkSurfaceFormatKHR>, vve::Error>
      surfaceFormats(VkSurfaceKHR surface, std::string_view window_id) const {
         std::uint32_t format_count = 0;
         VkResult result = vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device_, surface, &format_count, nullptr);
         if (result != VK_SUCCESS || format_count == 0) {
            std::cerr << "[VulkanGraphicsBackend] no surface formats for window '" << window_id
                      << "': " << string_VkResult(result) << '\n';
            return std::unexpected(vve::Error::internal_error);
         }

         std::vector<VkSurfaceFormatKHR> formats(format_count);
         result = vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device_, surface, &format_count, formats.data());
         if (result != VK_SUCCESS && result != VK_INCOMPLETE) {
            std::cerr << "[VulkanGraphicsBackend] vkGetPhysicalDeviceSurfaceFormatsKHR failed for window '"
                      << window_id << "': " << string_VkResult(result) << '\n';
            return std::unexpected(vve::Error::internal_error);
         }
         formats.resize(format_count);
         return formats;
      }

      [[nodiscard]] std::expected<std::vector<VkPresentModeKHR>, vve::Error>
      presentModes(VkSurfaceKHR surface, std::string_view window_id) const {
         std::uint32_t mode_count = 0;
         VkResult result = vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device_, surface, &mode_count, nullptr);
         if (result != VK_SUCCESS || mode_count == 0) {
            std::cerr << "[VulkanGraphicsBackend] no present modes for window '" << window_id
                      << "': " << string_VkResult(result) << '\n';
            return std::unexpected(vve::Error::internal_error);
         }

         std::vector<VkPresentModeKHR> modes(mode_count);
         result = vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device_, surface, &mode_count, modes.data());
         if (result != VK_SUCCESS && result != VK_INCOMPLETE) {
            std::cerr << "[VulkanGraphicsBackend] vkGetPhysicalDeviceSurfacePresentModesKHR failed for window '"
                      << window_id << "': " << string_VkResult(result) << '\n';
            return std::unexpected(vve::Error::internal_error);
         }
         modes.resize(mode_count);
         return modes;
      }

      [[nodiscard]] std::expected<void, vve::Error>
      createVulkanSwapchain(VulkanWindowSwapchainResources &resources, const NativeWindowHandle &window) {
         VkSurfaceCapabilitiesKHR capabilities{};
         VkResult result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device_, resources.surface, &capabilities);
         if (result != VK_SUCCESS) {
            std::cerr << "[VulkanGraphicsBackend] vkGetPhysicalDeviceSurfaceCapabilitiesKHR failed for window '"
                      << window.window_id << "': " << string_VkResult(result) << '\n';
            return std::unexpected(vve::Error::internal_error);
         }

         const auto formats = surfaceFormats(resources.surface, window.window_id);
         if (!formats) {
            return std::unexpected(formats.error());
         }
         const auto modes = presentModes(resources.surface, window.window_id);
         if (!modes) {
            return std::unexpected(modes.error());
         }

         const auto surface_format = chooseSurfaceFormat(*formats);
         const auto present_mode = choosePresentMode(*modes);
         const auto extent = chooseSwapchainExtent(capabilities, window);
         std::uint32_t image_count = capabilities.minImageCount + 1;
         if (capabilities.maxImageCount > 0 && image_count > capabilities.maxImageCount) {
            image_count = capabilities.maxImageCount;
         }

         const VkSwapchainCreateInfoKHR create_info{.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
                                                    .pNext = nullptr,
                                                    .flags = 0,
                                                    .surface = resources.surface,
                                                    .minImageCount = image_count,
                                                    .imageFormat = surface_format.format,
                                                    .imageColorSpace = surface_format.colorSpace,
                                                    .imageExtent = extent,
                                                    .imageArrayLayers = 1,
                                                    .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
                                                    .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
                                                    .queueFamilyIndexCount = 0,
                                                    .pQueueFamilyIndices = nullptr,
                                                    .preTransform = capabilities.currentTransform,
                                                    .compositeAlpha =
                                                        chooseCompositeAlpha(capabilities.supportedCompositeAlpha),
                                                    .presentMode = present_mode,
                                                    .clipped = VK_TRUE,
                                                    .oldSwapchain = VK_NULL_HANDLE};
         result = vkCreateSwapchainKHR(device_, &create_info, nullptr, &resources.swapchain);
         if (result != VK_SUCCESS) {
            std::cerr << "[VulkanGraphicsBackend] vkCreateSwapchainKHR failed for window '" << window.window_id
                      << "': " << string_VkResult(result) << '\n';
            return std::unexpected(vve::Error::internal_error);
         }

         std::uint32_t actual_image_count = 0;
         result = vkGetSwapchainImagesKHR(device_, resources.swapchain, &actual_image_count, nullptr);
         if (result != VK_SUCCESS || actual_image_count == 0) {
            std::cerr << "[VulkanGraphicsBackend] vkGetSwapchainImagesKHR failed for window '" << window.window_id
                      << "': " << string_VkResult(result) << '\n';
            return std::unexpected(vve::Error::internal_error);
         }

         resources.images.resize(actual_image_count);
         result = vkGetSwapchainImagesKHR(device_, resources.swapchain, &actual_image_count, resources.images.data());
         if (result != VK_SUCCESS && result != VK_INCOMPLETE) {
            std::cerr << "[VulkanGraphicsBackend] vkGetSwapchainImagesKHR failed for window '" << window.window_id
                      << "': " << string_VkResult(result) << '\n';
            return std::unexpected(vve::Error::internal_error);
         }

         resources.images.resize(actual_image_count);
         resources.format = surface_format.format;
         resources.present_mode = present_mode;
         resources.extent = extent;
         return {};
      }

      [[nodiscard]] std::expected<void, vve::Error>
      createSwapchainImageViews(VulkanWindowSwapchainResources &resources) {
         resources.image_views.reserve(resources.images.size());
         for (const auto image : resources.images) {
            const VkImageViewCreateInfo create_info{.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                                                    .pNext = nullptr,
                                                    .flags = 0,
                                                    .image = image,
                                                    .viewType = VK_IMAGE_VIEW_TYPE_2D,
                                                    .format = resources.format,
                                                    .components = {.r = VK_COMPONENT_SWIZZLE_IDENTITY,
                                                                   .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                                                                   .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                                                                   .a = VK_COMPONENT_SWIZZLE_IDENTITY},
                                                    .subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                                                                         .baseMipLevel = 0,
                                                                         .levelCount = 1,
                                                                         .baseArrayLayer = 0,
                                                                         .layerCount = 1}};
            VkImageView image_view = VK_NULL_HANDLE;
            const VkResult result = vkCreateImageView(device_, &create_info, nullptr, &image_view);
            if (result != VK_SUCCESS) {
               std::cerr << "[VulkanGraphicsBackend] vkCreateImageView failed: " << string_VkResult(result)
                         << '\n';
               return std::unexpected(vve::Error::internal_error);
            }
            resources.image_views.push_back(image_view);
         }

         return {};
      }

      [[nodiscard]] std::expected<void, vve::Error>
      createSwapchainDepthResources(VulkanWindowSwapchainResources &resources) {
         if (resources.extent.width == 0 || resources.extent.height == 0) {
            return std::unexpected(vve::Error::invalid_argument);
         }

         constexpr VkFormat selected_depth_format = VK_FORMAT_D32_SFLOAT;
         VkFormatProperties format_properties{};
         vkGetPhysicalDeviceFormatProperties(physical_device_, selected_depth_format, &format_properties);
         if ((format_properties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) == 0) {
            std::cerr << "[VulkanGraphicsBackend] depth format " << string_VkFormat(selected_depth_format)
                      << " does not support depth-stencil attachment usage\n";
            return std::unexpected(vve::Error::internal_error);
         }

         const VkImageCreateInfo image_info{.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
                                            .pNext = nullptr,
                                            .flags = 0,
                                            .imageType = VK_IMAGE_TYPE_2D,
                                            .format = selected_depth_format,
                                            .extent = {.width = resources.extent.width,
                                                       .height = resources.extent.height,
                                                       .depth = 1},
                                            .mipLevels = 1,
                                            .arrayLayers = 1,
                                            .samples = VK_SAMPLE_COUNT_1_BIT,
                                            .tiling = VK_IMAGE_TILING_OPTIMAL,
                                            .usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                                            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
                                            .queueFamilyIndexCount = 0,
                                            .pQueueFamilyIndices = nullptr,
                                            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED};
         VkResult result = vkCreateImage(device_, &image_info, nullptr, &resources.depth_image);
         if (result != VK_SUCCESS) {
            std::cerr << "[VulkanGraphicsBackend] vkCreateImage(depth) failed: " << string_VkResult(result)
                      << '\n';
            return std::unexpected(vve::Error::internal_error);
         }

         VkMemoryRequirements requirements{};
         vkGetImageMemoryRequirements(device_, resources.depth_image, &requirements);
         const auto memory_type = findMemoryType(requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
         if (!memory_type) {
            return std::unexpected(memory_type.error());
         }

         const VkMemoryAllocateInfo allocate_info{.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
                                                  .pNext = nullptr,
                                                  .allocationSize = requirements.size,
                                                  .memoryTypeIndex = *memory_type};
         result = vkAllocateMemory(device_, &allocate_info, nullptr, &resources.depth_memory);
         if (result != VK_SUCCESS) {
            std::cerr << "[VulkanGraphicsBackend] vkAllocateMemory(depth) failed: " << string_VkResult(result)
                      << '\n';
            return std::unexpected(vve::Error::internal_error);
         }

         result = vkBindImageMemory(device_, resources.depth_image, resources.depth_memory, 0);
         if (result != VK_SUCCESS) {
            std::cerr << "[VulkanGraphicsBackend] vkBindImageMemory(depth) failed: " << string_VkResult(result)
                      << '\n';
            return std::unexpected(vve::Error::internal_error);
         }

         const VkImageViewCreateInfo view_info{.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                                               .pNext = nullptr,
                                               .flags = 0,
                                               .image = resources.depth_image,
                                               .viewType = VK_IMAGE_VIEW_TYPE_2D,
                                               .format = selected_depth_format,
                                               .components = {.r = VK_COMPONENT_SWIZZLE_IDENTITY,
                                                              .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                                                              .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                                                              .a = VK_COMPONENT_SWIZZLE_IDENTITY},
                                               .subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
                                                                    .baseMipLevel = 0,
                                                                    .levelCount = 1,
                                                                    .baseArrayLayer = 0,
                                                                    .layerCount = 1}};
         result = vkCreateImageView(device_, &view_info, nullptr, &resources.depth_image_view);
         if (result != VK_SUCCESS) {
            std::cerr << "[VulkanGraphicsBackend] vkCreateImageView(depth) failed: " << string_VkResult(result)
                      << '\n';
            return std::unexpected(vve::Error::internal_error);
         }

         resources.depth_format = selected_depth_format;
         return {};
      }

      [[nodiscard]] std::expected<void, vve::Error>
      createSwapchainClearRenderPass(VulkanWindowSwapchainResources &resources) {
         if (resources.depth_format == VK_FORMAT_UNDEFINED || resources.depth_image_view == VK_NULL_HANDLE) {
            return std::unexpected(vve::Error::invalid_argument);
         }

         const VkAttachmentDescription color_attachment{.flags = 0,
                                                        .format = resources.format,
                                                        .samples = VK_SAMPLE_COUNT_1_BIT,
                                                        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                                                        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
                                                        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                                                        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
                                                        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                                                        .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR};
         const VkAttachmentDescription depth_attachment{
             .flags = 0,
             .format = resources.depth_format,
             .samples = VK_SAMPLE_COUNT_1_BIT,
             .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
             .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
             .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
             .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
             .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
             .finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};
         const std::array attachments{color_attachment, depth_attachment};
         const VkAttachmentReference color_reference{.attachment = 0,
                                                     .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
         const VkAttachmentReference depth_reference{.attachment = 1,
                                                     .layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};
         const VkSubpassDescription subpass{.flags = 0,
                                            .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
                                            .inputAttachmentCount = 0,
                                            .pInputAttachments = nullptr,
                                            .colorAttachmentCount = 1,
                                            .pColorAttachments = &color_reference,
                                            .pResolveAttachments = nullptr,
                                            .pDepthStencilAttachment = &depth_reference,
                                            .preserveAttachmentCount = 0,
                                            .pPreserveAttachments = nullptr};
         const std::array dependencies{
             VkSubpassDependency{.srcSubpass = VK_SUBPASS_EXTERNAL,
                                 .dstSubpass = 0,
                                 .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                                                 VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
                                 .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                                                 VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
                                 .srcAccessMask = 0,
                                 .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                                                  VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                                 .dependencyFlags = 0},
             VkSubpassDependency{.srcSubpass = 0,
                                 .dstSubpass = VK_SUBPASS_EXTERNAL,
                                 .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                                                 VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
                                 .dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                                 .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                                                  VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                                 .dstAccessMask = 0,
                                 .dependencyFlags = 0}};
         const VkRenderPassCreateInfo create_info{.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
                                                  .pNext = nullptr,
                                                  .flags = 0,
                                                  .attachmentCount = static_cast<std::uint32_t>(attachments.size()),
                                                  .pAttachments = attachments.data(),
                                                  .subpassCount = 1,
                                                  .pSubpasses = &subpass,
                                                  .dependencyCount =
                                                      static_cast<std::uint32_t>(dependencies.size()),
                                                  .pDependencies = dependencies.data()};
         const VkResult result =
             vkCreateRenderPass(device_, &create_info, nullptr, &resources.clear_render_pass);
         if (result != VK_SUCCESS) {
            std::cerr << "[VulkanGraphicsBackend] vkCreateRenderPass for swapchain clear failed: "
                      << string_VkResult(result) << '\n';
            return std::unexpected(vve::Error::internal_error);
         }

         return {};
      }

      [[nodiscard]] std::expected<void, vve::Error>
      createSwapchainFramebuffers(VulkanWindowSwapchainResources &resources) {
         if (resources.clear_render_pass == VK_NULL_HANDLE || resources.extent.width == 0 ||
             resources.extent.height == 0 || resources.depth_image_view == VK_NULL_HANDLE) {
            return std::unexpected(vve::Error::invalid_argument);
         }

         resources.framebuffers.reserve(resources.image_views.size());
         for (const auto image_view : resources.image_views) {
            const std::array attachments{image_view, resources.depth_image_view};
            const VkFramebufferCreateInfo create_info{.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
                                                      .pNext = nullptr,
                                                      .flags = 0,
                                                      .renderPass = resources.clear_render_pass,
                                                      .attachmentCount = static_cast<std::uint32_t>(attachments.size()),
                                                      .pAttachments = attachments.data(),
                                                      .width = resources.extent.width,
                                                      .height = resources.extent.height,
                                                      .layers = 1};
            VkFramebuffer framebuffer = VK_NULL_HANDLE;
            const VkResult result = vkCreateFramebuffer(device_, &create_info, nullptr, &framebuffer);
            if (result != VK_SUCCESS) {
               std::cerr << "[VulkanGraphicsBackend] vkCreateFramebuffer failed: " << string_VkResult(result)
                         << '\n';
               return std::unexpected(vve::Error::internal_error);
            }
            resources.framebuffers.push_back(framebuffer);
         }

         return {};
      }

      [[nodiscard]] std::expected<void, vve::Error>
      allocateSwapchainCommandBuffers(VulkanWindowSwapchainResources &resources) {
         if (command_pool_ == VK_NULL_HANDLE || resources.images.empty()) {
            return std::unexpected(vve::Error::invalid_argument);
         }

         resources.command_buffers.resize(resources.images.size(), VK_NULL_HANDLE);
         const VkCommandBufferAllocateInfo allocate_info{
             .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
             .pNext = nullptr,
             .commandPool = command_pool_,
             .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
             .commandBufferCount = static_cast<std::uint32_t>(resources.command_buffers.size())};
         const VkResult result = vkAllocateCommandBuffers(device_, &allocate_info, resources.command_buffers.data());
         if (result != VK_SUCCESS) {
            std::cerr << "[VulkanGraphicsBackend] vkAllocateCommandBuffers failed: " << string_VkResult(result)
                      << '\n';
            resources.command_buffers.clear();
            return std::unexpected(vve::Error::internal_error);
         }

         return {};
      }

      [[nodiscard]] std::expected<void, vve::Error>
      createSwapchainFrameSync(VulkanWindowSwapchainResources &resources) {
         const VkSemaphoreCreateInfo semaphore_info{.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
                                                    .pNext = nullptr,
                                                    .flags = 0};
         const VkFenceCreateInfo fence_info{.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
                                            .pNext = nullptr,
                                            .flags = VK_FENCE_CREATE_SIGNALED_BIT};

         const auto slot_count = frameSyncSlotCount(resources.images.size());
         resources.frame_sync.reserve(slot_count);
         for (std::uint32_t slot = 0; slot < slot_count; ++slot) {
            VulkanSwapchainFrameSync sync{};
            VkResult result = vkCreateSemaphore(device_, &semaphore_info, nullptr, &sync.image_available);
            if (result != VK_SUCCESS) {
               std::cerr << "[VulkanGraphicsBackend] vkCreateSemaphore(image_available) failed: "
                         << string_VkResult(result) << '\n';
               return std::unexpected(vve::Error::internal_error);
            }

            result = vkCreateSemaphore(device_, &semaphore_info, nullptr, &sync.render_finished);
            if (result != VK_SUCCESS) {
               std::cerr << "[VulkanGraphicsBackend] vkCreateSemaphore(render_finished) failed: "
                         << string_VkResult(result) << '\n';
               vkDestroySemaphore(device_, sync.image_available, nullptr);
               return std::unexpected(vve::Error::internal_error);
            }

            result = vkCreateFence(device_, &fence_info, nullptr, &sync.render_fence);
            if (result != VK_SUCCESS) {
               std::cerr << "[VulkanGraphicsBackend] vkCreateFence failed: " << string_VkResult(result) << '\n';
               vkDestroySemaphore(device_, sync.render_finished, nullptr);
               vkDestroySemaphore(device_, sync.image_available, nullptr);
               return std::unexpected(vve::Error::internal_error);
            }

            resources.frame_sync.push_back(sync);
         }

         return {};
      }

      [[nodiscard]] std::expected<void, vve::Error>
      recordSwapchainCommand(VulkanWindowSwapchainResources &resources, std::uint32_t image_index,
                             const WindowDrawPacketList &draw_packets) {
         if (image_index >= resources.command_buffers.size() || image_index >= resources.framebuffers.size() ||
             resources.clear_render_pass == VK_NULL_HANDLE) {
            return std::unexpected(vve::Error::invalid_argument);
         }
         if (draw_packets.window.value.isValid() &&
             draw_packets.window.value.value() != resources.summary.window.value.value()) {
            return std::unexpected(vve::Error::invalid_argument);
         }

         const auto command_buffer = resources.command_buffers[image_index];
         VkResult result = vkResetCommandBuffer(command_buffer, 0);
         if (result != VK_SUCCESS) {
            std::cerr << "[VulkanGraphicsBackend] vkResetCommandBuffer failed: " << string_VkResult(result)
                      << '\n';
            return std::unexpected(vve::Error::internal_error);
         }

         const VkCommandBufferBeginInfo begin_info{.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                                                   .pNext = nullptr,
                                                   .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
                                                   .pInheritanceInfo = nullptr};
         result = vkBeginCommandBuffer(command_buffer, &begin_info);
         if (result != VK_SUCCESS) {
            std::cerr << "[VulkanGraphicsBackend] vkBeginCommandBuffer failed: " << string_VkResult(result)
                      << '\n';
            return std::unexpected(vve::Error::internal_error);
         }

         const std::array clear_values{VkClearValue{.color = windowClearColor(resources.summary)},
                                       VkClearValue{.depthStencil = VkClearDepthStencilValue{.depth = 1.0F,
                                                                                             .stencil = 0}}};
         const VkRenderPassBeginInfo render_pass_info{.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
                                                      .pNext = nullptr,
                                                      .renderPass = resources.clear_render_pass,
                                                      .framebuffer = resources.framebuffers[image_index],
                                                      .renderArea = {.offset = {.x = 0, .y = 0},
                                                                     .extent = resources.extent},
                                                      .clearValueCount = static_cast<std::uint32_t>(clear_values.size()),
                                                      .pClearValues = clear_values.data()};
         vkCmdBeginRenderPass(command_buffer, &render_pass_info, VK_SUBPASS_CONTENTS_INLINE);
         if (auto draw_result = recordDrawPackets(command_buffer, resources, draw_packets); !draw_result) {
            vkCmdEndRenderPass(command_buffer);
            [[maybe_unused]] const auto end_result = vkEndCommandBuffer(command_buffer);
            return std::unexpected(draw_result.error());
         }
         vkCmdEndRenderPass(command_buffer);

         result = vkEndCommandBuffer(command_buffer);
         if (result != VK_SUCCESS) {
            std::cerr << "[VulkanGraphicsBackend] vkEndCommandBuffer failed: " << string_VkResult(result)
                      << '\n';
            return std::unexpected(vve::Error::internal_error);
         }

         return {};
      }

      [[nodiscard]] std::expected<void, vve::Error>
      recordDrawPackets(VkCommandBuffer command_buffer, const VulkanWindowSwapchainResources &resources,
                        const WindowDrawPacketList &draw_packets) {
         if (draw_packets.packets.empty()) {
            return {};
         }

         const VkViewport viewport{.x = 0.0F,
                                   .y = 0.0F,
                                   .width = static_cast<float>(resources.extent.width),
                                   .height = static_cast<float>(resources.extent.height),
                                   .minDepth = 0.0F,
                                   .maxDepth = 1.0F};
         const VkRect2D scissor{.offset = {.x = 0, .y = 0}, .extent = resources.extent};
         vkCmdSetViewport(command_buffer, 0, 1, &viewport);
         vkCmdSetScissor(command_buffer, 0, 1, &scissor);

         std::unordered_set<vve::Handle::value_type> refreshed_pipeline_resources{};
         for (const auto &packet : draw_packets.packets) {
            if (!packet.window.value.isValid() ||
                packet.window.value.value() != resources.summary.window.value.value() ||
                packet.index_count == 0 || packet.instance_count == 0 ||
                !packet.graphics_pipeline.value.isValid() || !packet.vertex_buffer.value.isValid() ||
                !packet.index_buffer.value.isValid()) {
               return std::unexpected(vve::Error::invalid_argument);
            }

            const auto pipeline = graphics_pipelines_.find(packet.graphics_pipeline.value.value());
            const auto vertex_buffer = buffers_.find(packet.vertex_buffer.value.value());
            const auto index_buffer = buffers_.find(packet.index_buffer.value.value());
            if (pipeline == graphics_pipelines_.end() || pipeline->second.pipeline == VK_NULL_HANDLE ||
                vertex_buffer == buffers_.end() || vertex_buffer->second.buffer == VK_NULL_HANDLE ||
                vertex_buffer->second.summary.usage != GpuBufferUsage::vertex ||
                index_buffer == buffers_.end() || index_buffer->second.buffer == VK_NULL_HANDLE ||
                index_buffer->second.summary.usage != GpuBufferUsage::index) {
               return std::unexpected(vve::Error::invalid_argument);
            }

            const auto pipeline_resource =
                pipeline_resources_.find(pipeline->second.summary.backend_resources.value.value());
            if (pipeline_resource == pipeline_resources_.end() ||
                pipeline_resource->second.pipeline_layout == VK_NULL_HANDLE) {
               return std::unexpected(vve::Error::invalid_argument);
            }

            const auto pipeline_resource_key = pipeline->second.summary.backend_resources.value.value();
            if (refreshed_pipeline_resources.insert(pipeline_resource_key).second) {
               if (auto frame_update = updateFrameDescriptorBuffers(pipeline_resource->second, resources.extent);
                   !frame_update) {
                  return std::unexpected(frame_update.error());
               }
            }
            const auto descriptor_sets = descriptorSetsForDraw(pipeline_resource->second, packet);
            if (!descriptor_sets) {
               return std::unexpected(descriptor_sets.error());
            }

            const VkDeviceSize vertex_offset = 0;
            vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->second.pipeline);
            if (!(*descriptor_sets)->empty()) {
               vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                       pipeline_resource->second.pipeline_layout, 0,
                                       static_cast<std::uint32_t>((*descriptor_sets)->size()),
                                       (*descriptor_sets)->data(), 0, nullptr);
            }
            vkCmdBindVertexBuffers(command_buffer, 0, 1, &vertex_buffer->second.buffer, &vertex_offset);
            vkCmdBindIndexBuffer(command_buffer, index_buffer->second.buffer, 0, VK_INDEX_TYPE_UINT32);
            vkCmdDrawIndexed(command_buffer, packet.index_count, packet.instance_count, packet.first_index,
                             packet.vertex_offset, 0);
         }

         return {};
      }

      void updateSwapchainFrameSummary(VulkanWindowSwapchainResources &resources) noexcept {
         resources.summary.current_image_index = resources.active_image_index;
         resources.summary.frame_acquired = resources.frame_acquired;
      }

      [[nodiscard]] std::expected<void, vve::Error>
      acquireWindowFrame(VulkanWindowSwapchainResources &resources) {
         if (resources.swapchain == VK_NULL_HANDLE || resources.frame_sync.empty() ||
             resources.command_buffers.empty() || resources.framebuffers.empty()) {
            return {};
         }
         if (resources.frame_acquired) {
            return std::unexpected(vve::Error::internal_error);
         }

         auto &sync = resources.frame_sync[resources.current_frame_slot];
         VkResult result = vkWaitForFences(device_, 1, &sync.render_fence, VK_TRUE, UINT64_MAX);
         if (result != VK_SUCCESS) {
            std::cerr << "[VulkanGraphicsBackend] vkWaitForFences failed for window '"
                      << resources.summary.window_id << "': " << string_VkResult(result) << '\n';
            return std::unexpected(vve::Error::internal_error);
         }

         std::uint32_t image_index = 0;
         result = vkAcquireNextImageKHR(device_, resources.swapchain, UINT64_MAX, sync.image_available,
                                        VK_NULL_HANDLE, &image_index);
         if (result == VK_ERROR_OUT_OF_DATE_KHR) {
            resources.summary.swapchain_dirty = true;
            return {};
         }
         if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
            std::cerr << "[VulkanGraphicsBackend] vkAcquireNextImageKHR failed for window '"
                      << resources.summary.window_id << "': " << string_VkResult(result) << '\n';
            return std::unexpected(vve::Error::internal_error);
         }
         const bool acquired_suboptimal = result == VK_SUBOPTIMAL_KHR;

         resources.active_frame_slot = resources.current_frame_slot;
         resources.active_image_index = image_index;
         resources.frame_acquired = true;
         resources.command_recorded = false;
         resources.frame_submitted = false;
         resources.current_frame_slot =
             (resources.current_frame_slot + 1U) % static_cast<std::uint32_t>(resources.frame_sync.size());
         resources.summary.swapchain_dirty = resources.summary.swapchain_dirty || acquired_suboptimal;
         updateSwapchainFrameSummary(resources);
         return {};
      }

      [[nodiscard]] std::expected<void, vve::Error>
      recordWindowFrame(VulkanWindowSwapchainResources &resources, const WindowDrawPacketList &draw_packets) {
         if (!resources.frame_acquired) {
            return {};
         }
         if (resources.active_image_index >= resources.images.size()) {
            return std::unexpected(vve::Error::internal_error);
         }

         if (auto record_result = recordSwapchainCommand(resources, resources.active_image_index, draw_packets);
             !record_result) {
            return std::unexpected(record_result.error());
         }

         resources.command_recorded = true;
         return {};
      }

      [[nodiscard]] std::expected<void, vve::Error>
      submitWindowFrame(VulkanWindowSwapchainResources &resources) {
         if (!resources.frame_acquired) {
            return {};
         }
         if (!resources.command_recorded || resources.active_frame_slot >= resources.frame_sync.size() ||
             resources.active_image_index >= resources.command_buffers.size()) {
            return std::unexpected(vve::Error::internal_error);
         }

         auto &sync = resources.frame_sync[resources.active_frame_slot];
         VkResult result = vkResetFences(device_, 1, &sync.render_fence);
         if (result != VK_SUCCESS) {
            std::cerr << "[VulkanGraphicsBackend] vkResetFences failed for window '"
                      << resources.summary.window_id << "': " << string_VkResult(result) << '\n';
            return std::unexpected(vve::Error::internal_error);
         }

         const VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
         const VkSubmitInfo submit_info{.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                                        .pNext = nullptr,
                                        .waitSemaphoreCount = 1,
                                        .pWaitSemaphores = &sync.image_available,
                                        .pWaitDstStageMask = &wait_stage,
                                        .commandBufferCount = 1,
                                        .pCommandBuffers = &resources.command_buffers[resources.active_image_index],
                                        .signalSemaphoreCount = 1,
                                        .pSignalSemaphores = &sync.render_finished};
         result = vkQueueSubmit(graphics_queue_, 1, &submit_info, sync.render_fence);
         if (result != VK_SUCCESS) {
            std::cerr << "[VulkanGraphicsBackend] vkQueueSubmit failed for window '"
                      << resources.summary.window_id << "': " << string_VkResult(result) << '\n';
            return std::unexpected(vve::Error::internal_error);
         }

         resources.frame_submitted = true;
         updateSwapchainFrameSummary(resources);
         return {};
      }

      [[nodiscard]] std::expected<void, vve::Error>
      presentWindowFrame(VulkanWindowSwapchainResources &resources) {
         if (!resources.frame_acquired || resources.swapchain == VK_NULL_HANDLE ||
             resources.active_frame_slot >= resources.frame_sync.size()) {
            return {};
         }
         if (!resources.frame_submitted) {
            return std::unexpected(vve::Error::internal_error);
         }

         auto &sync = resources.frame_sync[resources.active_frame_slot];
         const VkPresentInfoKHR present_info{.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
                                             .pNext = nullptr,
                                             .waitSemaphoreCount = 1,
                                             .pWaitSemaphores = &sync.render_finished,
                                             .swapchainCount = 1,
                                             .pSwapchains = &resources.swapchain,
                                             .pImageIndices = &resources.active_image_index,
                                             .pResults = nullptr};
         const VkResult result = vkQueuePresentKHR(graphics_queue_, &present_info);
         resources.frame_acquired = false;
         resources.command_recorded = false;
         resources.frame_submitted = false;
         if (result == VK_SUCCESS) {
            ++resources.summary.presented_frame_count;
            updateSwapchainFrameSummary(resources);
            return {};
         }
         if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
            resources.summary.swapchain_dirty = true;
            updateSwapchainFrameSummary(resources);
            return {};
         }

         std::cerr << "[VulkanGraphicsBackend] vkQueuePresentKHR failed for window '"
                   << resources.summary.window_id << "': " << string_VkResult(result) << '\n';
         updateSwapchainFrameSummary(resources);
         return std::unexpected(vve::Error::internal_error);
      }

      void destroyBufferResources(VulkanGpuBufferResources &resources) noexcept {
         if (device_ == VK_NULL_HANDLE) {
            return;
         }

         if (resources.buffer != VK_NULL_HANDLE) {
            vkDestroyBuffer(device_, resources.buffer, nullptr);
            resources.buffer = VK_NULL_HANDLE;
         }
         if (resources.memory != VK_NULL_HANDLE) {
            vkFreeMemory(device_, resources.memory, nullptr);
            resources.memory = VK_NULL_HANDLE;
         }

         resources.summary.buffer_created = false;
         resources.summary.memory_bound = false;
      }

      void destroyImageResources(VulkanGpuImageResources &resources) noexcept {
         if (device_ == VK_NULL_HANDLE) {
            return;
         }

         if (resources.sampler != VK_NULL_HANDLE) {
            vkDestroySampler(device_, resources.sampler, nullptr);
            resources.sampler = VK_NULL_HANDLE;
         }
         if (resources.image_view != VK_NULL_HANDLE) {
            vkDestroyImageView(device_, resources.image_view, nullptr);
            resources.image_view = VK_NULL_HANDLE;
         }
         if (resources.image != VK_NULL_HANDLE) {
            vkDestroyImage(device_, resources.image, nullptr);
            resources.image = VK_NULL_HANDLE;
         }
         if (resources.memory != VK_NULL_HANDLE) {
            vkFreeMemory(device_, resources.memory, nullptr);
            resources.memory = VK_NULL_HANDLE;
         }

         resources.summary.image_created = false;
         resources.summary.image_view_created = false;
         resources.summary.sampler_created = false;
         resources.summary.resident = false;
      }

      void destroyPipelineResources(VulkanPipelineResources &resources) noexcept {
         if (device_ == VK_NULL_HANDLE) {
            return;
         }

         if (resources.pipeline_layout != VK_NULL_HANDLE) {
            vkDestroyPipelineLayout(device_, resources.pipeline_layout, nullptr);
            resources.pipeline_layout = VK_NULL_HANDLE;
         }

         if (resources.descriptor_pool != VK_NULL_HANDLE) {
            vkDestroyDescriptorPool(device_, resources.descriptor_pool, nullptr);
            resources.descriptor_pool = VK_NULL_HANDLE;
         }
         resources.descriptor_sets.clear();

         for (auto &sampler : resources.descriptor_samplers) {
            if (sampler.sampler != VK_NULL_HANDLE) {
               vkDestroySampler(device_, sampler.sampler, nullptr);
               sampler.sampler = VK_NULL_HANDLE;
            }
         }
         resources.descriptor_samplers.clear();

         for (auto &image : resources.descriptor_images) {
            if (image.image_view != VK_NULL_HANDLE) {
               vkDestroyImageView(device_, image.image_view, nullptr);
               image.image_view = VK_NULL_HANDLE;
            }
            if (image.image != VK_NULL_HANDLE) {
               vkDestroyImage(device_, image.image, nullptr);
               image.image = VK_NULL_HANDLE;
            }
            if (image.memory != VK_NULL_HANDLE) {
               vkFreeMemory(device_, image.memory, nullptr);
               image.memory = VK_NULL_HANDLE;
            }
         }
         resources.descriptor_images.clear();

         for (auto &buffer : resources.descriptor_buffers) {
            if (buffer.buffer != VK_NULL_HANDLE) {
               vkDestroyBuffer(device_, buffer.buffer, nullptr);
               buffer.buffer = VK_NULL_HANDLE;
            }
            if (buffer.memory != VK_NULL_HANDLE) {
               vkFreeMemory(device_, buffer.memory, nullptr);
               buffer.memory = VK_NULL_HANDLE;
            }
            buffer.size = 0;
         }
         resources.descriptor_buffers.clear();

         for (auto descriptor_set_layout : resources.descriptor_set_layouts) {
            if (descriptor_set_layout != VK_NULL_HANDLE) {
               vkDestroyDescriptorSetLayout(device_, descriptor_set_layout, nullptr);
            }
         }
         resources.descriptor_set_layouts.clear();
         resources.descriptor_bindings.clear();
         resources.draw_descriptor_sets.clear();

         for (auto shader_module : resources.shader_modules) {
            if (shader_module.module != VK_NULL_HANDLE) {
               vkDestroyShaderModule(device_, shader_module.module, nullptr);
            }
         }
         resources.shader_modules.clear();
      }

      void destroyGraphicsPipelineResources(VulkanGraphicsPipelineResources &resources) noexcept {
         if (device_ == VK_NULL_HANDLE) {
            return;
         }

         if (resources.pipeline != VK_NULL_HANDLE) {
            vkDestroyPipeline(device_, resources.pipeline, nullptr);
            resources.pipeline = VK_NULL_HANDLE;
         }

         if (resources.render_pass != VK_NULL_HANDLE) {
            vkDestroyRenderPass(device_, resources.render_pass, nullptr);
            resources.render_pass = VK_NULL_HANDLE;
         }
      }

      void destroyWindowSwapchainResources(VulkanWindowSwapchainResources &resources) noexcept {
         if (device_ != VK_NULL_HANDLE) {
            for (auto &sync : resources.frame_sync) {
               if (sync.render_fence != VK_NULL_HANDLE) {
                  vkDestroyFence(device_, sync.render_fence, nullptr);
                  sync.render_fence = VK_NULL_HANDLE;
               }
               if (sync.render_finished != VK_NULL_HANDLE) {
                  vkDestroySemaphore(device_, sync.render_finished, nullptr);
                  sync.render_finished = VK_NULL_HANDLE;
               }
               if (sync.image_available != VK_NULL_HANDLE) {
                  vkDestroySemaphore(device_, sync.image_available, nullptr);
                  sync.image_available = VK_NULL_HANDLE;
               }
            }
            resources.frame_sync.clear();

            if (command_pool_ != VK_NULL_HANDLE && !resources.command_buffers.empty()) {
               vkFreeCommandBuffers(device_, command_pool_,
                                    static_cast<std::uint32_t>(resources.command_buffers.size()),
                                    resources.command_buffers.data());
               resources.command_buffers.clear();
            }

            for (auto framebuffer : resources.framebuffers) {
               if (framebuffer != VK_NULL_HANDLE) {
                  vkDestroyFramebuffer(device_, framebuffer, nullptr);
               }
            }
            resources.framebuffers.clear();

            if (resources.clear_render_pass != VK_NULL_HANDLE) {
               vkDestroyRenderPass(device_, resources.clear_render_pass, nullptr);
               resources.clear_render_pass = VK_NULL_HANDLE;
            }

            for (auto image_view : resources.image_views) {
               if (image_view != VK_NULL_HANDLE) {
                  vkDestroyImageView(device_, image_view, nullptr);
               }
            }
            resources.image_views.clear();

            if (resources.depth_image_view != VK_NULL_HANDLE) {
               vkDestroyImageView(device_, resources.depth_image_view, nullptr);
               resources.depth_image_view = VK_NULL_HANDLE;
            }
            if (resources.depth_image != VK_NULL_HANDLE) {
               vkDestroyImage(device_, resources.depth_image, nullptr);
               resources.depth_image = VK_NULL_HANDLE;
            }
            if (resources.depth_memory != VK_NULL_HANDLE) {
               vkFreeMemory(device_, resources.depth_memory, nullptr);
               resources.depth_memory = VK_NULL_HANDLE;
            }
            resources.depth_format = VK_FORMAT_UNDEFINED;

            if (resources.swapchain != VK_NULL_HANDLE) {
               vkDestroySwapchainKHR(device_, resources.swapchain, nullptr);
               resources.swapchain = VK_NULL_HANDLE;
            }
         }

         resources.images.clear();
         if (instance_ != VK_NULL_HANDLE && resources.surface != VK_NULL_HANDLE) {
            SDL_Vulkan_DestroySurface(instance_, resources.surface, nullptr);
            resources.surface = VK_NULL_HANDLE;
         }

         resources.frame_acquired = false;
         resources.summary.depth_image_created = false;
         resources.summary.depth_image_view_created = false;
         resources.summary.depth_image_count = 0;
         resources.summary.depth_image_view_count = 0;
         resources.summary.frame_acquired = false;
      }

      void destroy() noexcept {
         if (device_ != VK_NULL_HANDLE) {
            (void)vkDeviceWaitIdle(device_);
         }

         for (auto &[handle, resources] : window_swapchains_) {
            (void)handle;
            destroyWindowSwapchainResources(resources);
         }
         window_swapchains_.clear();

         for (auto &[handle, resources] : graphics_pipelines_) {
            (void)handle;
            destroyGraphicsPipelineResources(resources);
         }
         graphics_pipelines_.clear();

         for (auto &[handle, resources] : pipeline_resources_) {
            (void)handle;
            destroyPipelineResources(resources);
         }
         pipeline_resources_.clear();

         for (auto &[handle, resources] : buffers_) {
            (void)handle;
            destroyBufferResources(resources);
         }
         buffers_.clear();

         for (auto &[handle, resources] : images_) {
            (void)handle;
            destroyImageResources(resources);
         }
         images_.clear();

         if (command_pool_ != VK_NULL_HANDLE) {
            vkDestroyCommandPool(device_, command_pool_, nullptr);
            command_pool_ = VK_NULL_HANDLE;
         }

         if (pipeline_cache_ != VK_NULL_HANDLE) {
            vkDestroyPipelineCache(device_, pipeline_cache_, nullptr);
            pipeline_cache_ = VK_NULL_HANDLE;
         }

         if (device_ != VK_NULL_HANDLE) {
            vkDestroyDevice(device_, nullptr);
            device_ = VK_NULL_HANDLE;
         }
         if (instance_ != VK_NULL_HANDLE) {
            vkDestroyInstance(instance_, nullptr);
            instance_ = VK_NULL_HANDLE;
         }

         graphics_queue_ = VK_NULL_HANDLE;
         physical_device_ = VK_NULL_HANDLE;
         graphics_queue_family_ = 0;
         presentation_enabled_ = false;
         initialized_ = false;
      }

      VkInstance instance_{VK_NULL_HANDLE};       ///< Persistent Vulkan instance used for backend resources.
      VkPhysicalDevice physical_device_{VK_NULL_HANDLE}; ///< Selected physical device.
      VkDevice device_{VK_NULL_HANDLE};           ///< Logical device owning backend resources.
      VkQueue graphics_queue_{VK_NULL_HANDLE};    ///< Graphics-capable queue for future work.
      VkPipelineCache pipeline_cache_{VK_NULL_HANDLE}; ///< Backend-owned cache for future VkPipeline creation.
      VkCommandPool command_pool_{VK_NULL_HANDLE}; ///< Command buffers used for presentation clears.
      std::uint32_t graphics_queue_family_{0};    ///< Queue family used to create the logical device.
      std::unordered_map<vve::Handle::value_type, VulkanPipelineResources> pipeline_resources_{};
      std::unordered_map<vve::Handle::value_type, VulkanGraphicsPipelineResources> graphics_pipelines_{};
      std::unordered_map<vve::Handle::value_type, VulkanGpuBufferResources> buffers_{};
      std::unordered_map<vve::Handle::value_type, VulkanGpuImageResources> images_{};
      std::unordered_map<vve::Handle::value_type, VulkanWindowSwapchainResources> window_swapchains_{};
      bool presentation_enabled_{false}; ///< Tracks whether platform presentation support was requested.
      bool initialized_{false}; ///< Tracks whether backend initialization has completed.
   };

   /// @brief Constructs the public graphics-backend facade around the concrete implementation.
   VVE_V3_DEFINE_FACADE_CTOR(GraphicsBackendFacade, VulkanGraphicsBackendImplementation, (), ())

   /// @brief Returns the backend name for the public facade.
   VVE_V3_DEFINE_FACADE_METHOD(GraphicsBackendFacade, VulkanGraphicsBackendImplementation, name, (), (),
                               const noexcept, std::string_view)

   /// @brief Returns the graphics API through the public facade.
   VVE_V3_DEFINE_FACADE_METHOD(GraphicsBackendFacade, VulkanGraphicsBackendImplementation, api, (), (),
                               const noexcept, vve::GraphicsApi)

   /// @brief Initializes the backend through the public facade.
   VVE_V3_DEFINE_FACADE_METHOD(GraphicsBackendFacade, VulkanGraphicsBackendImplementation, init, (), (), ,
                               std::expected<void, vve::Error>)

   /// @brief Initializes the backend with presentation extensions through the public facade.
   VVE_V3_DEFINE_FACADE_METHOD(GraphicsBackendFacade, VulkanGraphicsBackendImplementation, init,
                               (const std::vector<std::string> &instance_extensions), (instance_extensions), ,
                               std::expected<void, vve::Error>)

   /// @brief Returns supported renderer descriptors through the public facade.
   VVE_V3_DEFINE_FACADE_METHOD(GraphicsBackendFacade, VulkanGraphicsBackendImplementation, supportedRenderers, (),
                               (), const, std::vector<RendererDesc>)

   /// @brief Creates or resolves a renderer descriptor through the public facade.
   VVE_V3_DEFINE_FACADE_METHOD(GraphicsBackendFacade, VulkanGraphicsBackendImplementation, createRenderer,
                               (std::string_view renderer_id), (renderer_id), const,
                               std::expected<RendererDesc, vve::Error>)

   /// @brief Builds a pipeline layout description through the public facade.
   VVE_V3_DEFINE_FACADE_METHOD(GraphicsBackendFacade, VulkanGraphicsBackendImplementation, createPipelineLayout,
                               (const RendererDesc &renderer, const ShaderMetadata &shader), (renderer, shader), const,
                               std::expected<PipelineLayoutDesc, vve::Error>)

   /// @brief Creates backend-owned pipeline resources through the public facade.
   VVE_V3_DEFINE_FACADE_METHOD(GraphicsBackendFacade, VulkanGraphicsBackendImplementation, createPipelineResources,
                               (const PipelineLayoutDesc &layout, const ShaderMetadata &shader), (layout, shader), ,
                               std::expected<PipelineBackendResources, vve::Error>)

   /// @brief Returns backend resource metadata through the public facade.
   VVE_V3_DEFINE_FACADE_METHOD(GraphicsBackendFacade, VulkanGraphicsBackendImplementation, pipelineResources,
                               (PipelineResourceHandle resources), (resources), const,
                               std::expected<std::optional<PipelineBackendResources>, vve::Error>)

   /// @brief Creates backend graphics pipeline preparation data through the public facade.
   VVE_V3_DEFINE_FACADE_METHOD(GraphicsBackendFacade, VulkanGraphicsBackendImplementation,
                               createGraphicsPipelineResources,
                               (const RendererPipelineBinding &binding, const GraphicsPipelineDesc &desc),
                               (binding, desc), , std::expected<GraphicsPipelineResources, vve::Error>)

   /// @brief Returns backend graphics pipeline metadata through the public facade.
   VVE_V3_DEFINE_FACADE_METHOD(GraphicsBackendFacade, VulkanGraphicsBackendImplementation,
                               graphicsPipelineResources, (GraphicsPipelineHandle pipeline), (pipeline), const,
                               std::expected<std::optional<GraphicsPipelineResources>, vve::Error>)

   /// @brief Creates a backend-owned GPU buffer through the public facade.
   VVE_V3_DEFINE_FACADE_METHOD(GraphicsBackendFacade, VulkanGraphicsBackendImplementation, createBuffer,
                               (vve::Handle owner, ResourceKind owner_kind, GpuBufferUsage usage,
                                std::span<const std::byte> bytes, std::uint32_t generation),
                               (owner, owner_kind, usage, bytes, generation), ,
                               std::expected<GpuBufferResources, vve::Error>)

   /// @brief Returns backend buffer metadata through the public facade.
   VVE_V3_DEFINE_FACADE_METHOD(GraphicsBackendFacade, VulkanGraphicsBackendImplementation, bufferResources,
                               (GpuBufferHandle buffer), (buffer), const,
                               std::expected<std::optional<GpuBufferResources>, vve::Error>)

   /// @brief Destroys a backend-owned GPU buffer through the public facade.
   VVE_V3_DEFINE_FACADE_METHOD(GraphicsBackendFacade, VulkanGraphicsBackendImplementation, destroyBuffer,
                               (GpuBufferHandle buffer), (buffer), , std::expected<void, vve::Error>)

   /// @brief Creates a backend-owned sampled image through the public facade.
   VVE_V3_DEFINE_FACADE_METHOD(GraphicsBackendFacade, VulkanGraphicsBackendImplementation, createSampledImage,
                               (vve::Handle owner, ResourceKind owner_kind, GpuImageFormat format,
                                std::uint32_t width, std::uint32_t height,
                                std::span<const std::byte> rgba_pixels, std::uint32_t generation),
                               (owner, owner_kind, format, width, height, rgba_pixels, generation), ,
                               std::expected<GpuTextureResources, vve::Error>)

   /// @brief Returns backend sampled image metadata through the public facade.
   VVE_V3_DEFINE_FACADE_METHOD(GraphicsBackendFacade, VulkanGraphicsBackendImplementation, imageResources,
                               (GpuImageHandle image), (image), const,
                               std::expected<std::optional<GpuTextureResources>, vve::Error>)

   /// @brief Destroys a backend-owned sampled image through the public facade.
   VVE_V3_DEFINE_FACADE_METHOD(GraphicsBackendFacade, VulkanGraphicsBackendImplementation, destroyImage,
                               (GpuImageHandle image), (image), , std::expected<void, vve::Error>)

   /// @brief Creates window swapchain resources through the public facade.
   VVE_V3_DEFINE_FACADE_METHOD(GraphicsBackendFacade, VulkanGraphicsBackendImplementation, createWindowSwapchain,
                               (const NativeWindowHandle &window), (window), ,
                               std::expected<WindowSwapchainResources, vve::Error>)

   /// @brief Recreates window swapchain resources through the public facade.
   VVE_V3_DEFINE_FACADE_METHOD(GraphicsBackendFacade, VulkanGraphicsBackendImplementation, recreateWindowSwapchain,
                               (const NativeWindowHandle &window), (window), ,
                               std::expected<WindowSwapchainResources, vve::Error>)

   /// @brief Returns window swapchain metadata through the public facade.
   VVE_V3_DEFINE_FACADE_METHOD(GraphicsBackendFacade, VulkanGraphicsBackendImplementation, windowSwapchain,
                               (SwapchainHandle swapchain), (swapchain), const,
                               std::expected<std::optional<WindowSwapchainResources>, vve::Error>)

   /// @brief Records one acquired window frame through the public facade.
   VVE_V3_DEFINE_FACADE_METHOD(GraphicsBackendFacade, VulkanGraphicsBackendImplementation, recordWindowFrame,
                               (SwapchainHandle swapchain), (swapchain), , std::expected<void, vve::Error>)

   /// @brief Records draw packets for one acquired window frame through the public facade.
   VVE_V3_DEFINE_FACADE_METHOD(GraphicsBackendFacade, VulkanGraphicsBackendImplementation, recordWindowFrame,
                               (SwapchainHandle swapchain, const WindowDrawPacketList &draw_packets),
                               (swapchain, draw_packets), , std::expected<void, vve::Error>)

   /// @brief Submits one recorded window frame through the public facade.
   VVE_V3_DEFINE_FACADE_METHOD(GraphicsBackendFacade, VulkanGraphicsBackendImplementation, submitWindowFrame,
                               (SwapchainHandle swapchain), (swapchain), , std::expected<void, vve::Error>)

   /// @brief Begins a frame through the public facade.
   VVE_V3_DEFINE_FACADE_METHOD(GraphicsBackendFacade, VulkanGraphicsBackendImplementation, beginFrame,
                               (const FrameContext &frame_context), (frame_context), ,
                               std::expected<void, vve::Error>)

   /// @brief Ends a frame through the public facade.
   VVE_V3_DEFINE_FACADE_METHOD(GraphicsBackendFacade, VulkanGraphicsBackendImplementation, endFrame,
                               (const FrameContext &frame_context), (frame_context), ,
                               std::expected<void, vve::Error>)

   /// @brief Registers backend tasks through the public facade.
   VVE_V3_DEFINE_FACADE_VOID_METHOD(GraphicsBackendFacade, VulkanGraphicsBackendImplementation, registerTasks,
                                    (TaskGraphBuilder &builder), (builder), )

   /// @brief Emits the explicit graphics-backend facade instantiation for v3.
   template class GraphicsBackendFacade<VulkanGraphicsBackendImplementation>;

} // namespace vve::v3
