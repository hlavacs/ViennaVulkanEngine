/**
 * @file
 * @brief CPU coverage of device-selected scene texture limits, capacity changes and released-slot reuse.
 *
 * Functional objects:
 * - main checks scene capacity without loading Vulkan or allocating GPU textures.
 */
import std;
import VVEngine.Simple.Types;
import VVEngine.Simple.RenderResources;
import VVEngine.Simple.Scene;

/// @brief Checks capacity exhaustion, live-slot preservation, shrinking and clearing with one-pixel embedded sources.
int main() {
   vve::simple::RenderScene scene;
   auto image = std::make_shared<vve::simple::EmbeddedImage>();
   image->extent = {1U, 1U};
   image->bytes = {std::byte{255}, std::byte{0}, std::byte{0}, std::byte{255}};
   const auto acquire = [&](std::size_t index) {
      return scene.acquireTexture("capacity-" + std::to_string(index) + "#*0",
         vve::simple::MaterialTextureSemantic::base_color, image);
   };
   // Scene building before renderer creation must not retain the former fixed 64-slot limit.
   for (const auto index : std::views::iota(0U, 65U)) {
      const auto slot = acquire(index);
      if (!slot || *slot != index) { return 1; }
   }
   scene.clear();
   constexpr auto capacity = 4U;
   if (!scene.setTextureCapacity(capacity)) { return 2; }
   std::array<vve::simple::RenderMaterialHandle, capacity> materials{};
   for (const auto index : std::views::iota(0U, capacity)) {
      const auto slot = acquire(index);
      if (!slot || *slot != index) { return 3; }
      materials[index] = scene.addMaterial({.base_color_texture_index = *slot});
   }
   const auto decoded = scene.textureDecodeCount();
   const auto overflow = acquire(capacity);
   if (overflow || overflow.error() != vve::Error::capacity_exceeded || scene.textureDecodeCount() != decoded ||
      acquire(capacity - 1U) != capacity - 1U) { return 4; }
   // A hole at slot zero does not make the live high slot fit into a smaller descriptor array.
   scene.eraseMaterial(materials[0]);
   if (scene.releaseUnusedTextures() != 1U || scene.textureCount() != capacity - 1U) { return 5; }
   const auto shrink = scene.setTextureCapacity(capacity - 1U);
   if (shrink || shrink.error() != vve::Error::capacity_exceeded || scene.findTexture(capacity - 1U) == nullptr) { return 6; }
   const auto reused = acquire(capacity);
   if (!reused || *reused != 0U || scene.textureCount() != capacity) { return 7; }
   materials[0] = scene.addMaterial({.base_color_texture_index = *reused});
   // Replacing the high slot proves that a failed shrink kept the previous capacity.
   scene.eraseMaterial(materials.back());
   if (scene.releaseUnusedTextures() != 1U) { return 8; }
   const auto replacement = acquire(capacity + 1U);
   if (!replacement || *replacement != capacity - 1U) { return 9; }
   materials.back() = scene.addMaterial({.base_color_texture_index = *replacement});
   scene.eraseMaterial(materials.back());
   if (scene.releaseUnusedTextures() != 1U || !scene.setTextureCapacity(capacity - 1U)) { return 10; }
   const auto reduced_overflow = acquire(capacity + 2U);
   if (reduced_overflow || reduced_overflow.error() != vve::Error::capacity_exceeded) { return 11; }
   // Clearing textures preserves the renderer's selected bound rather than restoring a default limit.
   scene.clear();
   for (const auto index : std::views::iota(0U, capacity - 1U)) {
      if (!acquire(index)) { return 12; }
   }
   const auto cleared_overflow = acquire(capacity);
   if (cleared_overflow || cleared_overflow.error() != vve::Error::capacity_exceeded) { return 13; }
   scene.clear();
   if (!scene.setTextureCapacity(0U)) { return 14; }
   const auto exhausted = acquire(0U);
   if (exhausted || exhausted.error() != vve::Error::capacity_exceeded) { return 15; }
   std::println("[RenderTextureCapacityTests] default_slots=65 capacity=4 overflow=capacity_exceeded reuse=0 failed_shrink_preserved=true reduced_capacity=3 clear_preserved=true zero_capacity=true");
   return 0;
}
