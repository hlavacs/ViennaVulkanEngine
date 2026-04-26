module;

#include "FacadeMacros.hpp"
#include <cstring>
#include <iostream>
#include <slang/slang-com-ptr.h>
#include <slang/slang.h>

module VEEngine.V3;
import std;
import :Internal;

/**
 * @file
 * @brief v3 shader-reflection implementation.
 *
 * The shader system uses the Slang library shipped with the Vulkan SDK to
 * compile SPIR-V entry points and extract resource reflection metadata.
 */
namespace vve::v3 {

   namespace {

   /// @brief Converts a public renderer enum into the shader metadata string form.
   [[nodiscard]] std::string_view toRendererName(vve::RendererKind renderer) noexcept {
      switch (renderer) {
      case vve::RendererKind::forward_renderer:
         return "forward";
      case vve::RendererKind::deferred_renderer:
         return "deferred";
      case vve::RendererKind::path_tracing:
         return "path_tracing";
      }

      return "unknown";
   }

   /// @brief Converts a public shadow enum into the shader metadata string form.
   [[nodiscard]] std::string_view toShadowName(vve::ShadowKind shadow) noexcept {
      switch (shadow) {
      case vve::ShadowKind::none:
         return "none";
      case vve::ShadowKind::shadow_map:
         return "shadow_map";
      case vve::ShadowKind::ray_traced:
         return "ray_traced";
      }

      return "unknown";
   }

   /// @brief Converts a Slang stage into the public shader-stage enum.
   [[nodiscard]] std::optional<ShaderStage> toShaderStage(SlangStage stage) noexcept {
      switch (stage) {
      case SLANG_STAGE_VERTEX:
         return ShaderStage::vertex;
      case SLANG_STAGE_FRAGMENT:
         return ShaderStage::fragment;
      case SLANG_STAGE_COMPUTE:
         return ShaderStage::compute;
      default:
         return std::nullopt;
      }
   }

   /// @brief Returns a stable text label for a Slang parameter category.
   [[nodiscard]] std::string_view toParameterCategoryName(slang::ParameterCategory category) noexcept {
      switch (category) {
      case slang::ParameterCategory::ConstantBuffer:
         return "constant_buffer";
      case slang::ParameterCategory::ShaderResource:
         return "shader_resource";
      case slang::ParameterCategory::UnorderedAccess:
         return "unordered_access";
      case slang::ParameterCategory::SamplerState:
         return "sampler_state";
      case slang::ParameterCategory::DescriptorTableSlot:
         return "descriptor_table_slot";
      case slang::ParameterCategory::PushConstantBuffer:
         return "push_constant_buffer";
      case slang::ParameterCategory::RegisterSpace:
         return "register_space";
      case slang::ParameterCategory::SubElementRegisterSpace:
         return "sub_element_register_space";
      case slang::ParameterCategory::Uniform:
         return "uniform";
      case slang::ParameterCategory::VaryingInput:
         return "varying_input";
      case slang::ParameterCategory::VaryingOutput:
         return "varying_output";
      default:
         return "unknown";
      }
   }

   /// @brief Returns a compact name for reflected Slang type kinds.
   [[nodiscard]] std::string_view toTypeKindName(slang::TypeReflection::Kind kind) noexcept {
      switch (kind) {
      case slang::TypeReflection::Kind::Struct:
         return "struct";
      case slang::TypeReflection::Kind::Array:
         return "array";
      case slang::TypeReflection::Kind::Matrix:
         return "matrix";
      case slang::TypeReflection::Kind::Vector:
         return "vector";
      case slang::TypeReflection::Kind::Scalar:
         return "scalar";
      case slang::TypeReflection::Kind::ConstantBuffer:
         return "constant_buffer";
      case slang::TypeReflection::Kind::Resource:
         return "resource";
      case slang::TypeReflection::Kind::SamplerState:
         return "sampler_state";
      case slang::TypeReflection::Kind::ShaderStorageBuffer:
         return "shader_storage_buffer";
      case slang::TypeReflection::Kind::ParameterBlock:
         return "parameter_block";
      default:
         return "unknown";
      }
   }

