#include <cmath>
#include <string>

import VEEngine;

namespace {

struct Tag {
   std::string value{};
};

[[nodiscard]] bool nearly(float lhs, float rhs) {
   return std::abs(lhs - rhs) < 0.0001F;
}

} // namespace

int main() {
   auto engine = vve::makeEngine(
      vve::ApplicationName{"world-tests"},
      vve::MaxFrames{.value = vve::FrameCount{.value = 1}},
      vve::WindowSetups{vve::WindowSetup{}
                           .id("main")
                           .title("world-tests")
                           .extent(vve::PixelExtent{.width = 64, .height = 64})
                           .visible(false)});
   if (!engine.init()) { return 1; }

   auto world = engine.world();
   auto window_system = world.windowSystem();
   const auto camera = world.spawn(vve::Camera{});
   if (!camera || !window_system.setActiveCamera(*camera)) { return 2; }
   const auto active = window_system.activeCamera();
   if (!active || *active != *camera) { return 3; }
   if (!window_system.setWindowCamera("main", *camera)) { return 4; }
   const auto window_camera = window_system.windowCamera("main");
   if (!window_camera || *window_camera != *camera) { return 5; }

   const auto transform = vve::Transform{
      .translation = vve::Position{.value = vve::Vec3{1.0F, 2.0F, 3.0F}},
      .scale = vve::Scale{.value = vve::Vec3{2.0F, 2.0F, 2.0F}}};
   const auto entity = world.spawn(transform, Tag{.value = "crate"});
   if (!entity) { return 6; }

   const auto read_transform = world.getTransform(*entity);
   const auto tag = world.getComponent<Tag>(*entity);
   if (!read_transform || !read_transform->has_value() || !tag || !tag->has_value()) { return 7; }
   if (!nearly((*read_transform)->translation.value.x, 1.0F) || (*tag)->value != "crate") { return 8; }

   const auto updated = vve::Transform{.translation = vve::Position{.value = vve::Vec3{4.0F, 5.0F, 6.0F}}};
   if (!world.setTransform(*entity, updated)) { return 9; }
   const auto moved = world.getTransform(*entity);
   if (!moved || !moved->has_value() || !nearly((*moved)->translation.value.x, 4.0F)) { return 10; }
   if (!world.destroy(*entity)) { return 11; }

   return 0;
}
