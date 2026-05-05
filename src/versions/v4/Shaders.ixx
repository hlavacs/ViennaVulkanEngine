export module VEEngine.V4:Shaders;
import std;
export import :Types;

/// @file
/// @brief Stub shader descriptors; Slang compilation/reflection will plug in later.

export namespace vve::v4 {

   /// @brief Shader stage categories used by reflection descriptors.
   enum class ShaderStage {
      vertex,   ///< Vertex shader stage.
      fragment, ///< Fragment shader stage.
      compute   ///< Compute shader stage.
   };

} // namespace vve::v4

namespace vve::v4 {

   /// @brief Internal shader program record.
   struct ShaderRecord {
      ShaderHandle handle{};                    ///< Stable shader handle.
      ObjectName name{};                        ///< Human-readable shader name.
      Vector<ShaderStage> stages{};             ///< Shader stages present in the program.
      Vector<std::string> reflected_bindings{}; ///< Placeholder binding names from reflection.
   };

} // namespace vve::v4

export namespace vve::v4 {

   /// @brief Minimal shader table; no Slang calls happen in this stub.
   class ShaderSystem {
   public:
      /// @brief Adds a shader record and returns its handle.
      [[nodiscard]] std::expected<ShaderHandle, Error> addShader(ObjectName name, Vector<ShaderStage> stages) {
         const auto handle = makeCounterHandle<ShaderHandle>();
         const auto [_, inserted] = shaders_.emplace(
            handle, ShaderRecord{.handle = handle, .name = std::move(name), .stages = std::move(stages)});
         if (!inserted) { return std::unexpected(Error::duplicate_object); }
         return handle;
      }

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

   private:
      std::map<ShaderHandle, ShaderRecord> shaders_{}; ///< Shaders by handle.
   };

} // namespace vve::v4
