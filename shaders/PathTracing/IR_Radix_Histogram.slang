// implementation based on https://github.com/MircoWerner/VkRadixSort

// Constraints
// 1. WORKGROUP_SIZE must be a multiple of 32
// 2. WORKGROUP_SIZE >= RADIX_SORT_BINS
// 3. RADIX_SORT_BINS >= SUBGROUP_SIZE

// Using preprocessor macros for specialization constants
#ifndef WORKGROUP_SIZE
#define WORKGROUP_SIZE 256
#endif

#ifndef RADIX_SORT_BINS
#define RADIX_SORT_BINS 256
#endif

struct PushConstants {
    uint g_num_elements;
    uint g_shift;
    uint g_num_workgroups;
    uint g_num_blocks_per_workgroup;
};

// Vulkan Push Constants
[[vk::push_constant]]
ConstantBuffer<PushConstants> PushArgs;

// Storage Buffers (Set 0)
[[vk::binding(0, 0)]] StructuredBuffer<uint> g_elements_in;
[[vk::binding(1, 0)]] RWStructuredBuffer<uint> g_histograms;

// Group Shared Memory
groupshared uint histogram[RADIX_SORT_BINS];

[shader("compute")]
[numthreads(WORKGROUP_SIZE, 1, 1)]
void main(
    uint3 local_id_vec : SV_GroupThreadID,
    uint3 group_id_vec : SV_GroupID) 
{
    uint local_id = local_id_vec.x;
    uint group_id = group_id_vec.x;

    // initialize histogram
    if (local_id < RADIX_SORT_BINS) {
        histogram[local_id] = 0u;
    }
    
    // barrier() maps to:
    GroupMemoryBarrierWithGroupSync();

    for (uint index = 0; index < PushArgs.g_num_blocks_per_workgroup; index++) {
        uint elementId = group_id * PushArgs.g_num_blocks_per_workgroup * WORKGROUP_SIZE + index * WORKGROUP_SIZE + local_id;
        
        if (elementId < PushArgs.g_num_elements) {
            // determine the bin
            uint bin = (g_elements_in[elementId] >> PushArgs.g_shift) & (RADIX_SORT_BINS - 1);
            
            // atomicAdd maps to InterlockedAdd
            InterlockedAdd(histogram[bin], 1u);
        }
    }
    
    GroupMemoryBarrierWithGroupSync();

    if (local_id < RADIX_SORT_BINS) {
        g_histograms[RADIX_SORT_BINS * group_id + local_id] = histogram[local_id];
    }
}