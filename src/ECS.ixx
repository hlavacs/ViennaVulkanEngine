export module VEEngine:ECS;
import std;
export import :Types;

/**
 * @file
 * @brief Small facade ECS built from typed handles and STL component maps.
 */
export namespace vve {

   /// @brief Default ECS trait reserved for future slot-map policy knobs.
   struct DefaultECSTraits {
      static constexpr bool use_slot_map_handles{false}; ///< Slot maps are not implemented in this skeleton yet.
   };

   /// @brief Small component store keyed by 64-bit entity handles.
   template <typename TTraits = DefaultECSTraits> class BasicECS {
      /// @brief Type-erased base so destruction can erase components from every pool.
      struct PoolBase {
         virtual ~PoolBase() = default;
         virtual void erase(Entity entity) = 0;
      };

      /// @brief Concrete component pool for one component type.
      template <typename T> struct Pool final : PoolBase {
         std::map<Entity, T> data{}; ///< Components keyed by owning entity handle.
         void erase(Entity entity) override { data.erase(entity); }

      };

   public:
      /// @brief Creates a live entity with a fresh 64-bit handle.
      [[nodiscard]] Entity create() {
         static_assert(!TTraits::use_slot_map_handles, "facade prepares slot-map handles but has no slot map yet");
         const auto entity = makeCounterHandle<Entity>();
         alive_.insert(entity);
         return entity;
      }

      /// @brief Returns whether an entity handle is live in this ECS.
      [[nodiscard]] bool exists(Entity entity) const { return alive_.contains(entity); }

      /// @brief Erases a live entity and all of its attached components.
      [[nodiscard]] std::expected<void, Error> erase(Entity entity) {
         if (!alive_.erase(entity)) { return std::unexpected(Error::invalid_handle); }
         for (auto &[_, pool] : pools_) { pool->erase(entity); }
         return {};
      }

      /// @brief Adds a component; fails if the entity is invalid or already has the component.
      template <typename T>
      [[nodiscard]] std::expected<void, Error> add(Entity entity, T component) {
         if (!exists(entity)) { return std::unexpected(Error::invalid_handle); }
         auto &data = pool<T>().data;
         const auto [_, inserted] = data.emplace(entity, std::move(component));
         if (!inserted) { return std::unexpected(Error::duplicate_component); }
         return {};
      }

      /// @brief Reads a required component by value.
      template <typename T>
      [[nodiscard]] std::expected<T, Error> get(Entity entity) const {
         if (!exists(entity)) { return std::unexpected(Error::invalid_handle); }
         const auto *typed_pool = findPool<T>();
         if (typed_pool == nullptr) { return std::unexpected(Error::missing_component); }
         const auto it = typed_pool->data.find(entity);
         if (it == typed_pool->data.end()) { return std::unexpected(Error::missing_component); }
         return it->second;
      }

      /// @brief Reads an optional component; invalid entities still produce an error.
      template <typename T>
      [[nodiscard]] std::expected<std::optional<T>, Error> tryGet(Entity entity) const {
         if (!exists(entity)) { return std::unexpected(Error::invalid_handle); }
         const auto *typed_pool = findPool<T>();
         if (typed_pool == nullptr) { return std::optional<T>{}; }
         const auto it = typed_pool->data.find(entity);
         return it == typed_pool->data.end() ? std::optional<T>{} : std::optional<T>{it->second};
      }

      /// @brief Inserts or replaces a component on a live entity.
      template <typename T>
      [[nodiscard]] std::expected<void, Error> put(Entity entity, T component) {
         if (!exists(entity)) { return std::unexpected(Error::invalid_handle); }
         pool<T>().data.insert_or_assign(entity, std::move(component));
         return {};
      }

      /// @brief Tests whether a live entity owns a component type.
      template <typename T>
      [[nodiscard]] std::expected<bool, Error> has(Entity entity) const {
         if (!exists(entity)) { return std::unexpected(Error::invalid_handle); }
         const auto *typed_pool = findPool<T>();
         return typed_pool != nullptr && typed_pool->data.contains(entity);
      }

      /// @brief Removes a component if present; missing components are ignored.
      template <typename T>
      [[nodiscard]] std::expected<void, Error> remove(Entity entity) {
         if (!exists(entity)) { return std::unexpected(Error::invalid_handle); }
         if (auto *typed_pool = findPool<T>()) { typed_pool->data.erase(entity); }
         return {};
      }

      /// @brief Returns every live entity that owns all requested component types.
      template <typename... T>
      [[nodiscard]] Vector<Entity> view() const {
         Vector<Entity> result{};
         for (const auto entity : alive_) {
            if ((contains<T>(entity) && ...)) { result.push_back(entity); }
         }
         return result;
      }

   private:
      /// @brief Creates or returns the pool for component type T.
      template <typename T> [[nodiscard]] Pool<T> &pool() {
         const std::type_index key{typeid(T)};
         auto &slot = pools_[key];
         if (!slot) { slot = std::make_unique<Pool<T>>(); }
         return static_cast<Pool<T> &>(*slot);
      }

      /// @brief Finds the const pool for component type T, or null if unused.
      template <typename T> [[nodiscard]] const Pool<T> *findPool() const {
         const auto it = pools_.find(std::type_index(typeid(T)));
         return it == pools_.end() ? nullptr : static_cast<const Pool<T> *>(it->second.get());
      }

      /// @brief Finds the mutable pool for component type T, or null if unused.
      template <typename T> [[nodiscard]] Pool<T> *findPool() {
         const auto it = pools_.find(std::type_index(typeid(T)));
         return it == pools_.end() ? nullptr : static_cast<Pool<T> *>(it->second.get());
      }

      /// @brief Checks raw component-pool membership without validating liveness.
      template <typename T> [[nodiscard]] bool contains(Entity entity) const {
         const auto *typed_pool = findPool<T>();
         return typed_pool != nullptr && typed_pool->data.contains(entity);
      }

      std::set<Entity> alive_{};                                    ///< Live entity handles.
      std::map<std::type_index, std::unique_ptr<PoolBase>> pools_{}; ///< Component pools by type.
   };

   using ECS = BasicECS<>; ///< Default ECS type owned by the active engine implementation.

} // namespace vve
