# Vice City DSi development source

This is a **source-only distribution**. It contains no retail game files,
converted assets, generated mission dialogue, screenshots, videos, ROMs,
emulator binaries, firmware or upstream source checkouts. Builders supply
their own compatible PC game installation and obtain dependencies separately.

Start with [BUILDING.md](BUILDING.md). Dependency versions, patch provenance
and original notices are recorded in [THIRD_PARTY.md](THIRD_PARTY.md).

## Scope

The runtime uses simplified models and adapted missions, not the complete
original engine or unrestricted SCM execution. Live gameplay and story movies
share a 128×72 source resolution with a 256×144 letterboxed presentation.
Gameplay uses reduced cutscene-derived character meshes; cinematic facial
animation and full cinematic geometry are not reproduced in gameplay.
Streaming can still stall. Save/load and full original-game feature parity remain incomplete.

## Layout

- `source/`: ARM9 application and frame loop.
- `ports/nds/`: portable gameplay, platform, rendering, streaming and codecs.
- `ports/nds/patches/`: small patches for the separately obtained dependencies.
- `tools/`: asset readers, conversion, packaging and validation tools.
- `tests/`: synthetic tests and tests that use locally generated game data.
- `cmake/`, `Makefile`: host and devkitARM build definitions.
- `licenses/`: verbatim notices for the referenced upstream revisions.

