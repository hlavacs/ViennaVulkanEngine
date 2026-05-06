export module VEEngine.V4:ECS;
import std;
export import :Types;

/// @file
/// @brief v4 ECS implementation.

export namespace vve::v4 {

   /// @brief Default ECS trait reserved for future slot-map policy knobs.
   struct DefaultECSTraits {
      static constexpr bool use_slot_map_handles{false}; ///< Slot maps are not implemented in this skeleton yet.
   };

   /// @brief Small component store keyed by v4 entity handles.
   template <typename TTraits = DefaultECSTraits> class BasicECS {
      struct PoolBase {
         virtual ~PoolBase() = default;
         virtual void erase(Entity entity) = 0;
      };

      template <typename T> struct Pool final : PoolBase {
         std::map<Entity, T> data{};
         void erase(Entity entity) override { data.erase(entity); }
      };

   public:
      [[nodiscard]] Entity create();

      [[nodiscard]] bool exists(Entity entity) const;

      [[nodiscard]] std::expected<void, Error> erase(Entity entity);

      template <typename T>
      [[nodiscard]] std::expected<void, Error> add(Entity entity, T component);

      template <typename T>
      [[nodiscard]] std::expected<T, Error> get(Entity entity) const;

      template <typename T>
      [[nodiscard]] std::expected<std::optional<T>, Error> tryGet(Entity entity) const;

      template <typename T>
      [[nodiscard]] std::expected<void, Error> put(Entity entity, T component);

      template <typename T>
      [[nodiscard]] std::expected<bool, Error> has(Entity entity) const;

      template <typename T>
      [[nodiscard]] std::expected<void, Error> remove(Entity entity);

      template <typename... T>
      [[nodiscard]] Vector<Entity> view() const;

   private:
      template <typename T> [[nodiscard]] Pool<T> &pool();

      template <typename T> [[nodiscard]] const Pool<T> *findPool() const;

      template <typename T> [[nodiscard]] Pool<T> *findPool();

      template <typename T> [[nodiscard]] bool contains(Entity entity) const;

      std::set<Entity> alive_{};
      std::map<std::type_index, std::unique_ptr<PoolBase>> pools_{};
   };

   /// @brief Creates a new live entity handle.
   template <typename TTraits> Entity BasicECS<TTraits>::create() {
      static_assert(!TTraits::use_slot_map_handles, "v4 prepares slot-map handles but has no slot map yet");
      const auto entity = makeCounterHandle<Entity>();
      alive_.insert(entity);
      return entity;
   }

   /// @brief Returns whether an entity is still alive.
   template <typename TTraits> bool BasicECS<TTraits>::exists(Entity entity) const { return alive_.contains(entity); }

   /// @brief Erases an entity and all of its components.
   template <typename TTraits> std::expected<void, Error> BasicECS<TTraits>::erase(Entity entity) {
      if (!alive_.erase(entity)) { return std::unexpected(Error::invalid_handle); }
      for (auto &[_, pool] : pools_) { pool->erase(entity); }
      return {};
   }

   /// @brief Adds a component to an entity that does not already have that component type.
   template <typename TTraits>
   template <typename T>
   std::expected<void, Error> BasicECS<TTraits>::add(Entity entity, T component) {
      if (!exists(entity)) { return std::unexpected(Error::invalid_handle); }
      auto &data = pool<T>().data;
      const auto [_, inserted] = data.emplace(entity, std::move(component));
      if (!inserted) { return std::unexpected(Error::duplicate_component); }
      return {};
   }

   /// @brief Returns a required component copy.
   template <typename TTraits>
   template <typename T>
   std::expected<T, Error> BasicECS<TTraits>::get(Entity entity) const {
      if (!exists(entity)) { return std::unexpected(Error::invalid_handle); }
      const auto *typed_pool = findPool<T>();
      if (typed_pool == nullptr) { return std::unexpected(Error::missing_component); }
      const auto it = typed_pool->data.find(entity);
      if (it == typed_pool->data.end()) { return std::unexpected(Error::missing_component); }
      return it->second;
   }

   /// @brief Returns an optional component copy.
   template <typename TTraits>
   template <typename T>
   std::expected<std::optional<T>, Error> BasicECS<TTraits>::tryGet(Entity entity) const {
      if (!exists(entity)) { return std::unexpected(Error::invalid_handle); }
      const auto *typed_pool = findPool<T>();
      if (typed_pool == nullptr) { return std::optional<T>{}; }
      const auto it = typed_pool->data.find(entity);
      return it == typed_pool->data.end() ? std::optional<T>{} : std::optional<T>{it->second};
   }

   /// @brief Inserts or replaces one component for an entity.
   template <typename TTraits>
   template <typename T>
   std::expected<void, Error> BasicECS<TTraits>::put(Entity entity, T component) {
      if (!exists(entity)) { return std::unexpected(Error::invalid_handle); }
      pool<T>().data.insert_or_assign(entity, std::move(component));
      return {};
   }

   /// @brief Returns whether an entity has a component of one type.
   template <typename TTraits>
   template <typename T>
   std::expected<bool, Error> BasicECS<TTraits>::has(Entity entity) const {
      if (!exists(entity)) { return std::unexpected(Error::invalid_handle); }
      const auto *typed_pool = findPool<T>();
      return typed_pool != nullptr && typed_pool->data.contains(entity);
   }

   /// @brief Removes one component type from an entity.
   template <typename TTraits>
   template <typename T>
   std::expected<void, Error> BasicECS<TTraits>::remove(Entity entity) {
      if (!exists(entity)) { return std::unexpected(Error::invalid_handle); }
      if (auto *typed_pool = findPool<T>()) { typed_pool->data.erase(entity); }
      return {};
   }

   /// @brief Returns all entities that have every requested component type.
   template <typename TTraits>
   template <typename... T>
   Vector<Entity> BasicECS<TTraits>::view() const {
      Vector<Entity> result{};
      for (const auto entity : alive_) {
         if ((contains<T>(entity) && ...)) { result.push_back(entity); }
      }
      return result;
   }

   /// @brief Returns a mutable component pool and creates it on first use.
   template <typename TTraits>
   template <typename T>
   typename BasicECS<TTraits>::template Pool<T> &BasicECS<TTraits>::pool() {
      const std::type_index key{typeid(T)};
      auto &slot = pools_[key];
      if (!slot) { slot = std::make_unique<Pool<T>>(); }
      return static_cast<Pool<T> &>(*slot);
   }

   /// @brief Returns a read-only component pool when it exists.
   template <typename TTraits>
   template <typename T>
   const typename BasicECS<TTraits>::template Pool<T> *BasicECS<TTraits>::findPool() const {
      const auto it = pools_.find(std::type_index(typeid(T)));
      return it == pools_.end() ? nullptr : static_cast<const Pool<T> *>(it->second.get());
   }

   /// @brief Returns a mutable component pool when it exists.
   template <typename TTraits>
   template <typename T>
   typename BasicECS<TTraits>::template Pool<T> *BasicECS<TTraits>::findPool() {
      const auto it = pools_.find(std::type_index(typeid(T)));
      return it == pools_.end() ? nullptr : static_cast<Pool<T> *>(it->second.get());
   }

   /// @brief Returns whether a component pool contains one entity.
   template <typename TTraits>
   template <typename T>
   bool BasicECS<TTraits>::contains(Entity entity) const {
      const auto *typed_pool = findPool<T>();
      return typed_pool != nullptr && typed_pool->data.contains(entity);
   }

   using ECS = BasicECS<>; ///< Default v4 ECS type.

} // namespace vve::v4
