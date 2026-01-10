// Push constant is always binding slot 0.
#define BIND_PUSH_CONSTANT 0

// Use line macro to generate unique binding locations.
// If you need to bind any resource in the shader add an entry here.
// Make sure to have no empty lines between binding locations.
#line 1
const int BIND_FRAME_UNIFORMS = __LINE__;
const int BIND_CAMERA_UNIFORMS = __LINE__;
const int BIND_MATERIAL_TEXTURES = __LINE__;
const int BIND_MATERIAL_BUFFER = __LINE__;
const int BIND_DRAW_BUFFER = __LINE__;
const int BIND_LIGHT_BUFFER = __LINE__;
const int BIND_NODE_BUFFER = __LINE__;
const int BIND_TRIANGLE_BUFFER = __LINE__;
const int BIND_TEXTURE_GBUFFER_DEPTH = __LINE__;
const int BIND_TEXTURE_GBUFFER_NORMAL = __LINE__;
const int BIND_TEXTURE_GBUFFER_ALBEDO = __LINE__;
const int BIND_TEXTURE_GBUFFER_MATERIAL = __LINE__;
const int BIND_TEXTURE_GBUFFER_MOTION = __LINE__;
const int BIND_TEXTURE_NOISE = __LINE__;
const int BIND_TEXTURE_POSTPROCESS_INPUT = __LINE__;
const int BIND_TEXTURE_REPROJECT_PREVIOUS_COLOR = __LINE__;
const int BIND_TEXTURE_REPROJECT_PREVIOUS_DEPTH = __LINE__;
const int BIND_TEXTURE_REPROJECT_PREVIOUS_NORMAL = __LINE__;
const int BIND_TEXTURE_TAA_PREVIOUS_DEPTH = __LINE__;
const int BIND_TEXTURE_TAA_PREVIOUS_NORMAL = __LINE__;
const int BIND_TEXTURE_TAA_PREVIOUS_COLOR = __LINE__;
const int BIND_TEXTURE_TAA_CURRENT_COLOR = __LINE__;
const int BIND_TEXTURE_LIGHTING_DIFFUSE_SOURCE = __LINE__;
const int BIND_TEXTURE_LIGHTING_SPECULAR_SOURCE = __LINE__;
const int BIND_TEXTURE_LIGHTING_DIFFUSE_TEMP_SOURCE = __LINE__;
const int BIND_TEXTURE_LIGHTING_SPECULAR_TEMP_SOURCE = __LINE__;
const int BIND_TEXTURE_LIGHTING_DIFFUSE_REPROJECTED_SOURCE = __LINE__;
const int BIND_TEXTURE_LIGHTING_SPECULAR_REPROJECTED_SOURCE = __LINE__;
const int BIND_TEXTURE_COPY_SOURCE = __LINE__;
const int BIND_TEXTURE_DISPLAY_SOURCE = __LINE__;
const int BIND_TEXTURE_DEBUG_OVERDRAW = __LINE__;
const int BIND_TLAS = __LINE__;
const int BIND_RESERVOIR_LIGHT_INDEX = __LINE__;
const int BIND_RESERVOIR_LIGHT_WEIGHT = __LINE__;
const int BIND_RESERVOIR_WEIGHT_SUM = __LINE__;
const int BIND_RESERVOIR_NUM_PROCESSED_LIGHTS = __LINE__;
const int BIND_LAST_BINDING_PLUS_ONE = __LINE__; // Make sure, that this is the last binding!

// Different material features that can be checked in the shader.
#define MATERIAL_FEATURE_ALBEDO_MAP 1
#define MATERIAL_FEATURE_METALLICROUGHNESS_MAP 2
#define MATERIAL_FEATURE_NORMAL_MAP 4
#define MATERIAL_FEATURE_OCCLUSION_MAP 8
#define MATERIAL_FEATURE_EMISSIVE_MAP 16
#define MATERIAL_FEATURE_OPACITY_TRANSPARENT 32
#define MATERIAL_FEATURE_OPACITY_CUTOUT 64

// Definition of supported light types.
#define LIGHT_TYPE_POINT 0
#define LIGHT_TYPE_SPOT 1
#define LIGHT_TYPE_DIRECTIONAL 2

