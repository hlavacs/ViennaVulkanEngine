export module VEEngine:ECS;
import std;
import :Handle;
import :Error;

#ifndef VVE_DEFAULT_ENGINE_NAMESPACE
#define VVE_DEFAULT_ENGINE_NAMESPACE v3
#endif

#if defined(_WIN32)
#if defined(VVE_ENGINE_BUILD)
#define VVE_API __declspec(dllexport)
#else
#define VVE_API __declspec(dllimport)
#endif
#else
#define VVE_API
#endif

#include "versions/v3/ECS.ixx"

/**
 * @file
 * @brief Public ECS facade for entity and component storage access.
 *
 * This module exposes the reusable engine-facing ECS contract while delegating
 * storage policy to the selected versioned implementation.
 */
export namespace vve {

   namespace detail {

      // Route the public alias through the currently selected engine version so
      // the outward-facing ECS contract stays stable across implementations.
      template <typename TTraits = vve::VVE_DEFAULT_ENGINE_NAMESPACE::DefaultECSTraits>
      using DefaultECSImplementationTemplate = vve::VVE_DEFAULT_ENGINE_NAMESPACE::BasicECSImplementation<TTraits>;

      // Most callers bind directly to the default implementation chosen above.
      using DefaultECSImplementation = DefaultECSImplementationTemplate<>;

   } // namespace detail

   /// @brief Constrains component APIs to reject `Handle` itself as a component type.
   template <typename T>
   concept NotHandle = !std::same_as<std::remove_cvref_t<T>, Handle>;

   /**
    * @brief Engine-facing facade over the configured ECS implementation.
    *
    * The facade intentionally exposes a narrow API: entity creation and
    * destruction, component insertion and mutation, and handle-based queries.
    * It does not expose storage internals or reference stability guarantees.
    */
   template <typename TImplementation> class VVE_API ECSFacade {
   public:
      /**
       * @brief Creates a new entity handle.
       * @return New entity handle on success, or an error when creation fails.
       */
      // Allocation remains delegated so the facade does not leak storage policy.
      [[nodiscard]] std::expected<Handle, Error> create() { return implementation_.create(); }

      /**
       * @brief Returns whether an entity currently exists.
       * @param entity Entity handle to query.
       * @return `true` when the entity exists, `false` when it does not, or an error when validation fails.
       */
      // The query stays intentionally narrow: callers learn only liveness.
      [[nodiscard]] std::expected<bool, Error> exists(Handle entity) const { return implementation_.exists(entity); }

      /**
       * @brief Destroys an entity and its attached components.
       * @param entity Entity handle to destroy.
       * @return Empty success result, or an error when the entity is invalid.
       */
      // Structural teardown remains explicit instead of being hidden behind mutation helpers.
      [[nodiscard]] std::expected<void, Error> erase(Handle entity) { return implementation_.erase(entity); }

      template <NotHandle TComponent>
      [[nodiscard]] std::expected<void, Error> addComponent(Handle entity, TComponent &&component);

      template <NotHandle TComponent>
      [[nodiscard]] std::expected<std::remove_cvref_t<TComponent>, Error> get(Handle entity) const;

      template <NotHandle TComponent>
      [[nodiscard]] std::expected<std::optional<std::remove_cvref_t<TComponent>>, Error> tryGet(Handle entity) const;

      template <NotHandle TComponent>
      [[nodiscard]] std::expected<void, Error> put(Handle entity, TComponent &&component);

      template <NotHandle TComponent> [[nodiscard]] std::expected<bool, Error> hasComponent(Handle entity) const;

      template <NotHandle TComponent> [[nodiscard]] std::expected<void, Error> eraseComponent(Handle entity);

      template <NotHandle... TComponents>
         requires(sizeof...(TComponents) > 0)
      [[nodiscard]] std::expected<std::vector<Handle>, Error> view() const;

   private:
      TImplementation implementation_{};  ///< Concrete ECS implementation hidden behind the public facade.
   };

   /**
    * @brief Adds a component through the selected ECS implementation.
    * @tparam TImplementation Concrete ECS implementation type.
    * @tparam TComponent Component type to attach.
    * @param entity Entity handle receiving the component.
    * @param component Component value to store.
    * @return Empty success result, or an error reported by the implementation.
   */
   template <typename TImplementation>
   template <NotHandle TComponent>
   [[nodiscard]] std::expected<void, Error> ECSFacade<TImplementation>::addComponent(Handle entity,
                                                                                     TComponent &&component) {
      // Preserve move/copy semantics supplied by the caller while forwarding to
      return implementation_.addComponent(entity, std::forward<TComponent>(component)); // the concrete storage backend.
   }

