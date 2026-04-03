#include <string>

import VEEngine;

namespace {

   struct Position {
      int x{0};
      int y{0};

      [[nodiscard]] friend bool operator==(const Position &, const Position &) = default;
   };

   struct Tag {
      std::string value{};

      [[nodiscard]] friend bool operator==(const Tag &, const Tag &) = default;
   };

} // namespace

int main() {
   vve::ECS<> ecs{};
   vve::World world{ecs};

   const auto entity_result = world.createEntity();
   if (!entity_result) {
      return 1;
   }

   const auto entity = *entity_result;
   const auto exists_result = world.exists(entity);
   if (!exists_result || !*exists_result) {
      return 2;
   }

   if (!world.addComponent(entity, Position{1, 2})) {
      return 3;
   }

   if (!world.addComponent(entity, Tag{"player"})) {
      return 4;
   }

   const auto position_result = world.getComponent<Position>(entity);
   if (!position_result || !position_result->has_value() || **position_result != Position{1, 2}) {
      return 5;
   }

   if (!world.modifyComponent<Position>(entity, [](Position &position) {
          position.x += 10;
          position.y += 20;
       })) {
      return 6;
   }

   const auto moved_position_result = world.getComponent<Position>(entity);
   if (!moved_position_result || !moved_position_result->has_value() || **moved_position_result != Position{11, 22}) {
      return 7;
   }

   const auto spawned_result = world.spawn(Position{7, 9}, Tag{"enemy"});
   if (!spawned_result) {
      return 8;
   }

   const auto spawned = *spawned_result;
   const auto has_tag_result = world.hasComponent<Tag>(spawned);
   if (!has_tag_result || !*has_tag_result) {
      return 9;
   }

   if (!world.setComponent(spawned, Position{4, 8})) {
      return 10;
   }

   const auto updated_position_result = world.getComponent<Position>(spawned);
   if (!updated_position_result || !updated_position_result->has_value() ||
       **updated_position_result != Position{4, 8}) {
      return 11;
   }

   if (!world.removeComponent<Tag>(spawned)) {
      return 12;
   }

   const auto has_removed_tag_result = world.hasComponent<Tag>(spawned);
   if (!has_removed_tag_result || *has_removed_tag_result) {
      return 13;
   }

   if (!world.destroyObject(spawned)) {
      return 14;
   }

   const auto exists_after_destroy_result = world.exists(spawned);
   if (!exists_after_destroy_result || *exists_after_destroy_result) {
      return 15;
   }

   return 0;
}
