# SMAA reference sources

Jimenez, Echevarria, Sousa and Gutierrez, *SMAA: Enhanced Subpixel
Morphological Antialiasing*, Eurographics 2012.
Upstream: https://github.com/iryoku/smaa
Pinned revision: `71c806a838bdd7d517df19192a20f0c61b3ca29d`.
Fetched once from raw.githubusercontent.com on 3 November 2026.
The licence is MIT; [`LICENSE`](LICENSE) is the upstream `LICENSE.txt`.
The other files retain upstream bytes.
The build uses these files locally and never downloads SMAA.

| File | SHA-256 |
| --- | --- |
| SMAA.hlsl | 4db53d92ef0b45661f8450303589ffabce93c352cae4282a6f32dd2b80efbeb9 |
| AreaTex.h | 1933bf43ce86ba71f7ed5f0fa94b148610c634dc3395b9bc67a817d51ee73af2 |
| SearchTex.h | 1fac315ab5f87b60083c9b8387af17f056204d3d1bf21500f2acc5b3e8c1a37a |
| LICENSE | 2b848b67ba50ac40b53e11cf1ccdaee2a190ca83b4652cba695a5f1ce81f52ae |

`src/versions/simple/shaders/simple_smaa.slang` compiles the pinned reference
through Slang using `SMAA_CUSTOM_SL`; `RendererSmaa.cpp` owns setup and recording.
HIGH uses threshold 0.1, 16 orthogonal / 8 diagonal search
steps and corner rounding 25. AreaTex is 160x560 RG8; packed SearchTex is
64x16 R8. Both use linear clamp sampling, with point clamp for edge luma.
There is no temporal reconstruction, predication or reprojection.
