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

#include "versions/v3/V3ECS.ixx"

export namespace vve {

template <typename T>
concept NotHandle = !std::same_as<std::remove_cvref_t<T>, Handle>;

template <typename TImplementation>
class VVE_API ECSFacade {
public:
    [[nodiscard]] std::expected<Handle, Error> create();

    [[nodiscard]] std::expected<bool, Error> exists(Handle entity) const;

    [[nodiscard]] std::expected<void, Error> erase(Handle entity);

    template <NotHandle TComponent>
    [[nodiscard]] std::expected<void, Error> addComponent(
        Handle entity,
        TComponent&& component) {
        using TStoredComponent = std::remove_cvref_t<TComponent>;
        return implementation_.template addComponent<TStoredComponent>(
            entity,
            std::forward<TComponent>(component));
    }

    template <NotHandle TComponent>
    [[nodiscard]] std::expected<std::optional<std::remove_cvref_t<TComponent>>, Error> get(
        Handle entity) const {
        using TStoredComponent = std::remove_cvref_t<TComponent>;
        return implementation_.template get<TStoredComponent>(entity);
    }

    template <NotHandle TComponent>
    [[nodiscard]] std::expected<void, Error> put(
        Handle entity,
        TComponent&& component) {
        using TStoredComponent = std::remove_cvref_t<TComponent>;
        return implementation_.template put<TStoredComponent>(
            entity,
            std::forward<TComponent>(component));
    }

    template <NotHandle TComponent>
    [[nodiscard]] std::expected<void, Error> eraseComponent(Handle entity) {
        using TStoredComponent = std::remove_cvref_t<TComponent>;
        return implementation_.template eraseComponent<TStoredComponent>(entity);
    }

private:
    TImplementation implementation_{};
};

template <typename TImplementation = vve::v3::BasicECSImplementation<>>
using ECS = ECSFacade<TImplementation>;

} // namespace vve
