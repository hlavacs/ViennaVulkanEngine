module;

#include "FacadeMacros.hpp"

module VEEngine.V3;
import std;
import :Internal;

/**
 * @file
 * @brief v3 render-system implementation.
 *
 * This file assembles the per-window render graph and provides the render
 * tasks that bridge scene data into backend-facing render work.
 */
namespace vve::v3 {

   /**
    * @brief Builds the optional shadow pass for the selected shadowing mode.
    * @param shadow Requested shadowing strategy.
    * @return Render-pass description when a shadow pass is required.
    */
   [[nodiscard]] std::optional<RenderPassDesc> buildShadowPass(vve::ShadowKind shadow) {
      switch (shadow) {
      case vve::ShadowKind::none:
         return std::nullopt;
      case vve::ShadowKind::shadow_map:
         return RenderPassDesc{.handle = RenderPassHandle{detail::makeStableHandle("render.shadow_map")},
                               .kernel = RenderKernelId::shadow_map,
                               .debug_name = "Shadow Map"};
      case vve::ShadowKind::ray_traced:
         return RenderPassDesc{.handle = RenderPassHandle{detail::makeStableHandle("render.ray_traced_shadows")},
                               .kernel = RenderKernelId::ray_traced_shadows,
                               .debug_name = "Ray Traced Shadows"};
      }

      return std::nullopt;
   }

   /// @brief Returns whether a render graph contains the expected renderer-specific main pass.
   [[nodiscard]] bool graphContainsKernel(const RenderGraph &graph, RenderKernelId kernel) {
      return std::ranges::any_of(graph.passes, [kernel](const RenderPassDesc &pass) {
         return pass.kernel == kernel;
      });
   }

   /// @brief Returns the pass that owns draw packets for a renderer kernel.
   [[nodiscard]] const RenderPassDesc *findKernelPass(const RenderGraph &graph, RenderKernelId kernel) {
      const auto pass = std::ranges::find_if(graph.passes, [kernel](const RenderPassDesc &candidate) {
         return candidate.kernel == kernel;
      });

      return pass == graph.passes.end() ? nullptr : std::addressof(*pass);
   }

   /// @brief Looks up a scene node by handle.
   [[nodiscard]] const SceneNodeDesc *findNode(const SceneData &scene, SceneNodeHandle node) {
      const auto index = scene.node_indices.find(node.value.value());
      if (index == scene.node_indices.end() || index->second >= scene.nodes.size()) {
         return nullptr;
      }

      return std::addressof(scene.nodes[index->second]);
   }

   /// @brief Looks up imported mesh data by handle.
   [[nodiscard]] const ImportedMesh *findMesh(const SceneData &scene, MeshHandle mesh) {
      const auto index = scene.mesh_indices.find(mesh.value.value());
      if (index == scene.mesh_indices.end() || index->second >= scene.meshes.size()) {
         return nullptr;
      }

      return std::addressof(scene.meshes[index->second]);
   }

   /// @brief Looks up uploaded mesh buffers by imported mesh handle.
   [[nodiscard]] const GpuMeshResources *findGpuMesh(const SceneData &scene, MeshHandle mesh) {
      const auto index = scene.gpu_mesh_indices.find(mesh.value.value());
      if (index == scene.gpu_mesh_indices.end() || index->second >= scene.gpu_meshes.size()) {
         return nullptr;
      }

      return std::addressof(scene.gpu_meshes[index->second]);
   }

   /// @brief Looks up material draw-state flags by material handle.
   [[nodiscard]] const ImportedMaterial *findMaterial(const SceneData &scene, MaterialHandle material) {
      if (!material.value.isValid()) {
         return nullptr;
      }

      const auto index = scene.material_indices.find(material.value.value());
      if (index == scene.material_indices.end() || index->second >= scene.materials.size()) {
         return nullptr;
      }

      return std::addressof(scene.materials[index->second]);
   }

   /// @brief Looks up the runtime material index used by later GPU material tables.
   [[nodiscard]] std::optional<std::uint32_t> findMaterialIndex(const SceneData &scene, MaterialHandle material) {
      if (!material.value.isValid()) {
         return std::nullopt;
      }

      const auto index = scene.material_indices.find(material.value.value());
      if (index == scene.material_indices.end() || index->second >= scene.materials.size() ||
          index->second > static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max())) {
         return std::nullopt;
      }

