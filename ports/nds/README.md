# Port source

Use the current source-only [build instructions](../../BUILDING.md) and
[dependency notices](../../THIRD_PARTY.md). Generated asset packs and historical
development reports are intentionally absent from this repository.

`CMakeLists.txt` builds either native validation programs
(`REVC_NDS_HOST_VALIDATION=ON`) or the devkitARM ROM. Live presentation defaults
to `REVC_GAMEPLAY_CINEMA_RESOLUTION=ON`. Keep the inspection flags OFF for a
normal gameplay build. Runtime geometry selection and streaming in this export
match the working October 18, 2026 version.
