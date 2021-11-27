/*-----------------------------------------------------------------------
Copyright (c) 2014-2018, NVIDIA. All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:
* Redistributions of source code must retain the above copyright
notice, this list of conditions and the following disclaimer.
* Neither the name of its contributors may be used to endorse
or promote products derived from this software without specific
prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
-----------------------------------------------------------------------*/

/*
Contacts for feedback:
- pgautron@nvidia.com (Pascal Gautron)
- mlefrancois@nvidia.com (Martin-Karl Lefrancois)

The ShaderBindingTable is a helper to construct the SBT. It helps to maintain the
proper offsets of each element, required when constructing the SBT, but also when filling the
dispatch rays description.

Example:


D3D12_GPU_DESCRIPTOR_HANDLE srvUavHeapHandle = m_srvUavHeap->GetGPUDescriptorHandleForHeapStart();
UINT64* heapPointer = reinterpret_cast< UINT64* >(srvUavHeapHandle.ptr);

m_sbtHelper.AddRayGenerationProgram(L"RayGen", {heapPointer});
m_sbtHelper.AddMissProgram(L"Miss", {});

m_sbtHelper.AddHitGroup(L"HitGroup",
{(void*)(m_constantBuffers[i]->GetGPUVirtualAddress())});
m_sbtHelper.AddHitGroup(L"ShadowHitGroup", {});


// Create the SBT on the upload heap
uint32_t sbtSize = 0;
m_sbtHelper.ComputeSBTSize(GetRTDevice(), &sbtSize);
m_sbtStorage = nv_helpers_dx12::CreateBuffer(m_device.Get(), sbtSize,
D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_GENERIC_READ,
nv_helpers_dx12::kUploadHeapProps);

m_sbtHelper.Generate(m_sbtStorage.Get(), m_rtStateObjectProps.Get());


//--------------------------------------------------------------------
Then setting the descriptor for the dispatch rays become way easier
//--------------------------------------------------------------------

D3D12_DISPATCH_RAYS_DESC desc = {};
// The layout of the SBT is as follows: ray generation shaders, miss shaders,
hit groups. As
// described in the CreateShaderBindingTable method, all SBT entries have the
same size to allow
// a fixed stride.

// The ray generation shaders are always at the beginning of the SBT. In this
// example we have only one RG, so the size of this SBT sections is
m_sbtEntrySize. uint32_t rayGenerationSectionSizeInBytes =
m_sbtHelper.GetRayGenSectionSize(); desc.RayGenerationShaderRecord.StartAddress
= m_sbtStorage->GetGPUVirtualAddress();
desc.RayGenerationShaderRecord.SizeInBytes = rayGenerationSectionSizeInBytes;

// The miss shaders are in the second SBT section, right after the ray
generation shader. We
// have one miss shader for the camera rays and one for the shadow rays, so this
section has a
// size of 2*m_sbtEntrySize. We also indicate the stride between the two miss
shaders, which is
// the size of a SBT entry
uint32_t missSectionSizeInBytes = m_sbtHelper.GetMissSectionSize();
desc.MissShaderTable.StartAddress =
m_sbtStorage->GetGPUVirtualAddress() + rayGenerationSectionSizeInBytes;
desc.MissShaderTable.SizeInBytes = missSectionSizeInBytes;
desc.MissShaderTable.StrideInBytes = m_sbtHelper.GetMissEntrySize();

// The hit groups section start after the miss shaders. In this sample we have 4
hit groups: 2
// for the triangles (1 used when hitting the geometry from a camera ray, 1 when
hitting the
// same geometry from a shadow ray) and 2 for the plane. We also indicate the
stride between the
// two miss shaders, which is the size of a SBT entry
// #Pascal: experiment with different sizes for the SBT entries
uint32_t hitGroupsSectionSize = m_sbtHelper.GetHitGroupSectionSize();
desc.HitGroupTable.StartAddress =
m_sbtStorage->GetGPUVirtualAddress() + rayGenerationSectionSizeInBytes +
missSectionSizeInBytes; desc.HitGroupTable.SizeInBytes = hitGroupsSectionSize;
desc.HitGroupTable.StrideInBytes = m_sbtHelper.GetHitGroupEntrySize();



*/

#pragma once

#include "vulkan/vulkan.h"
#include <vector>

