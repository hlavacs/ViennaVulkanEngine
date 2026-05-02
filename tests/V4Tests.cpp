#include <cmath>
#include <expected>
#include <filesystem>
#include <fstream>
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
         *last_frame = frame.frame_index;
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

   static_assert(sizeof(Handle) == sizeof(std::uint64_t));
   const auto counter = makeCounterHandle(41);
   if (!counter.valid() || !counter.isCounter() || counter.isSlotMapIndex() || counter.id() != 41) {
      return 1;
   }
   const auto slot = makeSlotMapHandle(7, 3);
   if (!slot.valid() || slot.isCounter() || !slot.isSlotMapIndex() || slot.slotIndex() != 7 || slot.generation() != 3) {
      return 2;
   }
   const auto next_counter = makeCounterHandle(42);
   if (counter == next_counter || !(counter < next_counter)) { return 4; }
   if (Handle{}.valid()) {
      return 3;
   }
   return 0;
}

[[nodiscard]] int testStrongMathTypes() {
   using namespace vve::v4;

   static_assert(std::is_same_v<Transform, vve::Transform>);
   static_assert(std::is_same_v<Camera, vve::Camera>);
   static_assert(std::is_same_v<Bounds, vve::Bounds>);

   const auto identity = math::identityMat4();
   const auto translated = math::translate(identity, math::Vec3{1.0F, 2.0F, 3.0F});
   const auto view = math::lookAt(math::Vec3{0.0F, 0.0F, 1.0F},
                                  math::Vec3{0.0F, 0.0F, 0.0F},
                                  math::Vec3{0.0F, 1.0F, 0.0F});
   const auto projection = math::perspective(0.9F, 1.0F, 0.1F, 10.0F);
   if (!nearly(translated[3].x, 1.0F) || !nearly(view[3].z, -1.0F) || nearly(projection[1].y, 0.0F)) {
      return 9;
   }

   const auto transform = Transform{
      .translation = Vec3{1.0F, 2.0F, 3.0F},
      .rotation = identityQuat(),
      .scale = Vec3{2.0F, 2.0F, 2.0F}};

   if (!nearly(transform.translation.x, 1.0F) ||
       !nearly(transform.translation.y, 2.0F) ||
       !nearly(transform.translation.z, 3.0F)) {
      return 10;
   }
   if (!nearly(transform.scale.x, 2.0F) ||
       !nearly(Direction{}.value.z, -1.0F)) {
      return 11;
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
   const auto scene = makeCounterHandle(100);
   const auto node = makeCounterHandle(101);
   const auto child = makeCounterHandle(102);
   const auto mesh = makeCounterHandle(103);
   const auto material = makeCounterHandle(104);
   const auto texture = makeCounterHandle(105);
   const auto light = makeCounterHandle(106);
   const auto camera = makeCounterHandle(107);

   if (!catalog.textures.add(TextureDescriptor{.handle = texture,
                                               .name = "stone",
                                               .source = "stone.png",
                                               .width = 1024,
                                               .height = 512,
                                               .channels = 4})) {
      return 30;
   }
   if (!catalog.materials.add(MaterialDescriptor{
          .handle = material,
          .name = "stone_mat",
          .textures = {TextureBinding{.texture = texture, .semantic = TextureSemantic::base_color}}})) {
      return 31;
   }
   if (!catalog.meshes.add(MeshDescriptor{
          .handle = mesh,
          .name = "arch",
          .vertex_count = 3,
          .index_count = 3,
          .material = material})) {
      return 32;
   }
   if (!catalog.nodes.add(NodeDescriptor{
          .handle = node,
          .name = "root",
          .meshes = {MeshUse{.mesh = mesh, .material = material}}})) {
      return 33;
   }
   if (!catalog.nodes.add(NodeDescriptor{.handle = child, .name = "child"})) {
      return 34;
   }
   if (!catalog.lights.add(LightDescriptor{.handle = light, .name = "sun", .kind = LightKind::directional}) ||
       !catalog.cameras.add(CameraDescriptor{.handle = camera, .name = "camera"})) {
      return 35;
   }
   auto tree = Tree{.root = node};
   tree.addChild(node, child);
   if (!catalog.scenes.add(SceneDescriptor{.handle = scene,
                                           .name = "scene",
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
   if (mesh_descriptor == nullptr || material_descriptor == nullptr || scene_descriptor == nullptr) {
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
   if (catalog.meshes.add(*mesh_descriptor)) {
      return 39;
   }
   if (catalog.meshes.add(MeshDescriptor{.handle = {}, .name = "bad"})) {
      return 49;
   }
   return 0;
}

[[nodiscard]] int testInputAndWorld() {
   using namespace vve::v4;

   const auto window = makeCounterHandle(200);
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
   if (!entity || world.findWindow("main") == nullptr) {
      return 64;
   }
   const auto velocity = world.getComponent<Velocity>(*entity);
   if (!velocity || !velocity->has_value() || !nearly((*velocity)->x, 2.0F)) {
      return 65;
   }
   if (!world.setActiveCamera(*entity) || !world.activeCamera().has_value()) {
      return 66;
   }
   return 0;
}

[[nodiscard]] int testGraphTopologicalOrder() {
   using namespace vve::v4;

   const auto a = makeCounterHandle(300);
   const auto b = makeCounterHandle(301);
   const auto c = makeCounterHandle(302);
   const auto d = makeCounterHandle(303);

   Graph graph{};
   graph.addEdge(a, c);
   graph.addEdge(b, c);
   graph.addEdge(c, d);
   const auto ordered = graph.topologicalOrder(Vector<Handle>{a, b, c, d});
   if (!ordered || ordered->size() != 4 || ordered->at(0) != a || ordered->at(1) != b ||
       ordered->at(2) != c || ordered->at(3) != d) {
      return 75;
   }

   Graph cyclic{};
   cyclic.addEdge(a, b);
   cyclic.addEdge(b, a);
   const auto cycle = cyclic.topologicalOrder(Vector<Handle>{a, b});
   if (cycle || cycle.error() != Error::cycle_detected) {
      return 76;
   }

   Graph missing_node{};
   missing_node.addEdge(a, b);
   const auto missing = missing_node.topologicalOrder(Vector<Handle>{a});
   if (missing || missing.error() != Error::missing_object) {
      return 77;
   }

   Graph invalid_node{};
   const auto invalid = invalid_node.topologicalOrder(Vector<Handle>{Handle{}});
   if (invalid || invalid.error() != Error::invalid_handle) {
      return 78;
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
   if (mesh == nullptr || mesh->vertex_count != 3 || mesh->index_count != 3) {
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
                            MaxFrames{2},
                            Windows{.value = {WindowDesc{.id = "main",
                                                          .title = "hidden",
                                                          .width = 64,
                                                          .height = 64,
                                                          .x = 20,
                                                          .y = 20,
                                                          .visible = false},
                                               WindowDesc{.id = "tools",
                                                          .title = "hidden-tools",
                                                          .width = 64,
                                                          .height = 64,
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
   const auto scene = engine.assets().addScene("stub");
   if (!scene || !scene->isCounter() || engine.assets().catalog().scenes.find(*scene) == nullptr) {
      return 42;
   }
   const auto resource = engine.resources().add(ResourceKind::mesh, "mesh");
   if (!resource || engine.resources().find(*resource) == nullptr) {
      return 43;
   }
   const auto task = makeCounterHandle(400);
   const auto child_task = makeCounterHandle(401);
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
   const auto pass = makeCounterHandle(500);
   const auto child_pass = makeCounterHandle(501);
   const auto isolated_pass = makeCounterHandle(502);
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
   const auto shader = makeCounterHandle(600);
   if (!engine.shaders().add(ShaderDescriptor{.handle = shader,
                                              .name = "shader",
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
   return 0;
}