// Constants used for bvh construction.
#define BVH_MAX_TRIANGLES 10000000
#define BVH_MAX_NODES 5000000
#define BVH_MAX_TRIANGLES_PER_NODE 16
#define BVH_MAX_CHILD_NODES 8

// Constants used for raytracing.
#define RENDERING_MAX_RECURSIONS 6
#define RENDERING_MAX_SAMPLES 1

// Constants used for denoising.
#define RENDERING_LIGHTING_DENOISE_MAX_FRAME_NUM 8

// Constants used for temporal anti-aliasing.
#define RENDERING_TAA_SAMPLE_COUNT 64

// Constants used for performance measure.
#define RENDERING_FRAMETIMES_COUNT 128

// Constants used for swapchain, resource management.
#define MAX_RENDERING_FRAMES_IN_FLIGHT 3

#define RENDERING_UPSCALE_TYPE_BILINEAR 0
#define RENDERING_UPSCALE_TYPE_BICUBIC 1
#define RENDERING_UPSCALE_TYPE_TAAU 2

// Actual needed rendering framebuffers can be smaller depending on gpu and driver.
int GlobalRenderingFramesInFlight = MAX_RENDERING_FRAMES_IN_FLIGHT;

// Define some glsl types to use the corresponding c++ types so that we 
// can define some structs below that are shared between glsl and c++.
#ifdef __cplusplus
    #define sampler2D uint64_t
    #define buffer_address uint64_t
    #define vec2 glm::vec2
    #define vec3 glm::vec3
    #define vec4 glm::vec4
    #define mat4 glm::mat4
    #define uint uint32_t

    #define layout(packing, binding)
    #define vk_layout(binding) 
    #define vk_image(bindingValue, type) 
    #define vk_uniform(type, binding) struct 
    #define vk_push() struct
    
    #define IndexBufferReference uint64_t
    #define VertexBufferReference uint64_t
#else
    #define buffer_address uvec2
    #define gl_VertexID gl_VertexIndex
    #define vk_layout(binding) layout(binding)
    #define vk_image(bindingValue, type) layout(binding = bindingValue, type)
    #define vk_uniform(type, binding) layout(type, binding) uniform 
    #define vk_push() layout( push_constant) uniform 

    // Constants used for TAA, will be exposed to UI in the future.
    const int FilterTypes_Box = 0;
    const int FilterTypes_Triangle = 1;
    const int FilterTypes_Gaussian = 2;
    const int FilterTypes_BlackmanHarris = 3;
    const int FilterTypes_Smoothstep = 4;
    const int FilterTypes_BSpline = 5;
    const int FilterTypes_CatmullRom = 6;
    const int FilterTypes_Mitchell = 7;
    const int FilterTypes_GeneralizedCubic = 8;
    const int FilterTypes_Sinc = 9;
    
    const int ClampModes_Disabled = 0;
    const int ClampModes_RGB_Clamp = 1;
    const int ClampModes_RGB_Clip = 2;
    const int ClampModes_Variance_Clip = 3;
        
    const int DilationModes_CenterAverage = 0;
    const int DilationModes_DilateNearestDepth = 1;
    const int DilationModes_DilateGreatestVelocity = 2;
    
    const int ResolveFilterType = FilterTypes_Mitchell;
    const float ResolveFilterDiameter = 2.0;
    const float GaussianSigma = 0.5;
    const float CubicB = 0.33;
    const float CubicC = 0.33;
    const bool UseStandardResolve = false;
    const bool InverseLuminanceFiltering = false;
    const bool UseExposureFiltering = false;
    const float ExposureFilterOffset = 2.0;
    const float ExposureScale = 0.0;
    const float ManualExposure = -2.5;
    const bool UseGradientMipLevel = false;
    const bool EnableTemporalAA = true;
    const float TemporalAABlendFactor = 0.995;
    const bool UseTemporalColorWeighting = false;
    const int NeighborhoodClampMode = ClampModes_Variance_Clip;
    const float VarianceClipGamma = 1.5;
    const float LowFreqWeight = 0.25;
    const float HiFreqWeight = 0.85;
    const float SharpeningAmount = 0.0;
    const int DilationMode = DilationModes_DilateNearestDepth;
    const float MipBias = 0.0;
    const int ReprojectionFilter = FilterTypes_CatmullRom;
    const bool UseStandardReprojection = false;

    // Some hardware (e.g. Mac) do not have unpackHalf2x16 functions so we define a custom one here.
    // Copyright 2002 The ANGLE Project Authors
    // https://chromium.googlesource.com/angle/angle/+/master/src/compiler/translator/BuiltInFunctionEmulatorGLSL.cpp
    #if !defined(GL_ARB_shading_language_packing)
        float f16tof32(uint val)
        {
            uint sign = (val & 0x8000u) << 16;
            int exponent = int((val & 0x7C00u) >> 10);
            uint mantissa = val & 0x03FFu;
            float f32 = 0.0;
            if(exponent == 0)
            {
                if (mantissa != 0u)
                {
                    const float scale = 1.0 / (1 << 24);
                    f32 = scale * mantissa;
                }
            }
            else if (exponent == 31)
            {
                return uintBitsToFloat(sign | 0x7F800000u | mantissa);
            }
            else
            {
                exponent -= 15;
                float scale;
                if(exponent < 0)
                {
                    // The negative unary operator is buggy on OSX.
                    // Work around this by using abs instead.
                    scale = 1.0 / (1 << abs(exponent));
                }
                else
                {
                    scale = 1 << exponent;
                }
                float decimal = 1.0 + float(mantissa) / float(1 << 10);
                f32 = scale * decimal;
            }

            if (sign != 0u)
            {
                f32 = -f32;
            }

            return f32;
        }
    #endif

    vec2 unpackHalf2x16_emu(uint u)
    {
        #if defined(GL_ARB_shading_language_packing)
            return unpackHalf2x16(u);
        #else
            uint y = (u >> 16);
            uint x = u & 0xFFFFu;
            return vec2(f16tof32(x), f16tof32(y));
        #endif
    }
    
    // Some HLSL interop convinience functions.
    float saturate(float x) {
        return clamp(x, 0.0, 1.0);
    }
    vec2 saturate(vec2 x) {
        return clamp(x, vec2(0.0, 0.0), vec2(1.0, 1.0));
    }
    vec3 saturate(vec3 x) {
        return clamp(x, vec3(0.0, 0.0, 0.0), vec3(1.0, 1.0, 1.0));
    }
    vec4 saturate(vec4 x) {
        return clamp(x, vec4(0.0, 0.0, 0.0, 0.0), vec4(1.0, 1.0, 1.0, 1.0));
    }
