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

} // namespace vve::v4

namespace vve::v4 {

   /// @brief Internal resource record addressed by a resource handle.
   struct ResourceRecord {
      ResourceHandle handle{};                  ///< Stable resource handle.
      ResourceKind kind{ResourceKind::unknown}; ///< Resource category.
      ObjectName name{};                        ///< Human-readable resource name.
   };

} // namespace vve::v4

export namespace vve::v4 {

   /// @brief Minimal resource table; real upload and residency are future steps.
   class ResourceSystem {
   public:
      /// @brief Adds a resource and returns its handle.
      [[nodiscard]] std::expected<ResourceHandle, Error> add(ResourceKind kind, ObjectName name) {
         auto handle = makeCounterHandle<ResourceHandle>();
         const auto [_, inserted] = resources_.emplace(
            handle, ResourceRecord{.handle = handle, .kind = kind, .name = std::move(name)});
         if (!inserted) { return std::unexpected(Error::duplicate_object); }
         return handle;
      }

      /// @brief Returns whether a resource exists.
      [[nodiscard]] bool contains(ResourceHandle handle) const { return resources_.contains(handle); }

      /// @brief Returns the name for a resource.
      [[nodiscard]] std::expected<ObjectName, Error> resourceName(ResourceHandle handle) const {
         const auto resource = resources_.find(handle);
         if (resource == resources_.end()) { return std::unexpected(Error::missing_object); }
         return resource->second.name;
      }

      /// @brief Returns the kind for a resource.
      [[nodiscard]] std::expected<ResourceKind, Error> resourceKind(ResourceHandle handle) const {
         const auto resource = resources_.find(handle);
         if (resource == resources_.end()) { return std::unexpected(Error::missing_object); }
         return resource->second.kind;
      }

      /// @brief Returns the number of registered resources.
      [[nodiscard]] std::size_t resourceCount() const { return resources_.size(); }

   private:
      std::map<ResourceHandle, ResourceRecord> resources_{}; ///< Resources by handle.
   };

} // namespace vve::v4
