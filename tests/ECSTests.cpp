#include <string>

import VEEngine;

namespace {

struct Position {
   int x{0};
   int y{0};
};

struct Velocity {
   int dx{0};
   int dy{0};
};

struct Name {
   std::string value{};
};

} // namespace

int main() {
   auto engine = vve::makeEngine(vve::ApplicationName{"ecs-tests"});
   auto &ecs = engine.world().get<vve::ECS>();

   const auto entity = ecs.create();
   if (!entity.valid() || !ecs.exists(entity)) { return 1; }
   if (!ecs.add(entity, Position{.x = 1, .y = 2})) { return 2; }
   if (ecs.add(entity, Position{.x = 3, .y = 4})) { return 3; }
   if (!ecs.put(entity, Velocity{.dx = 5, .dy = 6})) { return 4; }
   if (!ecs.put(entity, Name{.value = "player"})) { return 5; }

   const auto position = ecs.get<Position>(entity);
   const auto velocity = ecs.tryGet<Velocity>(entity);
   const auto name = ecs.get<Name>(entity);
   if (!position || position->x != 1 || position->y != 2) { return 6; }
   if (!velocity || !velocity->has_value() || (*velocity)->dx != 5) { return 7; }
   if (!name || name->value != "player") { return 8; }

   const auto view = ecs.view<Position, Velocity>();
   if (view.size() != 1 || view.front() != entity) { return 9; }

   if (!ecs.remove<Velocity>(entity)) { return 10; }
   const auto removed = ecs.tryGet<Velocity>(entity);
   if (!removed || removed->has_value()) { return 11; }
   if (!ecs.erase(entity) || ecs.exists(entity)) { return 12; }

   return 0;
}
