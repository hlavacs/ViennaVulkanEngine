// ============================================================================
// DISCLAIMER:
// This implementation is adapted from the vkgs rendering engine
// by Jaesung Kim (https://github.com/jaesung-cs/vkgs).
// All incorporated code sections are explicitly marked and comply
// with the original project's license.
// This document contains documentation comments created or co-authored
// with the assistance of generative AI. The implementation logic and
// source code were authored manually.
// ============================================================================

#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <unordered_map>
#include <fstream>


namespace vve {

/**
 * @brief PLY file data structure matching VKGS format
 */
struct PLYData {
    uint32_t pointCount = 0;
    uint32_t stride = 0;  // floats per point
    std::vector<uint32_t> offsets;  // 60 offsets for property positions (in float units)
    std::vector<char> rawData;      // raw binary data from PLY file
};

/**
 * @brief PLY file loader for Gaussian Splat assets
 *
 * Provides interface for loading .ply files containing gaussian splat data.
 * Loads raw PLY data in VKGS format for GPU-side parsing.
 */
class GaussianAssetLoader {
public:
    /**
     * @brief Load a .ply file and return raw PLY data
     * @param filename Path to the .ply file
     * @param outData Pointer to PLYData to populate
     * @return True if load was successful
     */
    static bool LoadPLY(const std::string& filename, PLYData* outData);

    /**
     * @brief Check if a file is a valid .ply file
     * @param filename Path to check
     * @return True if file has .ply extension and exists
     */
    static bool IsPLYFile(const std::string& filename);

private:
    /**
     * @brief Parse PLY header and extract property offsets
     * @param file Input file stream (positioned at start)
     * @param outOffsetMap Map of property name to byte offset
     * @param outStride Total stride in bytes
     * @param outPointCount Number of points in the file
     * @return True if header was successfully parsed
     */
    static bool ParsePLYHeader(std::ifstream& file,
                               std::unordered_map<std::string, int>& outOffsetMap,
                               uint32_t& outStride,
                               uint32_t& outPointCount);

    /**
     * @brief Map property offsets to VKGS format (60-element array)
     * @param offsetMap Property name to byte offset mapping
     * @param stride Stride in bytes
     * @return 60-element offset array in VKGS format (in float units)
     */
    static std::vector<uint32_t> MapOffsetsToVKGS(const std::unordered_map<std::string, int>& offsetMap, uint32_t stride);
};

}  // namespace vve
