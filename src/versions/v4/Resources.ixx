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
      Handle handle{};                              ///< Stable resource handle.
      ResourceKind kind{ResourceKind::unknown};     ///< Resource category.
      std::string name{};                           ///< Human-readable resource name.
   };

   /// @brief Minimal resource table; real upload and residency are future steps.
   class ResourceSystem {
   public:
      /// @brief Adds a resource descriptor and returns its handle.
      [[nodiscard]] std::expected<Handle, Error> add(ResourceKind kind, std::string name) {
         auto handle = makeHandle(ObjectKind::resource, static_cast<std::uint32_t>(resources_.size()));
         auto added = resources_.add(ResourceDescriptor{.handle = handle, .kind = kind, .name = std::move(name)});
         if (!added) { return std::unexpected(added.error()); }
         return handle;
      }

      /// @brief Finds a resource by handle, or returns null.
      [[nodiscard]] const ResourceDescriptor *find(Handle handle) const { return resources_.find(handle); }
      /// @brief Returns the number of registered resources.
      [[nodiscard]] std::size_t size() const { return resources_.size(); }

   private:
      DescriptorMap<ResourceDescriptor> resources_{}; ///< Resource descriptors by handle.
   };

} // namespace vve::v4
