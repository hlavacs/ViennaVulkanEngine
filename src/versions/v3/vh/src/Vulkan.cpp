module;

#include <vulkan/vulkan.h>

module vh;
import std;
import vh.low;

namespace vh {

   std::expected<std::vector<VkExtensionProperties>, VkResult> enumerateInstanceExtensions() {
      std::vector<VkExtensionProperties> extensions{};
      if (const VkResult result = low::enumerateInstanceExtensions(extensions); result != VK_SUCCESS) {
         return std::unexpected(result);
      }
      return extensions;
   }

   std::expected<std::vector<VkPhysicalDevice>, VkResult> enumeratePhysicalDevices(VkInstance instance) {
      std::vector<VkPhysicalDevice> devices{};
      if (const VkResult result = low::enumeratePhysicalDevices(instance, devices); result != VK_SUCCESS) {
         return std::unexpected(result);
      }
      return devices;
   }

   std::expected<low::PhysicalDeviceSelection, VkResult>
   selectPhysicalDevice(const low::PhysicalDeviceSelectionRequest &request) {
      low::PhysicalDeviceSelection selection{};
      if (const VkResult result = low::selectPhysicalDevice(request, selection); result != VK_SUCCESS) {
         return std::unexpected(result);
      }
      return selection;
   }

   std::expected<low::DeviceCreation, VkResult> createDevice(const low::DeviceProfile &profile) {
      low::DeviceCreation creation{};
      if (const VkResult result = low::createDevice(profile, creation); result != VK_SUCCESS) {
         return std::unexpected(result);
      }
      return creation;
   }

   Instance::Instance(VkInstance instance, low::InstancePlan plan) noexcept
       : instance_{instance}, plan_{std::move(plan)} {}

   Instance::~Instance() {
      reset();
   }

   Instance::Instance(Instance &&other) noexcept
       : instance_{other.release()}, plan_{std::move(other.plan_)} {}

   Instance &Instance::operator=(Instance &&other) noexcept {
      if (this != &other) {
         reset(other.release());
         plan_ = std::move(other.plan_);
      }
      return *this;
   }

   std::expected<Instance, VkResult> Instance::create(const low::InstanceProfile &profile) {
      VkInstance instance = VK_NULL_HANDLE;
      low::InstancePlan plan{};
      if (const VkResult result = low::createInstance(profile, &plan, &instance); result != VK_SUCCESS) {
         return std::unexpected(result);
      }
      return Instance{instance, std::move(plan)};
   }

   VkInstance Instance::release() noexcept {
      return std::exchange(instance_, VK_NULL_HANDLE);
   }

   void Instance::reset(VkInstance instance) noexcept {
      if (instance_ != VK_NULL_HANDLE) {
         vkDestroyInstance(instance_, nullptr);
      }
      instance_ = instance;
      if (instance_ == VK_NULL_HANDLE) {
         plan_ = {};
      }
   }

   Device::Device(low::DeviceCreation creation, std::uint32_t queue_family) noexcept
       : creation_{std::move(creation)}, queue_family_{queue_family} {}

   Device::~Device() {
      reset();
   }

   Device::Device(Device &&other) noexcept
       : creation_{std::exchange(other.creation_, low::DeviceCreation{})},
         queue_family_{std::exchange(other.queue_family_, 0U)} {}

   Device &Device::operator=(Device &&other) noexcept {
      if (this != &other) {
         reset();
         creation_ = std::exchange(other.creation_, low::DeviceCreation{});
         queue_family_ = std::exchange(other.queue_family_, 0U);
      }
      return *this;
   }

   std::expected<Device, VkResult> Device::create(const low::DeviceProfile &profile) {
      low::DeviceCreation creation{};
      if (const VkResult result = low::createDevice(profile, creation); result != VK_SUCCESS) {
         return std::unexpected(result);
      }
      return Device{std::move(creation), profile.queue_family};
   }

   VkDevice Device::release() noexcept {
      const auto released = creation_.device;
      creation_ = {};
      queue_family_ = 0;
      return released;
   }

   void Device::reset(VkDevice device) noexcept {
      if (creation_.device != VK_NULL_HANDLE) {
         (void)vkDeviceWaitIdle(creation_.device);
         vkDestroyDevice(creation_.device, nullptr);
      }
      creation_ = {};
      creation_.device = device;
      queue_family_ = 0;
   }

   CommandPool::CommandPool(VkDevice device, VkCommandPool command_pool) noexcept
       : device_{device}, command_pool_{command_pool} {}

   CommandPool::~CommandPool() {
      reset();
   }

