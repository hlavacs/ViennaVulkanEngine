#include <algorithm>
#include <ranges>
#include <string>
#include <vector>

import VEEngine;

namespace {

   struct Position {
      int x{0};
      int y{0};

      [[nodiscard]] friend bool operator==(const Position &, const Position &) = default;
   };

   struct Velocity {
      int dx{0};
      int dy{0};

      [[nodiscard]] friend bool operator==(const Velocity &, const Velocity &) = default;
   };

   struct Health {
      int value{0};

      [[nodiscard]] friend bool operator==(const Health &, const Health &) = default;
   };

   struct Tag {
      std::string value{};

      [[nodiscard]] friend bool operator==(const Tag &, const Tag &) = default;
   };

   [[nodiscard]] bool containsEntity(const std::vector<vve::Handle> &entities, vve::Handle entity) {
      return std::ranges::find(entities, entity) != entities.end();
   }

} // namespace

int main() {
   vve::ECS<> ecs{};
   const vve::Handle invalid_entity{9999};

   const auto entity_a_result = ecs.create();
   if (!entity_a_result) {
      return 1;
   }

   const auto entity_b_result = ecs.create();
   if (!entity_b_result) {
      return 2;
   }

   const auto entity_c_result = ecs.create();
   if (!entity_c_result) {
      return 3;
   }

   const auto entity_a = *entity_a_result;
   const auto entity_b = *entity_b_result;
   const auto entity_c = *entity_c_result;

   if (entity_a == entity_b || entity_a == entity_c || entity_b == entity_c) {
      return 4;
   }

   const auto exists_result = ecs.exists(entity_a);
   if (!exists_result || !*exists_result) {
      return 5;
   }

   const auto exists_b_result = ecs.exists(entity_b);
   if (!exists_b_result || !*exists_b_result) {
      return 6;
   }

   const auto invalid_exists_result = ecs.exists(invalid_entity);
   if (!invalid_exists_result || *invalid_exists_result) {
      return 7;
   }

   const auto missing_velocity_result = ecs.get<Velocity>(entity_a);
   if (!missing_velocity_result || missing_velocity_result->has_value()) {
      return 8;
   }

   const auto missing_velocity_has_result = ecs.hasComponent<Velocity>(entity_a);
   if (!missing_velocity_has_result || *missing_velocity_has_result) {
      return 9;
   }

   if (!ecs.eraseComponent<Velocity>(entity_a)) {
      return 10;
   }

   if (!ecs.addComponent(entity_a, Position{1, 2})) {
      return 11;
   }

   Position lvalue_position{9, 18};
   if (!ecs.addComponent(entity_b, lvalue_position)) {
      return 12;
   }

   if (lvalue_position != Position{9, 18}) {
      return 13;
   }

   if (!ecs.addComponent(entity_a, Velocity{3, 4})) {
      return 14;
   }

   if (!ecs.addComponent(entity_b, Velocity{5, 6})) {
      return 15;
   }

   if (!ecs.addComponent(entity_b, Tag{"enemy"})) {
      return 16;
   }

   if (!ecs.addComponent(entity_c, Health{100})) {
      return 17;
   }

   if (!ecs.put(entity_c, Velocity{7, 8})) {
      return 18;
   }

   if (ecs.addComponent(entity_a, Position{7, 9})) {
      return 19;
   }

   if (ecs.addComponent(invalid_entity, Position{7, 9})) {
      return 20;
   }

   const auto has_position_result = ecs.hasComponent<Position>(entity_a);
   if (!has_position_result || !*has_position_result) {
      return 21;
   }

   const auto invalid_has_position_result = ecs.hasComponent<Position>(invalid_entity);
   if (invalid_has_position_result) {
      return 22;
   }

   const auto position_result = ecs.get<Position>(entity_a);
   if (!position_result || !position_result->has_value() || **position_result != Position{1, 2}) {
      return 23;
   }

   const auto lvalue_position_result = ecs.get<Position>(entity_b);
   if (!lvalue_position_result || !lvalue_position_result->has_value() || **lvalue_position_result != Position{9, 18}) {
      return 24;
   }

   const auto velocity_result = ecs.get<Velocity>(entity_c);
   if (!velocity_result || !velocity_result->has_value() || **velocity_result != Velocity{7, 8}) {
      return 25;
   }

   const auto health_result = ecs.get<Health>(entity_c);
   if (!health_result || !health_result->has_value() || **health_result != Health{100}) {
      return 26;
   }

   const auto invalid_position_result = ecs.get<Position>(invalid_entity);
   if (invalid_position_result) {
      return 27;
   }

   if (!ecs.put(entity_a, Position{4, 8})) {
      return 28;
   }

   Position replacement_position{6, 12};
   if (!ecs.put(entity_a, replacement_position)) {
      return 29;
   }

   if (ecs.put(invalid_entity, Position{11, 12})) {
      return 30;
   }

   const auto updated_position_result = ecs.get<Position>(entity_a);
   if (!updated_position_result || !updated_position_result->has_value() ||
       **updated_position_result != Position{6, 12}) {
      return 31;
   }

   if (!ecs.put(entity_c, Tag{"boss"})) {
      return 32;
   }

   const auto tag_result = ecs.get<Tag>(entity_c);
   if (!tag_result || !tag_result->has_value() || **tag_result != Tag{"boss"}) {
      return 33;
   }

   const auto position_velocity_view_result = ecs.view<Position, Velocity>();
   if (!position_velocity_view_result) {
      return 34;
   }

   if (position_velocity_view_result->size() != 2 || !containsEntity(*position_velocity_view_result, entity_a) ||
       !containsEntity(*position_velocity_view_result, entity_b) || containsEntity(*position_velocity_view_result, entity_c)) {
      return 35;
   }

   const auto velocity_health_view_result = ecs.view<Velocity, Health>();
   if (!velocity_health_view_result || velocity_health_view_result->size() != 1 ||
       !containsEntity(*velocity_health_view_result, entity_c)) {
      return 36;
   }

   const auto tag_velocity_view_result = ecs.view<Tag, Velocity>();
   if (!tag_velocity_view_result || tag_velocity_view_result->size() != 2 ||
       !containsEntity(*tag_velocity_view_result, entity_b) || !containsEntity(*tag_velocity_view_result, entity_c)) {
      return 37;
   }

   const auto position_tag_health_view_result = ecs.view<Position, Tag, Health>();
   if (!position_tag_health_view_result || !position_tag_health_view_result->empty()) {
      return 38;
   }

   if (!ecs.eraseComponent<Position>(entity_a)) {
      return 39;
   }

   if (ecs.eraseComponent<Position>(invalid_entity)) {
      return 40;
   }

   const auto has_removed_position_result = ecs.hasComponent<Position>(entity_a);
   if (!has_removed_position_result || *has_removed_position_result) {
      return 41;
   }

   const auto removed_position_result = ecs.get<Position>(entity_a);
   if (!removed_position_result || removed_position_result->has_value()) {
      return 42;
   }

   const auto position_velocity_after_removal = ecs.view<Position, Velocity>();
   if (!position_velocity_after_removal || position_velocity_after_removal->size() != 1 ||
       !containsEntity(*position_velocity_after_removal, entity_b)) {
      return 43;
   }

   if (!ecs.erase(entity_c)) {
      return 44;
   }

   const auto exists_after_erase_c = ecs.exists(entity_c);
   if (!exists_after_erase_c || *exists_after_erase_c) {
      return 45;
   }

   const auto health_after_entity_erase = ecs.get<Health>(entity_c);
   if (health_after_entity_erase) {
      return 46;
   }

   const auto tag_velocity_after_entity_erase = ecs.view<Tag, Velocity>();
   if (!tag_velocity_after_entity_erase || tag_velocity_after_entity_erase->size() != 1 ||
       !containsEntity(*tag_velocity_after_entity_erase, entity_b)) {
      return 47;
   }

   if (!ecs.erase(entity_a)) {
      return 48;
   }

   if (ecs.erase(entity_a)) {
      return 49;
   }

   const auto exists_after_erase = ecs.exists(entity_a);
   if (!exists_after_erase || *exists_after_erase) {
      return 50;
   }

   const auto has_position_after_entity_erase = ecs.hasComponent<Position>(entity_a);
   if (has_position_after_entity_erase) {
      return 51;
   }

   const auto velocity_after_entity_erase = ecs.get<Velocity>(entity_a);
   if (velocity_after_entity_erase) {
      return 52;
   }

   const auto final_position_velocity_view = ecs.view<Position, Velocity>();
   if (!final_position_velocity_view || final_position_velocity_view->size() != 1 ||
       !containsEntity(*final_position_velocity_view, entity_b)) {
      return 53;
   }

   return 0;
}
