module;

#include <slang/slang-com-ptr.h>
#include <slang/slang.h>

export module VEEngine.V4:Shaders;
import std;
export import :Types;

/// @file
/// @brief Slang-backed shader compilation and compact reflection records for v4 renderers.

export namespace vve::v4 {

   /// @brief Shader stage categories used by reflection descriptors.
   enum class ShaderStage {
      vertex,   ///< Vertex shader stage.
      fragment, ///< Fragment shader stage.
      compute   ///< Compute shader stage.
   };

   /// @brief One compiled entry point and its SPIR-V words.
   struct ShaderStageBinary {
      ShaderStage stage{ShaderStage::vertex}; ///< Stage represented by this binary.
      std::string entry_point{};              ///< Slang entry-point function name.
      Vector<std::uint32_t> spirv{};          ///< Compiled SPIR-V words.
   };

   /// @brief One compact reflected shader binding or parameter path.
   struct ShaderBindingReflection {
      std::string name{};     ///< Flattened reflected name, for example gVveForward.frame.
      std::string category{}; ///< Slang parameter category.
      std::string type{};     ///< Slang binding type when available.
      std::uint32_t set{0};   ///< Reflected Vulkan descriptor set/register space.
      std::uint32_t binding{0}; ///< Reflected Vulkan descriptor binding/register index.
   };

   /// @brief Shader entry-point reflection summary.
   struct ShaderEntryPointReflection {
      std::string name{};                 ///< Entry-point function name.
      ShaderStage stage{ShaderStage::vertex}; ///< Reflected shader stage.
      Vector<std::string> varying_inputs{};   ///< Vertex/pixel input semantic names.
      Vector<std::string> varying_outputs{};  ///< Vertex/pixel output semantic names.
   };

   /// @brief Compact reflection result stored beside compiled SPIR-V.
   struct ShaderReflection {
      Vector<ShaderEntryPointReflection> entry_points{}; ///< Entry-point summaries.
      Vector<ShaderBindingReflection> bindings{};        ///< Flattened parameter/binding summaries.
      Vector<std::string> type_names{};                  ///< Types found by name for verification.
   };

} // namespace vve::v4

namespace vve::v4 {

   /// @brief Internal shader program record.
   struct ShaderRecord {
      ShaderHandle handle{};                 ///< Stable shader handle.
      ObjectName name{};                     ///< Human-readable shader name.
      std::filesystem::path source{};        ///< Slang source path.
      Vector<ShaderStage> stages{};          ///< Stages present in the program.
      Vector<ShaderStageBinary> binaries{};  ///< Compiled SPIR-V by entry point.
      ShaderReflection reflection{};         ///< Compact reflected layout information.
   };

   namespace {

      [[nodiscard]] std::string safeName(const char *name) { return name == nullptr ? std::string{} : name; }

      [[nodiscard]] ShaderStage toShaderStage(SlangStage stage) {
         switch (stage) {
         case SLANG_STAGE_FRAGMENT:
            return ShaderStage::fragment;
         case SLANG_STAGE_COMPUTE:
            return ShaderStage::compute;
         default:
            return ShaderStage::vertex;
         }
      }

      [[nodiscard]] std::string parameterCategoryName(slang::ParameterCategory category) {
         static const std::map<slang::ParameterCategory, std::string> names{
            {slang::ParameterCategory::ConstantBuffer, "constant_buffer"},
            {slang::ParameterCategory::DescriptorTableSlot, "descriptor_table_slot"},
            {slang::ParameterCategory::PushConstantBuffer, "push_constant_buffer"},
            {slang::ParameterCategory::SamplerState, "sampler"},
            {slang::ParameterCategory::ShaderResource, "shader_resource"},
            {slang::ParameterCategory::Uniform, "uniform"},
            {slang::ParameterCategory::UnorderedAccess, "unordered_access"},
            {slang::ParameterCategory::VaryingInput, "varying_input"},
            {slang::ParameterCategory::VaryingOutput, "varying_output"}};
         if (const auto it = names.find(category); it != names.end()) { return it->second; }
         return "unknown";
      }

