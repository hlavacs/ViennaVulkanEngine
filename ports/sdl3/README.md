# SDL3 Windows configuration fix

This overlay is the SDL3 3.4.0 port selected by VVE's vcpkg baseline
`8cf84605ce9764de46caeb47b9e710db9493add9` (port tree
`d2ee74ca7389a31ac2090e949f895da5c002434c`). Its only build change is to omit
`SDL_UNIX_CONSOLE_BUILD` on Windows, where SDL does not use it. Other targets retain
the upstream option. SDL's version, source hash, features and patches are unchanged.

`vcpkg-configuration.json` selects this overlay automatically. No edit to the global
vcpkg checkout or generated build files is needed. The next dependency install may
rebuild SDL3 and its dependents because the port changed.

When updating VVE's SDL baseline, review this overlay too. Remove it and its
configuration entry once the selected upstream port handles this option correctly.

Upstream: https://github.com/microsoft/vcpkg/tree/8cf84605ce9764de46caeb47b9e710db9493add9/ports/sdl3
The copied port files use vcpkg's MIT license; see `LICENSE.vcpkg.txt`.
