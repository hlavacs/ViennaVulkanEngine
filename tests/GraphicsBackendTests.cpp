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
   if (!hasRendererId(supported, "forward") || !hasRendererId(supported, "deferred") ||
       !hasRendererId(supported, "path_tracing")) {
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
   if (!deferred_alias || deferred_alias->id != "deferred" ||
       deferred_alias->main_kernel != vve::v3::RenderKernelId::deferred_gbuffer) {
      return 4;
   }

   const auto path_tracer_alias = backend.createRenderer("path-tracer");
   if (!path_tracer_alias || path_tracer_alias->id != "path_tracing" ||
       path_tracer_alias->main_kernel != vve::v3::RenderKernelId::path_trace) {
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

   const auto mesh_owner = vve::Handle::fromHash(std::string_view{"tests.graphics_backend.triangle_mesh"});
   const std::array<float, 15> triangle_vertices{
       0.0F, -0.5F, 0.0F, 1.0F, 0.0F,
       0.5F, 0.5F, 0.0F, 0.0F, 1.0F,
       -0.5F, 0.5F, 0.0F, 0.0F, 0.0F};
   const std::array<std::uint32_t, 3> triangle_indices{0U, 1U, 2U};
   const auto vertex_bytes = std::as_bytes(std::span{triangle_vertices});
   const auto index_bytes = std::as_bytes(std::span{triangle_indices});

   const auto create_before_init = backend.createBuffer(mesh_owner, vve::v3::ResourceKind::mesh,
                                                        vve::v3::GpuBufferUsage::vertex, vertex_bytes, 1);
   if (create_before_init || create_before_init.error() != vve::Error::not_initialized) {
      return 8;
   }

   const auto init = backend.init();
   if (!init) {
      return 9;
   }

   const auto empty_upload = backend.createBuffer(mesh_owner, vve::v3::ResourceKind::mesh,
                                                 vve::v3::GpuBufferUsage::vertex,
                                                 std::span<const std::byte>{}, 1);
   if (empty_upload || empty_upload.error() != vve::Error::invalid_argument) {
      return 10;
   }

   const auto vertex_buffer = backend.createBuffer(mesh_owner, vve::v3::ResourceKind::mesh,
                                                   vve::v3::GpuBufferUsage::vertex, vertex_bytes, 1);
   if (!vertex_buffer || !vertex_buffer->handle.value.isValid() || vertex_buffer->owner != mesh_owner ||
       vertex_buffer->owner_kind != vve::v3::ResourceKind::mesh ||
       vertex_buffer->usage != vve::v3::GpuBufferUsage::vertex ||
       vertex_buffer->byte_size != vertex_bytes.size() || vertex_buffer->generation != 1 ||
       !vertex_buffer->buffer_created || !vertex_buffer->memory_bound) {
      return 11;
   }

   const auto duplicate_vertex_buffer = backend.createBuffer(mesh_owner, vve::v3::ResourceKind::mesh,
                                                            vve::v3::GpuBufferUsage::vertex, vertex_bytes, 1);
   if (!duplicate_vertex_buffer || duplicate_vertex_buffer->handle.value != vertex_buffer->handle.value) {
      return 12;
   }

   const auto vertex_lookup = backend.bufferResources(vertex_buffer->handle);
   if (!vertex_lookup || !vertex_lookup->has_value() ||
       (*vertex_lookup)->handle.value != vertex_buffer->handle.value ||
       (*vertex_lookup)->byte_size != vertex_buffer->byte_size) {
      return 13;
   }

   const auto index_buffer = backend.createBuffer(mesh_owner, vve::v3::ResourceKind::mesh,
                                                  vve::v3::GpuBufferUsage::index, index_bytes, 1);
   if (!index_buffer || !index_buffer->handle.value.isValid() ||
       index_buffer->usage != vve::v3::GpuBufferUsage::index ||
       index_buffer->byte_size != index_bytes.size() || !index_buffer->buffer_created ||
       !index_buffer->memory_bound) {
      return 14;
   }

   const auto destroy_vertex = backend.destroyBuffer(vertex_buffer->handle);
   if (!destroy_vertex) {
      return 15;
   }

   const auto missing_vertex = backend.bufferResources(vertex_buffer->handle);
   if (!missing_vertex || missing_vertex->has_value()) {
      return 16;
   }

   const auto destroy_index = backend.destroyBuffer(index_buffer->handle);
   if (!destroy_index) {
      return 17;
   }

   return 0;
}
