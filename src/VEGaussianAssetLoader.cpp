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

#include "VEGaussianAssetLoader.h"
#include <iostream>
#include <sstream>
#include <filesystem>


namespace vve {

bool GaussianAssetLoader::LoadPLY(const std::string& filename, PLYData* outData) {
    if (!outData) {
        std::cerr << "GaussianAssetLoader::LoadPLY - outData is null" << std::endl;
        return false;
    }

    if (!IsPLYFile(filename)) {
        std::cerr << "GaussianAssetLoader::LoadPLY - Invalid .ply file: " << filename << std::endl;
        return false;
    }

    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "GaussianAssetLoader::LoadPLY - Failed to open file: " << filename << std::endl;
        return false;
    }

    // Parse header
    std::unordered_map<std::string, int> offsetMap;
    uint32_t stride = 0;
    uint32_t pointCount = 0;

    if (!ParsePLYHeader(file, offsetMap, stride, pointCount)) {
        std::cerr << "GaussianAssetLoader::LoadPLY - Failed to parse PLY header" << std::endl;
        return false;
    }

    // Map offsets to VKGS format
    outData->offsets = MapOffsetsToVKGS(offsetMap, stride);
    outData->pointCount = pointCount;
    outData->stride = stride / 4;  // Convert bytes to floats

    // Read binary data
    size_t dataSize = static_cast<size_t>(stride) * pointCount;
    outData->rawData.resize(dataSize);

    file.read(outData->rawData.data(), dataSize);
    if (!file) {
        std::cerr << "GaussianAssetLoader::LoadPLY - Failed to read binary data" << std::endl;
        return false;
    }

    file.close();

    std::cout << "GaussianAssetLoader: Loaded " << filename << " ("
              << pointCount << " points, " << stride << " bytes/point)" << std::endl;

    return true;
}

bool GaussianAssetLoader::IsPLYFile(const std::string& filename) {
    std::filesystem::path path(filename);

    if (path.extension() != ".ply") {
        return false;
    }

    if (!std::filesystem::exists(path)) {
        std::cerr << "GaussianAssetLoader::IsPLYFile - File does not exist: " << filename << std::endl;
        return false;
    }

    return true;
}

bool GaussianAssetLoader::ParsePLYHeader(std::ifstream& file,
                                         std::unordered_map<std::string, int>& outOffsetMap,
                                         uint32_t& outStride,
                                         uint32_t& outPointCount) {
    int offset = 0;
    size_t pointCount = 0;
    std::string line;

    while (std::getline(file, line)) {
        if (line == "end_header") {
            break;
        }

        std::istringstream iss(line);
        std::string word;
        iss >> word;

        if (word == "property") {
            int size = 0;
            std::string type, property;
            iss >> type >> property;

            if (type == "float") {
                size = 4;
            } else {
                std::cerr << "GaussianAssetLoader: Unsupported property type: " << type << std::endl;
                return false;
            }

            outOffsetMap[property] = offset;
            offset += size;

        } else if (word == "element") {
            std::string type;
            size_t count;
            iss >> type >> count;

            if (type == "vertex") {
                pointCount = count;
            }
        }
    }

    if (pointCount == 0) {
        std::cerr << "GaussianAssetLoader: No vertices found in PLY file" << std::endl;
        return false;
    }

    outStride = offset;
    outPointCount = static_cast<uint32_t>(pointCount);
    return true;
}

std::vector<uint32_t> GaussianAssetLoader::MapOffsetsToVKGS(const std::unordered_map<std::string, int>& offsetMap,
                                                             uint32_t stride) {
    // VKGS offset format (60 elements):
    // [0-2]: position (x, y, z)
    // [3-5]: scale (scale_0, scale_1, scale_2)
    // [6-9]: rotation quaternion (rot_1=qx, rot_2=qy, rot_3=qz, rot_0=qw)
    // [10-57]: spherical harmonics (f_dc_* and f_rest_*)
    // [58]: opacity
    // [59]: stride in floats

    std::vector<uint32_t> offsets(60, 0);

    // Position
    offsets[0] = offsetMap.at("x") / 4;
    offsets[1] = offsetMap.at("y") / 4;
    offsets[2] = offsetMap.at("z") / 4;

    // Scale
    offsets[3] = offsetMap.at("scale_0") / 4;
    offsets[4] = offsetMap.at("scale_1") / 4;
    offsets[5] = offsetMap.at("scale_2") / 4;

    // Rotation (qx, qy, qz, qw)
    offsets[6] = offsetMap.at("rot_1") / 4;
    offsets[7] = offsetMap.at("rot_2") / 4;
    offsets[8] = offsetMap.at("rot_3") / 4;
    offsets[9] = offsetMap.at("rot_0") / 4;

    // Spherical harmonics DC components
    offsets[10 + 0] = offsetMap.at("f_dc_0") / 4;
    offsets[10 + 16] = offsetMap.at("f_dc_1") / 4;
    offsets[10 + 32] = offsetMap.at("f_dc_2") / 4;

    // Spherical harmonics rest components (45 values)
    for (int i = 0; i < 15; ++i) {
        std::string rest_r = "f_rest_" + std::to_string(i);
        std::string rest_g = "f_rest_" + std::to_string(15 + i);
        std::string rest_b = "f_rest_" + std::to_string(30 + i);

        offsets[10 + 1 + i] = offsetMap.at(rest_r) / 4;
        offsets[10 + 17 + i] = offsetMap.at(rest_g) / 4;
        offsets[10 + 33 + i] = offsetMap.at(rest_b) / 4;
    }

    // Opacity
    offsets[58] = offsetMap.at("opacity") / 4;

    // Stride (in floats)
    offsets[59] = stride / 4;

    return offsets;
}

}  // namespace vve
