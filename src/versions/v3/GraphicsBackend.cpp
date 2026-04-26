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

      /// @brief Backend-owned Vulkan objects created for one reflected pipeline layout.
      struct VulkanPipelineResources {
         PipelineBackendResources summary{};
         std::vector<VkShaderModule> shader_modules{};
         std::vector<VkDescriptorSetLayout> descriptor_set_layouts{};
         VkPipelineLayout pipeline_layout{VK_NULL_HANDLE};
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

      /// @brief Returns whether a physical device supports a queue family with graphics work.
      [[nodiscard]] std::optional<std::uint32_t> graphicsQueueFamily(VkPhysicalDevice device) {
         std::uint32_t queue_family_count = 0;
         vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, nullptr);
         if (queue_family_count == 0) {
            return std::nullopt;
         }

         std::vector<VkQueueFamilyProperties> queue_families(queue_family_count);
         vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, queue_families.data());
         for (std::uint32_t index = 0; index < queue_family_count; ++index) {
            if ((queue_families[index].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0 &&
                queue_families[index].queueCount > 0) {
               return index;
            }
         }

         return std::nullopt;
      }

      /// @brief Selects the first physical device that can create graphics-capable resources.
      [[nodiscard]] std::expected<SelectedPhysicalDevice, vve::Error> selectPhysicalDevice(VkInstance instance) {
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
            if (const auto queue_family = graphicsQueueFamily(device)) {
               return SelectedPhysicalDevice{.device = device, .graphics_queue_family = *queue_family};
            }
         }

         std::cerr << "[VulkanGraphicsBackend] no graphics-capable Vulkan queue family found\n";
         return std::unexpected(vve::Error::internal_error);
      }

      /// @brief Returns device extensions required for the selected physical device.
      [[nodiscard]] std::vector<const char *> requiredDeviceExtensions(VkPhysicalDevice device) {
         std::vector<const char *> extensions{};
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
      ~VulkanGraphicsBackendImplementation() { destroy(); }

      /// @brief Returns the backend name for diagnostics.
      [[nodiscard]] std::string_view name() const noexcept { return "VulkanGraphicsBackend"; }

      /// @brief Returns the public graphics API implemented by this backend.
      [[nodiscard]] vve::GraphicsApi api() const noexcept { return vve::GraphicsApi::vulkan; }

      /// @brief Initializes backend-owned state.
      [[nodiscard]] std::expected<void, vve::Error> init() {
         if (initialized_) {
            return {};
         }

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
         instance_ = *instance;

         if (auto device_result = logPhysicalDevices(instance_); !device_result) {
            destroy();
            return std::unexpected(device_result.error());
         }

         const auto selected_device = selectPhysicalDevice(instance_);
         if (!selected_device) {
            destroy();
            return std::unexpected(selected_device.error());
         }

         physical_device_ = selected_device->device;
         graphics_queue_family_ = selected_device->graphics_queue_family;
         if (auto logical_device = createLogicalDevice(); !logical_device) {
            destroy();
            return std::unexpected(logical_device.error());
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
      [[nodiscard]] std::expected<void, vve::Error> createLogicalDevice() {
         const float queue_priority = 1.0F;
         const VkDeviceQueueCreateInfo queue_create_info{.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                                                         .pNext = nullptr,
                                                         .flags = 0,
                                                         .queueFamilyIndex = graphics_queue_family_,
                                                         .queueCount = 1,
                                                         .pQueuePriorities = &queue_priority};
         const auto enabled_extensions = requiredDeviceExtensions(physical_device_);
         const VkDeviceCreateInfo create_info{.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
                                              .pNext = nullptr,
                                              .flags = 0,
                                              .queueCreateInfoCount = 1,
                                              .pQueueCreateInfos = &queue_create_info,
                                              .enabledLayerCount = 0,
                                              .ppEnabledLayerNames = nullptr,
                                              .enabledExtensionCount =
                                                  static_cast<std::uint32_t>(enabled_extensions.size()),
                                              .ppEnabledExtensionNames = enabled_extensions.data(),
                                              .pEnabledFeatures = nullptr};

         const VkResult result = vkCreateDevice(physical_device_, &create_info, nullptr, &device_);
         if (result != VK_SUCCESS) {
            std::cerr << "[VulkanGraphicsBackend] vkCreateDevice failed: " << string_VkResult(result) << '\n';
            return std::unexpected(vve::Error::internal_error);
         }

         vkGetDeviceQueue(device_, graphics_queue_family_, 0, &graphics_queue_);
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

            resources.shader_modules.push_back(shader_module);
         }

         return {};
      }

      [[nodiscard]] std::expected<void, vve::Error>
      createDescriptorSetLayouts(VulkanPipelineResources &resources, const PipelineLayoutDesc &layout) {
         resources.descriptor_set_layouts.reserve(layout.descriptor_sets.size());
         for (const auto &descriptor_set : layout.descriptor_sets) {
            std::map<std::uint32_t, VkDescriptorSetLayoutBinding> bindings_by_index{};
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
            }

            std::vector<VkDescriptorSetLayoutBinding> bindings{};
            bindings.reserve(bindings_by_index.size());
            for (const auto &[binding_index, binding] : bindings_by_index) {
               (void)binding_index;
               bindings.push_back(binding);
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
         }

         return {};
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

      void destroyPipelineResources(VulkanPipelineResources &resources) noexcept {
         if (device_ == VK_NULL_HANDLE) {
            return;
         }

         if (resources.pipeline_layout != VK_NULL_HANDLE) {
            vkDestroyPipelineLayout(device_, resources.pipeline_layout, nullptr);
            resources.pipeline_layout = VK_NULL_HANDLE;
         }

         for (auto descriptor_set_layout : resources.descriptor_set_layouts) {
            if (descriptor_set_layout != VK_NULL_HANDLE) {
               vkDestroyDescriptorSetLayout(device_, descriptor_set_layout, nullptr);
            }
         }
         resources.descriptor_set_layouts.clear();

         for (auto shader_module : resources.shader_modules) {
            if (shader_module != VK_NULL_HANDLE) {
               vkDestroyShaderModule(device_, shader_module, nullptr);
            }
         }
         resources.shader_modules.clear();
      }

      void destroy() noexcept {
         for (auto &[handle, resources] : pipeline_resources_) {
            (void)handle;
            destroyPipelineResources(resources);
         }
         pipeline_resources_.clear();

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
         initialized_ = false;
      }

      VkInstance instance_{VK_NULL_HANDLE};       ///< Persistent Vulkan instance used for backend resources.
      VkPhysicalDevice physical_device_{VK_NULL_HANDLE}; ///< Selected physical device.
      VkDevice device_{VK_NULL_HANDLE};           ///< Logical device owning backend resources.
      VkQueue graphics_queue_{VK_NULL_HANDLE};    ///< Graphics-capable queue for future work.
      std::uint32_t graphics_queue_family_{0};    ///< Queue family used to create the logical device.
      std::unordered_map<vve::Handle::value_type, VulkanPipelineResources> pipeline_resources_{};
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

   /// @brief Creates backend-owned pipeline resources through the public facade.
   VVE_V3_DEFINE_FACADE_METHOD(GraphicsBackendFacade, VulkanGraphicsBackendImplementation, createPipelineResources,
                               (const PipelineLayoutDesc &layout, const ShaderMetadata &shader), (layout, shader), ,
                               std::expected<PipelineBackendResources, vve::Error>)

   /// @brief Returns backend resource metadata through the public facade.
   VVE_V3_DEFINE_FACADE_METHOD(GraphicsBackendFacade, VulkanGraphicsBackendImplementation, pipelineResources,
                               (PipelineResourceHandle resources), (resources), const,
                               std::expected<std::optional<PipelineBackendResources>, vve::Error>)

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
