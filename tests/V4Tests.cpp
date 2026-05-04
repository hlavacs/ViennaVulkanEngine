#include <cmath>
#include <expected>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>

import VEEngine.V4;

namespace {

struct Velocity {
   float x{0.0F};
};

struct CountingSystem {
   int *init_count{nullptr};
   int *update_count{nullptr};
   std::uint64_t *last_frame{nullptr};

   std::expected<void, vve::v4::Error> init(vve::v4::World &world) {
      if (init_count != nullptr) {
         ++*init_count;
      }
      return world.windows().empty() ? std::unexpected(vve::v4::Error::missing_object)
                                     : std::expected<void, vve::v4::Error>{};
   }

   std::expected<void, vve::v4::Error> update(vve::v4::World &, const vve::v4::FrameContext &frame,
                                              const vve::v4::WindowFrameData &window_frame) {
      if (update_count != nullptr) {
         ++*update_count;
      }
      if (last_frame != nullptr) {
         *last_frame = frame.frame_index.value;
      }
      if (window_frame.windows.empty()) {
         return std::unexpected(vve::v4::Error::missing_object);
      }
      return {};
   }
};

[[nodiscard]] bool nearly(float lhs, float rhs) {
   return std::abs(lhs - rhs) < 0.0001F;
}

[[nodiscard]] int testHandles() {
   using namespace vve::v4;

   static_assert(sizeof(SceneHandle) == sizeof(std::uint64_t));
   static_assert(!std::is_same_v<MeshHandle, TextureHandle>);
   static_assert(!std::is_convertible_v<MeshHandle, TextureHandle>);
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

[[nodiscard]] int testStrongMathTypes() {
   using namespace vve::v4;

   static_assert(std::is_same_v<Transform, vve::Transform>);
   static_assert(std::is_same_v<Camera, vve::Camera>);
   static_assert(std::is_same_v<Bounds, vve::Bounds>);
   static_assert(std::is_same_v<LinearColor, vve::LinearColor>);
   static_assert(std::is_same_v<LightIntensity, vve::LightIntensity>);
   static_assert(std::is_same_v<FovY, vve::FovY>);
   static_assert(std::is_same_v<ClipPlanes, vve::ClipPlanes>);
   static_assert(std::is_same_v<DeltaTime, vve::DeltaTime>);
   static_assert(std::is_same_v<PixelExtent, vve::PixelExtent>);

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
   using namespace vve::v4;

   ECS ecs{};
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

[[nodiscard]] int testDescriptorCatalog() {
   using namespace vve::v4;

   ObjectCatalog catalog{};
   const auto scene = makeHandleForTest<SceneHandle>(100);
   const auto node = makeHandleForTest<NodeHandle>(101);
   const auto child = makeHandleForTest<NodeHandle>(102);
   const auto mesh = makeHandleForTest<MeshHandle>(103);
   const auto material = makeHandleForTest<MaterialHandle>(104);
   const auto texture = makeHandleForTest<TextureHandle>(105);
   const auto light = makeHandleForTest<LightHandle>(106);
   const auto camera = makeHandleForTest<CameraHandle>(107);

   if (!catalog.textures.add(TextureDescriptor{.handle = texture,
                                               .name = ObjectName{.value = "stone"},
                                               .source = "stone.png",
                                               .extent = PixelExtent{.width = 1024, .height = 512},
                                               .channels = TextureChannelCount{.value = 4}})) {
      return 30;
   }
   if (!catalog.materials.add(MaterialDescriptor{
          .handle = material,
          .name = ObjectName{.value = "stone_mat"},
          .textures = {TextureBinding{.texture = texture, .semantic = TextureSemantic::base_color}}})) {
      return 31;
   }
   if (!catalog.meshes.add(MeshDescriptor{
          .handle = mesh,
          .name = ObjectName{.value = "arch"},
          .vertex_count = VertexCount{.value = 3},
          .index_count = IndexCount{.value = 3},
          .material = material})) {
      return 32;
   }
   if (!catalog.nodes.add(NodeDescriptor{
          .handle = node,
          .name = ObjectName{.value = "root"},
          .meshes = {MeshUse{.mesh = mesh, .material = material}}})) {
      return 33;
   }
   if (!catalog.nodes.add(NodeDescriptor{.handle = child, .name = ObjectName{.value = "child"}})) {
      return 34;
   }
   if (!catalog.lights.add(LightDescriptor{.handle = light,
                                           .name = ObjectName{.value = "sun"},
                                           .kind = LightKind::directional,
                                           .color = LinearColor{.value = Vec3{0.9F, 0.8F, 0.7F}},
                                           .intensity = LightIntensity{.value = 4.0F}}) ||
       !catalog.cameras.add(CameraDescriptor{.handle = camera,
                                             .name = ObjectName{.value = "camera"},
                                             .fov_y = FovY{.radians = 0.8F},
                                             .clip = ClipPlanes{.near_plane = 0.2F, .far_plane = 200.0F}})) {
      return 35;
   }
   auto tree = Tree{.root = node};
   tree.addChild(node, child);
   if (!catalog.scenes.add(SceneDescriptor{.handle = scene,
                                           .name = ObjectName{.value = "scene"},
                                           .tree = std::move(tree),
                                           .nodes = {node, child},
                                           .meshes = {mesh},
                                           .materials = {material},
                                           .textures = {texture},
                                           .lights = {light},
                                           .cameras = {camera}})) {
      return 36;
   }

   const auto *mesh_descriptor = catalog.meshes.find(mesh);
   const auto *material_descriptor = catalog.materials.find(material);
   const auto *scene_descriptor = catalog.scenes.find(scene);
   const auto *texture_descriptor = catalog.textures.find(texture);
   const auto *light_descriptor = catalog.lights.find(light);
   const auto *camera_descriptor = catalog.cameras.find(camera);
   if (mesh_descriptor == nullptr || material_descriptor == nullptr || scene_descriptor == nullptr ||
       texture_descriptor == nullptr || light_descriptor == nullptr || camera_descriptor == nullptr) {
      return 37;
   }
   const auto [first_child, last_child] = scene_descriptor->tree.childRange(node);
   if (mesh_descriptor->material != material ||
       material_descriptor->textures.front().texture != texture ||
       scene_descriptor->meshes.front() != mesh ||
       scene_descriptor->tree.root != node ||
       first_child == last_child ||
       first_child->second != child) {
      return 38;
   }
   if (texture_descriptor->extent.width != 1024 || !nearly(light_descriptor->intensity.value, 4.0F) ||
       !nearly(camera_descriptor->clip.near_plane, 0.2F)) {
      return 29;
   }
   if (catalog.meshes.add(*mesh_descriptor)) {
      return 39;
   }
   if (catalog.meshes.add(MeshDescriptor{.handle = {}, .name = ObjectName{.value = "bad"}})) {
      return 49;
   }
   if (!catalog.meshes.remove(mesh) || catalog.meshes.find(mesh) != nullptr) {
      return 82;
   }
   return 0;
}

[[nodiscard]] int testInputAndWorld() {
   using namespace vve::v4;

   const auto window = makeHandleForTest<WindowHandle>(200);
   InputState input{};
   input.pressKey('W');
   if (!input.isKeyDown('W') || !input.wasKeyPressed('W')) {
      return 60;
   }
   input.beginFrame();
   if (!input.isKeyDown('W') || input.wasKeyPressed('W')) {
      return 61;
   }
   input.releaseKey('W');
   if (input.isKeyDown('W') || !input.wasKeyReleased('W')) {
      return 62;
   }
   input.setMousePosition(window, Vec2{10.0F, 20.0F});
   input.addMouseDelta(window, Vec2{1.0F, 2.0F});
   input.addMouseWheelDelta(window, Vec2{0.0F, -1.0F});
   const auto position = input.mousePosition(window);
   if (!position || !nearly(position->x, 10.0F) || !nearly(input.mouseDelta(window).y, 2.0F) ||
       !nearly(input.mouseWheelDelta(window).y, -1.0F)) {
      return 63;
   }

   ECS ecs{};
   World world{ecs};
   if (std::addressof(world.ecs()) != std::addressof(ecs)) {
      return 67;
   }
   world.windows().push_back(WindowInfo{.handle = window, .id = "main", .title = "test"});
   const auto entity = world.spawn(Transform{}, Velocity{2.0F});
   const auto camera = world.spawn(Camera{});
   if (!entity || !camera || world.findWindow("main") == nullptr) {
      return 64;
   }
   const auto velocity = world.getComponent<Velocity>(*entity);
   if (!velocity || !velocity->has_value() || !nearly((*velocity)->x, 2.0F)) {
      return 65;
   }
   const auto main_camera = world.setWindowCamera("main", *camera) ? world.windowCamera("main")
                                                                   : std::optional<Entity>{};
   if (!main_camera || *main_camera != *camera) {
      return 66;
   }
   if (!world.clearWindowCamera(window) || world.windowCamera(window).has_value()) {
      return 68;
   }
   const auto active_camera = world.setActiveCamera(*camera) ? world.activeCamera() : std::optional<Entity>{};
   if (!active_camera || *active_camera != *camera) {
      return 69;
   }
   return 0;
}

[[nodiscard]] int testGraphTopologicalOrder() {
   using namespace vve::v4;

   const auto a = makeHandleForTest<TaskHandle>(300);
   const auto b = makeHandleForTest<TaskHandle>(301);
   const auto c = makeHandleForTest<TaskHandle>(302);
   const auto d = makeHandleForTest<TaskHandle>(303);

   Graph<TaskHandle> graph{};
   graph.addEdge(a, c);
   graph.addEdge(b, c);
   graph.addEdge(c, d);
   const auto [incoming_c, incoming_c_end] = graph.parentRange(c);
   bool has_a_parent = false;
   bool has_b_parent = false;
   for (auto it = incoming_c; it != incoming_c_end; ++it) {
      has_a_parent = has_a_parent || it->second == a;
      has_b_parent = has_b_parent || it->second == b;
   }
   if (!has_a_parent || !has_b_parent) {
      return 88;
   }
   const auto ordered = graph.topologicalOrder(Vector<TaskHandle>{a, b, c, d});
   if (!ordered || ordered->size() != 4 || ordered->at(0) != a || ordered->at(1) != b ||
       ordered->at(2) != c || ordered->at(3) != d) {
      return 75;
   }

   Graph<TaskHandle> cyclic{};
   cyclic.addEdge(a, b);
   cyclic.addEdge(b, a);
   const auto cycle = cyclic.topologicalOrder(Vector<TaskHandle>{a, b});
   if (cycle || cycle.error() != Error::cycle_detected) {
      return 76;
   }

   Graph<TaskHandle> missing_node{};
   missing_node.addEdge(a, b);
   const auto missing = missing_node.topologicalOrder(Vector<TaskHandle>{a});
   if (missing || missing.error() != Error::missing_object) {
      return 77;
   }

   Graph<TaskHandle> invalid_node{};
   const auto invalid = invalid_node.topologicalOrder(Vector<TaskHandle>{TaskHandle{}});
   if (invalid || invalid.error() != Error::invalid_handle) {
      return 78;
   }

   graph.removeNode(c);
   const auto after_remove = graph.topologicalOrder(Vector<TaskHandle>{a, b, d});
   if (!after_remove || after_remove->size() != 3 ||
       after_remove->at(0) != a || after_remove->at(1) != b || after_remove->at(2) != d) {
      return 83;
   }

   BasicTree<TaskHandle> tree{.root = a};
   tree.addChild(a, b);
   tree.addChild(b, c);
   const auto parent_b = tree.parentOf(b);
   const auto parent_c = tree.parentOf(c);
   if (!parent_b || !parent_c || *parent_b != a || *parent_c != b) {
      return 89;
   }
   tree.removeNode(b);
   const auto [root_child, root_child_end] = tree.childRange(a);
   const auto [removed_child, removed_child_end] = tree.childRange(b);
   if (root_child != root_child_end || removed_child != removed_child_end || tree.root != a) {
      return 84;
   }
   tree.removeNode(a);
   if (tree.root.valid()) {
      return 85;
   }
   return 0;
}

[[nodiscard]] int testAssimpSceneImport() {
   using namespace vve::v4;

   const auto path = std::filesystem::temp_directory_path() / "vve_v4_assimp_triangle.obj";
   {
      std::ofstream file{path};
      file << "o Triangle\n"
           << "v 0 0 0\n"
           << "v 1 0 0\n"
           << "v 0 1 0\n"
           << "f 1 2 3\n";
   }

   AssetSystem assets{};
   const auto scene_handle = assets.loadScene(path);
   std::error_code remove_error{};
   std::filesystem::remove(path, remove_error);

   if (!scene_handle) {
      return 70;
   }
   const auto *scene = assets.catalog().scenes.find(*scene_handle);
   if (scene == nullptr || !scene->tree.root.valid() || scene->nodes.empty() || scene->meshes.empty()) {
      return 71;
   }
   const auto *mesh = assets.catalog().meshes.find(scene->meshes.front());
   if (mesh == nullptr || mesh->vertex_count.value != 3 || mesh->index_count.value != 3) {
      return 72;
   }
   if (mesh->material.valid() && assets.catalog().materials.find(mesh->material) == nullptr) {
      return 73;
   }
   if (assets.catalog().nodes.find(scene->tree.root) == nullptr) {
      return 74;
   }
   return 0;
}

[[nodiscard]] int testStubSystems() {
   using namespace vve::v4;

   int init_count = 0;
   int update_count = 0;
   std::uint64_t last_frame = 99;
   auto engine = makeEngine(ApplicationName{"test"},
                            MaxFrames{.value = FrameCount{.value = 2}},
                            Windows{.value = {WindowDesc{.id = "main",
                                                          .title = "hidden",
                                                          .extent = PixelExtent{.width = 64, .height = 64},
                                                          .x = 20,
                                                          .y = 20,
                                                          .visible = false},
                                               WindowDesc{.id = "tools",
                                                          .title = "hidden-tools",
                                                          .extent = PixelExtent{.width = 64, .height = 64},
                                                          .x = 100,
                                                          .y = 20,
                                                          .visible = false}}},
                            makeUserSystems(CountingSystem{.init_count = &init_count,
                                                           .update_count = &update_count,
                                                           .last_frame = &last_frame}));
   if (engine.versionMajor() != 4 || engine.versionName() != std::string_view{"v4"}) {
      return 40;
   }
   if (std::addressof(engine.ecs()) != std::addressof(engine.world().ecs())) {
      return 56;
   }
   if (!engine.init()) {
      return 41;
   }
   const auto camera = engine.world().spawn(Camera{});
   if (!camera || !engine.world().setWindowCamera("main", *camera)) {
      return 57;
   }
   const auto scene = engine.assets().addScene(ObjectName{.value = "stub"});
   if (!scene || !scene->isCounter() || engine.assets().catalog().scenes.find(*scene) == nullptr) {
      return 42;
   }
   const auto resource = engine.resources().add(ResourceKind::mesh, ObjectName{.value = "mesh"});
   if (!resource || engine.resources().find(*resource) == nullptr) {
      return 43;
   }
   const auto task = makeHandleForTest<TaskHandle>(400);
   const auto child_task = makeHandleForTest<TaskHandle>(401);
   if (!engine.tasks().add(TaskNode{.handle = task, .name = "task"}) ||
       !engine.tasks().add(TaskNode{.handle = child_task, .name = "child-task"}) ||
       engine.tasks().find(task) == nullptr) {
      return 44;
   }
   engine.tasks().addEdge(task, child_task);
   const auto [task_child, task_child_end] = engine.tasks().graph().childRange(task);
   if (task_child == task_child_end || task_child->second != child_task) {
      return 45;
   }
   const auto task_order = engine.tasks().topologicalOrder();
   if (!task_order || task_order->size() != 2 || task_order->front() != task || task_order->back() != child_task) {
      return 54;
   }
   if (!engine.tasks().remove(child_task) || engine.tasks().find(child_task) != nullptr) {
      return 86;
   }
   const auto pass = makeHandleForTest<RenderPassHandle>(500);
   const auto child_pass = makeHandleForTest<RenderPassHandle>(501);
   const auto isolated_pass = makeHandleForTest<RenderPassHandle>(502);
   if (!engine.renderGraph().add(RenderPassNode{.handle = pass, .name = "pass"}) ||
       !engine.renderGraph().add(RenderPassNode{.handle = child_pass, .name = "child-pass"}) ||
       !engine.renderGraph().add(RenderPassNode{.handle = isolated_pass, .name = "isolated-pass"}) ||
       engine.renderGraph().find(pass) == nullptr) {
      return 46;
   }
   engine.renderGraph().addEdge(pass, child_pass);
   const auto [pass_child, pass_child_end] = engine.renderGraph().graph().childRange(pass);
   if (pass_child == pass_child_end || pass_child->second != child_pass) {
      return 47;
   }
   const auto pass_order = engine.renderGraph().topologicalOrder();
   if (!pass_order || pass_order->size() != 3 || pass_order->at(0) != pass ||
       pass_order->at(1) != child_pass || pass_order->at(2) != isolated_pass) {
      return 55;
   }
   if (!engine.renderGraph().remove(child_pass) || engine.renderGraph().find(child_pass) != nullptr) {
      return 87;
   }
   const auto shader = makeHandleForTest<ShaderHandle>(600);
   if (!engine.shaders().add(ShaderDescriptor{.handle = shader,
                                              .name = ObjectName{.value = "shader"},
                                              .stages = {ShaderStage::vertex, ShaderStage::fragment}}) ||
       engine.shaders().find(shader) == nullptr) {
      return 48;
   }
   const auto label = engine.gui().label("hello");
   if (!label || engine.gui().find(*label) == nullptr) {
      return 50;
   }
   const auto first = engine.step();
   const auto second = engine.step();
   if (!first || !second || *first != FrameStatus::running || *second != FrameStatus::stopped) {
      return 51;
   }
   if (init_count != 1 || update_count != 2 || last_frame != 1 || engine.world().findWindow("main") == nullptr) {
      return 52;
   }
   if (engine.world().windows().size() != 2 || engine.world().findWindow("tools") == nullptr) {
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

   static_assert(sizeof(SceneHandle) == sizeof(std::uint64_t));
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
   if (const int result = testHandles(); result != 0) {
      return result;
   }
   if (const int result = testStrongMathTypes(); result != 0) {
      return result;
   }
   if (const int result = testECS(); result != 0) {
      return result;
   }
   if (const int result = testDescriptorCatalog(); result != 0) {
      return result;
   }
   if (const int result = testInputAndWorld(); result != 0) {
      return result;
   }
   if (const int result = testGraphTopologicalOrder(); result != 0) {
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
