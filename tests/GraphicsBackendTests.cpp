#include <array>
#include <cstddef>
#include <cstdint>
#include <algorithm>
#include <span>
#include <string_view>
#include <vector>

import std;
import VEEngine;
import VEEngine.V3;

/**
 * @file
 * @brief Regression tests for backend renderer-id selection.
 */
namespace {

   /// @brief Returns whether a renderer list contains a canonical id.
   [[nodiscard]] bool hasRendererId(const std::vector<vve::v3::RendererDesc> &renderers, std::string_view id) {
      return std::ranges::any_of(renderers, [id](const vve::v3::RendererDesc &renderer) {
         return renderer.id == id;
      });
   }

} // namespace

/**
 * @brief Executes the graphics-backend renderer registry regression tests.
 * @return Process exit code expected by the lightweight test runner.
 */
int main() {
   vve::v3::GraphicsBackend backend{};

   const auto supported = backend.supportedRenderers();
   if (!hasRendererId(supported, "forward") || hasRendererId(supported, "deferred") ||
       hasRendererId(supported, "path_tracing")) {
      return 1;
   }

   const auto forward = backend.createRenderer("forward");
   if (!forward || forward->id != "forward" || forward->api != vve::GraphicsApi::vulkan ||
       forward->kind != vve::RendererKind::forward_renderer ||
       forward->main_kernel != vve::v3::RenderKernelId::forward_opaque) {
      return 2;
   }

   const auto forward_alias = backend.createRenderer(" Forward-Renderer ");
   if (!forward_alias || forward_alias->id != forward->id || forward_alias->handle.value != forward->handle.value) {
      return 3;
   }

   const auto deferred_alias = backend.createRenderer("deferred-renderer");
   if (deferred_alias || deferred_alias.error() != vve::Error::unsupported_version) {
      return 4;
   }

   const auto path_tracer_alias = backend.createRenderer("path-tracer");
   if (path_tracer_alias || path_tracer_alias.error() != vve::Error::unsupported_version) {
      return 5;
   }

   const auto unsupported = backend.createRenderer("software");
   if (unsupported || unsupported.error() != vve::Error::invalid_argument) {
      return 6;
   }

   const auto begin_before_init = backend.beginFrame(vve::v3::FrameContext{});
   if (begin_before_init || begin_before_init.error() != vve::Error::not_initialized) {
      return 7;
   }

   const auto packet_record_before_init = backend.recordWindowFrame(
       vve::v3::SwapchainHandle{.value = vve::Handle::fromHash(std::string_view{"tests.graphics_backend.swapchain"})},
       vve::v3::WindowDrawPacketList{});
   if (packet_record_before_init || packet_record_before_init.error() != vve::Error::not_initialized) {
      return 8;
   }

   const auto mesh_owner = vve::Handle::fromHash(std::string_view{"tests.graphics_backend.triangle_mesh"});
   const auto texture_owner = vve::Handle::fromHash(std::string_view{"tests.graphics_backend.texture"});
   const std::array<float, 15> triangle_vertices{
       0.0F, -0.5F, 0.0F, 1.0F, 0.0F,
       0.5F, 0.5F, 0.0F, 0.0F, 1.0F,
       -0.5F, 0.5F, 0.0F, 0.0F, 0.0F};
   const std::array<std::uint32_t, 3> triangle_indices{0U, 1U, 2U};
   const std::array<std::uint8_t, 16> texture_pixels{
       255U, 0U, 0U, 255U,
       0U, 255U, 0U, 255U,
       0U, 0U, 255U, 255U,
       255U, 255U, 255U, 255U};
   const auto vertex_bytes = std::as_bytes(std::span{triangle_vertices});
   const auto index_bytes = std::as_bytes(std::span{triangle_indices});
   const auto texture_bytes = std::as_bytes(std::span{texture_pixels});

   const auto create_before_init = backend.createBuffer(mesh_owner, vve::v3::ResourceKind::mesh,
                                                        vve::v3::GpuBufferUsage::vertex, vertex_bytes, 1);
   if (create_before_init || create_before_init.error() != vve::Error::not_initialized) {
      return 9;
   }

   const auto image_before_init = backend.createSampledImage(
       texture_owner, vve::v3::ResourceKind::texture, vve::v3::GpuImageFormat::rgba8_srgb, 2, 2, texture_bytes, 1);
   if (image_before_init || image_before_init.error() != vve::Error::not_initialized) {
      return 10;
   }

   const auto init = backend.init();
   if (!init) {
      return 11;
   }

   const auto empty_upload = backend.createBuffer(mesh_owner, vve::v3::ResourceKind::mesh,
                                                 vve::v3::GpuBufferUsage::vertex,
                                                 std::span<const std::byte>{}, 1);
   if (empty_upload || empty_upload.error() != vve::Error::invalid_argument) {
      return 12;
   }

   const auto vertex_buffer = backend.createBuffer(mesh_owner, vve::v3::ResourceKind::mesh,
                                                   vve::v3::GpuBufferUsage::vertex, vertex_bytes, 1);
   if (!vertex_buffer || !vertex_buffer->handle.value.isValid() || vertex_buffer->owner != mesh_owner ||
       vertex_buffer->owner_kind != vve::v3::ResourceKind::mesh ||
       vertex_buffer->usage != vve::v3::GpuBufferUsage::vertex ||
       vertex_buffer->byte_size != vertex_bytes.size() || vertex_buffer->generation != 1 ||
       !vertex_buffer->buffer_created || !vertex_buffer->memory_bound) {
      return 13;
   }

   const auto duplicate_vertex_buffer = backend.createBuffer(mesh_owner, vve::v3::ResourceKind::mesh,
                                                            vve::v3::GpuBufferUsage::vertex, vertex_bytes, 1);
   if (!duplicate_vertex_buffer || duplicate_vertex_buffer->handle.value != vertex_buffer->handle.value) {
      return 14;
   }

   const auto vertex_lookup = backend.bufferResources(vertex_buffer->handle);
   if (!vertex_lookup || !vertex_lookup->has_value() ||
       (*vertex_lookup)->handle.value != vertex_buffer->handle.value ||
       (*vertex_lookup)->byte_size != vertex_buffer->byte_size) {
      return 15;
   }

   const auto index_buffer = backend.createBuffer(mesh_owner, vve::v3::ResourceKind::mesh,
                                                  vve::v3::GpuBufferUsage::index, index_bytes, 1);
   if (!index_buffer || !index_buffer->handle.value.isValid() ||
       index_buffer->usage != vve::v3::GpuBufferUsage::index ||
       index_buffer->byte_size != index_bytes.size() || !index_buffer->buffer_created ||
       !index_buffer->memory_bound) {
      return 16;
   }

   const auto texture_image = backend.createSampledImage(
       texture_owner, vve::v3::ResourceKind::texture, vve::v3::GpuImageFormat::rgba8_srgb, 2, 2, texture_bytes, 1);
   if (!texture_image || !texture_image->image.value.isValid() || !texture_image->sampler.value.isValid() ||
       texture_image->texture.value != texture_owner || texture_image->format != vve::v3::GpuImageFormat::rgba8_srgb ||
       texture_image->width != 2 || texture_image->height != 2 || texture_image->generation != 1 ||
       texture_image->mip_levels != 2 ||
       !texture_image->image_created || !texture_image->image_view_created ||
       !texture_image->sampler_created || !texture_image->resident) {
      return 17;
   }

   const auto duplicate_texture_image = backend.createSampledImage(
       texture_owner, vve::v3::ResourceKind::texture, vve::v3::GpuImageFormat::rgba8_srgb, 2, 2, texture_bytes, 1);
   if (!duplicate_texture_image || duplicate_texture_image->image.value != texture_image->image.value ||
       duplicate_texture_image->sampler.value != texture_image->sampler.value) {
      return 18;
   }

   const auto texture_lookup = backend.imageResources(texture_image->image);
   if (!texture_lookup || !texture_lookup->has_value() ||
       (*texture_lookup)->image.value != texture_image->image.value ||
       (*texture_lookup)->width != texture_image->width ||
       (*texture_lookup)->height != texture_image->height) {
      return 19;
   }

   const auto destroy_texture = backend.destroyImage(texture_image->image);
   if (!destroy_texture) {
      return 20;
   }

   const auto missing_texture = backend.imageResources(texture_image->image);
   if (!missing_texture || missing_texture->has_value()) {
      return 21;
   }

   const auto destroy_vertex = backend.destroyBuffer(vertex_buffer->handle);
   if (!destroy_vertex) {
      return 22;
   }

   const auto missing_vertex = backend.bufferResources(vertex_buffer->handle);
   if (!missing_vertex || missing_vertex->has_value()) {
      return 23;
   }

   const auto destroy_index = backend.destroyBuffer(index_buffer->handle);
   if (!destroy_index) {
      return 24;
   }

   return 0;
}