      [[nodiscard]] std::string bindingTypeName(slang::BindingType type) {
         static const std::map<slang::BindingType, std::string> names{
            {slang::BindingType::CombinedTextureSampler, "combined_texture_sampler"},
            {slang::BindingType::ConstantBuffer, "constant_buffer"},
            {slang::BindingType::ParameterBlock, "parameter_block"},
            {slang::BindingType::PushConstant, "push_constant"},
            {slang::BindingType::RawBuffer, "raw_buffer"},
            {slang::BindingType::Sampler, "sampler"},
            {slang::BindingType::Texture, "texture"},
            {slang::BindingType::TypedBuffer, "typed_buffer"},
            {slang::BindingType::Unknown, "unknown"}};
         if (const auto it = names.find(type); it != names.end()) { return it->second; }
         return "unknown";
      }

      [[nodiscard]] std::optional<Vector<std::uint32_t>> spirvWords(slang::IBlob *code) {
         if (code == nullptr || code->getBufferPointer() == nullptr || code->getBufferSize() % 4 != 0) {
            return std::nullopt;
         }

         Vector<std::uint32_t> words{};
         const auto word_count = code->getBufferSize() / sizeof(std::uint32_t);
         words.reserve(word_count);
         const auto *data = static_cast<const std::uint32_t *>(code->getBufferPointer());
         for (std::size_t index = 0; index < word_count; ++index) { words.push_back(data[index]); }
         return words;
      }

      void collectFields(ShaderReflection &reflection, std::string prefix, slang::TypeLayoutReflection *type_layout) {
         if (type_layout == nullptr) { return; }
         for (unsigned field_index = 0; field_index < type_layout->getFieldCount(); ++field_index) {
            auto *field = type_layout->getFieldByIndex(field_index);
            if (field == nullptr) { continue; }
            auto name = prefix.empty() ? safeName(field->getName()) : prefix + "." + safeName(field->getName());
            reflection.bindings.push_back(ShaderBindingReflection{
               .name = name,
               .category = parameterCategoryName(field->getCategory()),
               .type = parameterCategoryName(field->getCategory()),
               .set = static_cast<std::uint32_t>(field->getBindingSpace()),
               .binding = static_cast<std::uint32_t>(field->getBindingIndex())});
            collectFields(reflection, std::move(name), field->getTypeLayout());
         }
      }

      void collectElementFields(ShaderReflection &reflection, const std::string &prefix,
                                slang::TypeLayoutReflection *type_layout) {
         if (type_layout == nullptr) { return; }
         auto *element = type_layout->getElementTypeLayout();
         if (element == nullptr || element == type_layout) { return; }
         collectFields(reflection, prefix, element);
      }

      void collectBindingRanges(ShaderReflection &reflection, std::string prefix,
                                slang::TypeLayoutReflection *type_layout) {
         if (type_layout == nullptr) { return; }
         for (SlangInt range = 0; range < type_layout->getBindingRangeCount(); ++range) {
            auto *leaf = type_layout->getBindingRangeLeafVariable(range);
            auto name = prefix;
            if (leaf != nullptr && leaf->getName() != nullptr && leaf->getName()[0] != '\0') {
               name = prefix.empty() ? leaf->getName() : prefix + "." + leaf->getName();
            }
            reflection.bindings.push_back(ShaderBindingReflection{
               .name = name,
               .category = "binding_range",
               .type = bindingTypeName(type_layout->getBindingRangeType(range)),
               .set = static_cast<std::uint32_t>(type_layout->getBindingRangeDescriptorSetIndex(range)),
               .binding = static_cast<std::uint32_t>(type_layout->getBindingRangeFirstDescriptorRangeIndex(range))});
         }
      }

