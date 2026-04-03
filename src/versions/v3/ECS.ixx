namespace vve::v3 {

   struct DefaultECSTraits {
      using entity_value_type = vve::Handle::value_type;
      static constexpr entity_value_type first_entity_value = 1;
   };

   template <typename TTraits = DefaultECSTraits> class BasicECSImplementation {
   public:
      using traits_type = TTraits;
      using entity_value_type = typename traits_type::entity_value_type;

      [[nodiscard]] std::expected<vve::Handle, vve::Error> create() {
         if (next_entity_value_ == 0) {
            return std::unexpected(vve::Error::internal_error);
         }

         const vve::Handle entity{static_cast<vve::Handle::value_type>(next_entity_value_++)};
         entities_.insert(entity.value());
         return entity;
      }

      [[nodiscard]] std::expected<bool, vve::Error> exists(vve::Handle entity) const {
         return entities_.contains(entity.value());
      }

      [[nodiscard]] std::expected<void, vve::Error> erase(vve::Handle entity) {
         if (!entities_.erase(entity.value())) {
            return std::unexpected(vve::Error::invalid_argument);
         }

         for (auto &[_, pool] : component_pools_) {
            pool.erase(entity.value());
         }

         return {};
      }

      template <typename TComponent>
         requires(!std::same_as<std::remove_cvref_t<TComponent>, vve::Handle>)
      [[nodiscard]] std::expected<void, vve::Error> addComponent(vve::Handle entity, TComponent &&component) {
         if (!entities_.contains(entity.value())) {
            return std::unexpected(vve::Error::invalid_argument);
         }

         auto &pool = component_pools_[std::type_index(typeid(TComponent))];
         const auto [_, inserted] = pool.emplace(entity.value(), std::forward<TComponent>(component));
         if (!inserted) {
            return std::unexpected(vve::Error::invalid_argument);
         }

         return {};
      }

      template <typename TComponent>
         requires(!std::same_as<std::remove_cvref_t<TComponent>, vve::Handle>)
      [[nodiscard]] std::expected<std::optional<TComponent>, vve::Error> get(vve::Handle entity) const {
         if (!entities_.contains(entity.value())) {
            return std::unexpected(vve::Error::invalid_argument);
         }

         const auto pool_it = component_pools_.find(std::type_index(typeid(TComponent)));
         if (pool_it == component_pools_.end()) {
            return std::optional<TComponent>{};
         }

         const auto component_it = pool_it->second.find(entity.value());
         if (component_it == pool_it->second.end()) {
            return std::optional<TComponent>{};
         }

         if (const auto *component = std::any_cast<TComponent>(&component_it->second)) {
            return std::optional<TComponent>{*component};
         }

         return std::unexpected(vve::Error::internal_error);
      }

      template <typename TComponent>
         requires(!std::same_as<std::remove_cvref_t<TComponent>, vve::Handle>)
      [[nodiscard]] std::expected<void, vve::Error> put(vve::Handle entity, TComponent &&component) {
         if (!entities_.contains(entity.value())) {
            return std::unexpected(vve::Error::invalid_argument);
         }

         auto &pool = component_pools_[std::type_index(typeid(TComponent))];
         pool.insert_or_assign(entity.value(), std::forward<TComponent>(component));
         return {};
      }

      template <typename TComponent>
         requires(!std::same_as<std::remove_cvref_t<TComponent>, vve::Handle>)
      [[nodiscard]] std::expected<bool, vve::Error> hasComponent(vve::Handle entity) const {
         if (!entities_.contains(entity.value())) {
            return std::unexpected(vve::Error::invalid_argument);
         }

         const auto pool_it = component_pools_.find(std::type_index(typeid(TComponent)));
         if (pool_it == component_pools_.end()) {
            return false;
         }

         return pool_it->second.contains(entity.value());
      }

      template <typename TComponent>
         requires(!std::same_as<std::remove_cvref_t<TComponent>, vve::Handle>)
      [[nodiscard]] std::expected<void, vve::Error> eraseComponent(vve::Handle entity) {
         if (!entities_.contains(entity.value())) {
            return std::unexpected(vve::Error::invalid_argument);
         }

         const auto pool_it = component_pools_.find(std::type_index(typeid(TComponent)));
         if (pool_it == component_pools_.end()) {
            return {};
         }

         pool_it->second.erase(entity.value());
         return {};
      }

   private:
      using component_pool_type = std::unordered_map<vve::Handle::value_type, std::any>;

      std::unordered_set<vve::Handle::value_type> entities_{};
      std::unordered_map<std::type_index, component_pool_type> component_pools_{};
      entity_value_type next_entity_value_{traits_type::first_entity_value};
   };

} // namespace vve::v3
