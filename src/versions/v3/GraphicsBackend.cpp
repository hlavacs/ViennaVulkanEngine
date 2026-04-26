module;

#include <cctype>
#include "FacadeMacros.hpp"
#include <cstdlib>
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

      /// @brief Creates a short-lived Vulkan instance for startup diagnostics.
      [[nodiscard]] std::expected<VkInstance, vve::Error> createDiagnosticInstance() {
         const auto extensions = enumerateInstanceExtensions();
         if (!extensions) {
            return std::unexpected(extensions.error());
         }

         std::vector<const char *> enabled_extensions{};
         VkInstanceCreateFlags create_flags = 0;
         if (hasExtension(*extensions, VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME)) {
            enabled_extensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
         }
         if (hasExtension(*extensions, VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME)) {
            enabled_extensions.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
            create_flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
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
      /// @brief Returns the backend name for diagnostics.
      [[nodiscard]] std::string_view name() const noexcept { return "VulkanGraphicsBackend"; }

      /// @brief Returns the public graphics API implemented by this backend.
      [[nodiscard]] vve::GraphicsApi api() const noexcept { return vve::GraphicsApi::vulkan; }

      /// @brief Initializes backend-owned state.
      [[nodiscard]] std::expected<void, vve::Error> init() {
         if (auto diagnostics_result = logVulkanStartupDiagnostics(); !diagnostics_result) {
            return std::unexpected(diagnostics_result.error());
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

      /// @brief Performs begin-frame backend work.
      [[nodiscard]] std::expected<void, vve::Error> beginFrame(const FrameContext &) {
         if (!initialized_) {
            return std::unexpected(vve::Error::not_initialized);
         }

         return {};
      }

      /// @brief Performs end-frame backend work.
      [[nodiscard]] std::expected<void, vve::Error> endFrame(const FrameContext &) {
         if (!initialized_) {
            return std::unexpected(vve::Error::not_initialized);
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