   /// @brief Returns a compact name for Slang descriptor binding kinds.
   [[nodiscard]] std::string_view toBindingTypeName(slang::BindingType binding_type) noexcept {
      switch (binding_type) {
      case slang::BindingType::Sampler:
         return "sampler";
      case slang::BindingType::Texture:
         return "texture";
      case slang::BindingType::ConstantBuffer:
         return "constant_buffer";
      case slang::BindingType::ParameterBlock:
         return "parameter_block";
      case slang::BindingType::TypedBuffer:
         return "typed_buffer";
      case slang::BindingType::RawBuffer:
         return "raw_buffer";
      case slang::BindingType::CombinedTextureSampler:
         return "combined_texture_sampler";
      case slang::BindingType::InputRenderTarget:
         return "input_render_target";
      case slang::BindingType::InlineUniformData:
         return "inline_uniform_data";
      case slang::BindingType::RayTracingAccelerationStructure:
         return "ray_tracing_acceleration_structure";
      case slang::BindingType::PushConstant:
         return "push_constant";
      default:
         return "unknown";
      }
   }

   /// @brief Returns whether a reflected variable is a descriptor-like resource binding.
   [[nodiscard]] bool isDescriptorLike(slang::ParameterCategory category) noexcept {
      switch (category) {
      case slang::ParameterCategory::ConstantBuffer:
      case slang::ParameterCategory::ShaderResource:
      case slang::ParameterCategory::UnorderedAccess:
      case slang::ParameterCategory::SamplerState:
      case slang::ParameterCategory::DescriptorTableSlot:
      case slang::ParameterCategory::PushConstantBuffer:
      case slang::ParameterCategory::RegisterSpace:
      case slang::ParameterCategory::SubElementRegisterSpace:
         return true;
      default:
         return false;
      }
   }

   /// @brief Builds a Slang-safe module name from a source filename.
   [[nodiscard]] std::string makeModuleName(const std::filesystem::path &shader_path) {
      auto module_name = shader_path.stem().string();
      if (module_name.empty()) {
         module_name = "shader";
      }

      for (auto &character : module_name) {
         const bool valid = (character >= 'A' && character <= 'Z') || (character >= 'a' && character <= 'z') ||
                            (character >= '0' && character <= '9') || character == '_';
         if (!valid) {
            character = '_';
         }
      }

      if (module_name.front() >= '0' && module_name.front() <= '9') {
         module_name.insert(module_name.begin(), '_');
      }

      return module_name;
   }

   /// @brief Reads a shader file into a null-terminated source payload for Slang.
   [[nodiscard]] std::expected<ShaderSource, vve::Error> readShaderSource(const std::filesystem::path &shader_path) {
      if (!std::filesystem::exists(shader_path)) {
         return std::unexpected(vve::Error::file_not_found);
      }

      std::ifstream input(shader_path, std::ios::binary);
      if (!input) {
         return std::unexpected(vve::Error::io_error);
      }

      return ShaderSource{.source_path = shader_path,
                          .source_code = std::string(std::istreambuf_iterator<char>(input),
                                                     std::istreambuf_iterator<char>())};
   }

   /// @brief Emits Slang diagnostics when a compile/reflection operation fails.
   void logSlangFailure(std::string_view context, slang::IBlob *diagnostics) {
      std::cerr << "[SlangShaderSystem] " << context;
      if (diagnostics != nullptr && diagnostics->getBufferPointer() != nullptr && diagnostics->getBufferSize() > 0) {
         const auto *message = static_cast<const char *>(diagnostics->getBufferPointer());
         std::cerr << ": " << std::string_view(message, diagnostics->getBufferSize());
      }
      std::cerr << '\n';
   }