      void collectEntryPoint(ShaderReflection &reflection, slang::EntryPointReflection *entry_point) {
         if (entry_point == nullptr) { return; }

         auto record = ShaderEntryPointReflection{.name = safeName(entry_point->getName()),
                                                  .stage = toShaderStage(entry_point->getStage())};
         for (unsigned parameter = 0; parameter < entry_point->getParameterCount(); ++parameter) {
            auto *layout = entry_point->getParameterByIndex(parameter);
            if (layout == nullptr) { continue; }
            auto semantic = safeName(layout->getSemanticName());
            if (layout->getCategory() == slang::ParameterCategory::VaryingInput) {
               record.varying_inputs.push_back(semantic.empty() ? safeName(layout->getName()) : semantic);
            } else if (layout->getCategory() == slang::ParameterCategory::VaryingOutput) {
               record.varying_outputs.push_back(semantic.empty() ? safeName(layout->getName()) : semantic);
            }
         }

         if (auto *result = entry_point->getResultVarLayout(); result != nullptr) {
            record.varying_outputs.push_back(safeName(result->getSemanticName()));
         }
         reflection.entry_points.push_back(std::move(record));
      }

      void collectReflection(ShaderReflection &reflection, slang::ProgramLayout *layout,
                             const Vector<std::string> &entry_points) {
         if (layout == nullptr) { return; }
         for (unsigned parameter = 0; parameter < layout->getParameterCount(); ++parameter) {
            auto *variable = layout->getParameterByIndex(parameter);
            if (variable == nullptr) { continue; }
            const auto name = safeName(variable->getName());
            reflection.bindings.push_back(ShaderBindingReflection{
               .name = name,
               .category = parameterCategoryName(variable->getCategory()),
               .type = parameterCategoryName(variable->getCategory()),
               .set = static_cast<std::uint32_t>(variable->getBindingSpace()),
               .binding = static_cast<std::uint32_t>(variable->getBindingIndex())});
            collectFields(reflection, name, variable->getTypeLayout());
            collectElementFields(reflection, name, variable->getTypeLayout());
            collectBindingRanges(reflection, name, variable->getTypeLayout());
            if (auto *element = variable->getTypeLayout()->getElementTypeLayout(); element != nullptr) {
               collectBindingRanges(reflection, name, element);
            }
         }

         for (const auto &entry : entry_points) {
            collectEntryPoint(reflection, layout->findEntryPointByName(entry.c_str()));
         }
         for (const auto *type : {"VveForwardDebugSample", "VveForwardParams", "VveLightingConstants"}) {
            if (layout->findTypeByName(type) != nullptr) { reflection.type_names.push_back(type); }
         }
      }

   } // namespace

} // namespace vve::v4

export namespace vve::v4 {

   /// @brief Slang-backed shader table storing SPIR-V and reflection data.
   class ShaderSystem {
   public:
      /// @brief Adds a shader record without compiling, useful for small table tests.
      [[nodiscard]] std::expected<ShaderHandle, Error> addShader(ObjectName name, Vector<ShaderStage> stages) {
         const auto handle = makeCounterHandle<ShaderHandle>();
         const auto [_, inserted] = shaders_.emplace(
            handle, ShaderRecord{.handle = handle, .name = std::move(name), .stages = std::move(stages)});
         if (!inserted) { return std::unexpected(Error::duplicate_object); }
         return handle;
      }

      /// @brief Compiles selected Slang entry points to SPIR-V and stores compact reflection data.
      [[nodiscard]] std::expected<ShaderHandle, Error> compileAndReflect(const std::filesystem::path &source,
                                                                         Vector<std::string> entry_points);

      /// @brief Returns whether a shader exists.
      [[nodiscard]] bool containsShader(ShaderHandle handle) const { return shaders_.contains(handle); }

      /// @brief Returns the shader name.
      [[nodiscard]] std::expected<ObjectName, Error> shaderName(ShaderHandle handle) const {
         const auto shader = shaders_.find(handle);
         if (shader == shaders_.end()) { return std::unexpected(Error::missing_object); }
         return shader->second.name;
      }

