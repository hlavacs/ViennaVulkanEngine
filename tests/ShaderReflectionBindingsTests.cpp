/**
 * @file
 * @brief CPU-only Slang reflection guard for the simple forward descriptor contract.
 *
 * Functional objects:
 * - main: compiles simple_forward.slang, reflects set 0, prints every descriptor binding,
 *   and verifies the renderer layout and pool guard contract without creating Vulkan objects.
 */

#include <vulkan/vulkan_core.h>

import std;

import VEEngine.Simple.Shaders;

namespace {

/// @brief One expected reflected descriptor layout binding.
struct ExpectedBinding {
   std::uint32_t binding{};                  ///< Vulkan descriptor binding index.
   VkDescriptorType descriptor_type{};       ///< Vulkan descriptor resource category.
   std::uint32_t descriptor_count{};         ///< Descriptor array size expected by the renderer.
   VkShaderStageFlags stage_flags{};         ///< Shader stages allowed to read the descriptor.
};

/// @brief Provides stable descriptor type names for deterministic diagnostics.
[[nodiscard]] std::string_view descriptorTypeName(VkDescriptorType type) {
   switch (type) {
   case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
      return "uniform_buffer";
   case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
      return "combined_image_sampler";
   default:
      return "unexpected_descriptor_type";
   }
}

/// @brief Emits the reflected set-layout bindings in a stable, LLM-readable form.
void printBindings(const vve::simple::Vector<VkDescriptorSetLayoutBinding> &bindings) {
   std::println("shader_reflection_bindings count={}", bindings.size());
   for (const auto &binding : bindings) {
      std::println("shader_reflection_binding binding={} type={} count={} stageFlags=0x{:x}",
                   binding.binding, descriptorTypeName(binding.descriptorType), binding.descriptorCount,
                   static_cast<std::uint32_t>(binding.stageFlags));
   }
}

/// @brief Emits raw Slang reflection rows when conversion to Vulkan layout bindings rejects the contract.
void printRawBindings(const vve::simple::Vector<vve::simple::ShaderBindingReflection> &bindings) {
   std::println("shader_reflection_raw_bindings count={}", bindings.size());
   for (const auto &binding : bindings) {
      std::println("shader_reflection_raw_binding name={} category={} type={} set={} binding={}", binding.name,
                   binding.category, binding.type, binding.set, binding.binding);
   }
}

/// @brief Verifies one reflected row against the exact descriptor contract.
[[nodiscard]] bool matchesExpected(const VkDescriptorSetLayoutBinding &actual, const ExpectedBinding &expected) {
   return actual.binding == expected.binding && actual.descriptorType == expected.descriptor_type &&
          actual.descriptorCount == expected.descriptor_count && actual.stageFlags == expected.stage_flags;
}

} // namespace

int main() {
   vve::simple::ShaderSystem shaders{};
   vve::simple::Vector<std::string> entries{};
   entries.push_back("vertexMain");
   entries.push_back("fragmentMain");

   const auto shader = shaders.compileAndReflect(std::filesystem::path{VVE_SIMPLE_SHADER_SOURCE}, std::move(entries));
   if (!shader) {
      std::println("shader_reflection_bindings failed stage=compile_and_reflect error={}",
                   vve::simple::errorName(shader.error()));
      return 1;
   }

   const auto bindings = shaders.descriptorSetLayoutBindings(*shader, 0U);
   if (!bindings) {
      std::println("shader_reflection_bindings failed stage=descriptor_set_layout error={}",
                   vve::simple::errorName(bindings.error()));
      if (const auto raw_bindings = shaders.reflectedBindings(*shader); raw_bindings) { printRawBindings(*raw_bindings); }
      return 2;
   }

   printBindings(*bindings);

   constexpr VkShaderStageFlags vertex_fragment{VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT};
   constexpr std::array expected{
      ExpectedBinding{0U, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1U, vertex_fragment},
      ExpectedBinding{1U, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1U, VK_SHADER_STAGE_FRAGMENT_BIT},
      ExpectedBinding{2U, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1U, VK_SHADER_STAGE_FRAGMENT_BIT},
      ExpectedBinding{4U, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1U, VK_SHADER_STAGE_FRAGMENT_BIT},
      ExpectedBinding{5U, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1U, VK_SHADER_STAGE_FRAGMENT_BIT},
      ExpectedBinding{6U, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1U, VK_SHADER_STAGE_FRAGMENT_BIT},
      ExpectedBinding{7U, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1U, VK_SHADER_STAGE_FRAGMENT_BIT},
   };

   if (bindings->size() != expected.size()) {
      std::println("shader_reflection_bindings failed stage=count expected={} actual={}", expected.size(),
                   bindings->size());
      return 3;
   }

   for (std::size_t index{}; index < expected.size(); ++index) {
      if (!matchesExpected((*bindings)[index], expected[index])) {
         std::println("shader_reflection_bindings failed stage=binding index={} expectedBinding={} expectedType={} "
                      "expectedCount={} expectedStageFlags=0x{:x}",
                      index, expected[index].binding, descriptorTypeName(expected[index].descriptor_type),
                      expected[index].descriptor_count, static_cast<std::uint32_t>(expected[index].stage_flags));
         return 4;
      }
   }

   std::println("shader_reflection_bindings passed ubo_count=1 combined_image_sampler_count=6");
   return 0;
}
