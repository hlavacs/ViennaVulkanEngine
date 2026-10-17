#include <vulkan/vulkan_core.h>

/// @file
/// @brief Checks fenced scene updates and retirement across two independently advancing windows.
import std;
import VVEngine;
import VVEngine.Simple;
import VVEngine.Simple.Renderer;
import VVEngine.Simple.Scene;
import VVEngine.Simple.Vulkan;

namespace {

using vve::simple::FrameOutcome; ///< Direct draw calls must present, rather than merely avoid a fatal error.

/// @brief Checks retirement against the last submission across windows, including a window whose sets stay stale.
[[nodiscard]] bool shadowRetirement(vve::simple::ForwardRenderer &renderer) {
   auto &a = renderer.targets.front();
   auto &b = renderer.targets.back();
   const auto last_old_submit = b.submitSerials[0];
   const auto idle_before = renderer.deviceWaitIdleCount();
   renderer.scene.directionalLights = {vve::simple::ForwardDirectionalLight{}};
   renderer.scene.spotLights = {vve::simple::ForwardSpotLight{}, vve::simple::ForwardSpotLight{}};
   renderer.scene.pointLights = {vve::simple::ForwardPointLight{}};
   // Slot 1 has never submitted; growth must retain all three old arrays and leave b's descriptors pending.
   if (renderer.drawFrame(a) != FrameOutcome::presented || renderer.retiredResourceCount() != 3U ||
       renderer.completedSubmitSerial() >= last_old_submit || b.shadowsDirty.count() != 2U ||
       a.shadowsDirty.count() != 1U || renderer.deviceWaitIdleCount() != idle_before) { return false; }
   // a's earlier fence does not yet cover b's last use of the old arrays.
   if (renderer.drawFrame(a) != FrameOutcome::presented || renderer.retiredResourceCount() != 3U ||
       renderer.completedSubmitSerial() >= last_old_submit) { return false; }
   // The following a fence covers b on the shared queue; b adopts the current arrays on each resumed slot.
   if (renderer.drawFrame(a) != FrameOutcome::presented || renderer.retiredResourceCount() != 0U ||
       renderer.completedSubmitSerial() < last_old_submit || renderer.drawFrame(b) != FrameOutcome::presented ||
       renderer.drawFrame(b) != FrameOutcome::presented ||
       b.shadowsDirty.any() || renderer.deviceWaitIdleCount() != idle_before) { return false; }
   std::println("R3 shadow_retirement last_old_submit={} completed={} pending={} idle_waits={}",
      last_old_submit, renderer.completedSubmitSerial(), renderer.retiredResourceCount(), renderer.deviceWaitIdleCount());
   return true;
}

/// @brief Reads the first uploaded vertex without assuming alignment of mapped storage.
[[nodiscard]] float firstVertexX(const vve::simple::VulkanBuffer &buffer) {
   vve::simple::RenderVertex vertex{};
   if (buffer.bytes().size() < sizeof(vertex)) { return std::numeric_limits<float>::quiet_NaN(); }
   std::memcpy(&vertex, buffer.bytes().data(), sizeof(vertex));
   return vertex.position.x;
}

} // namespace