      /// @brief Returns the number of stages in a shader program.
      [[nodiscard]] std::expected<std::size_t, Error> shaderStageCount(ShaderHandle handle) const {
         const auto shader = shaders_.find(handle);
         if (shader == shaders_.end()) { return std::unexpected(Error::missing_object); }
         return shader->second.stages.size();
      }

      /// @brief Returns the number of compiled SPIR-V words for one stage.
      [[nodiscard]] std::expected<std::size_t, Error> spirvWordCount(ShaderHandle handle, ShaderStage stage) const;

      /// @brief Returns whether compact reflection mentions a binding/parameter path.
      [[nodiscard]] std::expected<bool, Error> hasReflectedBinding(ShaderHandle handle, std::string_view name) const;

      /// @brief Returns whether compact reflection includes a named type.
      [[nodiscard]] std::expected<bool, Error> hasReflectedType(ShaderHandle handle, std::string_view name) const;

      /// @brief Returns reflected binding summaries.
      [[nodiscard]] std::expected<Vector<ShaderBindingReflection>, Error> reflectedBindings(ShaderHandle handle) const;

      /// @brief Returns reflected entry-point summaries.
      [[nodiscard]] std::expected<Vector<ShaderEntryPointReflection>, Error>
      reflectedEntryPoints(ShaderHandle handle) const;

   private:
      [[nodiscard]] const ShaderRecord *find(ShaderHandle handle) const {
         const auto shader = shaders_.find(handle);
         return shader == shaders_.end() ? nullptr : std::addressof(shader->second);
      }

      Slang::ComPtr<slang::IGlobalSession> global_session_{}; ///< Shared Slang compiler session.
      std::map<ShaderHandle, ShaderRecord> shaders_{};        ///< Shaders by handle.
   };

} // namespace vve::v4

namespace vve::v4 {

   std::expected<ShaderHandle, Error> ShaderSystem::compileAndReflect(const std::filesystem::path &source,
                                                                      Vector<std::string> entry_points) {
      if (source.empty() || entry_points.empty()) { return std::unexpected(Error::invalid_argument); }
      if (!std::filesystem::is_regular_file(source)) { return std::unexpected(Error::file_not_found); }
      if (global_session_ == nullptr && SLANG_FAILED(slang::createGlobalSession(global_session_.writeRef()))) {
         return std::unexpected(Error::internal_error);
      }

      const auto include_path = source.parent_path().string();
      slang::TargetDesc target_desc{};
      target_desc.format = SLANG_SPIRV;
      target_desc.profile = global_session_->findProfile("spirv_1_5");

      std::array search_paths{include_path.c_str()};
      slang::SessionDesc session_desc{};
      session_desc.targets = &target_desc;
      session_desc.targetCount = 1;
      session_desc.searchPaths = search_paths.data();
      session_desc.searchPathCount = search_paths.size();

      Slang::ComPtr<slang::ISession> session{};
      if (SLANG_FAILED(global_session_->createSession(session_desc, session.writeRef()))) {
         return std::unexpected(Error::internal_error);
      }

      Slang::ComPtr<slang::IBlob> diagnostics{};
      const auto module_name = source.stem().string();
      auto *module = session->loadModule(module_name.c_str(), diagnostics.writeRef());
      if (module == nullptr) { return std::unexpected(Error::internal_error); }

      std::vector<Slang::ComPtr<slang::IEntryPoint>> found_entry_points{};
      std::vector<slang::IComponentType *> components{module};
      for (const auto &entry : entry_points) {
         Slang::ComPtr<slang::IEntryPoint> entry_point{};
         if (SLANG_FAILED(module->findEntryPointByName(entry.c_str(), entry_point.writeRef())) ||
             entry_point == nullptr) {
            return std::unexpected(Error::missing_object);
         }
         components.push_back(entry_point.get());
         found_entry_points.push_back(std::move(entry_point));
      }

      Slang::ComPtr<slang::IComponentType> program{};
      diagnostics.setNull();
      const auto component_count = static_cast<SlangInt>(components.size());
      if (SLANG_FAILED(session->createCompositeComponentType(components.data(), component_count, program.writeRef(),
                                                            diagnostics.writeRef()))) {
         return std::unexpected(Error::internal_error);
      }

      std::array options{slang::CompilerOptionEntry{
         .name = slang::CompilerOptionName::VulkanEmitReflection,
         .value = {.kind = slang::CompilerOptionValueKind::Int, .intValue0 = 1}}};
      Slang::ComPtr<slang::IComponentType> linked_program{};
      diagnostics.setNull();
      if (SLANG_FAILED(program->linkWithOptions(linked_program.writeRef(), static_cast<std::uint32_t>(options.size()),
                                                options.data(), diagnostics.writeRef()))) {
         return std::unexpected(Error::internal_error);
      }

      ShaderRecord record{.handle = makeCounterHandle<ShaderHandle>(),
                          .name = ObjectName{.value = source.filename().string()},
                          .source = source};
      diagnostics.setNull();
      collectReflection(record.reflection, linked_program->getLayout(0, diagnostics.writeRef()), entry_points);

      for (SlangInt index = 0; index < static_cast<SlangInt>(entry_points.size()); ++index) {
         Slang::ComPtr<slang::IBlob> code{};
         diagnostics.setNull();
         if (SLANG_FAILED(linked_program->getEntryPointCode(index, 0, code.writeRef(), diagnostics.writeRef()))) {
            return std::unexpected(Error::internal_error);
         }
         auto words = spirvWords(code);
         if (!words) { return std::unexpected(Error::internal_error); }
         auto stage = index < static_cast<SlangInt>(record.reflection.entry_points.size())
                         ? record.reflection.entry_points[index].stage
                         : ShaderStage::vertex;
         record.stages.push_back(stage);
         record.binaries.push_back(ShaderStageBinary{.stage = stage,
                                                     .entry_point = entry_points[index],
                                                     .spirv = std::move(*words)});
      }

      const auto handle = record.handle;
      const auto [_, inserted] = shaders_.emplace(handle, std::move(record));
      if (!inserted) { return std::unexpected(Error::duplicate_object); }
      return handle;
   }