      return static_cast<std::uint32_t>(index->second);
   }

   /// @brief Looks up uploaded material resources by imported material handle.
   [[nodiscard]] const GpuMaterialResources *findGpuMaterial(const SceneData &scene, MaterialHandle material) {
      if (!material.value.isValid()) {
         return nullptr;
      }

      const auto index = scene.gpu_material_indices.find(material.value.value());
      if (index == scene.gpu_material_indices.end() || index->second >= scene.gpu_materials.size()) {
         return nullptr;
      }

      return std::addressof(scene.gpu_materials[index->second]);
   }

   /// @brief Copies imported material scalars into the backend-neutral draw payload.
   [[nodiscard]] DrawMaterialConstants drawMaterialConstants(const ImportedMaterial *material) {
      if (material == nullptr) {
         return {};
      }

      return DrawMaterialConstants{.base_color_factor = material->base_color_factor,
                                   .emissive_factor = material->emissive_factor,
                                   .roughness_factor = material->roughness_factor,
                                   .metallic_factor = material->metallic_factor,
                                   .normal_scale = material->normal_scale,
                                   .alpha_cutoff = material->alpha_cutoff};
   }

   /// @brief Selects the forward renderer pipeline variant required by material raster state.
   [[nodiscard]] GraphicsPipelineVariant forwardPipelineVariant(bool double_sided, bool alpha_blend) {
      if (double_sided && alpha_blend) {
         return GraphicsPipelineVariant::double_sided_alpha_blend;
      }
      if (double_sided) {
         return GraphicsPipelineVariant::double_sided;
      }
      if (alpha_blend) {
         return GraphicsPipelineVariant::alpha_blend;
      }

      return GraphicsPipelineVariant::opaque;
   }

   /// @brief Returns whether a forward variant requires transparent draw ordering.
   [[nodiscard]] bool isTransparentVariant(GraphicsPipelineVariant variant) {
      return variant == GraphicsPipelineVariant::alpha_blend ||
             variant == GraphicsPipelineVariant::double_sided_alpha_blend;
   }

   /// @brief Maps variants to deterministic coarse bins before any depth ordering is applied.
   [[nodiscard]] std::uint32_t pipelineVariantSortBucket(GraphicsPipelineVariant variant) {
      switch (variant) {
      case GraphicsPipelineVariant::opaque:
         return 0;
      case GraphicsPipelineVariant::double_sided:
         return 1;
      case GraphicsPipelineVariant::alpha_blend:
         return 2;
      case GraphicsPipelineVariant::double_sided_alpha_blend:
         return 3;
      }

      return 0;
   }

   /// @brief Extracts the world-space translation from the object transform.
   [[nodiscard]] vve::math::Vec3 worldTranslation(const vve::math::Mat4 &transform) {
      return vve::math::Vec3(transform[3][0], transform[3][1], transform[3][2]);
   }

   /// @brief Estimates depth along the current default forward-renderer view.
   [[nodiscard]] vve::math::Scalar defaultViewDepth(const vve::math::Mat4 &world_transform) {
      constexpr vve::math::Scalar default_camera_z = static_cast<vve::math::Scalar>(6.0);
      return default_camera_z - worldTranslation(world_transform).z;
   }

   /// @brief Orders packets for correct opaque/transparent rendering and stable descriptor keys.
   void sortDrawPackets(Vector<DrawPacket> &packets) {
      std::stable_sort(packets.begin(), packets.end(), [](const DrawPacket &lhs, const DrawPacket &rhs) {
         const bool lhs_transparent = isTransparentVariant(lhs.pipeline_variant);
         const bool rhs_transparent = isTransparentVariant(rhs.pipeline_variant);
         if (lhs_transparent != rhs_transparent) {
            return !lhs_transparent;
         }
         if (!lhs_transparent && lhs.sort_bucket != rhs.sort_bucket) {
            return lhs.sort_bucket < rhs.sort_bucket;
         }
         if (lhs_transparent && lhs.camera_depth != rhs.camera_depth) {
            return lhs.camera_depth > rhs.camera_depth;
         }
         if (lhs_transparent && lhs.sort_bucket != rhs.sort_bucket) {
            return lhs.sort_bucket < rhs.sort_bucket;
         }

         return lhs.draw_index < rhs.draw_index;
      });

      for (std::size_t index = 0; index < packets.size(); ++index) {
         packets[index].draw_index = static_cast<std::uint32_t>(index);
      }
   }

   /// @brief Returns the prepared graphics pipeline handle for a forward material-state variant.
   [[nodiscard]] GraphicsPipelineHandle graphicsPipelineForVariant(const RendererPipelineBinding &binding,
                                                                   GraphicsPipelineVariant variant) {
      switch (variant) {
      case GraphicsPipelineVariant::opaque:
         return binding.graphics_pipeline;
      case GraphicsPipelineVariant::double_sided:
         return binding.double_sided_graphics_pipeline;
      case GraphicsPipelineVariant::alpha_blend:
         return binding.alpha_blend_graphics_pipeline;
      case GraphicsPipelineVariant::double_sided_alpha_blend:
         return binding.double_sided_alpha_blend_graphics_pipeline;
      }

      return {};
   }

   /// @brief Stores a prepared graphics pipeline handle on the matching forward material-state slot.
   void setGraphicsPipelineForVariant(RendererPipelineBinding &binding, GraphicsPipelineVariant variant,
                                      GraphicsPipelineHandle handle) {
      switch (variant) {
      case GraphicsPipelineVariant::opaque:
         binding.graphics_pipeline = handle;
         break;
      case GraphicsPipelineVariant::double_sided:
         binding.double_sided_graphics_pipeline = handle;
         break;
      case GraphicsPipelineVariant::alpha_blend:
         binding.alpha_blend_graphics_pipeline = handle;
         break;
      case GraphicsPipelineVariant::double_sided_alpha_blend:
         binding.double_sided_alpha_blend_graphics_pipeline = handle;
         break;
      }
   }

   /// @brief Builds renderer-specific graphics pipeline state from a validated renderer binding.
   [[nodiscard]] std::expected<GraphicsPipelineDesc, vve::Error>
   graphicsPipelineDescForBinding(const RendererPipelineBinding &binding, GraphicsPipelineVariant variant) {
      if (!binding.ready_for_pipeline_creation || !binding.renderer.value.isValid() ||
          !binding.backend_resources.value.isValid() || binding.renderer_id.empty()) {
         return std::unexpected(vve::Error::invalid_argument);
      }

      GraphicsPipelineDesc desc{.renderer = binding.renderer,
                                .renderer_id = binding.renderer_id,
                                .backend_resources = binding.backend_resources,
                                .main_kernel = binding.main_kernel,
                                .variant = variant};

      if (binding.renderer_id == "forward" && binding.main_kernel == RenderKernelId::forward_opaque) {
         desc.topology = GraphicsPrimitiveTopology::triangle_list;
         desc.cull_mode =
             variant == GraphicsPipelineVariant::double_sided ||
                     variant == GraphicsPipelineVariant::double_sided_alpha_blend
                 ? GraphicsCullMode::none
                 : GraphicsCullMode::back;
         desc.front_face = GraphicsFrontFace::counter_clockwise;
         desc.depth_test_enabled = true;
         desc.depth_write_enabled =
             variant != GraphicsPipelineVariant::alpha_blend &&
             variant != GraphicsPipelineVariant::double_sided_alpha_blend;
         desc.depth_compare = GraphicsDepthCompareOp::less_equal;
         desc.blending_enabled =
             variant == GraphicsPipelineVariant::alpha_blend ||
             variant == GraphicsPipelineVariant::double_sided_alpha_blend;
         desc.color_format = binding.color_format;
         desc.depth_format = binding.depth_format;
         desc.color_attachment_count = 1;
         desc.vertex_binding_count = 1;
         desc.vertex_attribute_count = 5;
         return desc;
      }

      if ((binding.renderer_id == "deferred" && binding.main_kernel == RenderKernelId::deferred_gbuffer) ||
          (binding.renderer_id == "path_tracing" && binding.main_kernel == RenderKernelId::path_trace)) {
         return std::unexpected(vve::Error::unsupported_version);
      }

      return std::unexpected(vve::Error::invalid_argument);
   }

   /**
    * @brief Concrete render-system implementation used by v3.
    *
    * The implementation owns renderer selection policy and translates that
    * policy into both a static render graph and the frame tasks that execute
    * the per-window render pipeline.
    */
   class DefaultRenderSystemImplementation {
   public:
      /**
       * @brief Creates the render system for the selected renderer configuration.
       * @param renderer Requested renderer family.
       * @param shadow Requested shadow mode.
       * @param graphics_backend Active graphics backend used for diagnostics and future backend work.
       * @param imgui_enabled Whether an ImGui render pass should be appended.
       */
      DefaultRenderSystemImplementation(vve::RendererKind renderer, vve::ShadowKind shadow,
                                        GraphicsBackend &graphics_backend, bool imgui_enabled)
          : shadow_(shadow), graphics_backend_name_(graphics_backend.name()), imgui_enabled_(imgui_enabled) {
         (void)renderer;
      }

      /// @brief Returns the subsystem name for diagnostics.
      [[nodiscard]] std::string_view name() const noexcept { return "RenderSystem"; }

      /**
       * @brief Builds the static render graph for one window.
       * @param window Window receiving the render pipeline.
       * @param renderer Backend renderer selected for the window.
       * @return Immutable render graph used by later scheduling and graph-dump code.
       */
      [[nodiscard]] RenderGraph buildStaticGraph(WindowHandle window, const RendererDesc &renderer) {
         RenderGraph graph{};
         const auto window_salt = window.value.value();

         // Shadow work, when enabled, becomes the first prerequisite of the
         // main pass so later passes can remain renderer-agnostic.
         if (const auto shadow_pass = buildShadowPass(shadow_)) {
            graph.passes.push_back(*shadow_pass);
         }

         const auto main_pass = RenderPassHandle{detail::makeStableHandle("render.main", window_salt)};
         Vector<RenderPassHandle> main_dependencies{};
         if (!graph.passes.empty()) {
            main_dependencies.push_back(graph.passes.front().handle);
         }
         // The main pass represents the renderer-specific primary shading stage.
         graph.passes.push_back(RenderPassDesc{.handle = main_pass,
                                               .kernel = renderer.main_kernel,
                                               .depends_on = std::move(main_dependencies),
                                               .debug_name = renderer.display_name});

         // Post-processing remains explicit in the graph so graph dumps and
         // later backend integration can reason about ordering cleanly.
         const auto post_process_pass = RenderPassHandle{detail::makeStableHandle("render.post_process", window_salt)};
         graph.passes.push_back(RenderPassDesc{.handle = post_process_pass,
                                               .kernel = RenderKernelId::post_process,
                                               .depends_on = {main_pass},
                                               .debug_name = "Post Processing"});

         const auto post_post_process_pass =
             RenderPassHandle{detail::makeStableHandle("render.post_post_process", window_salt)};
         graph.passes.push_back(RenderPassDesc{.handle = post_post_process_pass,
                                               .kernel = RenderKernelId::post_post_process,
                                               .depends_on = {post_process_pass},
                                               .debug_name = "Post Post Processing"});

         if (imgui_enabled_) {
            // GUI rendering is modeled as an optional trailing pass so it can
            // depend on the fully composed scene image.
            graph.passes.push_back(
                RenderPassDesc{.handle = RenderPassHandle{detail::makeStableHandle("render.imgui", window_salt)},
                               .kernel = RenderKernelId::imgui,
                               .depends_on = {post_post_process_pass},
                               .debug_name = std::string("ImGui (") + graphics_backend_name_ + ")"});
         }

         return graph;
      }

      /**
       * @brief Binds backend resources to the renderer instance selected for one window.
       * @param pipeline Fully assembled window pipeline with backend resources.
       * @return Renderer-side binding state ready for later graphics-pipeline creation.
       */
      [[nodiscard]] std::expected<RendererPipelineBinding, vve::Error>
      bindPipelineResources(const WindowRenderPipeline &pipeline) {
         if (auto validation = validatePipelineBinding(pipeline); !validation) {
            return std::unexpected(validation.error());
         }

         RendererPipelineBinding binding{.window = pipeline.window,
                                         .window_id = pipeline.window_id,
                                         .renderer = pipeline.renderer.handle,
                                         .renderer_id = pipeline.renderer.id,
                                         .shader_program = pipeline.shader_program,
                                         .backend_resources = pipeline.backend_resources.handle,
                                         .main_kernel = pipeline.renderer.main_kernel,
                                         .color_format = pipeline.swapchain.color_attachment_format,
                                         .depth_format = pipeline.swapchain.depth_attachment_format,
                                         .shader_stage_count = pipeline.pipeline_layout.shader_stages.size(),
                                         .descriptor_set_layout_count =
                                             pipeline.backend_resources.descriptor_set_layout_count,
                                         .ready_for_pipeline_creation =
                                             pipeline.backend_resources.pipeline_layout_created};
         renderer_bindings_[pipeline.window.value.value()] = binding;
         return binding;
      }

      /// @brief Returns the renderer binding installed for a window, when present.
      [[nodiscard]] std::expected<std::optional<RendererPipelineBinding>, vve::Error>
      rendererPipeline(WindowHandle window) const {
         if (!window.value.isValid()) {
            return std::unexpected(vve::Error::invalid_argument);
         }

         const auto binding = renderer_bindings_.find(window.value.value());
         if (binding == renderer_bindings_.end()) {
            return std::optional<RendererPipelineBinding>{};
         }

         return binding->second;
      }

      /**
       * @brief Requests graphics pipeline preparation for an already bound renderer instance.
       * @param binding Renderer binding produced by bindPipelineResources.
       * @return Backend graphics-pipeline preparation summary.
       */
      [[nodiscard]] std::expected<GraphicsPipelineResources, vve::Error>
      createGraphicsPipeline(GraphicsBackend &graphics_backend, const RendererPipelineBinding &binding) {
         const auto stored_binding = renderer_bindings_.find(binding.window.value.value());
         if (stored_binding == renderer_bindings_.end() ||
             stored_binding->second.backend_resources.value != binding.backend_resources.value) {
            return std::unexpected(vve::Error::invalid_argument);
         }

         constexpr std::array variants{GraphicsPipelineVariant::opaque,
                                       GraphicsPipelineVariant::double_sided,
                                       GraphicsPipelineVariant::alpha_blend,
                                       GraphicsPipelineVariant::double_sided_alpha_blend};
         std::optional<GraphicsPipelineResources> primary_pipeline{};
         bool all_variants_ready = true;
         for (const auto variant : variants) {
            const auto desc = graphicsPipelineDescForBinding(binding, variant);
            if (!desc) {
               return std::unexpected(desc.error());
            }

            const auto graphics_pipeline = graphics_backend.createGraphicsPipelineResources(binding, *desc);
            if (!graphics_pipeline) {
               return std::unexpected(graphics_pipeline.error());
            }

            setGraphicsPipelineForVariant(stored_binding->second, variant, graphics_pipeline->handle);
            all_variants_ready = all_variants_ready && graphics_pipeline->pipeline_cache_ready;
            if (variant == GraphicsPipelineVariant::opaque) {
               primary_pipeline = *graphics_pipeline;
            }
         }

         if (!primary_pipeline.has_value()) {
            return std::unexpected(vve::Error::internal_error);
         }

         stored_binding->second.graphics_pipeline_ready = all_variants_ready;
         return *primary_pipeline;
      }

      /// @brief Performs placeholder GPU visibility work for one window graph.
      [[nodiscard]] std::expected<void, vve::Error> cullVisibilityGpu(const FrameContext &, const SceneData &,
                                                                      WindowHandle, const RenderGraph &) {
         return {};
      }

      /// @brief Builds backend-neutral draw packets from uploaded mesh resources.
      [[nodiscard]] std::expected<void, vve::Error> buildDrawPackets(const FrameContext &frame_context,
                                                                     const SceneData &scene, WindowHandle window,
                                                                     const RenderGraph &render_graph) {
         if (!window.value.isValid()) {
            return std::unexpected(vve::Error::invalid_argument);
         }

         const auto binding = renderer_bindings_.find(window.value.value());
         if (binding == renderer_bindings_.end()) {
            return std::unexpected(vve::Error::invalid_argument);
         }

         const auto *pass = findKernelPass(render_graph, binding->second.main_kernel);
         if (pass == nullptr) {
            return std::unexpected(vve::Error::invalid_argument);
         }

         WindowDrawPacketList packets{.window = window, .frame_index = frame_context.frame_index};
         for (const auto &instance : scene.mesh_instances) {
            const auto *node = findNode(scene, instance.node);
            const auto *mesh = findMesh(scene, instance.mesh);
            const auto *gpu_mesh = findGpuMesh(scene, instance.mesh);
            if (node == nullptr || mesh == nullptr) {
               return std::unexpected(vve::Error::invalid_argument);
            }
            if (gpu_mesh == nullptr || !gpu_mesh->resident || !gpu_mesh->vertex_buffer.value.isValid() ||
                !gpu_mesh->index_buffer.value.isValid()) {
               continue;
            }

            for (const auto &submesh : mesh->submeshes) {
               if (submesh.index_count == 0) {
                  continue;
               }
               if (submesh.index_offset > gpu_mesh->index_count ||
                   submesh.index_count > gpu_mesh->index_count - submesh.index_offset) {
                  return std::unexpected(vve::Error::invalid_argument);
               }

               const auto material = instance.material_override.value_or(submesh.material);
               const auto *material_data = findMaterial(scene, material);
               const auto material_index = findMaterialIndex(scene, material);
               const auto *gpu_material = findGpuMaterial(scene, material);
               if (packets.packets.size() >
                   static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max())) {
                  return std::unexpected(vve::Error::invalid_argument);
               }

               const bool double_sided = material_data != nullptr && material_data->double_sided;
               const bool alpha_blend = material_data != nullptr && material_data->alpha_blend;
               const auto pipeline_variant = forwardPipelineVariant(double_sided, alpha_blend);
               const auto graphics_pipeline = graphicsPipelineForVariant(binding->second, pipeline_variant);
               const auto camera_depth = defaultViewDepth(node->world_transform);
               if (binding->second.graphics_pipeline_ready && !graphics_pipeline.value.isValid()) {
                  return std::unexpected(vve::Error::invalid_argument);
               }

               packets.packets.push_back(DrawPacket{.window = window,
                                                    .pass = pass->handle,
                                                    .kernel = pass->kernel,
                                                    .graphics_pipeline = graphics_pipeline,
                                                    .pipeline_variant = pipeline_variant,
                                                    .sort_bucket = pipelineVariantSortBucket(pipeline_variant),
                                                    .camera_depth = camera_depth,
                                                    .draw_index = static_cast<std::uint32_t>(packets.packets.size()),
                                                    .node = node->handle,
                                                    .mesh_instance = instance.handle,
                                                    .mesh = instance.mesh,
                                                    .material = material,
                                                    .material_index = material_index,
                                                    .material_constants = drawMaterialConstants(material_data),
                                                    .material_constants_buffer =
                                                        gpu_material != nullptr && gpu_material->constants_uploaded
                                                            ? gpu_material->constants_buffer
                                                            : GpuBufferHandle{},
                                                    .material_textures =
                                                        gpu_material != nullptr ? gpu_material->textures
                                                                                : Vector<GpuMaterialTextureBinding>{},
                                                    .vertex_buffer = gpu_mesh->vertex_buffer,
                                                    .index_buffer = gpu_mesh->index_buffer,
                                                    .first_index = submesh.index_offset,
                                                    .index_count = submesh.index_count,
                                                    .vertex_offset = 0,
                                                    .instance_count = 1,
                                                    .world_transform = node->world_transform,
                                                    .double_sided = double_sided,
                                                    .alpha_blend = alpha_blend});
            }
         }

         sortDrawPackets(packets.packets);
         draw_packet_lists_[window.value.value()] = std::move(packets);
         return {};
      }

      /// @brief Returns the latest draw packets built for a window.
      [[nodiscard]] std::expected<std::optional<WindowDrawPacketList>, vve::Error>
      drawPackets(WindowHandle window) const {
         if (!window.value.isValid()) {
            return std::unexpected(vve::Error::invalid_argument);
         }

         const auto packets = draw_packet_lists_.find(window.value.value());
         if (packets == draw_packet_lists_.end()) {
            return std::optional<WindowDrawPacketList>{};
         }

         return packets->second;
      }

      /// @brief Records placeholder render work for the supplied render graph.
      [[nodiscard]] std::expected<void, vve::Error> record(GraphicsBackend &graphics_backend, const FrameContext &,
                                                           const SceneData &, WindowHandle window,
                                                           SwapchainHandle swapchain, const RenderGraph &render_graph) {
         for (const auto &pass : render_graph.passes) {
            // Iterating the passes keeps the placeholder implementation aligned
            // with the future shape where each pass will emit backend commands.
         }

         const auto packets = drawPackets(window);
         if (!packets) {
            return std::unexpected(packets.error());
         }
         if (!packets->has_value()) {
            return graphics_backend.recordWindowFrame(swapchain, WindowDrawPacketList{.window = window});
         }

         return graphics_backend.recordWindowFrame(swapchain, **packets);
      }

      /// @brief Consumes the produced frame output for a window.
      [[nodiscard]] std::expected<void, vve::Error> consumeOutput(GraphicsBackend &graphics_backend,
                                                                  const FrameContext &, const SceneData &, WindowHandle,
                                                                  SwapchainHandle swapchain, const RenderGraph &) {
         return graphics_backend.submitWindowFrame(swapchain);
      }

      /**
       * @brief Registers render tasks for each active window pipeline.
       * @param builder Shared frame task-graph builder.
       * @param scene Runtime scene data for the current graph build.
       * @param render_pipelines Per-window render graphs to wire into the task graph.
       */
      void registerTasks(TaskGraphBuilder &builder, const SceneData &, GraphicsBackend &graphics_backend,
                         VectorConstRange<WindowRenderPipeline> render_pipelines) {
         for (const auto &pipeline : render_pipelines) {
            const auto window = pipeline.window;
            const auto swapchain = pipeline.swapchain.handle;
            const auto *render_graph = &pipeline.graph;
            const auto cull_visibility_gpu_name = std::format("task.window.{}.cull_visibility_gpu", pipeline.window_id);
            const auto build_draw_packets_name = std::format("task.window.{}.build_draw_packets", pipeline.window_id);
            const auto record_render_graph_name = std::format("task.window.{}.record_render_graph", pipeline.window_id);
            const auto consume_frame_output_name =
                std::format("task.window.{}.consume_frame_output", pipeline.window_id);

            // Render work is serialized explicitly per window so later DAG
            // compilation does not have to infer pass ordering heuristically.
            const auto cull_visibility_gpu_task = builder.addTask(
                cull_visibility_gpu_name, TaskKernelId::cull_visibility_gpu,
                detail::requireFrameScene([this, window, render_graph](const FrameContext &frame_context,
                                                                       const SceneData &scene) {
                   return cullVisibilityGpu(frame_context, scene, window, *render_graph);
                }),
                {TaskGraphBuilder::taskHandleFor("task.upload_resources")}, {},
                std::string("Cull Visibility GPU (") + pipeline.window_id + ")", TaskPhase::render,
                TaskScope::window, pipeline.window);
            const auto build_draw_packets_task = builder.addTask(
                build_draw_packets_name, TaskKernelId::build_draw_packets,
                detail::requireFrameScene([this, window, render_graph](const FrameContext &frame_context,
                                                                       const SceneData &scene) {
                   return buildDrawPackets(frame_context, scene, window, *render_graph);
                }),
                {cull_visibility_gpu_task}, {},
                std::string("Build Draw Packets (") + pipeline.window_id + ")", TaskPhase::render, TaskScope::window,
                pipeline.window);
            const auto record_render_graph_task = builder.addTask(
                record_render_graph_name, TaskKernelId::record_render_graph,
                detail::requireFrameScene([this, &graphics_backend, window, swapchain, render_graph](
                                             const FrameContext &frame_context, const SceneData &scene) {
                   return record(graphics_backend, frame_context, scene, window, swapchain, *render_graph);
                }),
                {build_draw_packets_task}, {},
                std::string("Record Render Graph (") + pipeline.window_id + ")", TaskPhase::render,
                TaskScope::window, pipeline.window);
            const auto consume_frame_output_task = builder.addTask(
                consume_frame_output_name, TaskKernelId::consume_frame_output,
                detail::requireFrameScene([this, &graphics_backend, window, swapchain, render_graph](
                                             const FrameContext &frame_context, const SceneData &scene) {
                   return consumeOutput(graphics_backend, frame_context, scene, window, swapchain, *render_graph);
                }),
                {record_render_graph_task}, {},
                std::string("Consume Frame Output (") + pipeline.window_id + ")", TaskPhase::render,
                TaskScope::window, pipeline.window);
         }
      }

   private:
      /// @brief Checks that renderer, reflected layout, backend resources, and graph all describe the same pipeline.
      [[nodiscard]] static std::expected<void, vve::Error>
      validatePipelineBinding(const WindowRenderPipeline &pipeline) {
         if (!pipeline.window.value.isValid() || pipeline.window_id.empty() ||
             !pipeline.renderer.handle.value.isValid() || pipeline.renderer.id.empty() ||
             pipeline.renderer.api != vve::GraphicsApi::vulkan ||
             !pipeline.shader_program.value.isValid() || pipeline.pipeline_layout.shader_stages.empty() ||
             !pipeline.backend_resources.handle.value.isValid()) {
            return std::unexpected(vve::Error::invalid_argument);
         }

         if (pipeline.pipeline_layout.renderer.value != pipeline.renderer.handle.value ||
             pipeline.pipeline_layout.renderer_id != pipeline.renderer.id ||
             pipeline.pipeline_layout.shader_program.value != pipeline.shader_program.value) {
            return std::unexpected(vve::Error::invalid_argument);
         }

         if (pipeline.backend_resources.renderer.value != pipeline.renderer.handle.value ||
             pipeline.backend_resources.shader_program.value != pipeline.shader_program.value ||
             !pipeline.backend_resources.pipeline_layout_created) {
            return std::unexpected(vve::Error::invalid_argument);
         }

         if (pipeline.backend_resources.shader_module_count != pipeline.pipeline_layout.shader_stages.size() ||
             pipeline.backend_resources.descriptor_set_layout_count != pipeline.pipeline_layout.descriptor_sets.size()) {
            return std::unexpected(vve::Error::invalid_argument);
         }

         if (!graphContainsKernel(pipeline.graph, pipeline.renderer.main_kernel)) {
            return std::unexpected(vve::Error::invalid_argument);
         }

         return {};
      }

      vve::ShadowKind shadow_{vve::ShadowKind::none};                   ///< Selected shadowing strategy.
      std::string graphics_backend_name_{};                              ///< Backend name copied for stable render-graph labels.
      std::unordered_map<vve::Handle::value_type, RendererPipelineBinding> renderer_bindings_{};
      std::unordered_map<vve::Handle::value_type, WindowDrawPacketList> draw_packet_lists_{};
      bool imgui_enabled_{true};                                        ///< Whether the GUI pass should be appended.
   };

   /// @brief Constructs the public render-system facade around the concrete implementation.
   VVE_V3_DEFINE_FACADE_CTOR(
       RenderSystemFacade, DefaultRenderSystemImplementation,
       (vve::RendererKind renderer, vve::ShadowKind shadow,
        GraphicsBackendFacade<VulkanGraphicsBackendImplementation> &graphics_backend, bool imgui_enabled),
       (renderer, shadow, graphics_backend, imgui_enabled))

   /// @brief Returns the render-system name for the public facade.
   VVE_V3_DEFINE_FACADE_METHOD(RenderSystemFacade, DefaultRenderSystemImplementation, name, (), (), const noexcept,
                               std::string_view)

   /// @brief Builds the static render graph through the public render-system facade.
   VVE_V3_DEFINE_FACADE_METHOD(RenderSystemFacade, DefaultRenderSystemImplementation, buildStaticGraph,
                               (WindowHandle window, const RendererDesc &renderer), (window, renderer), , RenderGraph)

   /// @brief Binds backend resources to a renderer instance through the public render-system facade.
   VVE_V3_DEFINE_FACADE_METHOD(RenderSystemFacade, DefaultRenderSystemImplementation, bindPipelineResources,
                               (const WindowRenderPipeline &pipeline), (pipeline), ,
                               std::expected<RendererPipelineBinding, vve::Error>)

   /// @brief Returns a renderer binding through the public render-system facade.
   VVE_V3_DEFINE_FACADE_METHOD(RenderSystemFacade, DefaultRenderSystemImplementation, rendererPipeline,
                               (WindowHandle window), (window), const,
                               std::expected<std::optional<RendererPipelineBinding>, vve::Error>)

   /// @brief Requests graphics pipeline preparation through the public render-system facade.
   VVE_V3_DEFINE_FACADE_METHOD(RenderSystemFacade, DefaultRenderSystemImplementation, createGraphicsPipeline,
                               (GraphicsBackendFacade<VulkanGraphicsBackendImplementation> &graphics_backend,
                                const RendererPipelineBinding &binding),
                               (graphics_backend, binding), ,
                               std::expected<GraphicsPipelineResources, vve::Error>)

   /// @brief Performs GPU visibility work through the public render-system facade.
   VVE_V3_DEFINE_FACADE_METHOD(RenderSystemFacade, DefaultRenderSystemImplementation, cullVisibilityGpu,
                               (const FrameContext &frame_context, const SceneData &scene, WindowHandle window,
                                const RenderGraph &render_graph),
                               (frame_context, scene, window, render_graph), , std::expected<void, vve::Error>)

   /// @brief Builds draw packets through the public render-system facade.
   VVE_V3_DEFINE_FACADE_METHOD(RenderSystemFacade, DefaultRenderSystemImplementation, buildDrawPackets,
                               (const FrameContext &frame_context, const SceneData &scene, WindowHandle window,
                                const RenderGraph &render_graph),
                               (frame_context, scene, window, render_graph), , std::expected<void, vve::Error>)

   /// @brief Returns draw packets through the public render-system facade.
   VVE_V3_DEFINE_FACADE_METHOD(RenderSystemFacade, DefaultRenderSystemImplementation, drawPackets,
                               (WindowHandle window), (window), const,
                               std::expected<std::optional<WindowDrawPacketList>, vve::Error>)

   /// @brief Records render work through the public render-system facade.
   VVE_V3_DEFINE_FACADE_METHOD(RenderSystemFacade, DefaultRenderSystemImplementation, record,
                               (GraphicsBackendFacade<VulkanGraphicsBackendImplementation> &graphics_backend,
                                const FrameContext &frame_context, const SceneData &scene, WindowHandle window,
                                SwapchainHandle swapchain, const RenderGraph &render_graph),
                               (graphics_backend, frame_context, scene, window, swapchain, render_graph), ,
                               std::expected<void, vve::Error>)

   /// @brief Consumes frame output through the public render-system facade.
   VVE_V3_DEFINE_FACADE_METHOD(RenderSystemFacade, DefaultRenderSystemImplementation, consumeOutput,
                               (GraphicsBackendFacade<VulkanGraphicsBackendImplementation> &graphics_backend,
                                const FrameContext &frame_context, const SceneData &scene, WindowHandle window,
                                SwapchainHandle swapchain, const RenderGraph &render_graph),
                               (graphics_backend, frame_context, scene, window, swapchain, render_graph), ,
                               std::expected<void, vve::Error>)

   /// @brief Registers render tasks through the public render-system facade.
   VVE_V3_DEFINE_FACADE_VOID_METHOD(RenderSystemFacade, DefaultRenderSystemImplementation, registerTasks,
                                    (TaskGraphBuilder &builder, const SceneData &scene,
                                     GraphicsBackendFacade<VulkanGraphicsBackendImplementation> &graphics_backend,
                                     VectorConstRange<WindowRenderPipeline> render_pipelines),
                                    (builder, scene, graphics_backend, render_pipelines), )

   /// @brief Emits the explicit render-system facade instantiation for v3.
   template class RenderSystemFacade<DefaultRenderSystemImplementation>;

} // namespace vve::v3