   /// @brief Returns a readable type name from Slang layout reflection.
   [[nodiscard]] std::string reflectedTypeName(slang::TypeLayoutReflection *type_layout) {
      if (type_layout == nullptr || type_layout->getType() == nullptr) {
         return "unknown";
      }

      const auto *name = type_layout->getType()->getName();
      if (name != nullptr && name[0] != '\0') {
         return name;
      }

      return std::string(toTypeKindName(type_layout->getKind()));
   }

   /// @brief Appends one reflected parameter binding when it represents a descriptor resource.
   void appendReflectedParameter(ShaderMetadata &metadata, std::string_view name,
                                 slang::VariableLayoutReflection *variable_layout) {
      if (variable_layout == nullptr || variable_layout->getTypeLayout() == nullptr) {
         return;
      }

      const auto category = variable_layout->getCategory();
      if (!isDescriptorLike(category)) {
         return;
      }

      metadata.parameters.push_back(ShaderParameter{
          .name = std::string(name),
          .type_name = reflectedTypeName(variable_layout->getTypeLayout()),
          .binding = static_cast<std::uint32_t>(variable_layout->getBindingIndex()),
          .set = static_cast<std::uint32_t>(variable_layout->getBindingSpace()),
          .binding_kind = std::string(toParameterCategoryName(category))});
   }

   /// @brief Recursively walks parameter-block and struct fields to expose nested descriptor bindings.
   void collectParameters(ShaderMetadata &metadata, std::string_view prefix, slang::TypeLayoutReflection *type_layout) {
      if (type_layout == nullptr) {
         return;
      }

      if ((type_layout->getKind() == slang::TypeReflection::Kind::ParameterBlock ||
           type_layout->getKind() == slang::TypeReflection::Kind::ConstantBuffer) &&
          type_layout->getElementTypeLayout() != nullptr && type_layout->getElementTypeLayout() != type_layout) {
         collectParameters(metadata, prefix, type_layout->getElementTypeLayout());
      }

      const auto field_count = type_layout->getFieldCount();
      for (unsigned int field_index = 0; field_index < field_count; ++field_index) {
         auto *field = type_layout->getFieldByIndex(field_index);
         if (field == nullptr) {
            continue;
         }

         const auto *field_name = field->getName();
         if (field_name == nullptr || field_name[0] == '\0') {
            continue;
         }

         auto full_name = std::string(prefix);
         if (!full_name.empty()) {
            full_name.push_back('.');
         }
         full_name += field_name;

         appendReflectedParameter(metadata, full_name, field);
         collectParameters(metadata, full_name, field->getTypeLayout());
      }
   }

   /// @brief Safely converts a non-negative Slang reflection index to the public unsigned form.
   [[nodiscard]] std::uint32_t reflectedIndex(SlangInt value) noexcept {
      if (value < 0) {
         return 0;
      }

      return static_cast<std::uint32_t>(value);
   }

   /// @brief Collects nested resource bindings from Slang parameter-block binding ranges.
   void collectBindingRanges(ShaderMetadata &metadata, std::string_view prefix,
                             slang::TypeLayoutReflection *type_layout) {
      if (type_layout == nullptr) {
         return;
      }

      const auto binding_range_count = type_layout->getBindingRangeCount();
      for (SlangInt binding_range_index = 0; binding_range_index < binding_range_count; ++binding_range_index) {
         const auto binding_type = type_layout->getBindingRangeType(binding_range_index);
         auto *leaf_variable = type_layout->getBindingRangeLeafVariable(binding_range_index);
         auto *leaf_type_layout = type_layout->getBindingRangeLeafTypeLayout(binding_range_index);
         const auto *leaf_name = leaf_variable != nullptr ? leaf_variable->getName() : nullptr;
         if (binding_type == slang::BindingType::ParameterBlock && (leaf_name == nullptr || leaf_name[0] == '\0')) {
            if (leaf_type_layout != nullptr && leaf_type_layout != type_layout) {
               collectBindingRanges(metadata, prefix, leaf_type_layout);
            }
            continue;
         }

         auto full_name = std::string(prefix);
         if (leaf_name != nullptr && leaf_name[0] != '\0') {
            if (!full_name.empty()) {
               full_name.push_back('.');
            }
            full_name += leaf_name;
         } else {
            if (!full_name.empty()) {
               full_name.push_back('.');
            }
            full_name += "binding";
            full_name += std::to_string(binding_range_index);
         }

         const auto set_index = type_layout->getBindingRangeDescriptorSetIndex(binding_range_index);
         const auto descriptor_range_index = type_layout->getBindingRangeFirstDescriptorRangeIndex(binding_range_index);
         auto binding_index = SlangInt{0};
         if (set_index >= 0 && descriptor_range_index >= 0) {
            binding_index = type_layout->getDescriptorSetDescriptorRangeIndexOffset(set_index, descriptor_range_index);
         }

         metadata.parameters.push_back(ShaderParameter{
             .name = std::move(full_name),
             .type_name = reflectedTypeName(leaf_type_layout),
             .binding = reflectedIndex(binding_index),
             .set = reflectedIndex(set_index),
             .binding_kind = std::string(toBindingTypeName(binding_type))});
      }
   }

