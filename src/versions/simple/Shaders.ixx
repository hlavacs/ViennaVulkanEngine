module;

#include <slang/slang-com-ptr.h>
#include <slang/slang.h>
#include <vulkan/vulkan_core.h>

#if defined(_WIN32) && defined(VVE_ENGINE_BUILD)
#define VVE_SIMPLE_API __declspec(dllexport)
#else
#define VVE_SIMPLE_API
#endif

export module VEEngine.Simple.Shaders;
import std;
export import VEEngine.Simple.Vector;
export import VEEngine.Simple.Error;
export import VEEngine.Simple.Handle;
export import VEEngine.Types;

/// @file
/// @brief Slang-backed shader compilation and compact reflection records for simple experiments.

export namespace vve::simple {
	struct ShaderHandleTag {};											///< simple-internal shader descriptor handle tag.
	using ShaderHandle = TypedHandle<ShaderHandleTag>;		///< simple-internal shader descriptor handle.

	/// @brief Shader stage categories used by reflection descriptors.
	enum class ShaderStage {
		vertex,																	///< Vertex shader stage.
		fragment,																///< Fragment shader stage.
		compute																	///< Compute shader stage.
	};

	/// @brief One compiled entry point and its SPIR-V words.
	struct ShaderStageBinary {
		ShaderStage stage{ShaderStage::vertex};						///< Stage represented by this binary.
		std::string entry_point{};											///< Slang entry-point function name.
		Vector<std::uint32_t> spirv{};									///< Compiled SPIR-V words.
	};

	/// @brief One compact reflected shader binding or parameter path.
	struct ShaderBindingReflection {
		std::string name{};													///< Flattened reflected name, for example gVveForward.frame.
		std::string category{};												///< Slang parameter category.
		std::string type{};													///< Slang binding type when available.
		std::uint32_t set{0};												///< Reflected Vulkan descriptor set/register space.
		std::uint32_t binding{0};											///< Reflected Vulkan descriptor binding/register index.
		std::uint32_t count{1};												///< Descriptor array element count of a binding range.
	};

	/// @brief Shader entry-point reflection summary.
	struct ShaderEntryPointReflection {
		std::string name{};													///< Entry-point function name.
		ShaderStage stage{ShaderStage::vertex};						///< Reflected shader stage.
		Vector<std::string> varying_inputs{};							///< Vertex/pixel input semantic names.
		Vector<std::string> varying_outputs{};							///< Vertex/pixel output semantic names.
	};

	/// @brief Compact reflection result stored beside compiled SPIR-V.
	struct ShaderReflection {
		Vector<ShaderEntryPointReflection> entry_points{};			///< Entry-point summaries.
		Vector<ShaderBindingReflection> bindings{};					///< Flattened parameter/binding summaries.
		Vector<std::string> type_names{};								///< Types found by name for verification.
	};

} // namespace vve::simple

namespace vve::simple {

	/// @brief Internal shader program record.
	struct ShaderRecord {
		ShaderHandle handle{};												///< Stable shader handle.
		ObjectName name{};													///< Human-readable shader name.
		std::filesystem::path source{};									///< Slang source path.
		Vector<ShaderStage> stages{};										///< Stages present in the program.
		Vector<ShaderStageBinary> binaries{};							///< Compiled SPIR-V by entry point.
		ShaderReflection reflection{};									///< Compact reflected layout information.
	};

} // namespace vve::simple

export namespace vve::simple {

