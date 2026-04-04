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

         for (auto &[_, erase_component] : componentErasers()) {
            erase_component(entity.value());
         }

         return {};
      }

      template <typename TComponent>
         requires(!std::same_as<std::remove_cvref_t<TComponent>, vve::Handle>)
      [[nodiscard]] std::expected<void, vve::Error> addComponent(vve::Handle entity, TComponent &&component) {
         if (!entities_.contains(entity.value())) {
            return std::unexpected(vve::Error::invalid_argument);
         }

         using component_type = std::remove_cvref_t<TComponent>;
         auto &pool = componentPool<component_type>();
         const auto [_, inserted] = pool.emplace(entity.value(), std::forward<TComponent>(component));
         if (!inserted) {
            return std::unexpected(vve::Error::invalid_argument);
         }

         return {};
      }

      template <typename TComponent>
         requires(!std::same_as<std::remove_cvref_t<TComponent>, vve::Handle>)
      [[nodiscard]] std::expected<std::optional<std::remove_cvref_t<TComponent>>, vve::Error> get(vve::Handle entity) const {
         using component_type = std::remove_cvref_t<TComponent>;

         if (!entities_.contains(entity.value())) {
            return std::unexpected(vve::Error::invalid_argument);
         }

         const auto &pool = componentPool<component_type>();
         const auto component_it = pool.find(entity.value());
         if (component_it == pool.end()) {
            return std::optional<component_type>{};
         }

         return std::optional<component_type>{component_it->second};
      }

      template <typename TComponent>
         requires(!std::same_as<std::remove_cvref_t<TComponent>, vve::Handle>)
      [[nodiscard]] std::expected<void, vve::Error> put(vve::Handle entity, TComponent &&component) {
         if (!entities_.contains(entity.value())) {
            return std::unexpected(vve::Error::invalid_argument);
         }

         using component_type = std::remove_cvref_t<TComponent>;
         componentPool<component_type>().insert_or_assign(entity.value(), std::forward<TComponent>(component));
         return {};
      }

      template <typename TComponent>
         requires(!std::same_as<std::remove_cvref_t<TComponent>, vve::Handle>)
      [[nodiscard]] std::expected<bool, vve::Error> hasComponent(vve::Handle entity) const {
         using component_type = std::remove_cvref_t<TComponent>;

         if (!entities_.contains(entity.value())) {
            return std::unexpected(vve::Error::invalid_argument);
         }

         return componentPool<component_type>().contains(entity.value());
      }

      template <typename TComponent>
         requires(!std::same_as<std::remove_cvref_t<TComponent>, vve::Handle>)
      [[nodiscard]] std::expected<void, vve::Error> eraseComponent(vve::Handle entity) {
         using component_type = std::remove_cvref_t<TComponent>;

         if (!entities_.contains(entity.value())) {
            return std::unexpected(vve::Error::invalid_argument);
         }

         componentPool<component_type>().erase(entity.value());
         return {};
      }

   private:
      using component_eraser_type = void (*)(vve::Handle::value_type);
      template <typename TComponent> using component_pool_type = std::unordered_map<vve::Handle::value_type, TComponent>;

      template <typename TComponent> static component_pool_type<TComponent> &componentPool() {
         static const bool registered = []() -> bool {
            componentErasers().insert_or_assign(std::type_index(typeid(TComponent)),
                                                +[](vve::Handle::value_type entity_value) { componentPool<TComponent>().erase(entity_value); });
            return true;
         }();
         static_cast<void>(registered);

         static std::unordered_map<vve::Handle::value_type, TComponent> pool{};
         return pool;
      }

      static std::unordered_map<std::type_index, component_eraser_type> &componentErasers() {
         static std::unordered_map<std::type_index, component_eraser_type> erasers{};
         return erasers;
      }

      std::unordered_set<vve::Handle::value_type> entities_{};
      entity_value_type next_entity_value_{traits_type::first_entity_value};
   };

} // namespace vve::v3