   /// @brief Copies a Slang SPIR-V blob into the metadata-owned word vector.
   [[nodiscard]] std::expected<std::vector<std::uint32_t>, vve::Error> copySpirvWords(slang::IBlob *code) {
      if (code == nullptr || code->getBufferPointer() == nullptr || code->getBufferSize() == 0) {
         return std::unexpected(vve::Error::internal_error);
      }

      const auto byte_count = code->getBufferSize();
      if (byte_count % sizeof(std::uint32_t) != 0) {
         return std::unexpected(vve::Error::internal_error);
      }

      std::vector<std::uint32_t> words(byte_count / sizeof(std::uint32_t));
      std::memcpy(words.data(), code->getBufferPointer(), byte_count);
      return words;
   }

   } // namespace

   /**
    * @brief Concrete shader-system implementation used by v3.
    */
   class SlangShaderSystemImplementation {
   public:
      /// @brief Returns the subsystem name for diagnostics.
      [[nodiscard]] std::string_view name() const noexcept { return "SlangShaderSystem"; }

      /// @brief Compiles a Slang source file and reflects its resource metadata.
      [[nodiscard]] std::expected<ShaderMetadata, vve::Error>
      reflect(const std::filesystem::path &shader_path, vve::RendererKind renderer, vve::ShadowKind shadow) {
         const auto absolute_shader_path = std::filesystem::absolute(shader_path);
         const auto shader_source = readShaderSource(absolute_shader_path);
         if (!shader_source) {
            return std::unexpected(shader_source.error());
         }

         return compileAndReflect(*shader_source, renderer, shadow);
      }

