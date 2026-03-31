export module VEEngine:ECS;
import std;
import :Handle;

export namespace vve {

class IECS {
public:
    virtual ~IECS() = default;

    [[nodiscard]] virtual Handle create() = 0;
    [[nodiscard]] virtual bool addComponent(
        Handle entity,
        std::type_index component_type,
        std::any component) = 0;
    [[nodiscard]] virtual std::optional<std::any> get(
        Handle entity,
        std::type_index component_type) const = 0;
    [[nodiscard]] virtual bool put(
        Handle entity,
        std::type_index component_type,
        std::any component) = 0;
    [[nodiscard]] virtual bool erase(Handle entity) = 0;
    [[nodiscard]] virtual bool eraseComponent(
        Handle entity,
        std::type_index component_type) = 0;

    template <typename TComponent>
    [[nodiscard]] bool addComponent(Handle entity, TComponent component) {
        return addComponent(
            entity,
            std::type_index(typeid(TComponent)),
            std::any(std::move(component)));
    }

    template <typename TComponent>
    [[nodiscard]] std::optional<TComponent> get(Handle entity) const {
        auto component = get(entity, std::type_index(typeid(TComponent)));
        if (!component.has_value()) {
            return std::nullopt;
        }

        if (const auto* value = std::any_cast<TComponent>(&*component)) {
            return *value;
        }

        return std::nullopt;
    }

    template <typename TComponent>
    [[nodiscard]] bool put(Handle entity, TComponent component) {
        return put(
            entity,
            std::type_index(typeid(TComponent)),
            std::any(std::move(component)));
    }

    template <typename TComponent>
    [[nodiscard]] bool eraseComponent(Handle entity) {
        return eraseComponent(entity, std::type_index(typeid(TComponent)));
    }
};

} // namespace vve
