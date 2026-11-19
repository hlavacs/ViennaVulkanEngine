# Complex Engine

- Keep this implementation independent. Use `vve::complex` and `VVEngine.Complex` modules. Do not import, include, or link sources from any other engine.
- Keep the public contract in `src/`. Reuse shared facade types and public shader vocabulary from there; keep implementation data and pipeline stages local.
- Keep window, GUI, audio, and Vulkan resources under the owning engine. Preserve the one-instance rule and support multiple windows within it.
- Preserve explicit frame passes, frame-fence resource retirement, and opt-in diagnostics. Add deterministic checks when changing these contracts.
- Build and test with `-DVVE_ENGINE_IMPLEMENTATION_NAMESPACE=complex` in a separate build directory.