namespace nv_helpers_vk
{
	/// Helper class to create and maintain a Shader Binding Table
	class ShaderBindingTableGenerator
	{
	public:
		/// Add a ray generation program by name, with its list of data pointers or values according to
		/// the layout of its root signature
		void AddRayGenerationProgram(uint32_t groupIndex, const std::vector<unsigned char> &inlineData);

		/// Add a miss program by name, with its list of data pointers or values according to
		/// the layout of its root signature
		void AddMissProgram(uint32_t groupIndex, const std::vector<unsigned char> &inlineData);

		/// Add a hit group by name, with its list of data pointers or values according to
		/// the layout of its root signature
		void AddHitGroup(uint32_t groupIndex, const std::vector<unsigned char> &inlineData);

		/// Compute the size of the SBT based on the set of programs and hit groups it contains
		VkDeviceSize ComputeSBTSize(const VkPhysicalDeviceRayTracingPropertiesNV &props);

		/// Build the SBT and store it into sbtBuffer, which has to be pre-allocated on the upload heap.
		/// Access to the raytracing pipeline object is required to fetch program identifiers using their
		/// names
		void Generate(VkDevice device,
			VkPipeline raytracingPipeline,
			VkBuffer sbtBuffer,
			VkDeviceMemory sbtMem);

		/// Reset the sets of programs and hit groups
		void Reset();

		/// The following getters are used to simplify the call to DispatchRays where the offsets of the
		/// shader programs must be exactly following the SBT layout

		/// Get the size in bytes of the SBT section dedicated to ray generation programs
		VkDeviceSize GetRayGenSectionSize() const;

		/// Get the size in bytes of one ray generation program entry in the SBT
		VkDeviceSize GetRayGenEntrySize() const;

		VkDeviceSize GetRayGenOffset() const;

		/// Get the size in bytes of the SBT section dedicated to miss programs
		VkDeviceSize GetMissSectionSize() const;

		/// Get the size in bytes of one miss program entry in the SBT
		VkDeviceSize GetMissEntrySize();

		VkDeviceSize GetMissOffset() const;

		/// Get the size in bytes of the SBT section dedicated to hit groups
		VkDeviceSize GetHitGroupSectionSize() const;

		/// Get the size in bytes of hit group entry in the SBT
		VkDeviceSize GetHitGroupEntrySize() const;

		VkDeviceSize GetHitGroupOffset() const;

	private:
		/// Wrapper for SBT entries, each consisting of the name of the program and a list of values,
		/// which can be either offsets or raw 32-bit constants
		struct SBTEntry
		{
			SBTEntry(uint32_t groupIndex, std::vector<unsigned char> inlineData);

			uint32_t m_groupIndex;
			const std::vector<unsigned char> m_inlineData;
		};

		/// For each entry, copy the shader identifier followed by its resource pointers and/or root
		/// constants in outputData, with a stride in bytes of entrySize, and returns the size in bytes
		/// actually written to outputData.
		VkDeviceSize CopyShaderData(VkDevice device,
			VkPipeline pipeline,
			uint8_t *outputData,
			const std::vector<SBTEntry> &shaders,
			VkDeviceSize entrySize,
			const uint8_t *shaderHandleStorage);

		/// Compute the size of the SBT entries for a set of entries, which is determined by the maximum
		/// number of parameters of their root signature
		VkDeviceSize GetEntrySize(const std::vector<SBTEntry> &entries);

		/// Ray generation shader entries
		std::vector<SBTEntry> m_rayGen;
		/// Miss shader entries
		std::vector<SBTEntry> m_miss;
		/// Hit group entries
		std::vector<SBTEntry> m_hitGroup;

		/// For each category, the size of an entry in the SBT depends on the maximum number of resources
		/// used by the shaders in that category.The helper computes those values automatically in
		/// GetEntrySize()
		VkDeviceSize m_rayGenEntrySize;
		VkDeviceSize m_missEntrySize;
		VkDeviceSize m_hitGroupEntrySize;

		/// The program names are translated into program identifiers.The size in bytes of an identifier
		/// is provided by the device and is the same for all categories.
		VkDeviceSize m_progIdSize;
		VkDeviceSize m_sbtSize;
	};
} // namespace nv_helpers_vk