   std::expected<std::size_t, Error> ShaderSystem::spirvWordCount(ShaderHandle handle, ShaderStage stage) const {
      const auto *shader = find(handle);
      if (shader == nullptr) { return std::unexpected(Error::missing_object); }
      const auto it = std::ranges::find_if(shader->binaries, [stage](const auto &binary) {
         return binary.stage == stage;
      });
      if (it == shader->binaries.end()) { return std::unexpected(Error::missing_object); }
      return it->spirv.size();
   }

   std::expected<bool, Error> ShaderSystem::hasReflectedBinding(ShaderHandle handle, std::string_view name) const {
      const auto *shader = find(handle);
      if (shader == nullptr) { return std::unexpected(Error::missing_object); }
      return std::ranges::any_of(shader->reflection.bindings, [name](const auto &binding) {
         return binding.name == name;
      });
   }

   std::expected<bool, Error> ShaderSystem::hasReflectedType(ShaderHandle handle, std::string_view name) const {
      const auto *shader = find(handle);
      if (shader == nullptr) { return std::unexpected(Error::missing_object); }
      return std::ranges::any_of(shader->reflection.type_names, [name](const auto &type) {
         return type == name;
      });
   }

   std::expected<Vector<ShaderBindingReflection>, Error> ShaderSystem::reflectedBindings(ShaderHandle handle) const {
      const auto *shader = find(handle);
      if (shader == nullptr) { return std::unexpected(Error::missing_object); }
      return shader->reflection.bindings;
   }

   std::expected<Vector<ShaderEntryPointReflection>, Error>
   ShaderSystem::reflectedEntryPoints(ShaderHandle handle) const {
      const auto *shader = find(handle);
      if (shader == nullptr) { return std::unexpected(Error::missing_object); }
      return shader->reflection.entry_points;
   }

} // namespace vve::v4
