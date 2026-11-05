# SMAA reference archive

`smaa-reference.tar.gz` preserves the 3 November 2026 IQ-03a freeze from
run `J20261103-183417-403786`: all 424 original files under `smaa/frozen`,
the original `smaa/freeze.log`, and the original IQ-00 `baseline/manifest.txt`
used to identify the capture environment. All 426 file contents were compared
byte-for-byte with the original Debug archive. No reference was re-rendered.

The freeze log records the same decoded RGBA FNV-1a64 hashes as
`tests/support/SmaaQualityReference.hpp`: `8fb02ade1fca61dd` for none and
`c8e5b19846691cb0` for MSAA4, for all three backends and repeats. The original
environment was llvmpipe LLVM 20.1.2, Mesa 25.2.8, X11, BGRA8 sRGB, with mutable
views. The test requires matching device, driver, API, surface and capture
provenance before applying exact historical equality; independent coverage and
SMAA stage checks also run.

CMake verifies SHA-256
`6a1f38f30e4a6dd3c7da94fc456c284ab205a32ae77f32fd576da672652a9c19`
and extracts into the current build's `tests/fixtures/smaa` directory. Runtime
captures remain under `verify/image-quality/smaa`; neither a pre-existing Debug
build nor files from another checkout are required. Archive ownership, timestamp
and permissions are normalized, with writable extracted files to permit repeated
configuration. The original image, manifest and log bytes are unchanged.