	/// @brief Slang-backed shader table storing SPIR-V and reflection data.
	class ShaderSystem {
	public:
		[[nodiscard]] VVE_SIMPLE_API std::expected<ShaderHandle, Error> addShader(ObjectName name,
																										Vector<ShaderStage> stages);
		[[nodiscard]] VVE_SIMPLE_API std::expected<ShaderHandle, Error>
		compileAndReflect(const std::filesystem::path &source, Vector<std::string> entry_points);
		[[nodiscard]] VVE_SIMPLE_API auto containsShader(ShaderHandle handle) const											-> bool;
		[[nodiscard]] VVE_SIMPLE_API auto shaderName(ShaderHandle handle) const												-> std::expected<ObjectName, Error>;
		[[nodiscard]] VVE_SIMPLE_API auto shaderStageCount(ShaderHandle handle) const										-> std::expected<std::size_t, Error>;
		[[nodiscard]] VVE_SIMPLE_API std::expected<std::size_t, Error> spirvWordCount(ShaderHandle handle,
																											ShaderStage stage) const;
		[[nodiscard]] VVE_SIMPLE_API std::expected<Vector<std::uint32_t>, Error> stageSpirv(ShaderHandle handle,
																													ShaderStage stage) const;
		[[nodiscard]] VVE_SIMPLE_API std::expected<bool, Error> hasReflectedBinding(ShaderHandle handle,
																										std::string_view name) const;
		[[nodiscard]] VVE_SIMPLE_API std::expected<bool, Error> hasReflectedType(ShaderHandle handle,
																									std::string_view name) const;
		[[nodiscard]] VVE_SIMPLE_API std::expected<Vector<ShaderBindingReflection>, Error>
		reflectedBindings(ShaderHandle handle) const;
		[[nodiscard]] VVE_SIMPLE_API std::expected<Vector<VkDescriptorSetLayoutBinding>, Error>
		descriptorSetLayoutBindings(ShaderHandle handle, std::uint32_t set) const;
		[[nodiscard]] VVE_SIMPLE_API std::expected<Vector<ShaderEntryPointReflection>, Error>
		reflectedEntryPoints(ShaderHandle handle) const;

	private:
		[[nodiscard]] static auto safeName(const char *name)																	-> std::string;
		[[nodiscard]] static auto toShaderStage(SlangStage stage)															-> ShaderStage;
		[[nodiscard]] static auto parameterCategoryName(slang::ParameterCategory category)							-> std::string;
		[[nodiscard]] static auto bindingTypeName(slang::BindingType type)												-> std::string;
		[[nodiscard]] static auto spirvWords(slang::IBlob *code)																-> std::optional<Vector<std::uint32_t>>;
		static void collectFields(ShaderReflection &reflection, std::string prefix,
											slang::TypeLayoutReflection *type_layout);
		static void collectElementFields(ShaderReflection &reflection, const std::string &prefix,
													slang::TypeLayoutReflection *type_layout);
		static void collectBindingRanges(ShaderReflection &reflection, std::string prefix,
													slang::TypeLayoutReflection *type_layout);
		static auto collectEntryPoint(ShaderReflection &reflection, slang::EntryPointReflection *entry_point)	-> void;
		static void collectReflection(ShaderReflection &reflection, slang::ProgramLayout *layout,
												const Vector<std::string> &entry_points);

		[[nodiscard]] const ShaderRecord *find(ShaderHandle handle) const;

		Slang::ComPtr<slang::IGlobalSession> global_session_{};	///< Shared Slang compiler session.
		std::map<ShaderHandle, ShaderRecord> shaders_{};			///< Shaders by handle.
	};

} // namespace vve::simple

export namespace vve::simple {

	/// @brief Adds a shader record without compiling, useful for small table tests.
	VVE_SIMPLE_API auto ShaderSystem::addShader(ObjectName name, Vector<ShaderStage> stages)									-> std::expected<ShaderHandle, Error>{
		const auto handle = makeCounterHandle<ShaderHandle>();
		const auto [_, inserted] = shaders_.emplace(
			handle, ShaderRecord{.handle = handle, .name = std::move(name), .stages = std::move(stages)});
		if (!inserted) { return std::unexpected(Error::duplicate_object); }
		return handle;
	}

	/// @brief Returns whether a shader exists.
	VVE_SIMPLE_API bool ShaderSystem::containsShader(ShaderHandle handle) const { return shaders_.contains(handle); }

	/// @brief Returns the shader name.
	VVE_SIMPLE_API auto ShaderSystem::shaderName(ShaderHandle handle) const															-> std::expected<ObjectName, Error>{
		const auto shader = shaders_.find(handle);
		if (shader == shaders_.end()) { return std::unexpected(Error::missing_object); }
		return shader->second.name;
	}

