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
Streaming can still stall, and a sustained 30 FPS on physical DSi is not
established. Save/load and full original-game feature parity remain incomplete.

The performance pass reuses map scanlines and floor classifications, compacts
streamed patches once, and reduces visibility and collision-grid arithmetic.
Before/after checks preserve geometry, UVs, selection order and collision
results. Deterministic ARM9 emulator component timings improved; physical DSi
gameplay FPS still needs measurement.

The Missions selector covers all 96 player-facing retail slots. The two opening
slots share a controller. Alongside the authored missions and 65 early routes
are 15 purchases, three service jobs and four Sparrow courses. The cutscene
library includes all 70 mission sequences, the scripted departure and two
startup movies. Progress lasts for the current journey. Original branching,
economy and vehicle behavior are partially adapted.
[Activity controls](ports/nds/README.md#property-purchases-and-side-activities)

## Layout

- `source/`: ARM9 application and frame loop.
- `ports/nds/`: portable gameplay, platform, rendering, streaming and codecs.
- `ports/nds/patches/`: small patches for the separately obtained dependencies.
- `tools/`: asset readers, conversion, packaging and validation tools.
- `tests/`: synthetic tests and tests that use locally generated game data.
- `cmake/`, `Makefile`: host and devkitARM build definitions.
- `licenses/`: verbatim notices for the referenced upstream revisions.

