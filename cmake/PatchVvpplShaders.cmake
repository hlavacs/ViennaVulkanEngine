# VVPPL v0.3 reads the input texture inside and after its convolution loops. KosmicKrisp
# in Vulkan SDK 1.4.350.0 declares the Metal texture inside the loop, then reuses it out
# of scope. Read the original pixel first to keep that declaration outside the loops.
# Apply to fetched sources so clean builds retain the fix without vendoring the library.
foreach(effect emboss sobel)
   set(shader "${viennavulkanpostprocessinglibrary_SOURCE_DIR}/shaders/${effect}.slang")
   file(READ "${shader}" source)
   set(sample "    float4 color = inImage[id.xy];")
   set(anchor "    int2 size = int2(width, height);")
   string(FIND "${source}" "${sample}" sample_position)
   string(FIND "${source}" "${anchor}" anchor_position)
   if(sample_position EQUAL -1 OR anchor_position EQUAL -1)
      message(FATAL_ERROR "VVPPL ${effect} changed; review the KosmicKrisp shader workaround")
   endif()
   # Preserve the bounds check and shader math; avoid rewriting on subsequent configures.
   if(sample_position GREATER anchor_position)
      string(REPLACE "${sample}\n" "" source "${source}")
      string(REPLACE "${anchor}"
         "    // Keep the Metal input texture in scope before entering the loops.\n${sample}\n\n${anchor}"
         source "${source}")
      file(WRITE "${shader}" "${source}")
   endif()
endforeach()
