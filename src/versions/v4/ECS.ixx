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
      [[nodiscard]] Entity create() {
         static_assert(!TTraits::use_slot_map_handles, "v4 prepares slot-map handles but has no slot map yet");
         const auto entity = makeCounterHandle<Entity>();
         alive_.insert(entity);
         return entity;
      }

      [[nodiscard]] bool exists(Entity entity) const { return alive_.contains(entity); }

      [[nodiscard]] std::expected<void, Error> erase(Entity entity) {
         if (!alive_.erase(entity)) { return std::unexpected(Error::invalid_handle); }
         for (auto &[_, pool] : pools_) { pool->erase(entity); }
         return {};
      }

      template <typename T>
      [[nodiscard]] std::expected<void, Error> add(Entity entity, T component) {
         if (!exists(entity)) { return std::unexpected(Error::invalid_handle); }
         auto &data = pool<T>().data;
         const auto [_, inserted] = data.emplace(entity, std::move(component));
         if (!inserted) { return std::unexpected(Error::duplicate_component); }
         return {};
      }

      template <typename T>
      [[nodiscard]] std::expected<T, Error> get(Entity entity) const {
         if (!exists(entity)) { return std::unexpected(Error::invalid_handle); }
         const auto *typed_pool = findPool<T>();
         if (typed_pool == nullptr) { return std::unexpected(Error::missing_component); }
         const auto it = typed_pool->data.find(entity);
         if (it == typed_pool->data.end()) { return std::unexpected(Error::missing_component); }
         return it->second;
      }

      template <typename T>
      [[nodiscard]] std::expected<std::optional<T>, Error> tryGet(Entity entity) const {
         if (!exists(entity)) { return std::unexpected(Error::invalid_handle); }
         const auto *typed_pool = findPool<T>();
         if (typed_pool == nullptr) { return std::optional<T>{}; }
         const auto it = typed_pool->data.find(entity);
         return it == typed_pool->data.end() ? std::optional<T>{} : std::optional<T>{it->second};
      }

      template <typename T>
      [[nodiscard]] std::expected<void, Error> put(Entity entity, T component) {
         if (!exists(entity)) { return std::unexpected(Error::invalid_handle); }
         pool<T>().data.insert_or_assign(entity, std::move(component));
         return {};
      }

      template <typename T>
      [[nodiscard]] std::expected<bool, Error> has(Entity entity) const {
         if (!exists(entity)) { return std::unexpected(Error::invalid_handle); }
         const auto *typed_pool = findPool<T>();
         return typed_pool != nullptr && typed_pool->data.contains(entity);
      }

      template <typename T>
      [[nodiscard]] std::expected<void, Error> remove(Entity entity) {
         if (!exists(entity)) { return std::unexpected(Error::invalid_handle); }
         if (auto *typed_pool = findPool<T>()) { typed_pool->data.erase(entity); }
         return {};
      }

      template <typename... T>
      [[nodiscard]] Vector<Entity> view() const {
         Vector<Entity> result{};
         for (const auto entity : alive_) {
            if ((contains<T>(entity) && ...)) { result.push_back(entity); }
         }
         return result;
      }

   private:
      template <typename T> [[nodiscard]] Pool<T> &pool() {
         const std::type_index key{typeid(T)};
         auto &slot = pools_[key];
         if (!slot) { slot = std::make_unique<Pool<T>>(); }
         return static_cast<Pool<T> &>(*slot);
      }

      template <typename T> [[nodiscard]] const Pool<T> *findPool() const {
         const auto it = pools_.find(std::type_index(typeid(T)));
         return it == pools_.end() ? nullptr : static_cast<const Pool<T> *>(it->second.get());
      }

      template <typename T> [[nodiscard]] Pool<T> *findPool() {
         const auto it = pools_.find(std::type_index(typeid(T)));
         return it == pools_.end() ? nullptr : static_cast<Pool<T> *>(it->second.get());
      }

      template <typename T> [[nodiscard]] bool contains(Entity entity) const {
         const auto *typed_pool = findPool<T>();
         return typed_pool != nullptr && typed_pool->data.contains(entity);
      }

      std::set<Entity> alive_{};
      std::map<std::type_index, std::unique_ptr<PoolBase>> pools_{};
   };

   using ECS = BasicECS<>; ///< Default v4 ECS type.

} // namespace vve::v4