   CommandPool::CommandPool(CommandPool &&other) noexcept
       : device_{std::exchange(other.device_, VK_NULL_HANDLE)},
         command_pool_{std::exchange(other.command_pool_, VK_NULL_HANDLE)} {}

   CommandPool &CommandPool::operator=(CommandPool &&other) noexcept {
      if (this != &other) {
         reset();
         device_ = std::exchange(other.device_, VK_NULL_HANDLE);
         command_pool_ = std::exchange(other.command_pool_, VK_NULL_HANDLE);
      }
      return *this;
   }

   std::expected<CommandPool, VkResult>
   CommandPool::create(VkDevice device, std::uint32_t queue_family, VkCommandPoolCreateFlags flags) {
      if (device == VK_NULL_HANDLE) {
         return std::unexpected(VK_ERROR_INITIALIZATION_FAILED);
      }

      const VkCommandPoolCreateInfo create_info{.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
                                                .pNext = nullptr,
                                                .flags = flags,
                                                .queueFamilyIndex = queue_family};
      VkCommandPool command_pool = VK_NULL_HANDLE;
      const VkResult result = vkCreateCommandPool(device, &create_info, nullptr, &command_pool);
      if (result != VK_SUCCESS) {
         return std::unexpected(result);
      }
      return CommandPool{device, command_pool};
   }

   VkCommandPool CommandPool::release() noexcept {
      const auto released = command_pool_;
      command_pool_ = VK_NULL_HANDLE;
      device_ = VK_NULL_HANDLE;
      return released;
   }

   void CommandPool::reset(VkCommandPool command_pool) noexcept {
      if (device_ != VK_NULL_HANDLE && command_pool_ != VK_NULL_HANDLE) {
         vkDestroyCommandPool(device_, command_pool_, nullptr);
      }
      command_pool_ = command_pool;
      if (command_pool_ == VK_NULL_HANDLE) {
         device_ = VK_NULL_HANDLE;
      }
   }

   Buffer::Buffer(VkDevice device, low::BufferAllocation allocation) noexcept
       : device_{device}, allocation_{allocation} {}

   Buffer::~Buffer() {
      reset();
   }

   Buffer::Buffer(Buffer &&other) noexcept
       : device_{std::exchange(other.device_, VK_NULL_HANDLE)},
         allocation_{std::exchange(other.allocation_, low::BufferAllocation{})} {}

   Buffer &Buffer::operator=(Buffer &&other) noexcept {
      if (this != &other) {
         reset();
         device_ = std::exchange(other.device_, VK_NULL_HANDLE);
         allocation_ = std::exchange(other.allocation_, low::BufferAllocation{});
      }
      return *this;
   }

   std::expected<Buffer, VkResult> Buffer::create(const low::BufferAllocationRequest &request) {
      low::BufferAllocation allocation{};
      if (const VkResult result = low::allocateBuffer(request, allocation); result != VK_SUCCESS) {
         return std::unexpected(result);
      }
      return Buffer{request.device, allocation};
   }

   low::BufferAllocation Buffer::release() noexcept {
      device_ = VK_NULL_HANDLE;
      return std::exchange(allocation_, low::BufferAllocation{});
   }

   void Buffer::reset() noexcept {
      low::destroyBuffer(device_, allocation_);
      device_ = VK_NULL_HANDLE;
   }

   Image2D::Image2D(VkDevice device, low::Image2DAllocation allocation) noexcept
       : device_{device}, allocation_{allocation} {}

   Image2D::~Image2D() {
      reset();
   }

   Image2D::Image2D(Image2D &&other) noexcept
       : device_{std::exchange(other.device_, VK_NULL_HANDLE)},
         allocation_{std::exchange(other.allocation_, low::Image2DAllocation{})} {}

   Image2D &Image2D::operator=(Image2D &&other) noexcept {
      if (this != &other) {
         reset();
         device_ = std::exchange(other.device_, VK_NULL_HANDLE);
         allocation_ = std::exchange(other.allocation_, low::Image2DAllocation{});
      }
      return *this;
   }

   std::expected<Image2D, VkResult> Image2D::create(const low::Image2DAllocationRequest &request) {
      low::Image2DAllocation allocation{};
      if (const VkResult result = low::allocateImage2D(request, allocation); result != VK_SUCCESS) {
         return std::unexpected(result);
      }
      return Image2D{request.device, allocation};
   }

   low::Image2DAllocation Image2D::release() noexcept {
      device_ = VK_NULL_HANDLE;
      return std::exchange(allocation_, low::Image2DAllocation{});
   }

   void Image2D::reset() noexcept {
      low::destroyImage2D(device_, allocation_);
      device_ = VK_NULL_HANDLE;
   }

} // namespace vh
