module;

#include <vulkan/vulkan.h>

export module vh;
export import vh.low;
import std;

export namespace vh {

   struct InstanceOptions {
      low::InstanceProfile profile{};
   };

   struct DeviceSelectionOptions {
      low::PhysicalDeviceSelectionRequest request{};
   };

   struct DeviceOptions {
      low::DeviceProfile profile{};
   };

   struct BufferOptions {
      low::BufferAllocationRequest request{};
   };

   struct Image2DOptions {
      low::Image2DAllocationRequest request{};
   };

   [[nodiscard]] std::expected<std::vector<VkExtensionProperties>, VkResult> enumerateInstanceExtensions();
   [[nodiscard]] std::expected<std::vector<VkPhysicalDevice>, VkResult> enumeratePhysicalDevices(VkInstance instance);
   [[nodiscard]] std::expected<low::PhysicalDeviceSelection, VkResult>
   selectPhysicalDevice(const low::PhysicalDeviceSelectionRequest &request);
   [[nodiscard]] std::expected<low::DeviceCreation, VkResult>
   createDevice(const low::DeviceProfile &profile);

   class Instance {
   public:
      Instance() = default;
      ~Instance();

      Instance(const Instance &) = delete;
      Instance &operator=(const Instance &) = delete;

      Instance(Instance &&other) noexcept;
      Instance &operator=(Instance &&other) noexcept;

      [[nodiscard]] static std::expected<Instance, VkResult> create(const low::InstanceProfile &profile);

      [[nodiscard]] VkInstance get() const noexcept { return instance_; }
      [[nodiscard]] const low::InstancePlan &plan() const noexcept { return plan_; }
      [[nodiscard]] explicit operator bool() const noexcept { return instance_ != VK_NULL_HANDLE; }

      VkInstance release() noexcept;
      void reset(VkInstance instance = VK_NULL_HANDLE) noexcept;

   private:
      Instance(VkInstance instance, low::InstancePlan plan) noexcept;

      VkInstance instance_{VK_NULL_HANDLE};
      low::InstancePlan plan_{};
   };

   class Device {
   public:
      Device() = default;
      ~Device();

      Device(const Device &) = delete;
      Device &operator=(const Device &) = delete;

      Device(Device &&other) noexcept;
      Device &operator=(Device &&other) noexcept;

      [[nodiscard]] static std::expected<Device, VkResult> create(const low::DeviceProfile &profile);

      [[nodiscard]] VkDevice get() const noexcept { return creation_.device; }
      [[nodiscard]] VkQueue queue() const noexcept { return creation_.queue; }
      [[nodiscard]] std::uint32_t queueFamily() const noexcept { return queue_family_; }
      [[nodiscard]] const std::vector<const char *> &enabledExtensions() const noexcept {
         return creation_.enabled_extensions;
      }
      [[nodiscard]] explicit operator bool() const noexcept { return creation_.device != VK_NULL_HANDLE; }

      VkDevice release() noexcept;
      void reset(VkDevice device = VK_NULL_HANDLE) noexcept;

   private:
      Device(low::DeviceCreation creation, std::uint32_t queue_family) noexcept;

      low::DeviceCreation creation_{};
      std::uint32_t queue_family_{0};
   };

   class CommandPool {
   public:
      CommandPool() = default;
      ~CommandPool();

      CommandPool(const CommandPool &) = delete;
      CommandPool &operator=(const CommandPool &) = delete;

      CommandPool(CommandPool &&other) noexcept;
      CommandPool &operator=(CommandPool &&other) noexcept;

      [[nodiscard]] static std::expected<CommandPool, VkResult>
      create(VkDevice device, std::uint32_t queue_family,
             VkCommandPoolCreateFlags flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

      [[nodiscard]] VkCommandPool get() const noexcept { return command_pool_; }
      [[nodiscard]] VkDevice device() const noexcept { return device_; }
      [[nodiscard]] explicit operator bool() const noexcept { return command_pool_ != VK_NULL_HANDLE; }

      VkCommandPool release() noexcept;
      void reset(VkCommandPool command_pool = VK_NULL_HANDLE) noexcept;

   private:
      CommandPool(VkDevice device, VkCommandPool command_pool) noexcept;

      VkDevice device_{VK_NULL_HANDLE};
      VkCommandPool command_pool_{VK_NULL_HANDLE};
   };

   class Buffer {
   public:
      Buffer() = default;
      ~Buffer();

      Buffer(const Buffer &) = delete;
      Buffer &operator=(const Buffer &) = delete;

      Buffer(Buffer &&other) noexcept;
      Buffer &operator=(Buffer &&other) noexcept;

      [[nodiscard]] static std::expected<Buffer, VkResult> create(const low::BufferAllocationRequest &request);

      [[nodiscard]] VkBuffer get() const noexcept { return allocation_.buffer; }
      [[nodiscard]] VkDeviceMemory memory() const noexcept { return allocation_.memory; }
      [[nodiscard]] VkDeviceSize size() const noexcept { return allocation_.size; }
      [[nodiscard]] explicit operator bool() const noexcept { return allocation_.buffer != VK_NULL_HANDLE; }

      low::BufferAllocation release() noexcept;
      void reset() noexcept;

   private:
      Buffer(VkDevice device, low::BufferAllocation allocation) noexcept;

      VkDevice device_{VK_NULL_HANDLE};
      low::BufferAllocation allocation_{};
   };

   class Image2D {
   public:
      Image2D() = default;
      ~Image2D();

      Image2D(const Image2D &) = delete;
      Image2D &operator=(const Image2D &) = delete;

      Image2D(Image2D &&other) noexcept;
      Image2D &operator=(Image2D &&other) noexcept;

      [[nodiscard]] static std::expected<Image2D, VkResult> create(const low::Image2DAllocationRequest &request);

      [[nodiscard]] VkImage get() const noexcept { return allocation_.image; }
      [[nodiscard]] VkDeviceMemory memory() const noexcept { return allocation_.memory; }
      [[nodiscard]] VkImageView view() const noexcept { return allocation_.view; }
      [[nodiscard]] VkSampler sampler() const noexcept { return allocation_.sampler; }
      [[nodiscard]] std::uint32_t width() const noexcept { return allocation_.width; }
      [[nodiscard]] std::uint32_t height() const noexcept { return allocation_.height; }
      [[nodiscard]] VkFormat format() const noexcept { return allocation_.format; }
      [[nodiscard]] explicit operator bool() const noexcept { return allocation_.image != VK_NULL_HANDLE; }

      low::Image2DAllocation release() noexcept;
      void reset() noexcept;

   private:
      Image2D(VkDevice device, low::Image2DAllocation allocation) noexcept;

      VkDevice device_{VK_NULL_HANDLE};
      low::Image2DAllocation allocation_{};
   };

} // namespace vh
