#include <vulkan/vulkan_core.h>

/// @file
/// @brief Checks fenced scene updates and retirement across two independently advancing windows.
import std;
import VEEngine;
import VEEngine.Simple;
import VEEngine.Simple.Renderer;
import VEEngine.Simple.Scene;
import VEEngine.Simple.Vulkan;

namespace {

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
   if (!renderer.drawFrame(a) || renderer.retiredResourceCount() != 3U ||
       renderer.completedSubmitSerial() >= last_old_submit || b.shadowsDirty.count() != 2U ||
       a.shadowsDirty.count() != 1U || renderer.deviceWaitIdleCount() != idle_before) { return false; }
   // a's earlier fence does not yet cover b's last use of the old arrays.
   if (!renderer.drawFrame(a) || renderer.retiredResourceCount() != 3U ||
       renderer.completedSubmitSerial() >= last_old_submit) { return false; }
   // The following a fence covers b on the shared queue; b adopts the current arrays on each resumed slot.
   if (!renderer.drawFrame(a) || renderer.retiredResourceCount() != 0U ||
       renderer.completedSubmitSerial() < last_old_submit || !renderer.drawFrame(b) || !renderer.drawFrame(b) ||
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

/// @brief Edits vertices, grows materials and replaces textures without any device-wide idle wait or validation error.
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
          target.texturesDirty[written].any() || target.texturesDirty[target.currentFrame].count() != 1U) { return 9; }
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
   if (!replacement || !renderer.drawFrame(a) || renderer.objectTextures[0].image == old_image ||
       renderer.retiredResourceCount() == 0U || b.texturesDirty[0].count() != 1U || b.texturesDirty[1].count() != 1U) { return 12; }
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
   render.waitIdle();
   engine.gui().shutdownVulkan();
   render.shutdown();
   std::println("R3 cleanup validation_errors={}", renderer.validationErrorCount());
   return renderer.validationErrorCount() == 0U ? 0 : 15;
}
