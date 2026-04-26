module;

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
