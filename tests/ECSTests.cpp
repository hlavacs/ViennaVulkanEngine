#include <string>

import VEEngine;

namespace {

struct Position {
    int x{0};
    int y{0};

    [[nodiscard]] friend bool operator==(const Position&, const Position&) = default;
};

} // namespace

int main() {
    vve::ECS<> ecs{};

    const auto entity_result = ecs.create();
    if (!entity_result) {
        return 1;
    }

    const auto entity = *entity_result;

    const auto exists_result = ecs.exists(entity);
    if (!exists_result || !*exists_result) {
        return 2;
    }

    if (!ecs.addComponent(entity, Position{1, 2})) {
        return 3;
    }

    const auto position_result = ecs.get<Position>(entity);
    if (!position_result || !position_result->has_value() || **position_result != Position{1, 2}) {
        return 4;
    }

    if (!ecs.put(entity, Position{4, 8})) {
        return 5;
    }

    const auto updated_position_result = ecs.get<Position>(entity);
    if (!updated_position_result || !updated_position_result->has_value() ||
        **updated_position_result != Position{4, 8}) {
        return 6;
    }

    if (!ecs.eraseComponent<Position>(entity)) {
        return 7;
    }

    const auto removed_position_result = ecs.get<Position>(entity);
    if (!removed_position_result || removed_position_result->has_value()) {
        return 8;
    }

    if (!ecs.erase(entity)) {
        return 9;
    }

    const auto exists_after_erase = ecs.exists(entity);
    if (!exists_after_erase || *exists_after_erase) {
        return 10;
    }

    return 0;
}