      /// @brief Compiles and reflects a shader source payload loaded by another subsystem.
      [[nodiscard]] std::expected<ShaderMetadata, vve::Error>
      compileAndReflect(const ShaderSource &shader_source, vve::RendererKind renderer, vve::ShadowKind shadow) {
         if (!ensureGlobalSession()) {
            return std::unexpected(vve::Error::internal_error);
         }

         const auto absolute_shader_path = std::filesystem::absolute(shader_source.source_path);

         slang::TargetDesc target_desc{};
         target_desc.format = SLANG_SPIRV;
         target_desc.profile = global_session_->findProfile("spirv_1_5");
         target_desc.flags = SLANG_TARGET_FLAG_GENERATE_SPIRV_DIRECTLY;

         std::array target_options{
             slang::CompilerOptionEntry{
                 .name = slang::CompilerOptionName::VulkanEmitReflection,
                 .value = {.kind = slang::CompilerOptionValueKind::Int, .intValue0 = 1}},
             slang::CompilerOptionEntry{
                 .name = slang::CompilerOptionName::PreserveParameters,
                 .value = {.kind = slang::CompilerOptionValueKind::Int, .intValue0 = 1}}};
         target_desc.compilerOptionEntries = target_options.data();
         target_desc.compilerOptionEntryCount = static_cast<std::uint32_t>(target_options.size());

         const auto shader_directory = absolute_shader_path.parent_path().generic_string();
         std::array search_paths{shader_directory.c_str()};

         slang::SessionDesc session_desc{};
         session_desc.targets = &target_desc;
         session_desc.targetCount = 1;
         session_desc.defaultMatrixLayoutMode = SLANG_MATRIX_LAYOUT_COLUMN_MAJOR;
         session_desc.searchPaths = search_paths.data();
         session_desc.searchPathCount = static_cast<SlangInt>(search_paths.size());

         Slang::ComPtr<slang::ISession> session;
         if (SLANG_FAILED(global_session_->createSession(session_desc, session.writeRef())) || session == nullptr) {
            logSlangFailure("failed to create Slang compile session", nullptr);
            return std::unexpected(vve::Error::internal_error);
         }

         Slang::ComPtr<slang::IBlob> diagnostics;
         Slang::ComPtr<slang::IModule> module;
         const auto module_name = makeModuleName(absolute_shader_path);
         const auto source_path_string = absolute_shader_path.generic_string();
         module.attach(session->loadModuleFromSourceString(module_name.c_str(),
                                                           source_path_string.c_str(),
                                                           shader_source.source_code.c_str(), diagnostics.writeRef()));
         if (module == nullptr) {
            logSlangFailure("failed to load Slang module", diagnostics);
            return std::unexpected(vve::Error::invalid_argument);
         }

         const auto entry_point_count = module->getDefinedEntryPointCount();
         if (entry_point_count <= 0) {
            logSlangFailure("shader module defines no entry points", nullptr);
            return std::unexpected(vve::Error::invalid_argument);
         }

         std::vector<Slang::ComPtr<slang::IEntryPoint>> entry_points{};
         entry_points.reserve(static_cast<std::size_t>(entry_point_count));
         std::vector<slang::IComponentType *> components{};
         components.reserve(static_cast<std::size_t>(entry_point_count) + 1);
         components.push_back(module);

         for (SlangInt32 entry_point_index = 0; entry_point_index < entry_point_count; ++entry_point_index) {
            Slang::ComPtr<slang::IEntryPoint> entry_point;
            diagnostics.setNull();
            if (SLANG_FAILED(module->getDefinedEntryPoint(entry_point_index, entry_point.writeRef())) ||
                entry_point == nullptr) {
               logSlangFailure("failed to enumerate Slang entry point", diagnostics);
               return std::unexpected(vve::Error::invalid_argument);
            }

            components.push_back(entry_point);
            entry_points.push_back(entry_point);
         }

         Slang::ComPtr<slang::IComponentType> program;
         diagnostics.setNull();
         if (SLANG_FAILED(session->createCompositeComponentType(components.data(), static_cast<SlangInt>(components.size()),
                                                               program.writeRef(), diagnostics.writeRef())) ||
             program == nullptr) {
            logSlangFailure("failed to compose Slang program", diagnostics);
            return std::unexpected(vve::Error::invalid_argument);
         }

         Slang::ComPtr<slang::IComponentType> linked_program;
         diagnostics.setNull();
         if (SLANG_FAILED(program->link(linked_program.writeRef(), diagnostics.writeRef())) || linked_program == nullptr) {
            logSlangFailure("failed to link Slang program", diagnostics);
            return std::unexpected(vve::Error::invalid_argument);
         }

         diagnostics.setNull();
         auto *layout = linked_program->getLayout(0, diagnostics.writeRef());
         if (layout == nullptr) {
            logSlangFailure("failed to reflect Slang program layout", diagnostics);
            return std::unexpected(vve::Error::invalid_argument);
         }

         ShaderMetadata metadata{};
         metadata.handle = ShaderHandle{detail::makeStableHandle(absolute_shader_path.generic_string())};
         metadata.shader_name = absolute_shader_path.filename().string();
         metadata.intended_renderer = std::string(toRendererName(renderer));
         metadata.intended_shadow = std::string(toShadowName(shadow));

         for (SlangUInt entry_point_index = 0; entry_point_index < layout->getEntryPointCount(); ++entry_point_index) {
            auto *entry_point_layout = layout->getEntryPointByIndex(entry_point_index);
            if (entry_point_layout == nullptr) {
               continue;
            }

            const auto stage = toShaderStage(entry_point_layout->getStage());
            if (!stage) {
               continue;
            }

            if (!std::ranges::contains(metadata.stages, *stage)) {
               metadata.stages.push_back(*stage);
            }

            Slang::ComPtr<slang::IBlob> code;
            diagnostics.setNull();
            if (SLANG_FAILED(linked_program->getEntryPointCode(static_cast<SlangInt>(entry_point_index), 0,
                                                               code.writeRef(), diagnostics.writeRef()))) {
               logSlangFailure("failed to compile Slang entry point", diagnostics);
               return std::unexpected(vve::Error::invalid_argument);
            }

            auto spirv_words = copySpirvWords(code);
            if (!spirv_words) {
               return std::unexpected(spirv_words.error());
            }

            const auto *entry_point_name = entry_point_layout->getName();
            metadata.binaries.push_back(ShaderStageBinary{
                .stage = *stage,
                .entry_point = entry_point_name != nullptr ? std::string(entry_point_name) : std::string{},
                .spirv_words = std::move(*spirv_words)});
         }

         for (unsigned parameter_index = 0; parameter_index < layout->getParameterCount(); ++parameter_index) {
            auto *parameter = layout->getParameterByIndex(parameter_index);
            if (parameter == nullptr) {
               continue;
            }

            const auto *parameter_name = parameter->getName();
            const auto reflected_name = parameter_name != nullptr ? std::string_view(parameter_name) : std::string_view{};
            appendReflectedParameter(metadata, reflected_name, parameter);
            collectParameters(metadata, reflected_name, parameter->getTypeLayout());
            collectBindingRanges(metadata, reflected_name, parameter->getTypeLayout());
         }

         return metadata;
      }