#endif

// Vertex definition for our static meshes.
struct Vertex {
    vec3 Position;
    vec3 Normal;
    vec2 TextureCoordinates;
};

// Data necessary to draw an object.
struct DrawData {
    mat4 World;
    mat4 WorldInvTrans;

    vec2 Padding;
    uint IndexOffset;
    int MaterialIndex;

    // Used for fetching geometry data in raytracing shaders.
    buffer_address Indices;
    buffer_address Vertices;
};

// Shared material structure.
struct RendererMaterial {
    vec4 AlbedoFactor;

    vec3 EmissiveFactor;
    float RoughnessFactor;
    
    float MetalnessFactor;
    float OcclusionStrength; 
    float NormalScale;
    uint FeatureMask;

    float AlphaCutoff;
    int AlbedoMap;
    int MetallicRoughnessMap;
    int NormalMap;

    int OcclusionMap;
    int EmissiveMap;
    vec2 Pad;
};

bool HasFeature(RendererMaterial material, uint mask) {
    return (material.FeatureMask & mask) != 0;
}

// Shared light structure.
struct RendererLight {
    vec3 Position;
    float Range;
    
    vec3 Direction;
    uint Type;

    vec3 Color;
    float Intensity;

    float AngleScale;
    float AngleOffset;
    float Radius;
    float Pad;
};

// All information needed to shade a surface point.
struct SurfacePoint {
    vec3 Position;
    vec3 Normal;
    vec4 Albedo;
    float Metalness;
    float Roughness;
};

// Camera uniform data.
vk_uniform(std140, binding = BIND_CAMERA_UNIFORMS) CameraUniforms {
    mat4 ViewProjection;
    mat4 ViewProjectionOld;
    mat4 InvViewProjection;
    mat4 InvView;
    mat4 InvProjection;

    vec3 CameraPosition;
    float CameraExposure;

    vec2 Jitter;
    vec2 JitterOld;
};

