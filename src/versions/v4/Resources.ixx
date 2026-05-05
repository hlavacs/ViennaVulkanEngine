export module VEEngine.V4:Resources;
import std;
export import :Types;

/// @file
/// @brief Stub resource registry for CPU/GPU objects that do not yet have real backends.

export namespace vve::v4 {

   /// @brief Coarse resource category used before concrete Vulkan resources exist.
   enum class ResourceKind {
      unknown,  ///< Unclassified resource.
      mesh,     ///< Mesh resource.
      texture,  ///< Texture resource.
      material, ///< Material resource.
      shader,   ///< Shader resource.
      buffer,   ///< Buffer resource.
      image     ///< Image resource.
   };

   /// @brief Resource descriptor addressed by a 64-bit handle.
   struct ResourceDescriptor {
      using HandleType = ResourceHandle;            ///< Descriptor handle type.
      ResourceHandle handle{};                      ///< Stable resource handle.
      ResourceKind kind{ResourceKind::unknown};     ///< Resource category.
      ObjectName name{};                            ///< Human-readable resource name.
   };

   /// @brief Minimal resource table; real upload and residency are future steps.
   class ResourceSystem {
   public:
      /// @brief Adds a resource descriptor and returns its handle.
      [[nodiscard]] std::expected<ResourceHandle, Error> add(ResourceKind kind, ObjectName name) {
         auto handle = makeCounterHandle<ResourceHandle>();
         const auto [_, inserted] = resources_.emplace(
            handle, ResourceDescriptor{.handle = handle, .kind = kind, .name = std::move(name)});
         if (!inserted) { return std::unexpected(Error::duplicate_object); }
         return handle;
      }

      /// @brief Finds a resource by handle, or returns null.
      [[nodiscard]] const ResourceDescriptor *find(ResourceHandle handle) const {
         const auto resource = resources_.find(handle);
         return resource == resources_.end() ? nullptr : std::addressof(resource->second);
      }

      /// @brief Returns the number of registered resources.
      [[nodiscard]] std::size_t size() const { return resources_.size(); }

   private:
      std::map<ResourceHandle, ResourceDescriptor> resources_{}; ///< Resource descriptors by handle.
   };

} // namespace vve::v4
