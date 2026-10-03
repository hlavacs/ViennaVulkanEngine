#include <cmath>
#include <functional>
#include <string>
#include <type_traits>

import VVEngine;

namespace {

struct Tag {
   std::string value{};
};

static_assert(std::constructible_from<vve::World<std::reference_wrapper<Tag>>, std::reference_wrapper<Tag>>);
static_assert(!std::constructible_from<vve::World<Tag>, Tag>);
static_assert(!std::constructible_from<vve::World<std::reference_wrapper<Tag *>>, std::reference_wrapper<Tag *>>);

[[nodiscard]] bool nearly(float lhs, float rhs) {
   return std::abs(lhs - rhs) < 0.0001F;
}

} // namespace

int main() {
   auto engine = vve::EngineBuilder<>{}
                    .applicationName("world-tests")
                    .maxFrames(vve::MaxFrames{.value = vve::FrameCount{.value = 1}})
                    .windows(vve::WindowSetups{vve::WindowSetup{}
                                                  .id("main")
                                                  .title("world-tests")
                                                  .extent(vve::PixelExtent{.width = 64, .height = 64})
                                                  .renderer(vve::RendererId{.value = "forward"})
                                                  .visible(false),
                                               vve::WindowSetup{}
                                                  .id("tools")
                                                  .title("world-tools")
                                                  .extent(vve::PixelExtent{.width = 64, .height = 64})
                                                  .visible(false)})
                    .build();
   if (!engine.init()) { return 1; }

   auto world = engine.world();
   auto &ecs = world.get<vve::ECS>();
   auto window_system = world.get<vve::WindowSystem>();
   const auto camera = ecs.create();
   if (const auto result = ecs.add(camera, vve::Camera{}); !result) { return 6; }
   const auto stored_camera = ecs.tryGet<vve::Camera>(camera);
   if (!stored_camera || !stored_camera->has_value()) { return 7; }
   auto &render = world.get<vve::RenderSystem>();
   const auto main = window_system.findWindow("main");
   if (!main || !render.setCamera(main->handle(), **stored_camera)) { return 8; }
   if (!render.clearCamera(main->handle())) { return 9; }

   const auto transform = vve::Transform{
      .translation = vve::Position{.value = vve::Vec3{1.0F, 2.0F, 3.0F}},
      .scale = vve::Scale{.value = vve::Vec3{2.0F, 2.0F, 2.0F}}};
   const auto entity = ecs.create();
   if (const auto result = ecs.add(entity, transform); !result) { return 10; }
   if (const auto result = ecs.add(entity, Tag{.value = "crate"}); !result) { return 10; }

   const auto read_transform = ecs.tryGet<vve::Transform>(entity);
   const auto tag = ecs.tryGet<Tag>(entity);
   if (!read_transform || !read_transform->has_value() || !tag || !tag->has_value()) { return 11; }
   if (!nearly((*read_transform)->translation.value.x, 1.0F) || (*tag)->value != "crate") { return 12; }

   const auto updated = vve::Transform{.translation = vve::Position{.value = vve::Vec3{4.0F, 5.0F, 6.0F}}};
   if (!ecs.put(entity, updated)) { return 13; }
   const auto moved = ecs.tryGet<vve::Transform>(entity);
   if (!moved || !moved->has_value() || !nearly((*moved)->translation.value.x, 4.0F)) { return 14; }
   if (!ecs.erase(entity)) { return 15; }

   return 0;
}