	/// @brief Returns the number of stages in a shader program.
	VVE_SIMPLE_API auto ShaderSystem::shaderStageCount(ShaderHandle handle) const													-> std::expected<std::size_t, Error>{
		const auto shader = shaders_.find(handle);
		if (shader == shaders_.end()) { return std::unexpected(Error::missing_object); }
		return shader->second.stages.size();
	}

	const ShaderRecord *ShaderSystem::find(ShaderHandle handle) const {
		const auto shader = shaders_.find(handle);
		return shader == shaders_.end() ? nullptr : std::addressof(shader->second);
	}

	std::string ShaderSystem::safeName(const char *name) { return name == nullptr ? std::string{} : name; }

	auto ShaderSystem::toShaderStage(SlangStage stage)																					-> ShaderStage{
		switch (stage) {
		case SLANG_STAGE_FRAGMENT:
			return ShaderStage::fragment;
		case SLANG_STAGE_COMPUTE:
			return ShaderStage::compute;
		default:
			return ShaderStage::vertex;
		}
	}

	auto ShaderSystem::parameterCategoryName(slang::ParameterCategory category)												-> std::string{
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

	auto ShaderSystem::bindingTypeName(slang::BindingType type)																		-> std::string{
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

	auto ShaderSystem::spirvWords(slang::IBlob *code)																					-> std::optional<Vector<std::uint32_t>>{
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

	void ShaderSystem::collectFields(ShaderReflection &reflection, std::string prefix,
												slang::TypeLayoutReflection *type_layout) {
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

	void ShaderSystem::collectElementFields(ShaderReflection &reflection, const std::string &prefix,
															slang::TypeLayoutReflection *type_layout) {
		if (type_layout == nullptr) { return; }
		auto *element = type_layout->getElementTypeLayout();
		if (element == nullptr || element == type_layout) { return; }
		collectFields(reflection, prefix, element);
	}

	void ShaderSystem::collectBindingRanges(ShaderReflection &reflection, std::string prefix,
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
				.binding = static_cast<std::uint32_t>(type_layout->getBindingRangeFirstDescriptorRangeIndex(range)),
				.count = static_cast<std::uint32_t>(std::max<SlangInt>(type_layout->getBindingRangeBindingCount(range), 1))});
		}
	}

	auto ShaderSystem::collectEntryPoint(ShaderReflection &reflection, slang::EntryPointReflection *entry_point)	-> void{
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

	void ShaderSystem::collectReflection(ShaderReflection &reflection, slang::ProgramLayout *layout,
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
			// An array type layout already reports its element's binding range with the array count; descending would duplicate it.
			if (auto *element = variable->getTypeLayout()->getElementTypeLayout();
				 element != nullptr && variable->getTypeLayout()->getKind() != slang::TypeReflection::Kind::Array) {
				collectBindingRanges(reflection, name, element);
			}
		}

		for (const auto &entry : entry_points) {
			collectEntryPoint(reflection, layout->findEntryPointByName(entry.c_str()));
		}
	}

	VVE_SIMPLE_API std::expected<ShaderHandle, Error> ShaderSystem::compileAndReflect(const std::filesystem::path &source,
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

		Slang::ComPtr<slang::IComponentType> linked_program{};
		diagnostics.setNull();
		if (SLANG_FAILED(program->link(linked_program.writeRef(), diagnostics.writeRef()))) {
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

	VVE_SIMPLE_API auto ShaderSystem::spirvWordCount(ShaderHandle handle, ShaderStage stage) const							-> std::expected<std::size_t, Error>{
		const auto *shader = find(handle);
		if (shader == nullptr) { return std::unexpected(Error::missing_object); }
		const auto it = std::ranges::find_if(shader->binaries, [stage](const auto &binary) {
			return binary.stage == stage;
		});
		if (it == shader->binaries.end()) { return std::unexpected(Error::missing_object); }
		return it->spirv.size();
	}

	VVE_SIMPLE_API std::expected<Vector<std::uint32_t>, Error> ShaderSystem::stageSpirv(ShaderHandle handle,
																								ShaderStage stage) const {
		const auto *shader = find(handle);
		if (shader == nullptr) { return std::unexpected(Error::missing_object); }
		const auto it = std::ranges::find_if(shader->binaries, [stage](const auto &binary) {
			return binary.stage == stage;
		});
		if (it == shader->binaries.end()) { return std::unexpected(Error::missing_object); }
		return it->spirv;
	}

	VVE_SIMPLE_API auto ShaderSystem::hasReflectedBinding(ShaderHandle handle, std::string_view name) const				-> std::expected<bool, Error>{
		const auto *shader = find(handle);
		if (shader == nullptr) { return std::unexpected(Error::missing_object); }
		return std::ranges::any_of(shader->reflection.bindings, [name](const auto &binding) {
			return binding.name == name;
		});
	}

	VVE_SIMPLE_API auto ShaderSystem::hasReflectedType(ShaderHandle handle, std::string_view name) const					-> std::expected<bool, Error>{
		const auto *shader = find(handle);
		if (shader == nullptr) { return std::unexpected(Error::missing_object); }
		return std::ranges::any_of(shader->reflection.type_names, [name](const auto &type) {
			return type == name;
		});
	}

	VVE_SIMPLE_API auto ShaderSystem::reflectedBindings(ShaderHandle handle) const													-> std::expected<Vector<ShaderBindingReflection>, Error>{
		const auto *shader = find(handle);
		if (shader == nullptr) { return std::unexpected(Error::missing_object); }
		return shader->reflection.bindings;
	}

	/// @brief Converts reflected set bindings into the Vulkan layout contract used by the forward renderer.
	VVE_SIMPLE_API auto ShaderSystem::descriptorSetLayoutBindings(ShaderHandle handle, std::uint32_t set) const			-> std::expected<Vector<VkDescriptorSetLayoutBinding>, Error>{
		const auto *shader = find(handle);
		if (shader == nullptr) { return std::unexpected(Error::missing_object); }
		const auto reflectedBindingIndex = [&shader](const ShaderBindingReflection &range) -> std::optional<std::uint32_t> {
			const auto parameter = std::ranges::find_if(shader->reflection.bindings, [&range](const auto &candidate) {
				return candidate.category != "binding_range" && candidate.name == range.name && candidate.set == range.set;
			});
			return parameter == shader->reflection.bindings.end() ? std::nullopt : std::optional{parameter->binding};
		};
		Vector<VkDescriptorSetLayoutBinding> bindings{};
		for (const auto &binding : shader->reflection.bindings) {
			if (binding.category != "binding_range" || binding.set != set) { continue; } // Only descriptor ranges become set-layout entries.
			if (binding.type != "constant_buffer" && binding.type != "combined_texture_sampler") { continue; }
			const auto descriptorBinding = reflectedBindingIndex(binding);
			if (!descriptorBinding) { return std::unexpected(Error::invalid_argument); }
			const VkShaderStageFlags stages = *descriptorBinding == 0U
															? VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT
															: VK_SHADER_STAGE_FRAGMENT_BIT;
			bindings.push_back(VkDescriptorSetLayoutBinding{
				.binding = *descriptorBinding,
				.descriptorType = binding.type == "constant_buffer" ? VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER : VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				.descriptorCount = binding.count,
				.stageFlags = stages,
				.pImmutableSamplers = nullptr});
		}
		constexpr std::array expectedBindings{0U, 1U, 2U, 4U, 5U, 6U, 7U}; // Current shader layout order is part of the renderer contract.
		if (bindings.size() != expectedBindings.size()) { return std::unexpected(Error::invalid_argument); }
		for (std::size_t index{}; index < expectedBindings.size(); ++index) {
			if (bindings[index].binding != expectedBindings[index]) { return std::unexpected(Error::invalid_argument); }
		}
		return bindings;
	}

	VVE_SIMPLE_API auto ShaderSystem::reflectedEntryPoints(ShaderHandle handle) const													-> std::expected<Vector<ShaderEntryPointReflection>, Error>{
		const auto *shader = find(handle);
		if (shader == nullptr) { return std::unexpected(Error::missing_object); }
		return shader->reflection.entry_points;
	}

} // namespace vve::simple