   private:
      /// @brief Lazily creates the shared Slang global session.
      [[nodiscard]] bool ensureGlobalSession() {
         if (global_session_ != nullptr) {
            return true;
         }

         SlangGlobalSessionDesc session_desc{};
         session_desc.minLanguageVersion = SLANG_LANGUAGE_VERSION_2026;
         return SLANG_SUCCEEDED(slang_createGlobalSession2(&session_desc, global_session_.writeRef())) &&
                global_session_ != nullptr;
      }

      Slang::ComPtr<slang::IGlobalSession> global_session_{}; ///< Shared Slang compiler session.
   };

   /// @brief Constructs the public shader-system facade around the concrete implementation.
   VVE_V3_DEFINE_FACADE_CTOR(ShaderSystemFacade, SlangShaderSystemImplementation, (), ())

   /// @brief Returns the shader-system name for the public facade.
   VVE_V3_DEFINE_FACADE_METHOD(ShaderSystemFacade, SlangShaderSystemImplementation, name, (), (), const noexcept,
                               std::string_view)

   /// @brief Reflects a shader through the public facade.
   VVE_V3_DEFINE_FACADE_METHOD(ShaderSystemFacade, SlangShaderSystemImplementation, reflect,
                               (const std::filesystem::path &shader_path, vve::RendererKind renderer,
                                vve::ShadowKind shadow),
                               (shader_path, renderer, shadow), , std::expected<ShaderMetadata, vve::Error>)

   /// @brief Compiles and reflects already-loaded shader source through the public facade.
   VVE_V3_DEFINE_FACADE_METHOD(ShaderSystemFacade, SlangShaderSystemImplementation, compileAndReflect,
                               (const ShaderSource &shader_source, vve::RendererKind renderer,
                                vve::ShadowKind shadow),
                               (shader_source, renderer, shadow), , std::expected<ShaderMetadata, vve::Error>)

   /// @brief Emits the explicit shader-system facade instantiation for v3.
   template class ShaderSystemFacade<SlangShaderSystemImplementation>;

} // namespace vve::v3
