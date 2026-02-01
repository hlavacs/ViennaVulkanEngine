// ============================================================================
// DISCLAIMER:
// This document contains documentation comments created or co-authored
// with the assistance of generative AI. The implementation logic and
// source code were authored manually.
// ============================================================================

#pragma once

namespace vve {

/**
 * @brief Gaussian splat component data structure
 *
 * Contains all information needed to render a Gaussian splat object,
 * including file path, transform, and rendering parameters.
 */
struct GaussianSplatData {
    std::string plyPath;                    // Path to .ply file containing gaussian splat data
    glm::mat4 transform{1.0f};              // Model transformation matrix
    bool isEnvironment = false;              // True if this is an environment/skybox splat
    float emissiveIntensity = 1.0f;         // Emissive light intensity for light probe extraction
    bool castShadows = false;                // TODO: Requires ray tracing implementation
    bool receiveShadows = true;              // Whether splat receives shadows from other objects
    uint32_t splatCount = 0;                 // Number of gaussian splats in this object
};

// Strong type wrapping GaussianSplatData for use as ECS component
using GaussianSplat = vsty::strong_type_t<GaussianSplatData, vsty::counter<>>;

// Handle type for referencing gaussian splat component
using GaussianSplatHandle = vsty::strong_type_t<vecs::Handle, vsty::counter<>>;

}  // namespace vve
