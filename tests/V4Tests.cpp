#include <cmath>
#include <expected>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>

import VEEngine;

namespace {

struct Velocity {
   float x{0.0F};
};

struct CountingSystem {
   int *init_count{nullptr};
   int *update_count{nullptr};
   std::uint64_t *last_frame{nullptr};

   template <typename TWorld> std::expected<void, vve::Error> init(TWorld &world) {
      if (init_count != nullptr) {
         ++*init_count;
      }
      return world.windowSystem().windowCount() == 0 ? std::unexpected(vve::Error::missing_object)
                                                     : std::expected<void, vve::Error>{};
   }

   template <typename TWorld, typename TWindowFrame>
   std::expected<void, vve::Error> update(TWorld &, const vve::FrameContext &frame,
                                          const TWindowFrame &window_frame) {
      if (update_count != nullptr) {
         ++*update_count;
      }
      if (last_frame != nullptr) {
         *last_frame = frame.frame_index.value;
      }
      if (window_frame.windows.empty()) {
         return std::unexpected(vve::Error::missing_object);
      }
      return {};
   }
};

[[nodiscard]] bool nearly(float lhs, float rhs) {
   return std::abs(lhs - rhs) < 0.0001F;
}

[[nodiscard]] int testFacadeContracts() {
   return 0;
}

[[nodiscard]] int testHandles() {
   using namespace vve;

   const auto counter = makeHandleForTest<MeshHandle>(41);
   if (!counter.valid() || !counter.isCounter() || counter.isSlotMapIndex() || counter.id() != 41) {
      return 1;
   }
   const auto slot = makeSlotMapHandleForTest<NodeHandle>(7, 3);
   if (!slot.valid() || slot.isCounter() || !slot.isSlotMapIndex() || slot.slotIndex() != 7 || slot.generation() != 3) {
      return 2;
   }
   const auto next_counter = makeHandleForTest<MeshHandle>(42);
   if (counter == next_counter || !(counter < next_counter)) { return 4; }
   if (MeshHandle{}.valid()) {
      return 3;
   }
   const auto runtime_counter = makeCounterHandle<TextureHandle>();
   if (!runtime_counter.valid() || !runtime_counter.isCounter()) {
      return 5;
   }
   const auto typed_slot = makeSlotMapHandleForTest<NodeHandle>(5, 2);
   if (!typed_slot.valid() || typed_slot.isCounter() || typed_slot.slotIndex() != 5 || typed_slot.generation() != 2) {
      return 6;
   }
   return 0;
}

[[nodiscard]] int testVector() {
   using namespace vve;

   Vector<int> values{};
   if (!values.empty() || values.capacity() != 0 || values.segmentCount() != 0 || values.segmentSize() != 256) {
      return 90;
   }

   for (int value = 0; value < 260; ++value) {
      values.push_back(value);
   }
   if (values.size() != 260 || values.segmentCount() != 2 || values.capacity() != 512 ||
       values.front() != 0 || values.back() != 259 || values.at(128) != 128) {
      return 91;
   }

   auto *stable_address = std::addressof(values[4]);
   values.push_back(260);
   if (std::addressof(values[4]) != stable_address || values.back() != 260) {
      return 92;
   }

   const auto inserted = values.insert(values.cbegin() + 1, 777);
   if (inserted == values.end() || *inserted != 777 || values[2] != 1) {
      return 93;
   }
   const auto erased = values.erase(values.cbegin() + 1);
   if (erased == values.end() || *erased != 1 || values[1] != 1) {
      return 94;
   }

   values.resize(300, -1);
   if (values.size() != 300 || values.back() != -1) {
      return 95;
   }
   values.appendRange(std::initializer_list<int>{301, 302});
   if (values.size() != 302 || values.back() != 302) {
      return 96;
   }

   const auto range = makeRange(values);
   if (std::ranges::distance(range) != static_cast<std::ptrdiff_t>(values.size())) {
      return 97;
   }

   values.clear();
   if (!values.empty() || values.capacity() != 512) {
      return 98;
   }
   return 0;
}

[[nodiscard]] int testStrongMathTypes() {
   using namespace vve;

   const auto identity = math::identityMat4();
   const auto translated = math::translate(identity, math::Vec3{1.0F, 2.0F, 3.0F});
   const auto view = math::lookAt(math::Vec3{0.0F, 0.0F, 1.0F},
                                  math::Vec3{0.0F, 0.0F, 0.0F},
                                  math::Vec3{0.0F, 1.0F, 0.0F});
   const auto projection = math::perspective(0.9F, 1.0F, 0.1F, 10.0F);
   if (!nearly(translated[3].x, 1.0F) || !nearly(view[3].z, -1.0F) || nearly(projection[1].y, 0.0F)) {
      return 9;
   }
   const auto added = math::add(Vec3{1.0F, 2.0F, 3.0F}, Vec3{4.0F, 5.0F, 6.0F});
   const auto subtracted = math::subtract(added, Vec3{1.0F, 1.0F, 1.0F});
   const auto scaled = math::scale(subtracted, Scalar{0.5F});
   const auto normalized = math::normalize(Vec3{0.0F, 3.0F, 4.0F});
   const auto minimum = math::min(Vec3{2.0F, -1.0F, 7.0F}, Vec3{1.0F, 3.0F, 4.0F});
   const auto maximum = math::max(Vec3{2.0F, -1.0F, 7.0F}, Vec3{1.0F, 3.0F, 4.0F});
   if (!nearly(added.z, 9.0F) || !nearly(subtracted.x, 4.0F) || !nearly(scaled.y, 3.0F)) {
      return 15;
   }
   if (!nearly(math::length(Vec3{0.0F, 3.0F, 4.0F}), 5.0F) || !nearly(normalized.z, 0.8F) ||
       !nearly(math::dot(Vec3{1.0F, 2.0F, 3.0F}, Vec3{2.0F, 0.0F, 1.0F}), 5.0F)) {
      return 16;
   }
   if (!nearly(math::cross(Vec3{1.0F, 0.0F, 0.0F}, Vec3{0.0F, 1.0F, 0.0F}).z, 1.0F) ||
       !nearly(minimum.x, 1.0F) || !nearly(maximum.z, 7.0F) ||
       !nearly(math::clamp(Scalar{5.0F}, Scalar{1.0F}, Scalar{3.0F}), 3.0F)) {
      return 17;
   }

   const auto transform = Transform{
      .translation = Position{.value = Vec3{1.0F, 2.0F, 3.0F}},
      .rotation = Rotation{.value = identityQuat()},
      .scale = Scale{.value = Vec3{2.0F, 2.0F, 2.0F}}};

   if (!nearly(transform.translation.value.x, 1.0F) ||
       !nearly(transform.translation.value.y, 2.0F) ||
       !nearly(transform.translation.value.z, 3.0F)) {
      return 10;
   }
   if (!nearly(transform.scale.value.x, 2.0F) ||
       !nearly(Direction{}.value.z, -1.0F)) {
      return 11;
   }

   const auto color = LinearColor{.value = Vec3{0.25F, 0.5F, 1.0F}};
   const auto intensity = LightIntensity{.value = 3.0F};
   const auto fov_y = FovY{.radians = 0.75F};
   const auto clip = ClipPlanes{.near_plane = 0.25F, .far_plane = 250.0F};
   const auto delta = DeltaTime{.seconds = 0.5};
   const auto extent = PixelExtent{.width = 640, .height = 480};
   if (!nearly(color.value.y, 0.5F) || !nearly(intensity.value, 3.0F) ||
       !nearly(fov_y.radians, 0.75F)) {
      return 12;
   }
   if (!nearly(clip.near_plane, 0.25F) || !nearly(clip.far_plane, 250.0F) ||
       delta.seconds != 0.5 || extent.width != 640 || extent.height != 480) {
      return 13;
   }
   const auto camera = Camera::lookAt(Position{.value = Vec3{0.0F, 1.0F, 5.0F}},
                                      Position{.value = Vec3{0.0F, 1.0F, 0.0F}},
                                      Direction{.value = Vec3{0.0F, 1.0F, 0.0F}},
                                      fov_y,
                                      clip);
   if (!nearly(camera.position.value.z, 5.0F) || !nearly(camera.forward.value.z, -5.0F) ||
       !nearly(camera.fov_y.radians, 0.75F) || !nearly(camera.clip.far_plane, 250.0F)) {
      return 14;
   }
   return 0;
}

[[nodiscard]] int testECS() {
   using namespace vve;

   auto engine = makeEngine(ApplicationName{"ecs-test"});
   auto ecs = engine.world().ecs();
   const auto entity = ecs.create();
   if (!entity.valid() || !entity.isCounter() || !ecs.exists(entity)) {
      return 20;
   }
   if (!ecs.add(entity, Position{Vec3{1.0F, 0.0F, 0.0F}})) {
      return 21;
   }
   if (ecs.add(entity, Position{})) {
      return 22;
   }
   if (!ecs.put(entity, Velocity{3.0F})) {
      return 23;
   }
   const auto position = ecs.get<Position>(entity);
   const auto velocity = ecs.get<Velocity>(entity);
   if (!position || !velocity || !nearly(position->value.x, 1.0F) || !nearly(velocity->x, 3.0F)) {
      return 24;
   }
   if (ecs.view<Position, Velocity>().size() != 1) {
      return 25;
   }
   if (!ecs.remove<Velocity>(entity)) {
      return 26;
   }
   const auto missing_velocity = ecs.tryGet<Velocity>(entity);
   if (!missing_velocity || missing_velocity->has_value()) {
      return 27;
   }
   if (!ecs.erase(entity) || ecs.exists(entity)) {
      return 28;
   }
   return 0;
}

[[nodiscard]] int testInputAndWorld() {
   using namespace vve;

   const auto window = makeHandleForTest<WindowHandle>(200);
   auto input_engine = makeEngine(ApplicationName{"input-test"});
   auto input = input_engine.world().input();
   input.pressKey('W');
   if (!input.isKeyDown('W') || !input.wasKeyPressed('W')) {
      return 60;
   }
   input.beginFrame();
   if (!input.isKeyDown('W') || input.wasKeyPressed('W')) {
      return 61;
   }
   input.releaseKey('W');
   if (input.isKeyDown('W') || input.isKeyDown('w') || !input.wasKeyReleased('W') || !input.wasKeyReleased('w')) {
      return 62;
   }
   input.beginFrame();
   input.pressKey('W');
   if (!input.isKeyDown('w') || !input.wasKeyPressed('w')) {
      return 68;
   }
   input.releaseKey('w');
   if (input.isKeyDown('W') || input.isKeyDown('w') || input.wasKeyPressed('W') || input.wasKeyPressed('w') ||
       !input.wasKeyReleased('W') || !input.wasKeyReleased('w')) {
      return 69;
   }
   input.setMousePosition(window, Vec2{10.0F, 20.0F});
   input.addMouseDelta(window, Vec2{1.0F, 2.0F});
   input.addMouseWheelDelta(window, Vec2{0.0F, -1.0F});
   const auto position = input.mousePosition(window);
   if (!position || !nearly(position->x, 10.0F) || !nearly(input.mouseDelta(window).y, 2.0F) ||
       !nearly(input.mouseWheelDelta(window).y, -1.0F)) {
      return 63;
   }

   auto engine = makeEngine(ApplicationName{"world-test"});
   auto world = engine.world();
   const auto ecs_entity = world.ecs().create();
   if (!world.ecs().exists(ecs_entity)) {
      return 67;
   }
   const auto entity = world.spawn(Transform{}, Velocity{2.0F});
   const auto camera = world.spawn(Camera{});
   if (!entity || !camera || world.windowSystem().windowCount() != 0 ||
       world.windowSystem().findWindow("main").has_value()) {
      return 64;
   }
   const auto velocity = world.getComponent<Velocity>(*entity);
   if (!velocity || !velocity->has_value() || !nearly((*velocity)->x, 2.0F)) {
      return 65;
   }
   return 0;
}

[[nodiscard]] int testAssimpSceneImport() {
   using namespace vve;

   const auto path = std::filesystem::temp_directory_path() / "vve_v4_assimp_triangle.obj";
   {
      std::ofstream file{path};
      file << "o Triangle\n"
           << "v 0 0 0\n"
           << "v 1 0 0\n"
           << "v 0 1 0\n"
           << "f 1 2 3\n";
   }

   auto engine = makeEngine(ApplicationName{"asset-import"});
   auto assets = engine.world().assets();
   const auto scene_handle = assets.loadScene(path);
   std::error_code remove_error{};
   std::filesystem::remove(path, remove_error);

   if (!scene_handle) {
      return 70;
   }
   const auto scene_name = assets.sceneName(*scene_handle);
   const auto node_count = assets.sceneNodeCount(*scene_handle);
   const auto mesh_count = assets.sceneMeshCount(*scene_handle);
   const auto material_count = assets.sceneMaterialCount(*scene_handle);
   const auto texture_count = assets.sceneTextureCount(*scene_handle);
   const auto light_count = assets.sceneLightCount(*scene_handle);
   const auto camera_count = assets.sceneCameraCount(*scene_handle);
   if (!assets.containsScene(*scene_handle) || !scene_name || scene_name->value != path.filename().string()) {
      return 71;
   }
   if (!node_count || *node_count == 0 || !mesh_count || *mesh_count == 0) {
      return 72;
   }
   if (!material_count || *material_count == 0) {
      return 73;
   }
   if (!texture_count || !light_count || !camera_count || *texture_count != 0 || *light_count != 0 ||
       *camera_count != 0) {
      return 74;
   }
   if (assets.containsScene(SceneHandle{}) || assets.sceneName(SceneHandle{})) {
      return 75;
   }
   return 0;
}

[[nodiscard]] int testStubSystems() {
   using namespace vve;

   int init_count = 0;
   int update_count = 0;
   std::uint64_t last_frame = 99;
   auto engine = makeEngine(ApplicationName{"test"},
                            MaxFrames{.value = FrameCount{.value = 2}},
                            WindowSetups{WindowSetup{}
                                            .id("main")
                                            .title("hidden")
                                            .extent(PixelExtent{.width = 64, .height = 64})
                                            .position(20, 20)
                                            .visible(false),
                                         WindowSetup{}
                                            .id("tools")
                                            .title("hidden-tools")
                                            .extent(PixelExtent{.width = 64, .height = 64})
                                            .position(100, 20)
                                            .visible(false)},
                            makeUserSystems(CountingSystem{.init_count = &init_count,
                                                           .update_count = &update_count,
                                                           .last_frame = &last_frame}));
   if (engine.versionMajor() != 4 || engine.versionName() != std::string_view{"v4"}) {
      return 40;
   }
   if (!engine.init()) {
      return 41;
   }
   const auto camera = engine.world().spawn(Camera{});
   if (!camera || !engine.world().setWindowCamera("main", *camera)) {
      return 57;
   }
   auto assets = engine.world().assets();
   const auto scene = assets.addScene(ObjectName{.value = "stub"});
   if (!scene || !scene->isCounter() || !assets.containsScene(*scene)) {
      return 42;
   }
   const auto first = engine.step();
   const auto second = engine.step();
   if (!first || !second || *first != FrameStatus::running || *second != FrameStatus::stopped) {
      return 51;
   }
   if (init_count != 1 || update_count != 2 || last_frame != 1 || !engine.world().windowSystem().findWindow("main")) {
      return 52;
   }
   if (engine.world().windowSystem().windowCount() != 2 || !engine.world().windowSystem().findWindow("tools")) {
      return 53;
   }
   const auto main_camera = engine.world().windowCamera("main");
   if (!main_camera || *main_camera != *camera) {
      return 58;
   }
   return 0;
}

[[nodiscard]] int testFacadeNames() {
   using namespace vve;

   const auto scene = makeHandleForTest<SceneHandle>(700);
   if (!scene.valid()) {
      return 80;
   }

   auto engine = makeEngine(ApplicationName{"facade-test"}, MaxFrames{});
   if (engine.versionMajor() != 4) {
      return 81;
   }
   if (engineImplementationNamespaceName != std::string_view{"v4"}) {
      return 82;
   }
   return 0;
}

} // namespace

int main() {
   if (const int result = testFacadeContracts(); result != 0) {
      return result;
   }
   if (const int result = testHandles(); result != 0) {
      return result;
   }
   if (const int result = testVector(); result != 0) {
      return result;
   }
   if (const int result = testStrongMathTypes(); result != 0) {
      return result;
   }
   if (const int result = testECS(); result != 0) {
      return result;
   }
   if (const int result = testInputAndWorld(); result != 0) {
      return result;
   }
   if (const int result = testAssimpSceneImport(); result != 0) {
      return result;
   }
   if (const int result = testStubSystems(); result != 0) {
      return result;
   }
   if (const int result = testFacadeNames(); result != 0) {
      return result;
   }
   return 0;
}
