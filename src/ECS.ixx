export module VEEngine:ECS;
import std;
import :Handle;
import :Error;

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

#ifndef VVE_DEFAULT_ECS_IMPLEMENTATION
#define VVE_DEFAULT_ECS_IMPLEMENTATION 3
#endif

export namespace vve {

   namespace detail {

#if VVE_DEFAULT_ECS_IMPLEMENTATION == 3
      template <typename TTraits = vve::v3::DefaultECSTraits>
      using DefaultECSImplementationTemplate = vve::v3::BasicECSImplementation<TTraits>;
#else
#error Unsupported VVE_DEFAULT_ECS_IMPLEMENTATION value
#endif

      using DefaultECSImplementation = DefaultECSImplementationTemplate<>;

   } // namespace detail

   template <typename T>
   concept NotHandle = !std::same_as<std::remove_cvref_t<T>, Handle>;

   template <typename TImplementation> class VVE_API ECSFacade {
   public:
      [[nodiscard]] std::expected<Handle, Error> create() { return implementation_.create(); }

      [[nodiscard]] std::expected<bool, Error> exists(Handle entity) const { return implementation_.exists(entity); }

      [[nodiscard]] std::expected<void, Error> erase(Handle entity) { return implementation_.erase(entity); }

      template <NotHandle TComponent>
      [[nodiscard]] std::expected<void, Error> addComponent(Handle entity, TComponent &&component);

      template <NotHandle TComponent>
      [[nodiscard]] std::expected<std::optional<std::remove_cvref_t<TComponent>>, Error> get(Handle entity) const;

      template <NotHandle TComponent>
      [[nodiscard]] std::expected<void, Error> put(Handle entity, TComponent &&component);

      template <NotHandle TComponent> [[nodiscard]] std::expected<bool, Error> hasComponent(Handle entity) const;

      template <NotHandle TComponent> [[nodiscard]] std::expected<void, Error> eraseComponent(Handle entity);

   private:
      TImplementation implementation_{};
   };
   template <typename TImplementation>
   template <NotHandle TComponent>
   [[nodiscard]] std::expected<void, Error> ECSFacade<TImplementation>::addComponent(Handle entity,
                                                                                     TComponent &&component) {
      using TStoredComponent = std::remove_cvref_t<TComponent>;
      return implementation_.template addComponent<TStoredComponent>(entity, std::forward<TComponent>(component));
   }

   template <typename TImplementation>
   template <NotHandle TComponent>
   [[nodiscard]] std::expected<std::optional<std::remove_cvref_t<TComponent>>, Error>
   ECSFacade<TImplementation>::get(Handle entity) const {
      using TStoredComponent = std::remove_cvref_t<TComponent>;
      return implementation_.template get<TStoredComponent>(entity);
   }

   template <typename TImplementation>
   template <NotHandle TComponent>
   [[nodiscard]] std::expected<void, Error> ECSFacade<TImplementation>::put(Handle entity, TComponent &&component) {
      using TStoredComponent = std::remove_cvref_t<TComponent>;
      return implementation_.template put<TStoredComponent>(entity, std::forward<TComponent>(component));
   }

   template <typename TImplementation>
   template <NotHandle TComponent>
   [[nodiscard]] std::expected<bool, Error> ECSFacade<TImplementation>::hasComponent(Handle entity) const {
      using TStoredComponent = std::remove_cvref_t<TComponent>;
      return implementation_.template hasComponent<TStoredComponent>(entity);
   }

   template <typename TImplementation>
   template <NotHandle TComponent>
   [[nodiscard]] std::expected<void, Error> ECSFacade<TImplementation>::eraseComponent(Handle entity) {
      using TStoredComponent = std::remove_cvref_t<TComponent>;
      return implementation_.template eraseComponent<TStoredComponent>(entity);
   }

   template <typename TImplementation = detail::DefaultECSImplementation> using ECS = ECSFacade<TImplementation>;

} // namespace vve
