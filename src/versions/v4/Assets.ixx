export module VEEngine.V4:Assets;
import std;
export import :Types;

/// @file
/// @brief Stub asset system that owns the v4 object catalog.

export namespace vve::v4 {

   /// @brief Minimal asset facade; loading code will later fill the descriptor maps.
   class AssetSystem {
   public:
      /// @brief Returns the mutable object catalog for tests and future loaders.
      [[nodiscard]] ObjectCatalog &catalog() { return catalog_; }
      /// @brief Returns the read-only object catalog.
      [[nodiscard]] const ObjectCatalog &catalog() const { return catalog_; }

      /// @brief Allocates the next handle for an object kind.
      [[nodiscard]] Handle next(ObjectKind kind) {
         return makeHandle(kind, next_indices_[kind]++);
      }

      /// @brief Adds an empty scene descriptor and returns its handle.
      [[nodiscard]] std::expected<Handle, Error> addScene(std::string name) {
         const auto handle = next(ObjectKind::scene);
         auto scene = SceneDescriptor{.handle = handle, .name = std::move(name)};
         if (auto added = catalog_.scenes.add(std::move(scene)); !added) {
            return std::unexpected(added.error());
         }
         return handle;
      }

   private:
      ObjectCatalog catalog_{};                         ///< All loaded object descriptors.
      std::map<ObjectKind, std::uint32_t> next_indices_{}; ///< Per-kind handle counters.
   };

} // namespace vve::v4
