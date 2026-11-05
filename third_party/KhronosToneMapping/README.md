# Khronos PBR Neutral

The operator in `src/versions/simple/shaders/simple_tonemap.slang` is adapted from
[KhronosGroup/ToneMapping, PBR_Neutral/pbrNeutral.glsl](https://github.com/KhronosGroup/ToneMapping/blob/180b1a7bddec33f73fe41712a2963cc3ad8e5547/PBR_Neutral/pbrNeutral.glsl),
pinned to revision `180b1a7bddec33f73fe41712a2963cc3ad8e5547`.
Copyright 2024 The Khronos Group, Inc.; Apache-2.0, reproduced in LICENSE.

VVE multiplies exposure first, adds explicit invalid-input/overflow handling,
and performs the presentation transfer curve only when the attachment does not.
The build embeds Slang-compiled SPIR-V; it does not fetch this source or licence.