   /**
   * @brief Reads a component copy through the selected ECS implementation.
   * @tparam TImplementation Concrete ECS implementation type.
   * @tparam TComponent Component type to read.
   * @param entity Entity handle to inspect.
   * @return Component copy, or an error reported by the implementation.
   */
   template <typename TImplementation>
   template <NotHandle TComponent>
   [[nodiscard]] std::expected<std::remove_cvref_t<TComponent>, Error> ECSFacade<TImplementation>::get(Handle entity) const {
      using TStoredComponent = std::remove_cvref_t<TComponent>;
      // Normalize the component type once so the backend never sees cv/ref noise.
      return implementation_.template get<TStoredComponent>(entity);
   }

   /**
    * @brief Reads an optional component copy through the selected ECS implementation.
    * @tparam TImplementation Concrete ECS implementation type.
    * @tparam TComponent Component type to read.
    * @param entity Entity handle to inspect.
    * @return Optional component copy, or an error reported by the implementation.
    */
   template <typename TImplementation>
   template <NotHandle TComponent>
   [[nodiscard]] std::expected<std::optional<std::remove_cvref_t<TComponent>>, Error>
   ECSFacade<TImplementation>::tryGet(Handle entity) const {
      using TStoredComponent = std::remove_cvref_t<TComponent>;
      // Normalize the component type once so the backend never sees cv/ref noise.
      return implementation_.template tryGet<TStoredComponent>(entity);
   }

   /**
    * @brief Writes a component through the selected ECS implementation.
    * @tparam TImplementation Concrete ECS implementation type.
    * @tparam TComponent Component type to write.
    * @param entity Entity handle to mutate.
    * @param component New component value.
    * @return Empty success result, or an error reported by the implementation.
    */
   template <typename TImplementation>
   template <NotHandle TComponent>
   [[nodiscard]] std::expected<void, Error> ECSFacade<TImplementation>::put(Handle entity, TComponent &&component) {
      // `put` intentionally exposes overwrite-or-insert semantics as a single API.
      return implementation_.put(entity, std::forward<TComponent>(component));
   }

   /**
    * @brief Tests component ownership through the selected ECS implementation.
    * @tparam TImplementation Concrete ECS implementation type.
    * @tparam TComponent Component type to test.
    * @param entity Entity handle to inspect.
    * @return `true` when the component exists, `false` when it does not, or an implementation error.
    */
   template <typename TImplementation>
   template <NotHandle TComponent>
   [[nodiscard]] std::expected<bool, Error> ECSFacade<TImplementation>::hasComponent(Handle entity) const {
      using TStoredComponent = std::remove_cvref_t<TComponent>;
      // The facade remains strongly typed even though the implementation works
      // with the canonical stored component type.
      return implementation_.template hasComponent<TStoredComponent>(entity);
   }

   /**
    * @brief Removes a component through the selected ECS implementation.
    * @tparam TImplementation Concrete ECS implementation type.
    * @tparam TComponent Component type to remove.
    * @param entity Entity handle to mutate.
    * @return Empty success result, or an implementation error.
    */
   template <typename TImplementation>
   template <NotHandle TComponent>
   [[nodiscard]] std::expected<void, Error> ECSFacade<TImplementation>::eraseComponent(Handle entity) {
      using TStoredComponent = std::remove_cvref_t<TComponent>;
      // Component removal stays separate from entity destruction so callers can
      return implementation_.template eraseComponent<TStoredComponent>(entity); // express structural intent precisely.
   }

   /**
    * @brief Builds a view through the selected ECS implementation.
    * @tparam TImplementation Concrete ECS implementation type.
    * @tparam TComponents Component types that must all be present.
    * @return Matching entity handles, or an implementation error.
    */
   template <typename TImplementation>
   template <NotHandle... TComponents>
      requires(sizeof...(TComponents) > 0)
   [[nodiscard]] std::expected<std::vector<Handle>, Error> ECSFacade<TImplementation>::view() const {
      // Views expose matching handles only; component retrieval remains explicit.
      return implementation_.template view<std::remove_cvref_t<TComponents>...>();
   }

   /// @brief Default ECS facade alias for the active engine version.
   template <typename TImplementation = detail::DefaultECSImplementation> using ECS = ECSFacade<TImplementation>;

} // namespace vve
