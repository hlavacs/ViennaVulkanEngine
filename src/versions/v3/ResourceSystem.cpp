module;

#include "FacadeMacros.hpp"

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
         scenes_.insert_or_assign(scene.handle.value.value(), scene);
         upsertRecord(ResourceRecord{.id = scene.handle.value,
                                     .kind = ResourceKind::unknown,
                                     .location = ResourceLocation::source_file,
                                     .generation = 1,
                                     .source_path = source_path});

         for (const auto &texture : scene.textures) {
            textures_.insert_or_assign(texture.handle.value.value(), texture);
            upsertRecord(ResourceRecord{.id = texture.handle.value,
                                        .kind = ResourceKind::texture,
                                        .location = ResourceLocation::cpu_memory,
                                        .generation = 1,
                                        .source_path = texture.resolved_path.empty() ? source_path
                                                                                    : texture.resolved_path});
         }

         for (const auto &mesh : scene.meshes) {
            meshes_.insert_or_assign(mesh.handle.value.value(), mesh);
            upsertRecord(ResourceRecord{.id = mesh.handle.value,
                                        .kind = ResourceKind::mesh,
                                        .location = ResourceLocation::cpu_memory,
                                        .generation = 1,
                                        .source_path = mesh.source_path.empty() ? source_path : mesh.source_path});
         }

         for (const auto &material : scene.materials) {
            materials_.insert_or_assign(material.handle.value.value(), material);
            upsertRecord(ResourceRecord{.id = material.handle.value,
                                        .kind = ResourceKind::material,
                                        .location = ResourceLocation::cpu_memory,
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
      void upsertRecord(ResourceRecord record) {
         const auto id = record.id.value();
         if (const auto record_index = record_indices_.find(id); record_index != record_indices_.end()) {
            records_[record_index->second] = std::move(record);
            return;
         }

         record_indices_.emplace(id, records_.size());
         records_.push_back(std::move(record));
      }

      Vector<ResourceRecord> records_{}; ///< Registered resource records owned by the subsystem.
      std::unordered_map<vve::Handle::value_type, std::size_t> record_indices_{}; ///< Record lookup cache keyed by stable resource id.
      std::unordered_map<vve::Handle::value_type, ImportedScene> scenes_{}; ///< Imported scenes retained in CPU memory.
      std::unordered_map<vve::Handle::value_type, ImportedTexture> textures_{}; ///< Imported texture references retained in CPU memory.
      std::unordered_map<vve::Handle::value_type, ImportedMesh> meshes_{}; ///< Imported mesh payloads retained in CPU memory.
      std::unordered_map<vve::Handle::value_type, ImportedMaterial> materials_{}; ///< Imported material payloads retained in CPU memory.
   };

   /// @brief Constructs the public resource-system facade around the concrete implementation.
   VVE_V3_DEFINE_FACADE_CTOR(ResourceSystemFacade, DefaultResourceSystemImplementation, (), ())

   /// @brief Returns the resource-system name for the public facade.
   VVE_V3_DEFINE_FACADE_METHOD(ResourceSystemFacade, DefaultResourceSystemImplementation, name, (), (), const noexcept,
                               std::string_view)

   /// @brief Registers imported scene resources through the public facade.
   VVE_V3_DEFINE_FACADE_METHOD(ResourceSystemFacade, DefaultResourceSystemImplementation, registerImportedScene,
                               (const ImportedScene &scene, const std::filesystem::path &source_path),
                               (scene, source_path), , std::expected<void, vve::Error>)

   /// @brief Enumerates registered resources through the public facade.
   VVE_V3_DEFINE_FACADE_METHOD(ResourceSystemFacade, DefaultResourceSystemImplementation, enumerate, (), (), const,
                               std::expected<std::vector<ResourceRecord>, vve::Error>)

   /// @brief Uploads resources through the public facade.
   VVE_V3_DEFINE_FACADE_METHOD(ResourceSystemFacade, DefaultResourceSystemImplementation, uploadResources,
                               (const FrameContext &frame_context, const SceneData &scene), (frame_context, scene), ,
                               std::expected<void, vve::Error>)

   /// @brief Registers resource tasks through the public facade.
   VVE_V3_DEFINE_FACADE_VOID_METHOD(ResourceSystemFacade, DefaultResourceSystemImplementation, registerTasks,
                                    (TaskGraphBuilder &builder, const SceneData &scene), (builder, scene), )

   /// @brief Emits the explicit resource-system facade instantiation for v3.
   template class ResourceSystemFacade<DefaultResourceSystemImplementation>;

} // namespace vve::v3