// Frame uniform data.
vk_uniform(std140, binding = BIND_FRAME_UNIFORMS) FrameUniforms {
    vec2 RenderingScale;
    vec2 RenderingScaleOld;

    uint FrameCount;
    uint LightCount;
    uint BuffersResized;
    uint UpscaleType;
};

// Push constants that are updated for each gbuffer draw.
vk_push() PushConstants {
    int DrawIndex;
};

#ifdef __cplusplus
    // Undef our defines from above so that we don't overwrite something by mistake that we need.
    #undef sampler2D
    #undef vec2 
    #undef vec3 
    #undef vec4 
    #undef mat4 
    #undef uint
    #undef layout
    #undef vk_layout
    #undef vk_image
    #undef vk_uniform
    #undef vk_push
    
    #undef IndexBufferReference
    #undef VertexBufferReference
#else
    // Define some shared helper constants and functions for our shaders.
    const float EPSILON = 0.000001;
    const float NORMAL_EPSILON = 0.000001;

    const float M_PI = 3.141592653589793;
    const float GOLDENRATIO_CONJUGATE = 0.61803398875f; 

    // Normal encoding/decoding to pack it into 2 floats instead of 3.
    // From http://jcgt.org/published/0003/02/01/ and https://discourse.panda3d.org/t/glsl-octahedral-normal-packing/15233
    vec2 sign_not_zero(vec2 v) {
        return fma(step(vec2(0.0), v), vec2(2.0), vec2(-1.0));
    }

    // Packs a 3-component normal to 2 channels using octahedron normals
    vec2 encodeNormal(vec3 v) {
        v.xy /= dot(abs(v), vec3(1));
        return mix(v.xy, (1.0 - abs(v.yx)) * sign_not_zero(v.xy), step(v.z, 0.0));
    }

    // Unpacking from octahedron normals, input is the output from pack_normal_octahedron
    vec3 decodeNormal(vec2 packed_nrm) {
        vec3 v = vec3(packed_nrm.xy, 1.0 - abs(packed_nrm.x) - abs(packed_nrm.y));
        if (v.z < 0) v.xy = (1.0 - abs(v.yx)) * sign_not_zero(v.xy);

        return normalize(v);
    }

    // This texture array contains all textures in our scene.
    vk_layout(binding = BIND_MATERIAL_TEXTURES) uniform sampler2DArray MaterialTextures;

    vec4 SampleMaterialTexture(int textureIndex, vec2 texCoords) {
        if(textureIndex == -1) {
            return vec4(0, 0, 0, 0);
        }
        return texture(MaterialTextures, vec3(texCoords.x, texCoords.y, float(textureIndex)));
    }

    // Sometimes (e.g. when casting rays) the GPU can not automatically determine the mip level 
    // of a texture so we can use this function to just sample a specific mip level of a texture.
    vec4 SampleMaterialTextureLod(int textureIndex, vec2 texCoords, float mipLevel) {
        if(textureIndex == -1) {
            return vec4(0, 0, 0, 0);
        }
        return textureLod(MaterialTextures, vec3(texCoords.x, texCoords.y, float(textureIndex)), mipLevel);
    }

    // Structures used for reading index and vertex data from buffer references in rayctracing shader.
    layout(scalar, buffer_reference, buffer_reference_align = 4) readonly buffer IndexBufferReference
    {
        uint data[];
    };
    layout(scalar, buffer_reference, buffer_reference_align = 4) readonly buffer VertexBufferReference
    {
        Vertex data[];
    };

    // Contains all lights from the scene.
    layout(std430, binding = BIND_LIGHT_BUFFER) buffer LightBuffer
    {
        RendererLight Lights[];
    };

    RendererLight GetLight(uint lightIndex) {
        return Lights[lightIndex];
    }

    // Contains all materials in the scene.
    layout(std430, binding = BIND_MATERIAL_BUFFER) buffer MaterialBuffer
    {
        RendererMaterial Materials[];
    };

    RendererMaterial GetMaterial(uint materialIndex) {
        return Materials[materialIndex];
    }

    // Contains all draw data for the scene.
    layout(std430, binding = BIND_DRAW_BUFFER) buffer DrawBuffer
    {
        DrawData DrawBufferData[];
    };

    DrawData GetDrawData(uint drawIndex) {
        return DrawBufferData[drawIndex];
    }
#endif
