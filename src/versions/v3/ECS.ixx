namespace vve::v3 {

/**
 * @file
 * @brief Concrete v3 ECS implementation behind the public ECS facade.
 */

   /**
    * @brief Default entity-allocation traits for the v3 ECS.
    */
   struct DefaultECSTraits {
      using entity_value_type = vve::Handle::value_type;				///< Underlying integer type used for entity values.
      static constexpr entity_value_type first_entity_value = 1;	///< First allocated entity value.
   };

   /**
    * @brief Simple v3 ECS storage implementation.
    *
    * The implementation stores entity existence separately from component pools
    * and uses one static pool per component type.
    */
   template <typename TTraits = DefaultECSTraits> class BasicECSImplementation {
   public:
      using traits_type = TTraits;	///< Traits type controlling allocation behavior.
      /// @brief Underlying integer type used for entity values.
      using entity_value_type = typename traits_type::entity_value_type;

      /**
       * @brief Creates a new entity.
       * @return New entity handle on success, or an internal error when allocation overflows.
       */
      [[nodiscard]] std::expected<vve::Handle, vve::Error> create() {
         if (next_entity_value_ == 0) { // Guard against handle wraparound so entity identity remains valid.
            return std::unexpected(vve::Error::internal_error);
         }

         // Allocate the next monotonically increasing entity id.
         const vve::Handle entity{static_cast<vve::Handle::value_type>(next_entity_value_++)};
         // Track liveness separately from component storage so destruction can
         // invalidate the whole entity before touching per-component pools.
         entities_.insert(entity.value());
         return entity;
      }

      /**
       * @brief Returns whether an entity exists.
       * @param entity Entity handle to query.
       * @return `true` when the entity exists, `false` when it does not.
       */
      [[nodiscard]] std::expected<bool, vve::Error> exists(vve::Handle entity) const {
         return entities_.contains(entity.value());
      }

      /**
       * @brief Erases an entity and all of its registered components.
       * @param entity Entity handle to destroy.
       * @return Empty success result, or an error when the entity is invalid.
       */
      [[nodiscard]] std::expected<void, vve::Error> erase(vve::Handle entity) {
         if (!entities_.erase(entity.value())) { // Reject destruction of unknown entities before mutating any pools.
            return std::unexpected(vve::Error::invalid_argument);
         }

         // Every registered component type gets a chance to discard its entry
         // for this entity. This keeps destruction generic across component
         // types without storing erased-component logic on each entity.
         for (auto &[_, erase_component] : componentErasers()) {
            erase_component(entity.value());
         }

         return {};
      }

      /**
       * @brief Adds a component to an entity.
       * @tparam TComponent Component type to attach.
       * @param entity Entity handle receiving the component.
       * @param component Component value to store.
       * @return Empty success result, or an error when the entity is invalid or the component already exists.
       */
      template <typename TComponent>
         requires(!std::same_as<std::remove_cvref_t<TComponent>, vve::Handle>)
      [[nodiscard]] std::expected<void, vve::Error> addComponent(vve::Handle entity, TComponent &&component) {
         // Component operations remain handle-based, so validate existence up front.
         if (!entities_.contains(entity.value())) {
            return std::unexpected(vve::Error::invalid_argument);
         }

         using component_type = std::remove_cvref_t<TComponent>;
         auto &pool = componentPool<component_type>();
         const auto [_, inserted] = pool.emplace(entity.value(), std::forward<TComponent>(component));
         if (!inserted) { // `addComponent` preserves "must not already exist" semantics.
            return std::unexpected(vve::Error::invalid_argument);
         }

         return {};
      }

      /**
       * @brief Returns a component copy when present.
       * @tparam TComponent Component type to read.
       * @param entity Entity handle to inspect.
       * @return Optional component copy, or an error when the entity is invalid.
       */
      template <typename TComponent>
         requires(!std::same_as<std::remove_cvref_t<TComponent>, vve::Handle>)
      [[nodiscard]] std::expected<std::optional<std::remove_cvref_t<TComponent>>, vve::Error> get(vve::Handle entity) const {
         using component_type = std::remove_cvref_t<TComponent>;

         if (!entities_.contains(entity.value())) { // Reads of non-existent entities are treated as invalid API usage.
            return std::unexpected(vve::Error::invalid_argument);
         }

         const auto &pool = componentPool<component_type>();
         const auto component_it = pool.find(entity.value());
         if (component_it == pool.end()) { // Missing component data is not an error for a valid entity.
            return std::optional<component_type>{};
         }

         // The public API returns a copy rather than a raw reference so the
         // facade does not expose storage stability guarantees.
         return std::optional<component_type>{component_it->second};
      }

      /**
       * @brief Replaces or inserts a component on an entity.
       * @tparam TComponent Component type to write.
       * @param entity Entity handle to mutate.
       * @param component New component value.
       * @return Empty success result, or an error when the entity is invalid.
       */
      template <typename TComponent>
         requires(!std::same_as<std::remove_cvref_t<TComponent>, vve::Handle>)
      [[nodiscard]] std::expected<void, vve::Error> put(vve::Handle entity, TComponent &&component) {
         if (!entities_.contains(entity.value())) { // Upserts still require a live entity.
            return std::unexpected(vve::Error::invalid_argument);
         }

         using component_type = std::remove_cvref_t<TComponent>;
         // `put` intentionally overwrites existing state or inserts when absent.
         componentPool<component_type>().insert_or_assign(entity.value(), std::forward<TComponent>(component));
         return {};
      }

      /**
       * @brief Returns whether an entity owns a component.
       * @tparam TComponent Component type to test.
       * @param entity Entity handle to inspect.
       * @return `true` when the component exists, `false` when it does not, or an error when the entity is invalid.
       */
      template <typename TComponent>
         requires(!std::same_as<std::remove_cvref_t<TComponent>, vve::Handle>)
      [[nodiscard]] std::expected<bool, vve::Error> hasComponent(vve::Handle entity) const {
         using component_type = std::remove_cvref_t<TComponent>;

         // Component-membership queries stay consistent with other entity APIs.
         if (!entities_.contains(entity.value())) {
            return std::unexpected(vve::Error::invalid_argument);
         }

         return componentPool<component_type>().contains(entity.value());
      }

      /**
       * @brief Removes a component from an entity.
       * @tparam TComponent Component type to remove.
       * @param entity Entity handle to mutate.
       * @return Empty success result, or an error when the entity is invalid.
       */
      template <typename TComponent>
         requires(!std::same_as<std::remove_cvref_t<TComponent>, vve::Handle>)
      [[nodiscard]] std::expected<void, vve::Error> eraseComponent(vve::Handle entity) {
         using component_type = std::remove_cvref_t<TComponent>;

         if (!entities_.contains(entity.value())) { // The entity must remain alive even if the component is absent.
            return std::unexpected(vve::Error::invalid_argument);
         }

         // Erasing a missing component is tolerated and becomes a no-op.
         componentPool<component_type>().erase(entity.value());
         return {};
      }

      /**
       * @brief Returns all entities that own every requested component type.
       * @tparam TComponents Component types that must all be present.
       * @return Matching entity handles.
       */
      template <typename... TComponents>
         requires(sizeof...(TComponents) > 0 && (... && !std::same_as<std::remove_cvref_t<TComponents>, vve::Handle>))
      [[nodiscard]] std::expected<std::vector<vve::Handle>, vve::Error> view() const {
         // Build a small summary for every requested pool so view construction
         // does not need to special-case the component pack later.
         const std::array pools{
             poolInfo<std::remove_cvref_t<TComponents>>()...,
         };

         // Iterate the smallest pool first to minimize containment checks across
         // the remaining component pools.
         const auto smallest_pool_it = std::ranges::min_element(pools, {}, &component_pool_info::size);
         std::vector<vve::Handle::value_type> candidate_entities{};
         smallest_pool_it->collectEntities(candidate_entities);

         std::vector<vve::Handle> result{};
         result.reserve(candidate_entities.size());
         for (const auto entity_value : candidate_entities) {
            // A candidate survives only if every requested component pool
            // contains the same entity id.
            if ((componentPool<std::remove_cvref_t<TComponents>>().contains(entity_value) && ...)) {
               result.emplace_back(entity_value);
            }
         }

         return result;
      }

   private:
      /// @brief Summary of one component pool used when building views.
      struct component_pool_info {
         std::size_t size;	///< Number of entities in the component pool.
         /// @brief Function that collects all entity ids in the pool.
         void (*collectEntities)(std::vector<vve::Handle::value_type> &);
         /// @brief Function that checks whether an entity id exists in the pool.
         bool (*contains)(vve::Handle::value_type);
      };

      /// @brief Function type used to erase one component instance by entity id.
      using component_eraser_type = void (*)(vve::Handle::value_type);
      /// @brief Storage map used for one concrete component type.
      template <typename TComponent> using component_pool_type = std::unordered_map<vve::Handle::value_type, TComponent>;

      /**
       * @brief Returns summary callbacks for one component pool.
       * @tparam TComponent Component type represented by the pool.
       * @return Summary information used to build efficient views.
       */
      template <typename TComponent> static component_pool_info poolInfo() {
         return component_pool_info{
             .size = componentPool<TComponent>().size(),
             .collectEntities =
                 +[](std::vector<vve::Handle::value_type> &entities) {
                    // Rebuild the candidate list from scratch so callers can
                    // reuse the same vector across different pool summaries.
                    entities.clear(); // Rebuild the candidate list from scratch for this pool summary.
                    entities.reserve(componentPool<TComponent>().size());
                    for (const auto &[entity_value, _] : componentPool<TComponent>()) {
                       entities.push_back(entity_value);
                    }
                 },
             // Membership tests are exposed as a tiny callback so the view logic
             // can stay generic across the component parameter pack.
             .contains = +[](vve::Handle::value_type entity_value) { return componentPool<TComponent>().contains(entity_value); }};
      }

      /**
       * @brief Returns the static pool for one component type.
       * @tparam TComponent Component type represented by the pool.
       * @return Reference to the static component pool for `TComponent`.
       */
      template <typename TComponent> static component_pool_type<TComponent> &componentPool() {
         static const bool registered = []() -> bool {
            // Register one erase callback per component type the first time that
            // type's pool is instantiated.
            componentErasers().insert_or_assign(std::type_index(typeid(TComponent)),
                                                +[](vve::Handle::value_type entity_value) { componentPool<TComponent>().erase(entity_value); });
            return true;
         }();
         static_cast<void>(registered); // Force one-time eraser registration for this component type.

         // The pool itself is static per component type, which keeps storage
         // centralized for the ECS implementation instance used by the facade.
         static std::unordered_map<vve::Handle::value_type, TComponent> pool{};
         return pool;
      }

      /**
       * @brief Returns the registry of component eraser callbacks.
       * @return Map keyed by component type used during entity destruction.
       */
      static std::unordered_map<std::type_index, component_eraser_type> &componentErasers() {
         static std::unordered_map<std::type_index, component_eraser_type> erasers{};
         return erasers;
      }

      std::unordered_set<vve::Handle::value_type> entities_{};						///< Set of currently alive entity ids.
      entity_value_type next_entity_value_{traits_type::first_entity_value};	///< Next entity value to allocate.
   };

} // namespace vve::v3
