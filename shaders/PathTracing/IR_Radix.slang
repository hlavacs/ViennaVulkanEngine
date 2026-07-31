// implementation based on https://github.com/MircoWerner/VkRadixSort

// Constraints
// 1. WORKGROUP_SIZE must be a multiple of 32
// 2. WORKGROUP_SIZE >= RADIX_SORT_BINS
// 3. RADIX_SORT_BINS >= SUBGROUP_SIZE
// 4. SUBGROUP_SIZE must match the vendor specific wave size (e.g., 32 on NVIDIA).

// In HLSL/Slang, specialization constants are often handled via preprocessor macros
// injected at compile time, or using Slang's [vk::constant_id(x)] attribute.
#ifndef WORKGROUP_SIZE
#define WORKGROUP_SIZE 256
#endif

#ifndef RADIX_SORT_BINS
#define RADIX_SORT_BINS 256
#endif

#ifndef SUBGROUP_SIZE
#define SUBGROUP_SIZE 32
#endif

struct PushConstants {
    uint g_num_elements;
    uint g_shift;
    uint g_num_workgroups;
    uint g_num_blocks_per_workgroup;
};

[[vk::push_constant]]
ConstantBuffer<PushConstants> PushArgs;

// --- KEYS (Morton Codes) ---
[[vk::binding(0, 0)]] StructuredBuffer<uint> g_elements_in;
[[vk::binding(1, 0)]] RWStructuredBuffer<uint> g_elements_out;

// --- HISTOGRAMS ---
[[vk::binding(2, 0)]] StructuredBuffer<uint> g_histograms;

// --- VALUES (Original Indices) - NEW ---
[[vk::binding(3, 0)]] StructuredBuffer<uint> g_indices_in;
[[vk::binding(4, 0)]] RWStructuredBuffer<uint> g_indices_out;

groupshared uint sums[RADIX_SORT_BINS / SUBGROUP_SIZE];
groupshared uint global_offsets[RADIX_SORT_BINS];
groupshared uint bin_flags[RADIX_SORT_BINS][WORKGROUP_SIZE / 32];

[shader("compute")]
[numthreads(WORKGROUP_SIZE, 1, 1)]
void main(
    uint3 local_id_vec: SV_GroupThreadID,
    uint3 group_id_vec: SV_GroupID)
{
    uint local_id = local_id_vec.x;
    uint group_id = group_id_vec.x;
    uint subgroup_id = local_id / SUBGROUP_SIZE;

    // === global offsets (prefix sum) ===

    uint local_histogram = 0;
    uint histogram_count = 0;

    if (local_id < RADIX_SORT_BINS) {
        for (uint j = 0; j < PushArgs.g_num_workgroups; j++) {
            uint t = g_histograms[RADIX_SORT_BINS * j + local_id];
            local_histogram = (j == group_id) ? histogram_count : local_histogram;
            histogram_count += t;
        }
    }

    uint sum = WaveActiveSum(histogram_count);
    uint prefix_sum = WavePrefixSum(histogram_count);

    if (WaveIsFirstLane()) {
        if (subgroup_id < (RADIX_SORT_BINS / SUBGROUP_SIZE)) {
            sums[subgroup_id] = sum;
        }
    }

    GroupMemoryBarrierWithGroupSync();

    if (local_id < RADIX_SORT_BINS) {
        uint sums_prefix_sum = 0;
        for (uint i = 0; i < subgroup_id; i++) {
            sums_prefix_sum += sums[i];
        }
        global_offsets[local_id] = sums_prefix_sum + prefix_sum + local_histogram;
    }

    // === scatter keys and indices according to global offsets ===

    uint flags_bin = local_id / 32;
    uint flags_bit = 1u << (local_id % 32);

    for (uint index = 0; index < PushArgs.g_num_blocks_per_workgroup; index++) {
        uint elementId = group_id * PushArgs.g_num_blocks_per_workgroup * WORKGROUP_SIZE + index * WORKGROUP_SIZE + local_id;

        // initialize bin flags
        if (local_id < RADIX_SORT_BINS) {
            for (int i = 0; i < WORKGROUP_SIZE / 32; i++) {
                bin_flags[local_id][i] = 0u;
            }
        }
        GroupMemoryBarrierWithGroupSync();

        uint element_in = 0;
        uint index_in = 0; // NEW
        uint binID = 0;
        uint binOffset = 0;

        if (elementId < PushArgs.g_num_elements) {
            element_in = g_elements_in[elementId];
            index_in = g_indices_in[elementId]; // NEW

            binID = (element_in >> PushArgs.g_shift) & (RADIX_SORT_BINS - 1);
            binOffset = global_offsets[binID];

            InterlockedAdd(bin_flags[binID][flags_bin], flags_bit);
        }
        GroupMemoryBarrierWithGroupSync();

        if (elementId < PushArgs.g_num_elements) {
            uint prefix = 0;
            uint count = 0;
            for (uint i = 0; i < WORKGROUP_SIZE / 32; i++) {
                uint bits = bin_flags[binID][i];
                uint full_count = countbits(bits);
                uint partial_count = countbits(bits & (flags_bit - 1));

                prefix += (i < flags_bin) ? full_count : 0u;
                prefix += (i == flags_bin) ? partial_count : 0u;
                count += full_count;
            }

            // Write both the key and the payload to the new offset
            g_elements_out[binOffset + prefix] = element_in;
            g_indices_out[binOffset + prefix] = index_in; // NEW

            if (prefix == count - 1) {
                InterlockedAdd(global_offsets[binID], count);
            }
        }

        GroupMemoryBarrierWithGroupSync();
    }
}