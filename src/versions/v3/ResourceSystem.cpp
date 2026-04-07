module VEEngine.V3;
import std;
import :Internal;

/**
 * @file
 * @brief v3 resource-system implementation.
 *
 * The resource system tracks imported resources and simulates resource upload
 * state transitions so the rest of the runtime can reason about asset
 * lifetime without a full backend-specific resource manager yet.
 */
namespace vve::v3 {

   /**
    * @brief Concrete resource-system implementation used by v3.
    */
   class DefaultResourceSystemImplementation {
   public:
      /// @brief Returns the subsystem name for diagnostics.
      [[nodiscard]] std::string_view name() const noexcept { return "ResourceSystem"; }

      /**
       * @brief Registers the resources referenced by an imported scene.
       * @param scene Imported scene to register.
       * @param source_path Source asset path associated with the scene.
       */
      [[nodiscard]] std::expected<void, vve::Error> registerImportedScene(const ImportedScene &scene,
                                                                          const std::filesystem::path &source_path) {
         // Record the scene itself as a source-file-backed resource.
         records_.push_back(ResourceRecord{.id = scene.handle.value,
                                           .kind = ResourceKind::unknown,
                                           .location = ResourceLocation::source_file,
                                           .generation = 1,
                                           .source_path = source_path});

         // Meshes and materials start as imported blobs until upload moves them
         for (const auto &mesh : scene.meshes) { // into GPU memory.
            records_.push_back(ResourceRecord{.id = mesh.handle.value,
                                              .kind = ResourceKind::mesh,
                                              .location = ResourceLocation::imported_blob,
                                              .generation = 1,
                                              .source_path = source_path});
         }

         for (const auto &material : scene.materials) {
            records_.push_back(ResourceRecord{.id = material.handle.value,
                                              .kind = ResourceKind::material,
                                              .location = ResourceLocation::imported_blob,
                                              .generation = 1,
                                              .source_path = source_path});
         }

        return {};
      }

      /// @brief Returns a copy of the currently registered resource records.
      [[nodiscard]] std::expected<std::vector<ResourceRecord>, vve::Error> enumerate() const {
         std::vector<ResourceRecord> records{};
         records.reserve(records_.size());
         for (const auto &record : records_) {
            records.push_back(record);
         }

         return records;
      }

      /// @brief Simulates uploading imported resources to GPU memory.
      [[nodiscard]] std::expected<void, vve::Error> uploadResources(const FrameContext &, const SceneData &) {
         // Transition imported resources into GPU-visible state and bump their
         for (auto &record : records_) { // generation to represent a new uploaded revision.
            if (record.location == ResourceLocation::imported_blob || record.location == ResourceLocation::cpu_memory) {
               record.location = ResourceLocation::gpu_memory;
               ++record.generation;
            }
         }

         return {};
      }

      /// @brief Registers the built-in resource upload task.
      void registerTasks(TaskGraphBuilder &builder, const SceneData &) {
         [[maybe_unused]] const auto upload_resources_task = builder.addTask(
             "task.upload_resources", TaskKernelId::upload_resources,
             detail::requireFrameScene([this](const FrameContext &frame_context, const SceneData &scene) {
                return uploadResources(frame_context, scene);
             }),
             {TaskGraphBuilder::taskHandleFor("task.cull_visibility_cpu")}, {}, "Upload Resources",
             TaskPhase::resources);
      }

   private:
      Vector<ResourceRecord> records_{}; ///< Registered resource records owned by the subsystem.
   };

   /// @brief Constructs the public resource-system facade around the concrete implementation.
   template <>
   ResourceSystemFacade<DefaultResourceSystemImplementation>::ResourceSystemFacade()
       : implementation_(new DefaultResourceSystemImplementation(),
                         [](DefaultResourceSystemImplementation *implementation) { delete implementation; }) {}

   /// @brief Returns the resource-system name for the public facade.
   std::string_view ResourceSystemFacade<DefaultResourceSystemImplementation>::name() const noexcept {
      return implementation_->name();
   }

   /// @brief Registers imported scene resources through the public facade.
   template <>
   std::expected<void, vve::Error> ResourceSystemFacade<DefaultResourceSystemImplementation>::registerImportedScene(
       const ImportedScene &scene, const std::filesystem::path &source_path) {
      return implementation_->registerImportedScene(scene, source_path);
   }

   /// @brief Enumerates registered resources through the public facade.
   template <>
   std::expected<std::vector<ResourceRecord>, vve::Error>
   ResourceSystemFacade<DefaultResourceSystemImplementation>::enumerate() const {
      return implementation_->enumerate();
   }

   /// @brief Uploads resources through the public facade.
   template <>
   std::expected<void, vve::Error>
   ResourceSystemFacade<DefaultResourceSystemImplementation>::uploadResources(const FrameContext &frame_context,
                                                                              const SceneData &scene) {
      return implementation_->uploadResources(frame_context, scene);
   }

   /// @brief Registers resource tasks through the public facade.
   template <>
   void ResourceSystemFacade<DefaultResourceSystemImplementation>::registerTasks(TaskGraphBuilder &builder,
                                                                                 const SceneData &scene) {
      implementation_->registerTasks(builder, scene);
   }

   /// @brief Emits the explicit resource-system facade instantiation for v3.
   template class ResourceSystemFacade<DefaultResourceSystemImplementation>;

} // namespace vve::v3
