export module VEEngine:ECS;
import std;
import :Handle;
import :Error;

export namespace vve {

template <typename T>
concept NotHandle = !std::same_as<std::remove_cvref_t<T>, Handle>;

template <typename TImplementation>
class ECSFacade {
public:
    [[nodiscard]] std::expected<Handle, Error> create() {
        return implementation_.create();
    }

    [[nodiscard]] std::expected<bool, Error> exists(Handle entity) const {
        return implementation_.exists(entity);
    }

    [[nodiscard]] std::expected<void, Error> erase(Handle entity) {
        return implementation_.erase(entity);
    }

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

template <typename TImplementation>
using ECS = ECSFacade<TImplementation>;

} // namespace vve
