module VEEngine.V3;
import std;
import :Internal;

namespace vve::v3 {

   class DefaultResourceSystemImplementation {
   public:
      [[nodiscard]] std::string_view name() const noexcept { return "ResourceSystem"; }

      [[nodiscard]] std::expected<void, vve::Error> registerImportedScene(const ImportedScene &scene,
                                                                          const std::filesystem::path &source_path) {
         records_.push_back(ResourceRecord{.id = scene.handle.value,
                                           .kind = ResourceKind::unknown,
                                           .location = ResourceLocation::source_file,
                                           .generation = 1,
                                           .source_path = source_path});

         for (const auto &mesh : scene.meshes) {
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

      [[nodiscard]] std::expected<std::vector<ResourceRecord>, vve::Error> enumerate() const {
         std::vector<ResourceRecord> records{};
         records.reserve(records_.size());
         for (const auto &record : records_) {
            records.push_back(record);
         }

         return records;
      }

      [[nodiscard]] std::expected<void, vve::Error> uploadResources(const FrameContext &, const SceneData &) {
         for (auto &record : records_) {
            if (record.location == ResourceLocation::imported_blob || record.location == ResourceLocation::cpu_memory) {
               record.location = ResourceLocation::gpu_memory;
               ++record.generation;
            }
         }

         return {};
      }

      void registerTasks(TaskGraphBuilder &builder, const SceneData &) {
         const auto upload_resources_task =
             builder.addTask("task.upload_resources", TaskKernelId::upload_resources, {},
                             {TaskGraphBuilder::taskHandleFor("task.cull_visibility_cpu")}, {}, "Upload Resources");

         builder.setTaskCallback(
             upload_resources_task,
             [this](const TaskExecutionContext &execution_context) -> std::expected<void, vve::Error> {
                if (execution_context.frame_context == nullptr || execution_context.scene == nullptr) {
                   return std::unexpected(vve::Error::invalid_argument);
                }

                return uploadResources(*execution_context.frame_context, *execution_context.scene);
             });
      }

   private:
      SegmentedVector<ResourceRecord> records_{};
   };

   template <>
   ResourceSystemFacade<DefaultResourceSystemImplementation>::ResourceSystemFacade()
       : implementation_(new DefaultResourceSystemImplementation(),
                         [](DefaultResourceSystemImplementation *implementation) { delete implementation; }) {}

   std::string_view ResourceSystemFacade<DefaultResourceSystemImplementation>::name() const noexcept {
      return implementation_->name();
   }

   template <>
   std::expected<void, vve::Error> ResourceSystemFacade<DefaultResourceSystemImplementation>::registerImportedScene(
       const ImportedScene &scene, const std::filesystem::path &source_path) {
      return implementation_->registerImportedScene(scene, source_path);
   }

   template <>
   std::expected<std::vector<ResourceRecord>, vve::Error>
   ResourceSystemFacade<DefaultResourceSystemImplementation>::enumerate() const {
      return implementation_->enumerate();
   }

   template <>
   std::expected<void, vve::Error>
   ResourceSystemFacade<DefaultResourceSystemImplementation>::uploadResources(const FrameContext &frame_context,
                                                                              const SceneData &scene) {
      return implementation_->uploadResources(frame_context, scene);
   }

   template <>
   void ResourceSystemFacade<DefaultResourceSystemImplementation>::registerTasks(TaskGraphBuilder &builder,
                                                                                 const SceneData &scene) {
      implementation_->registerTasks(builder, scene);
   }

   template class ResourceSystemFacade<DefaultResourceSystemImplementation>;

} // namespace vve::v3