/// @brief Checks fenced updates without idle waits, then checks one shared rebuild when texture capacity grows.
int main() {
   auto engine = vve::simple::Engine{vve::simple::Windows{.value = {
      vve::WindowDesc{.id = "a", .extent = {64, 64}, .visible = false},
      vve::WindowDesc{.id = "b", .extent = {64, 64}, .visible = false}}}};
   if (!engine.init()) { return 1; }
   auto &render = engine.renderSystem();
   auto &renderer = render.forward();
   render.clearScene();
   const auto triangle = render.addTriangleMesh(
      vve::Vector<vve::Vec3>{{-1.0F, 0.0F, 0.0F}, {1.0F, 0.0F, 0.0F}, {0.0F, 1.0F, 0.0F}},
      vve::Vector<std::uint32_t>{0U, 1U, 2U}, vve::LinearColor{});
   if (!triangle || !engine.renderFrame() || render.lastRenderedWindowCount() != 2U) { return 2; }
   if (!shadowRetirement(renderer)) { std::println("R3 failed: shadow retirement"); return 3; }
   const auto idle_before = renderer.deviceWaitIdleCount();
   const auto uploads_before = renderer.gpuMeshUploadCount();
   const auto mesh = renderer.meshes.begin()->first;

   // Every edit reaches the recording slot of both windows; the other slot retains the previous vertex bytes.
   for (const auto step : std::views::iota(1U, 6U)) {
      const float x = -1.0F + static_cast<float>(step) * 0.1F;
      if (!render.setObjectMeshPositions(*triangle,
          vve::Vector<vve::Vec3>{{x, 0.0F, 0.0F}, {1.0F, 0.0F, 0.0F}, {0.0F, 1.0F, 0.0F}}) ||
          !engine.renderFrame() || render.lastRenderedWindowCount() != 2U) { return 4; }
      for (const auto &target : renderer.targets) {
         const auto &[buffers, dirty] = target.dynamicVertices.at(mesh);
         const auto written = (target.currentFrame + 1U) % renderer.framesInFlight;
         if (std::abs(firstVertexX(buffers[written]) - x) > 1e-6F || dirty.test(written) ||
             !dirty.test(target.currentFrame)) { return 5; }
         if (step > 1U && (buffers[0].buffer == buffers[1].buffer ||
             std::abs(firstVertexX(buffers[target.currentFrame]) - (x - 0.1F)) > 1e-6F)) { return 6; }
      }
   }
   if (renderer.deviceWaitIdleCount() != idle_before || renderer.gpuMeshUploadCount() < uploads_before + 5U) { return 7; }

   const std::filesystem::path directory{VVE_TEST_TMP_DIR};
   std::filesystem::create_directories(directory);
   const auto texture_path = directory / "green.ppm";
   { std::ofstream file{texture_path, std::ios::binary}; file << "P6\n1 1\n255\n"; file.write("\0\xff\0", 3); }
   const auto cuboid = render.addTexturedCuboid(vve::Vec3{-0.5F, -0.5F, -0.5F},
      vve::Vec3{0.5F, 0.5F, 0.5F}, texture_path);
   if (!cuboid || !engine.renderFrame() || render.lastRenderedWindowCount() != 2U || renderer.gpuTextureCount() != 1U) { return 8; }
   // Material growth and a new texture update only the current slot; the other set remains pending.
   for (const auto &target : renderer.targets) {
      const auto written = (target.currentFrame + 1U) % renderer.framesInFlight;
      if (target.materialsDirty.test(written) || !target.materialsDirty.test(target.currentFrame) ||
          target.materialBuffers[written].size < 2U * sizeof(vve::simple::GpuMaterial) ||
          target.materialBuffers[target.currentFrame].size != sizeof(vve::simple::GpuMaterial) ||
          std::ranges::count(target.texturesDirty[written], true) != 0 ||
          std::ranges::count(target.texturesDirty[target.currentFrame], true) != 1) { return 9; }
   }
   if (!render.removeObject(*triangle) || !engine.renderFrame() || render.lastRenderedWindowCount() != 2U ||
       renderer.gpuMeshCount() != 1U || renderer.gpuTextureCount() != 1U || renderer.deviceWaitIdleCount() != idle_before) { return 10; }

   // Release and reuse slot zero while one window retains its earlier descriptor sets.
   if (!render.removeObject(*cuboid)) { return 11; }
   (void)render.purgeUnusedAssets();
   const auto blue_path = directory / "blue.ppm";
   { std::ofstream file{blue_path, std::ios::binary}; file << "P6\n1 1\n255\n"; file.write("\0\0\xff", 3); }
   const auto replacement = render.addTexturedCuboid(vve::Vec3{-0.5F, -0.5F, -0.5F},
      vve::Vec3{0.5F, 0.5F, 0.5F}, blue_path);
   auto &a = renderer.targets.front();
   auto &b = renderer.targets.back();
   const auto old_image = renderer.objectTextures[0].image;
   if (!replacement || renderer.drawFrame(a) != FrameOutcome::presented || renderer.objectTextures[0].image == old_image ||
       renderer.retiredResourceCount() == 0U || std::ranges::count(b.texturesDirty[0], true) != 1 ||
       std::ranges::count(b.texturesDirty[1], true) != 1) { return 12; }
   // Two further frames refresh every slot and collect resources after the fences that cover their last users.
   for (const auto frame : std::views::iota(0U, renderer.framesInFlight)) {
      (void)frame;
      if (!engine.renderFrame() || render.lastRenderedWindowCount() != 2U) { return 13; }
   }
   if (renderer.retiredResourceCount() != 0U || renderer.deviceWaitIdleCount() != idle_before ||
       renderer.gpuTextureCount() != 1U || renderer.forwardPipelineCreateCount() != 1U) { return 14; }
   std::println("R3 runtime idle_before={} idle_after={} mesh_uploads={} textures={} pending={} validation_active={} validation_errors={}",
      idle_before, renderer.deviceWaitIdleCount(), renderer.gpuMeshUploadCount() - uploads_before, renderer.gpuTextureCount(),
      renderer.retiredResourceCount(), renderer.validationActive(), renderer.validationErrorCount());
   // A second live texture grows the shared descriptor array once, including both windows' fenced sets.
   if (renderer.objectTextures.size() != 1U || renderer.textureCapacity() < 2U) { return 16; }
   const auto growth_idle_before = renderer.deviceWaitIdleCount();
   const auto pipelines_before = renderer.forwardPipelineCreateCount();
   const auto first_texture = renderer.objectTextures.front().image;
   const auto second = render.addTexturedCuboid(vve::Vec3{-0.5F, -0.5F, -0.5F},
      vve::Vec3{0.5F, 0.5F, 0.5F}, texture_path);
   if (!second || !engine.renderFrame() || render.lastRenderedWindowCount() != 2U ||
       renderer.objectTextures.size() != 2U || renderer.gpuTextureCount() != 2U ||
       renderer.objectTextures.front().image != first_texture || renderer.uploadedTextureGeneration(1U) == 0U ||
       renderer.deviceWaitIdleCount() != growth_idle_before + 1U ||
       renderer.forwardPipelineCreateCount() != pipelines_before + 1U ||
       renderer.descriptorSetLayout.bindings[vve::simple::shaderBinding::baseColorTextures].descriptorCount != 2U) { return 17; }
   for (const auto &target : renderer.targets) {
      if (target.descriptorSets.textureCapacity != 2U ||
          std::ranges::any_of(target.texturesDirty, [](const auto &dirty) { return dirty.size() != 2U; })) { return 18; }
   }
   // Advancing every frame slot must preserve the grown layout without another idle wait or pipeline rebuild.
   for (const auto frame : std::views::iota(0U, renderer.framesInFlight)) {
      (void)frame;
      if (!engine.renderFrame() || render.lastRenderedWindowCount() != 2U) { return 19; }
   }
   if (renderer.deviceWaitIdleCount() != growth_idle_before + 1U ||
       renderer.forwardPipelineCreateCount() != pipelines_before + 1U || renderer.validationErrorCount() != 0U) { return 20; }
   std::println("R3 texture_growth before=1 after={} windows={} idle_waits={} pipeline_rebuilds={} validation_errors={}",
      renderer.objectTextures.size(), render.lastRenderedWindowCount(), renderer.deviceWaitIdleCount() - growth_idle_before,
      renderer.forwardPipelineCreateCount() - pipelines_before, renderer.validationErrorCount());
   // Unloading source data between submissions must leave the submitted GPU mesh alive until normal fenced retirement.
   const auto imported_path = directory / "runtime_import.obj";
   { std::ofstream file{directory / "runtime_import.mtl"}; file << "newmtl Green\nKd 1 1 1\nmap_Kd green.ppm\n"; }
   { std::ofstream file{imported_path}; file << "mtllib runtime_import.mtl\no Imported\n"
      "v 0 0 0\nv 1 0 0\nv 0 1 0\nvt 0 0\nvt 1 0\nvt 0 1\nusemtl Green\nf 1/1 2/2 3/3\n"; }
   auto &assets = engine.assets();
   const auto catalog_before = assets.catalogEntryCount();
   const auto meshes_before_unload = renderer.gpuMeshCount();
   const auto unload_idle_before = renderer.deviceWaitIdleCount();
   const auto imported_scene = assets.loadScene(imported_path);
   if (!imported_scene) { return 21; }
   const auto source_meshes = assets.sceneMeshes(*imported_scene);
   const auto imported_instance = render.instantiateScene(*imported_scene);
   if (!source_meshes || source_meshes->empty() || !imported_instance || !engine.renderFrame() ||
       renderer.gpuMeshCount() != meshes_before_unload + 1U || renderer.gpuTextureCount() != 2U) { return 22; }
   const auto last_imported_submit = *std::ranges::max_element(b.submitSerials);
   if (!render.removeSceneInstance(*imported_instance) || !assets.unloadScene(*imported_scene) ||
       assets.containsScene(*imported_scene) || assets.meshVertexCount(source_meshes->front()) ||
       assets.catalogEntryCount() != catalog_before || renderer.gpuMeshCount() != meshes_before_unload + 1U ||
       renderer.deviceWaitIdleCount() != unload_idle_before) { return 23; }
   // Advancing only a waits an older fence; b's most recent use is not yet covered, so the removed mesh stays retired.
   if (renderer.drawFrame(a) != FrameOutcome::presented || renderer.gpuMeshCount() != meshes_before_unload || renderer.retiredResourceCount() == 0U ||
       renderer.completedSubmitSerial() >= last_imported_submit || renderer.deviceWaitIdleCount() != unload_idle_before) { return 24; }
   for (const auto frame : std::views::iota(0U, renderer.framesInFlight + 1U)) {
      (void)frame;
      if (!engine.renderFrame() || render.lastRenderedWindowCount() != 2U) { return 25; }
   }
   if (renderer.retiredResourceCount() != 0U || renderer.gpuMeshCount() != meshes_before_unload ||
       renderer.gpuTextureCount() != 2U || !render.objectTransform(*replacement) || !render.objectTransform(*second) ||
       renderer.deviceWaitIdleCount() != unload_idle_before || renderer.validationErrorCount() != 0U) { return 26; }
   std::println("R3 source_unload catalog_entries={} gpu_meshes={} pending={} idle_waits={} validation_errors={}",
      assets.catalogEntryCount(), renderer.gpuMeshCount(), renderer.retiredResourceCount(),
      renderer.deviceWaitIdleCount() - unload_idle_before, renderer.validationErrorCount());
   render.waitIdle();
   engine.gui().shutdownVulkan();
   render.shutdown();
   std::println("R3 cleanup validation_errors={}", renderer.validationErrorCount());
   return renderer.validationErrorCount() == 0U ? 0 : 15;
}
