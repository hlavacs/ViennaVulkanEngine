#include <string>

import VEEngine;

namespace {

   struct Position {
      int x{0};
      int y{0};

      [[nodiscard]] friend bool operator==(const Position &, const Position &) = default;
   };

} // namespace

int main() {
   vve::ECS<> ecs{};
   const vve::Handle invalid_entity{9999};

   const auto entity_result = ecs.create();
   if (!entity_result) {
      return 1;
   }

   const auto entity = *entity_result;

   const auto exists_result = ecs.exists(entity);
   if (!exists_result || !*exists_result) {
      return 2;
   }

   const auto invalid_exists_result = ecs.exists(invalid_entity);
   if (!invalid_exists_result || *invalid_exists_result) {
      return 3;
   }

   if (!ecs.addComponent(entity, Position{1, 2})) {
      return 4;
   }

   if (ecs.addComponent(invalid_entity, Position{7, 9})) {
      return 5;
   }

   const auto has_position_result = ecs.hasComponent<Position>(entity);
   if (!has_position_result || !*has_position_result) {
      return 6;
   }

   const auto invalid_has_position_result = ecs.hasComponent<Position>(invalid_entity);
   if (invalid_has_position_result) {
      return 7;
   }

   const auto position_result = ecs.get<Position>(entity);
   if (!position_result || !position_result->has_value() || **position_result != Position{1, 2}) {
      return 8;
   }

   const auto invalid_position_result = ecs.get<Position>(invalid_entity);
   if (invalid_position_result) {
      return 9;
   }

   if (!ecs.put(entity, Position{4, 8})) {
      return 10;
   }

   Position lvalue_position{6, 12};
   if (!ecs.put(entity, lvalue_position)) {
      return 101;
   }

   if (ecs.put(invalid_entity, Position{11, 12})) {
      return 11;
   }

   const auto updated_position_result = ecs.get<Position>(entity);
   if (!updated_position_result || !updated_position_result->has_value() ||
       **updated_position_result != Position{6, 12}) {
      return 12;
   }

   if (!ecs.eraseComponent<Position>(entity)) {
      return 13;
   }

   if (ecs.eraseComponent<Position>(invalid_entity)) {
      return 14;
   }

   const auto has_removed_position_result = ecs.hasComponent<Position>(entity);
   if (!has_removed_position_result || *has_removed_position_result) {
      return 15;
   }

   const auto removed_position_result = ecs.get<Position>(entity);
   if (!removed_position_result || removed_position_result->has_value()) {
      return 16;
   }

   if (!ecs.erase(entity)) {
      return 17;
   }

   if (ecs.erase(entity)) {
      return 18;
   }

   const auto exists_after_erase = ecs.exists(entity);
   if (!exists_after_erase || *exists_after_erase) {
      return 19;
   }

   const auto has_position_after_entity_erase = ecs.hasComponent<Position>(entity);
   if (has_position_after_entity_erase) {
      return 20;
   }

   return 0;
}
