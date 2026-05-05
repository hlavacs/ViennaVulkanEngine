export module VEEngine.V4:Shaders;
import std;
export import :Resources;

/// @file
/// @brief Stub shader descriptors; Slang compilation/reflection will plug in later.

export namespace vve::v4 {

   /// @brief Shader stage categories used by reflection descriptors.
   enum class ShaderStage {
      vertex,   ///< Vertex shader stage.
      fragment, ///< Fragment shader stage.
      compute   ///< Compute shader stage.
   };

   /// @brief Shader program descriptor with placeholder reflection data.
   struct ShaderDescriptor {
      using HandleType = ShaderHandle;         ///< Descriptor handle type.
      ShaderHandle handle{};                   ///< Stable shader handle.
      ObjectName name{};                       ///< Human-readable shader name.
      Vector<ShaderStage> stages{};            ///< Shader stages present in the program.
      Vector<std::string> reflected_bindings{}; ///< Placeholder binding names from reflection.
   };

   /// @brief Minimal shader table; no Slang calls happen in this stub.
   class ShaderSystem {
   public:
      /// @brief Adds a shader descriptor.
      [[nodiscard]] std::expected<void, Error> add(ShaderDescriptor shader) {
         if (!shader.handle.valid()) { return std::unexpected(Error::invalid_handle); }
         const auto [_, inserted] = shaders_.emplace(shader.handle, std::move(shader));
         return inserted ? std::expected<void, Error>{} : std::unexpected(Error::duplicate_object);
      }

      /// @brief Finds a shader by handle, or returns null.
      [[nodiscard]] const ShaderDescriptor *find(ShaderHandle handle) const {
         const auto shader = shaders_.find(handle);
         return shader == shaders_.end() ? nullptr : std::addressof(shader->second);
      }

   private:
      std::map<ShaderHandle, ShaderDescriptor> shaders_{}; ///< Shader descriptors by handle.
   };

} // namespace vve::v4
