# reVC DSi gameplay development

The opening cinematics and Ocean View phone ending lead into four Ken mission adaptations: The Party, Back Alley Brawl, Jury Fury and Riot. Each subsequent job begins at Ken's authored office marker. Original GXT text briefings use R to advance / L to skip. Mercedes and Lance follow and board the player's stopped car. Combat HUD shows ammunition/reserves, and objectives show chef health, attacker count, juror-car progress and riot progress. Mission combat uses the original chef/worker actor assets, bounded enemies, melee/pistol hit checks, destructible juror cars and three depot vans. Rewards are $100/$200/$400/$1,000 and are awarded once; completion advances to the next office job. After Riot, free roam remains available. R retries a failed job at its office-start stage without teleporting the player; Select restarts the opening journey.

`tools/stage_missions.py` imports individually reviewed commands from the supplied Steam SCM hash and its GXT briefings. This code does not skip unsupported VM opcodes or claim full script execution. The new missions use reduced set pieces, initially parked mission vehicles and text introductions. Their original cutscenes, all dialogue events, police responses, motorcycle tutorial, jury-car chase, full riot crowd, explosions, workshop interiors and mission-specific animations are not fully reproduced. Source coordinates are retained; nearby supported spawn positions are searched when static collision intersects an NPC. Walking/ranged attacks must have real static-world line of sight. New tests relocate between objectives to check bindings and transitions; they do not establish a continuous hardware playthrough.

The opening still plays the converted original Sonny/airport/dock/departure scenes and the original hotel phone scene, with streamed speech and subtitles. Start skips those cinematics. The live bottom-screen map, touch waypoints and pause zoom are retained. Progress has no save/load yet.

| Input | Gameplay | Cinematics / pause map |
| --- | --- | --- |
| Up / Down | Move toward / away from camera view / accelerate / reverse | — |
| Left / Right | Move left / right relative to camera / steer | — |
| L | Jump on foot / brake in car | — |
| R | Sprint / punch nearby mission targets while unarmed; fire if armed; retry failed job | — |
| L+R | Hijack nearest car / exit stopped car with D-pad released; walk with D-pad held | — |
| X | Move camera up | Toggle map zoom while paused |
| A | Turn camera right | — |
| B | Move camera down | — |
| Y | Turn camera left | Clear waypoint while paused |
| Start | Open pause menu (Resume / Map / New Game / Options) | Skip cinematic / resume map |
| Select | Restart playable journey; L+Select reloads if armed | — |
| Touch | Set / remove waypoint | Set / remove waypoint on map |

## Build

Existing staged data is ready; run `make` or `make revc`. To regenerate all inputs:

```sh
python3 tools/prepare.py '/mnt/STEAM/SteamLibrary/steamapps/common/Grand Theft Auto Vice City/'
python3 tools/stage_revc.py '/mnt/STEAM/SteamLibrary/steamapps/common/Grand Theft Auto Vice City/'
python3 tools/stage_world.py '/mnt/STEAM/SteamLibrary/steamapps/common/Grand Theft Auto Vice City/'
python3 tools/stage_fast.py
python3 tools/stage_quality.py '/mnt/STEAM/SteamLibrary/steamapps/common/Grand Theft Auto Vice City/'
python3 tools/stage_water.py '/mnt/STEAM/SteamLibrary/steamapps/common/Grand Theft Auto Vice City/'
python3 tools/stage_lighting.py '/mnt/STEAM/SteamLibrary/steamapps/common/Grand Theft Auto Vice City/'
python3 tools/stage_collision.py '/mnt/STEAM/SteamLibrary/steamapps/common/Grand Theft Auto Vice City/'
python3 tools/stage_terrain.py
python3 tools/stage_backdrops.py '/mnt/STEAM/SteamLibrary/steamapps/common/Grand Theft Auto Vice City/'
python3 tools/compact_assets.py
python3 tools/stage_gameplay.py '/mnt/STEAM/SteamLibrary/steamapps/common/Grand Theft Auto Vice City/'
python3 tools/stage_animation.py '/mnt/STEAM/SteamLibrary/steamapps/common/Grand Theft Auto Vice City/'
python3 tools/stage_player_motion.py '/mnt/STEAM/SteamLibrary/steamapps/common/Grand Theft Auto Vice City/'
python3 tools/stage_population.py '/mnt/STEAM/SteamLibrary/steamapps/common/Grand Theft Auto Vice City/'
python3 tools/stage_mission_assets.py '/mnt/STEAM/SteamLibrary/steamapps/common/Grand Theft Auto Vice City/'
python3 tools/stage_missions.py '/mnt/STEAM/SteamLibrary/steamapps/common/Grand Theft Auto Vice City/'
python3 tools/stage_distant.py '/mnt/STEAM/SteamLibrary/steamapps/common/Grand Theft Auto Vice City/'
python3 tools/stage_texture_averages.py
python3 tools/stage_scripts.py '/mnt/STEAM/SteamLibrary/steamapps/common/Grand Theft Auto Vice City/'
python3 tools/stage_fleet.py '/mnt/STEAM/SteamLibrary/steamapps/common/Grand Theft Auto Vice City/'
python3 tools/stage_navigation.py '/mnt/STEAM/SteamLibrary/steamapps/common/Grand Theft Auto Vice City/'
python3 tools/stage_cutscenes.py '/mnt/STEAM/SteamLibrary/steamapps/common/Grand Theft Auto Vice City/'
python3 tools/stage_departure.py '/mnt/STEAM/SteamLibrary/steamapps/common/Grand Theft Auto Vice City/'
make revc
make revc-test GAME_DIR='/mnt/STEAM/SteamLibrary/steamapps/common/Grand Theft Auto Vice City/'
make test
```

The result is `build/revc-nds/revc_dsi.nds`. The ROM embeds 82 runtime files through NitroFS, including animation caches and original `scripts/main.scm`, so no separate SD asset directory is required. World conversion requires NumPy and Pillow. Cinematic conversion also requires PyOpenGL, a headless EGL OpenGL compatibility context with GLU (Mesa works), ffmpeg and cpp. It runs locally from the owned assets; no PC game executable or downloaded video is used. The launcher must support NitroFS access. Device type 2 does not prove DSi-mode startup or hardware compatibility.

## Performance and validation

The full conversion covers 9,114 placements in 4,925 coarse regions. The close view excludes the four `IslandLOD` backdrops, including untextured faces identified from their original geometry. These distant meshes had been rendered over the close-up pavement and hidden the player's legs. At the regression spawn, the unwanted surface was 10.9477 metres high while the real pavement/player support is 10.0312 metres.

Collision now uses 622,764 clipped original COL records from 7,999 non-LOD placements, with no missing models. Original triangles and box surfaces are retained at 1/64-metre precision; sphere primitives are conservatively approximated by circumscribed octahedra. Up to 4,096 nearby collision faces are retained without a render-budget reduction; excess collision fails loading rather than silently removing support. The loaded collision region extends 24 metres and is reused until the player moves six metres or changes origin.

The terrain pack retains 3,441,975 clipped walkable render faces without a per-tile triangle cap. LZ10 compression preserves every byte. Native validation compares every decompressed tile with its original and tests 100 supported movement steps. Terrain gets 512 reserved resident slots, alongside up to 1,536 decorative faces; contact geometry near the player gets draw priority. Decorative eight-metre tiles retain their 128-face cap, and draw submission adapts from 128 to 900 near-map triangles, with a separate budget of up to 256 distant triangles. Detailed walls load within 24 metres and ±32 vertical metres; original terrain stays within a 16-metre contact neighborhood. The old budget-dependent 4–14 metre fog is replaced by a stable 64-metre fog limit and an 80-metre projection far plane. Collision and support remain independent of these rendering limits.

10,349 texture IDs share 5,304 identical 64×64 RGB15/256-index pages. A 24-page VRAM cache uses 96 KiB of pixels and 12 KiB of palettes and shares uploaded aliases too. Entries visible in the last six frames cannot be evicted; at most one new world page uploads per frame while stationary, or once per eight rendered frames while moving/turning. Streaming frames upload no textures. Cache misses use palette-weighted source colors. Distant geometry always uses these colors and never evicts nearby textures. There are 105 unresolved source texture references, including inactive assets. Partial texture alpha, lighting gradients and directional lighting are approximated. The SUNNY noon ambient/sky/water colors come from `timecyc.dat`. Original `WATERPRO.DAT` supplies 11,669 water cells, drawn out to 64 metres with an untextured color fallback when its raster is not cached; waves, swimming and boats remain absent.

Population streams local segments from 8,040 original pedestrian and 3,099 car path segments. Up to three civilians (male01/HFYST) and one taxi are active. Actors walk routes, wait/turn at connections, flee gunfire/nearby traffic, and avoid walls/actors. Physics is staggered between pedestrians while animations keep advancing. Male/female animation groups and root motion speeds come from the original IFP data. Clothing colors sample the original UVs/rasters. There is no civilian damage/death system, police dispatch or full traffic-light/lane simulation yet.

Riot uses the original `player3` Spand Express coveralls mesh/texture with all thirteen player animation clips. The Party uses `player2`. Changing outfits reuses the hero texture handle; 16 KiB of texture VRAM stays spare for updates.

Actors use original skin weights, inverse bind matrices and quaternion/translation keys. Topology-checked edge collapse replaces the old largest-triangle selection, preserving connected surfaces instead of discarding small triangles. Tommy has 520 triangles, Mercedes 120, and civilians up to 80. VCA6 stores shared animated vertices with per-face UVs: 8 poses for idle/actions and 16 for locomotion. Tommy's 13-clip cache is 217,108 bytes, versus the old 240,048-byte five-clip 300-triangle cache. The added clips are sprint, walk start, both sprint stops, jump launch, glide, landing and the original punch. Shared poses interpolate with fixed-point weights on ARM9. Original player/player2 rasters use 128×128 paletted textures; Sentinel (599 triangles including 96 wheel triangles) and taxi (420 including 96) use 256×256 material atlases with source body/wheel textures and stock paint. Actor textures occupy 144 KiB in VRAM alongside the 96 KiB world cache, across banks A/B. NPCs retain sampled triangle colors. Damaged alternatives remain excluded. Original Sentinel/taxi LODs (88/122 triangles) replace the detailed versions beyond eight metres; wheel rotation and ordinary gameplay audio remain unintegrated. Cinematics include streamed speech.

Player movement uses the original `walk_player`, `run_player` and `sprint_civi` forward root tracks, extracted by `tools/stage_player_motion.py` into `generated/player_motion_data.h`. In the supplied assets their cycle-average speeds are 0.785835, 6.439205 and 14.668074 metres/second respectively. These values are measured from the supplied files, not assumed universal stock-game values. The full digital D-pad requests run; holding L+R requests walk. Unarmed R requests sprint. Camera-relative D-pad directions turn the body using `pedstats.dat`'s 15-degree heading change per original timer unit; X/B raise/lower the camera orbit and Y/A turn it left/right, on foot and in vehicles. L jumps on foot and brakes in vehicles; R fires while armed. With the D-pad released, L+R enters/exits a stopped vehicle. L+Select reloads if armed.

`PlayerMotion` uses reVC's 0.07 move-amount acceleration per timer unit, synchronized walk/run/sprint periods, 2/second sprint blend, authored walk-start and run-stop travel, 150 initial stamina, -150 exhaustion floor, sprint drain 50/second, idle recovery 25/second and running recovery 7.5/second. It retains stamina/training across car entry and exit. The controller advances in fixed 1/60-second steps, including up to half a second of catch-up on each rendered frame. Pausing suspends the controller. Render stalls longer than this bound still limit catch-up; car simulation keeps its separate step cap.

Player physics uses the original 0.008 gravity in 50 Hz engine units (20 m/s²), 70 kg mass, 8.5 jump impulse, original minimum jump-forward velocity and air resistance rule. The authored launch delay precedes the impulse. Static-world contact uses the source three 0.35-metre spheres centered 0.79/1.19/1.59 metres above supported feet. Sweeps prevent wall tunneling; stepping off a ledge enters falling, floor sweeps land on lower surfaces, and head contact stops ascent. The controller/ground checks cover source-speed travel, stamina, stopping, diagonals, jump apex/landing, ledges, thin walls and low ceilings. This remains a bounded adaptation: the full CPed/CPhysical solver, moving-surface impulses, knockdowns, surface-specific responses, fall damage and swimming are not integrated. Exact full-engine physics parity is not established.

Actors reserve space in the DS geometry budget before world submission, with Tommy drawn first. Models remain reduced for DS hardware; this is not full PC geometry parity.

Driving uses the Sentinel's stock 2×4.9-metre dimensions, 165 km/h limit, engine acceleration, RWD adjustment, brake strength, 30-degree steering and traction values. Steering responds gradually, cornering is limited by traction, gear changes briefly interrupt acceleration, and momentum follows the heading. Four ground contacts align the car with ramps. This is a reduced vehicle model, without full suspension, airborne dynamics or deformation.

The camera orbits at a 3.05-metre distance on foot or 4.72 metres in a car and pulls forward at walls. X raises its elevation and B lowers it, clamped from -5 to 65 degrees; Y/A turn the view left/right. Vehicle steering does not overwrite the user-controlled camera heading. The face buttons have no gameplay action bindings while playing; paused map X/Y controls remain zoom/clear. Camera/shot traces use the collision grid and test duplicate faces once. The collision grid now prefers four-metre cells with an eight-metre fallback when the fixed reference pool would overflow. Cached original floor planes and conservative integer bounds reduce software floating-point work without changing the original COL surfaces. A 96-entry decorative tile cache and sorted region lookup avoid repeated scans. Startup near geometry IO is 224,064 bytes at the host test spawn; a forced warm repeat is 149,764 bytes, including terrain and COL. Unchanged residency performs no geometry reads. HUD IO/Read cover geometry and local path work, while draw time includes texture misses/uploads and FIFO waits; these are not separate GPU measurements.

The current ROM is 255,566,848 bytes with all runtime assets embedded. Exact cinematic build sizes and hashes are recorded in reports/first-mission-runtime.json. Animation/script allocations total 701,004 bytes; texture aliases, terrain buffers, region tables and stdio need additional memory. The live map adds 66,016 bytes for pixels and palette; its bitmap resides in the upper half of sub-screen VRAM C, separate from the console font and tile map. These are not peak-memory measurements. Host tests cover original loader comparison, collision, NPC support, ramp alignment, weapon behavior, pause/retry, full-map rebasing and $250 delivery. Physical DSi compatibility and performance remain unverified.

## Emulator verification — October 14, 2026

melonDS 1.1 is installed locally in `build/emulator/melonds/squashfs-root`. Tests use DS mode, built-in BIOS replacements, generated firmware, the interpreter CPU and software rendering. A read-only homebrew SD image synchronizes `build/emulator/fat`, containing a copy of the complete ROM; assets remain embedded. No DSi NAND/BIOS files are used, so these are DS-mode emulator results.

Before the story adaptation, the manual keyboard check completed pistol pickup, aiming, guard defeat, Sentinel entry, acceleration/braking, delivery and the $250 reward. It also verified walking, car exit, pause/resume and retry. Historical Street Job evidence is in `build/emulator/captures/final-ground-npcs.png` and `final-driving-delivery.png`, with ROM hash and observations in `reports/emulator.json`. The previous world-only baseline measured 59 FPS idle/42 walking. Population and fuller terrain add work; current samples are recorded in `reports/emulator.json`. They are individual observations, not benchmark averages or physical DSi measurements; moving/streaming frame costs still vary.

Testing exposed these runtime problems, now fixed:

- `tickGetCount()` was read without starting its timer. `tickInit()` now runs at startup; elapsed-time movement, animations, cooldowns and FPS counters advance.
- Pistol clips contain only upper-body bones. Their missing bones now inherit the idle pose rather than the DFF bind pose, keeping the actor upright during fire/reload. A host regression checks their vertical span.
- The left/right input signs were reversed relative to the heading convention. Left now aims toward the guard on the player's left.
- Vehicle collision repeated full orientation checks even for an unchanged heading or a stopped car. Those checks are skipped; moving footprints still validate collision. Collision buckets were reduced from 16 to 8 metres.

The installed DeSmuME 0.9.13 fails the modern SDK boot probe. A workspace build of current DeSmuME (`1275dc64f5d1f5ef18dc1bc1fa9e012024b6df1a`) passes that probe in interpreter mode, but its virtual storage/IRQ execution still fails this ROM. Its local command-line and diagnostic changes are retained in `patches/desmume-test.patch`. The successful game verification uses melonDS. Larger ROMs need a larger virtual SD image; the launcher detects an undersized existing image and creates a fresh size-specific image while preserving the old file.

To repeat with the installed local tools:

```sh
make revc
python3 tools/emulator_session.py
# Run control commands in another terminal while the session stays open:
python3 tools/emulator_control.py resize
python3 tools/emulator_control.py capture screen.png
python3 tools/emulator_control.py hold Up --seconds 1
python3 tools/emulator_control.py quit
```

The launcher copies the current ROM into the virtual SD folder and creates an isolated Xvfb display. New virtual SD images live in a workspace-specific temporary cache to avoid filling the project disk. Xvfb and xdotool are extracted under `build/emulator/local`; their system libraries are already present. Controls are X=A, Z=B, S=X, A=Y, Q=L, W=R, arrows=movement, Return=Start and Shift=Select. Quit closes the isolated test instance; installed files remain available.

Load errors: 12 = NitroFS; 14 = runtime index or actor mesh; 15 = streamed geometry; 16 = safe initial spawn; 17 = mission layout; 18 = animation cache. 25 = city map; 26 = costume animation; 27 = cinematic file/audio/streaming failure. The mission VM diagnostics are no longer shown over the map.

## Original mission script integration

`tools/stage_scripts.py` validates the original SCM jump headers, variable section, main section and all 97 mission offsets before copying the file unchanged. Its SHA-256 is `0fb3fd45b6a6dc21870cb48d9abe983510284b2b09b0c3d0eeac51d5dff338c7`. Only the 204,596-byte main section and one mission buffer (maximum 32,408 bytes) are resident. Mission data is loaded from the embedded ROM on demand.

The diagnostic interpreter in `scripts.cpp` follows the upstream argument types, negated comparisons and negative mission-relative labels. It supports NOP, WAIT, variable assignment/copy, immediate arithmetic, numeric comparisons, AND/OR, conditional/unconditional jumps, GOSUB/RETURN, script names and termination. Each step executes at most 256 commands. Bounds errors fail explicitly; unsupported commands halt without skipping or inventing engine effects. This is a limited port of SCM semantics, not the full upstream `CRunningScript` engine.

Main startup executes its header jumps and script name, then stops at `016A DO_FADE`, address 39662. All 97 mission starts are checked on the host and currently stop at nine distinct missing engine bindings: DELETE_OBJECT, SET_DEATHARREST_STATE, REMOVE_BLIP, DO_FADE, REQUEST_MODEL, REGISTER_MISSION_GIVEN, CLEAR_THIS_PRINT, ADD_SHORT_RANGE_SPRITE_BLIP_FOR_COORD and LOAD_MISSION_TEXT. Those are the first encountered blockers, not a complete list of required commands. The diagnostic interpreter now runs only in host checks, releasing 237,004 bytes of script buffers in the game. The diagnostic VM does not share game state with the story adaptation. Full original SCM mission execution still needs model/entity services, text/HUD, fades, mission lifecycle and the remaining commands. The default campaign instead adapts the verified opening objectives directly; later story missions are not implemented.

The new runtime test checks 38 distinct imported poses, paused/non-looping clocks, all mission offsets and starts, waits, signed parameters, arithmetic, NOT comparisons, conditional branching, bounded infinite loops and invalid returns. Host validation targets explicitly enable assertions in release builds.

## Changes to upstream

The patch files in `patches/` preserve changes independently of the ignored source checkout:

- 32-bit integer aliases compatible with devkitARM's legacy reader pointer APIs; an explicit ARM9 build.
- A handheld controller state, libnds key mapping, disabled mouse polling, and SD device-prefix handling.
- Bounded directory parsing and synchronous sector reads instead of POSIX threads and semaphores.
- Disabled desktop graphics menu options and extended GL effects.
- An experimental 2 MB streaming budget and four loaded vehicles. This configuration is compiled but has not been tested with the full engine. It is not evidence that the game fits in DSi memory.
- librw initialization declarations for the asset backend, PNG dimension type compatibility, and initialization of a texture pointer when texture loading is disabled.

The archive uses upstream's null audio manager. Input adapters are compiled, but this slice does not call `CPad::UpdatePads` or `CGame::Process`; its player controls use the custom gameplay module.

## Recreate the source checkout

Keep these revisions fixed before applying the patches. On a fresh checkout:

```sh
git clone --recurse-submodules -b miami https://github.com/mrxenginner/reVC.git external/reVC
git -C external/reVC checkout a26fff4af555a8a425ac9f17d1dd718d00d1f942
git -C external/reVC submodule update --init --recursive
git -C external/reVC apply ../../ports/nds/patches/revc.patch
git -C external/reVC/vendor/librw apply ../../../../ports/nds/patches/librw.patch
```

The root CMake build for reVC still targets its upstream platforms. Use `ports/nds/CMakeLists.txt`, not `external/reVC/CMakeLists.txt`, for this experimental target. Source, asset packs, and ROMs stay local; no upstream changes have been published.

## Full-game link result and remaining work

The explicit full-game link probe calls reVC initialization and processing, so it tests dependencies that this gameplay slice does not use:

```sh
cmake --build build/revc-nds --target revc_game_link_probe -j4
```

**This probe currently fails**, with the diagnostic log in `build/revc-full-game-link.log`. The stock memory layout exceeds the linker's 3.5 MB DS ARM9 allocation. A full DSi target must place suitable engine code/data in DSi-only sections and enforce DSi-mode startup. Missing platform symbols also include device initialization/selection, camera begin/present, mouse/menu compatibility, video mode reporting, and exit handling. The build intentionally leaves these unresolved instead of silently substituting no-op implementations.

Resolving the linker symbols alone will not produce a playable game. The main required work is:

1. Implement a librw DSi rendering device: cameras, rasters, palettized textures, static/skinned atomic pipelines, immediate geometry, HUD/font rendering, transparency, and hardware submission limits.
2. Establish DSi-only executable/data placement, heap and pool budgets, allocation failures, reduced textures/meshes, sector residency, and streaming buffer limits. Original PC assets need conversion and simplification.
3. Complete platform lifecycle and controls, integrate the existing reVC game loop, and validate collision, animation, vehicles, missions, and saves against the PC game.
4. Implement a hardware audio path. The current bootstrap and null audio build are silent.
5. Verify actual rendering, memory, loading latency, and frame rate on a DSi before describing the result as a port of the game.

## Credits and notices

This work uses [mrxenginner/reVC](https://github.com/mrxenginner/reVC), its [librw fork](https://github.com/mrxenginner/librw), and [devkitPro/libnds/calico](https://github.com/devkitPro). The original repository license and notices remain in the local source checkout, including `external/reVC/LICENSE.md` and `external/reVC/vendor/librw/LICENSE`. GTA-derived assets are supplied from the user's local installation; generated ROMs and asset packs are excluded from Git.

## Story and navigation pass (2026-10-15)

The ROM builds with 25 embedded files. A short melonDS check confirmed the new start, Sentinel entry, movement, local/full city maps, touch waypoint placement, distance/bearing, waypoint clearing and map zoom. Captures are in `build/emulator/captures/story-*.png`; `reports/navigation-runtime.json` records build and capture hashes separately. This pass did not perform a complete story playthrough or repeat the earlier test suite. Physical DSi performance remains unmeasured for these changes.

## Opening cinematic format

`tools/scm_cutscenes.py` reads the retail mission using the checked-in reVC opcode definitions, resolves the scene model/animation assignments and attachments, and extracts original subtitle times. `tools/cutscene_render.py` evaluates the original camera Beziers and skin poses against the full source models and textures. Negative ANPK bone tags are resolved by frame name, retaining independently animated prop and vehicle parts. Interior sets come from hotel.ipl; outdoor sets use original nearby placements. Original lighting/effects are approximated. The additional scripted departure reconstructs the original non-cutscene segment using authored cameras, positions, route and INTRO1..4 speech with ordinary character animations; its vehicle/actor timing is approximate.

`VCV1` has a 40-byte header, 88-byte timed subtitle records, a u32 compressed size per frame, signed mono PCM8 speech, then LZ10 frame records. Each decoded frame is a 512-byte RGB15 palette followed by 256×144 indexed pixels. At runtime, only frame buffers, the index/subtitles and a 32 KiB audio ring are resident. A bank-D direct framebuffer keeps the 3D texture banks intact. The clock selects frames, dropping late frames to keep speech synchronized; a long I/O stall re-primes audio and video at the same whole-second point. Header bounds, frame lengths, decoded size, subtitle bounds and the file end are checked. Unsupported or damaged data returns an explicit load error.

The opening's three source cinematics and the hotel scene retain their original camera/animation timing; converted frames are sampled at 10 fps. Original speech is downsampled to mono 16,384 Hz PCM8. The full reVC cutscene renderer, real-time lighting/shadows, facial fidelity, source mission AI and later cutscenes are not ported. These caches and the mission wrapper are a foundation for the next missions, not a claim of full engine integration.

## First mission pass verification

The final ROM links and embeds 30 byte-verified runtime files, including all five cinematic caches. The final packaged opening, timed subtitles, Start skip and return to Tommy with the live map were checked in melonDS. The hotel arrival/completion regression uses real original collision and the same mission state code; it checks vehicle rejection and a stable first-complete state. The complete driving route and physical DSi speech/video performance have not been verified. Build hashes and captures are listed in `reports/first-mission-runtime.json`.


Historical VFD2 layer (superseded by the complete-object renderer below): the distant pack contained 1,046 original individual building/terrain LOD placements, clipped into 16-metre tiles and losslessly LZ10 compressed: 507,324 faces, 11,096,151 bytes. Whole-island backdrops remain excluded. Runtime keeps up to 768 nearby distant faces and submits at most 256 after conservative frustum/range tests, with actor reservations and near geometry together staying below the hardware polygon limit. Far streaming updates after 12 metres of travel or rebasing; culling results are cached with conservative margins across half-metre near-view / one-metre far-view bins and small heading changes. Texture upload stalls do not reduce the adaptive geometry budget. At Ocean View, 12 host-tested headings contain distant geometry and façade vertices up to 20.97 metres above the player; the far cold read is 121,024 bytes and a stationary update reads zero. These counts establish coverage, not visual parity or physical DSi performance.

`-DREVC_INSPECT_OCEAN_VIEW=ON` creates a development build outside the hotel with opening playback disabled and renderer timing counters on the first HUD row. This flag is OFF in the delivered ROM.

When launched in DSi mode, the runtime requests the SDK's 134.06 MHz ARM9 clock. DS-mode launches keep the normal clock; emulator samples here were taken in DS mode.

Movement performance pass (2026-10-15): normal play now prepares the next world snapshot incrementally. Near streaming admits at most eight work units per frame, stops after crossing an 8 KiB read threshold or a 3 ms elapsed target, and never starts another unit after those limits. Terrain reads are split into 2 KiB pieces, decorative tile reads are at most 3,200 bytes, and COL reads are at most 2,560 bytes. An individual storage request or decompression can exceed the elapsed target; this is not asynchronous storage or a guaranteed frame deadline. Old render/collision geometry remains active until the replacement snapshot is complete. Collision has its own revision, so scenery-only updates do not rebuild the physics index. Pending collision storage is released after committing, and pending work is cancelled before a phone cinematic to reduce peak memory.

Streaming uses 0.6 seconds of measured horizontal travel as look-ahead, capped at nine metres on foot and twelve in a car. The fixed 2,048-face near residency now reserves 768 faces for original terrain and 1,280 for decoration; the draw cap and 64-metre fog remain. Far LOD updates are incremental too (eight units, 4 KiB admission threshold, 2 ms target), and ambient path refreshes read at most 64 edges at a time. Near, far and path reads are admitted on separate frames. Texture misses use their existing average-color fallback while the player or camera moves; texture uploads resume when stationary. Idle minimap updates restore marker rectangles instead of repainting unchanged backgrounds. Movement catch-up is capped at six 60 Hz ticks rather than thirty after a stall.

Focused host validation checks exact incremental/blocking geometry and collision equality, atomic commits and rebasing, incremental path IO, the same sprint collision trajectory with look-ahead, and agreement between cached and original slope/jump/landing calculations. Sample peak per-update reads are 10,720 bytes near, 4,640 bytes far and 952 bytes for paths; the old blocking warm near refresh reads 149,764 bytes in the same asset pack. Ground validation still reconstructs all 3,441,975 original terrain faces losslessly. Player speed/gravity, city visibility and the five adapted mission chain checks pass. A short melonDS DS-mode inspection sampled 24 FPS while sprinting, 2–3 ms collision/simulation, and a 31 ms streaming frame; storage variation remains. This is a local performance pass, not a physical DSi frame-rate guarantee or a full mission playthrough. See `reports/movement-performance-pass.json` for the packaged ROM and captures.

Renderer coverage pass (2026-10-15): `tools/stage_coverage.py` and the compatible `tools/stage_distant.py` entry point now export VFO1. The 1,046 complete original individual LOD objects contain 77,432 source faces; exact coplanar boundary/area/UV checks dissolve interiors to 76,896 faces, compressed into 1,507,025 bytes. Courtyards, disconnected boundaries, texture seams and non-affine UVs retain their original triangles. This replaces the old subdivision into 507,324 clipped 16-metre faces. The complete-object pack is the close and distant coverage layer; it no longer excludes everything within 16 metres. Fully offscreen triangles are culled before computing each object's cost; the remaining visible part is admitted as a whole rather than truncated when the budget fills. It retains up to 1,536 faces and submits up to 900 subject to the shared scene budget.

Near detail still uses the original bounded residency pack. At each snapshot commit, the renderer groups resident detail into spatial batches and sorts the material order once. During play it culls batch bounds and admits complete resident batches. Coverage renders first, with a small view-direction depth separation, followed by original detail; a texture miss retains geometry with its source average color. GPU far-plane intersection clipping is enabled (POLYGON_ATTR bit 12), and the near plane is 0.25 metres. Camera-angle cache keys use the actual angles, and culling follows the same 65-degree, 4:3 view transform as submission. Geometry RAM counters and the overflow flag reduce the scene budget when clipping uses too much headroom. These changes address triangle holes and clipping rejection; they do not make every original PC object fit the DSi polygon budget.

The world cache has 32 handles within a 96 KiB pixel budget. Opaque coverage pages use 32x32 samples of the original textures, with correctly scaled UVs; alpha cutouts and new near-detail pages retain 64x64. Cached pages can be shared at their existing resolution. Texture uploads still wait until movement stops, and the actor texture swap reserve is preserved. On-foot cars use their original vehicle LOD; the driven car uses its detailed model.

X now raises the view and B lowers it. Static camera collision is supplemented by oriented vehicle boxes, preventing the camera from entering parked cars. When collision pushes the camera inside the player's body, the player mesh is hidden rather than obstructing the entire view. The D-pad and horizontal camera bindings are unchanged.

`revc_renderer_test` covers complete-object budget admission at close range, near/far-plane intersections, pitched visibility, camera direction, vehicle camera collision, rebasing and cache invalidation. `test_coverage.py` covers planar simplification, preserved courtyards, material/UV boundaries and concave outlines. The Ocean View test checks 12 headings and verifies that submitted counts equal complete selected batches/objects within the combined hardware reservation. See `reports/renderer-coverage-pass.json` for the release hash and emulator evidence. The original terrain, collision, animation and mission data are not changed by this renderer pass. The polygon attribute behavior is documented in [GBATEK](https://mgba-emu.github.io/gbatek/#ds-3d-polygon-attributes).


Vehicle/frontend pass (2026-10-15): `tools/car_mesh.py` retains per-triangle identities and seven original rigid-part pivots in VPA1 sidecars (599 Sentinel triangles, 420 taxi triangles). `VehicleAnimation` transforms wheel roll, front steering and front-door opening with fixed-point vertex matrices; UVs and the neutral mesh are unchanged. `Street` coordinates bounded entry/exit, source transmission thresholds/boost/reverse/drag, braking and damped visual pitch/roll. The 119-triangle driver uses five original `ped.ifp` clips staged by `tools/stage_vehicle.py`. This is a planar handling adaptation; four independent wheel contacts, airborne chassis physics, collision impulses, damage parts, steering-arm blends and mission-car articulation remain incomplete.

`tools/stage_frontend.py GAME_DIR` extracts `models/fronten1.txd`, `movies/Logo.mpg` and `movies/GTAtitles.mpg`. Both complete movies use the existing VCV1 streaming format, 6 fps at 128×72 doubled to a 256×144 letterbox and mono 16,384 Hz audio. Boot shows movies → main menu → New Game → existing original-asset story opening. Menu art occupies the top framebuffer and console navigation the bottom screen. Options toggles cinematic subtitles or shows controls. Pause offers Resume, Map, New Game and Options; save/load remains unimplemented. All six added files are embedded; the release stays below 256 MiB. `revc_vehicle_animation_test` checks rigid shape preservation/neutral equality and source transmission equations; `revc_street_test` checks entry locks, door timing and pause. See `reports/vehicle-frontend-pass.json` for build and emulator evidence.


Collision/cutscene stability pass (2026-10-15): `World::updateCollision` now runs bounded COL work before visual streaming and publishes its complete collision snapshot independently. Original 24 m selection and four-wheel support checks are retained; collision lookahead is capped at 6 m independently of scenery lookahead. A priority update admits up to 16 reads / 32 KiB, with a 4 ms soft time bound on ARM9; an admitted 2.5 KiB read can cross the byte/time limit. Render snapshots preserve/rebase the active collision buffer and invalidate its grid whenever the origin changes. The old combined path remains for blocking loaders and reference validation. No movement-speed cap or collision bypass is used to hide absent data.

The added original-road regression uses -150/10/-350, roughly 90 m of source sprint and 144 km/h vehicle traversal, with decorative streaming scheduled only on alternate frames. It compares every position against blocking COL loads, tests independent collision commits and rebasing, and runs the old combined pipeline alongside it: old travel 35.94 m on foot / 27.20 m in a car versus 89.96 m / 88.00 m with priority collision. These are host correctness comparisons, not device FPS or storage benchmarks. The older short movement test remains as a regression and does not establish broad city traversal by itself.

`Cinematic` explicitly initializes Calico sound and owns channel 15 while playing. Stop commands are acknowledged with `soundSynchronize` before audio RAM is reused or freed, including stalled-stream recovery and frame errors. Start skips the current story scene; Select skips the remaining sequence. A visible elapsed/total timer distinguishes slow playback from a frozen scene, and error 27 now includes scene path and failure stage (header/open 1, allocation 2, index/subtitle layout 3, PCM read 4, frame/decompression 5). `revc_cinematic_test` compiles the production player against a host hardware harness, plays all seven complete caches sequentially, checks initialization/ownership, injects a long stall and corrupt frame, and tests both skip actions. ASan/UBSan also pass with leak detection disabled because LeakSanitizer is incompatible with this sandbox's tracing.

Calico's existing ARM7 image already handles audio and block-device requests. The renderer still submits to DS 3D hardware from ARM9; this pass does not add a custom ARM7 geometry worker. Future background decompression/culling work needs a bounded shared-memory job protocol and profiling to demonstrate a benefit. The previous build's whole unskipped story sequence completed in melonDS, so the reported physical-device softlock remains unconfirmed; the audio lifetime fix addresses an unsafe handoff without claiming a proven hardware root cause. Final evidence is in `reports/streaming-cutscene-stability.json`.

The collision grid now anchors its four/eight metre cells to the collision snapshot bounds. Collision that advances beyond the older scenery origin therefore retains the indexed broad phase instead of dropping to a full triangle scan. The translated-snapshot regression retains floor support, wall blocking and traces at local 250/-300, beyond the old fixed indexing box. Pending render/collision staging is released before pause menus and journey restarts to leave cinematic scratch memory available.


Fleet/texture update (2026-10-15): `tools/stage_fleet.py` regenerates Sentinel/taxi/Admiral/Spand Express meshes, shared 128-pixel atlases, original textured far LODs, seven-part wheel/door rigs, source seat/radius/wheelbase metadata, and pelvis-relative driver animations. Close detail is assigned to the closest visible car within 12 m, across parked/traffic/player roles. Up to six ambient cars drive along source car paths; ten total slots retain abandoned and borrowed mission cars. L+R hijacks the nearest healthy vehicle within 3.5 m, preserves model/damage and parks the former player car. Borrowed mission targets retain mission health/position and are no longer drawn twice. Ambient drivers flee after ejection. Mission cars start parked to preserve objective staging; original police/chase AI, traffic signals, per-model source handling and full suspension remain incomplete.

Texture residency uses four permanently uploaded 16 KiB car atlases, 48 world GPU entries capped at 144 KiB, and eight decoded CPU pages keyed by physical offset. Moving texture misses receive bounded upload work instead of waiting for the player to stop. Storage misses still fall back to average source colors; the two-ms admission limit cannot interrupt one slow SD read. The new `revc_fleet_test` covers actual fleet rigs, outward doors, all model hijacks, driver seats, health/identity retention, wreck escape, parked-car persistence, vehicle contact and texture alias/cache eviction. Existing world and campaign checks also cover multiple moving traffic cars and borrowed mission position/damage/epoch isolation.


Renderer budget repair (2026-10-16): `RenderBudget` reserves 512 near-world triangles and 64 primitive overhead before optional actor admission or car-detail upgrades. Standing-player cost is used when planning car detail in either occupancy state. Nearby geometry consumes the world allowance before distant whole-object coverage. The CPU frame-time controller has a 512-triangle minimum; the GPU scene ceiling remains 1536–1760, with the final 1980-primitive guard. `World::visible` now charges actual frustum-visible face counts, prioritizes nearby walkable surfaces, and preserves material ordering. `visibleTileTriangles` reports selected visible face costs. Frame-time pressure can lower optional actor detail/visibility while preserving local ground. `revc_near_visibility_test` checks prolonged budget reduction, crowded actor admission, on-foot/driving reservation, and floor coverage across 48 views around Ken and Ocean View. Physical DSi performance is not established by these host checks.


Collision refresh deadline update (2026-10-16): `World::updateCollision` tracks actual displacement/height against the published COL centre. At 16 m horizontal displacement or 5 m height change, pending collision is drained in the current update, overriding the normal background read/time admission. This leaves eight metres within the existing 24 m selection for hull support and the bounded movement step. Pending centres more than 12 m from the current player (or stale by height) are replaced during urgent work; a pending centre 16 m away also triggers urgent work even if the published centre is still close. The production call retains a 16-read background admission; the optional `readLimit` argument allows the host to model a slow SD frame with one read. `urgentCollisionRefresh` and its counter expose recovery work to host checks. Urgent loads can exceed 32 KiB / 4 ms and cause a frame pause; they never turn missing geometry into permission to pass through walls. Regression tests compare every movement frame with a fully loaded reference at source sprint speed and 144 km/h, including one-read frames, direction reversals and visual-origin rebasing.


Background/driving update (2026-10-17): the main loop no longer requires `world.lastReadBytes()==0` before doing any coverage work. `DistantWorld` keeps its existing 2 ms / eight-piece / 4 KiB soft admission and skips a frame only for urgent COL recovery. Complete resident objects are copied into the next snapshot by source object ID, with coordinates correctly rebased; only new objects perform file reads/LZ decoding. Published centre and completed snapshot counters are exposed for validation, separately from view-cache revision changes. Coverage prefetch uses the same six-metre displacement lead as scenery. `RenderBudget` reserves 512 near + 256 coverage + 64 overhead before optional actors or car-detail upgrades. The actual camera/fog near/far planes share `Projection` (0.25/80 m).

`World::update` adds an optional `driving` argument: its visual admission becomes 16 pieces / 16 KiB / 4 ms soft, with 8 KiB terrain read pieces. Walking remains eight pieces / 8 KiB / 3 ms soft / 2 KiB reads. Terrain candidate decoding increases from 128 to 512 faces per work piece; source faces/UVs remain unchanged. The 96-entry decoration cache retains its original allocation, avoiding eviction regression. Background may now read while near scenery is busy, so total IO admission can include both allowances. Existing urgent collision recovery takes precedence. Physical DSi performance and SD latency remain unmeasured.


If published visual geometry lags the player by over 10 m, `World::groundFallback` selects up to 128 walkable original COL faces near the player. The selected indices are cached by collision revision, four-metre horizontal/two-metre vertical focus bins and budget. They render beneath fine terrain in a neutral loading color; their cost is subtracted from the near allowance and included in total primitive accounting. No collision data is changed, and no extra file reads occur. A visual-stream freeze regression drives 80 m using priority COL updates and checks 98 supported floor samples against fallback triangles, including collision snapshot changes. Fine textured terrain takes precedence as it becomes available. This can show plain road surfaces during fast travel; it does not reproduce all original terrain materials during a storage stall.

## Expanded missions and touch weapon wheel (October 17)

Tap the pistol icon at the lower-right corner of the live bottom-screen map.
The radial wheel pauses simulation. Tap the top segment for fists, or the
upper-right segment for an owned pistol. Selection immediately restores the
map. Tap the centre or press Start to cancel. Empty sectors cannot be selected.
Holstering preserves pistol ownership and both ammunition counts; switching
never refills ammunition. This build supports fists and the pistol.

The campaign now has eight adapted mission steps: the opening hotel mission,
The Party, Back Alley Brawl, Jury Fury, Riot, Treacherous Swine, Mall Shootout,
and Four Iron. After Riot, follow the marker to Cortez's yacht for the next
two jobs, then Avery's construction site for Four Iron. Treacherous Swine uses
the penthouse entrance to transition to the original rooftop coordinates;
after eliminating Gonzalez, leave the entrance area and lay low for 15 seconds.
Mall Shootout includes the upstairs courier fight, chip pickup and delivery to
Cortez. Four Iron includes membership collection, the Leaf Links entrance,
bodyguards and the businessman. R retries a failed mission.

These are bounded adaptations using reviewed retail SCM locations, targets,
GXT dialogue and rewards. They do not execute the complete original SCM VM.
The new missions reuse existing pedestrian models and combat AI; the original
chainsaw, courier motorcycle pursuit, golf-cart escape, security checks, police
pursuit and clothing changes are not fully reproduced. Mission ordering is
linear. Progress is still kept in memory and resets on restart.

Successful playback or skipping moves directly into the objective. If an added
mission introduction cannot load, the original text briefing remains available
(R: next page, L: skip); the mission does not fail solely because of a movie.

Ten added original-asset cutscene conversions play at their mission triggers:
LAW_1A/B, LAW_2A/B/C, LAW_3, LAW_4, COL_1, COL_2 and TEX_1. Together with the
opening and title movies there are 17 embedded cinematics. Their source poses,
cameras, geometry, dialogue timing and speech are converted offline; they are
not real-time cutscenes. Story and mission caches are now 128x72 at 30 fps, doubled during playback,
with full-duration 16,384 Hz mono speech and subtitles. Logo and title movies
retain their existing 6 fps conversion. Both old VCV1 frame sizes remain
supported alongside the VCV3 motion-compressed caches. The
yacht scenery created dynamically by the main SCM is included in conversion.

Reproduction: run tools/stage_expanded_missions.py against the owned game, then
stage_cutscenes.py with --missions 1 2 3 4 5 6 7 8 18 --fps 30 --compact
--motion --output build/cutscenes-motion, and stage_departure.py with --fps 30
--motion --output build/cutscenes-motion. Copy the completed 15 scene caches
into sdcard/revc/cutscenes. Keep existing logo/title caches and frontend.bin.
The legacy compact_cinematics.py tool is for VCV1 only; it does not generate
new animation frames. Run make revc to package assets in NitroFS.
The native mission-chain check relocates between objectives and restores health
for each mission; it verifies mission logic against actual world collision,
not a continuous hardware playthrough. Physical DSi validation remains needed.

## Rendering and cutscene performance pass

The 15 story/mission cutscenes are regenerated from original source animation
and cameras at 30 fps. This is not frame duplication of the old 6 fps caches.
Their spatial resolution remains 128x72, displayed at 256x144. VCV3 uses a
shared scene palette, independently seekable keyframes every 30 frames,
8x8-block motion prediction and LZ10 residuals. Colour changes within one
RGB555 channel step can retain the predicted colour to stabilize quantization.
The decoder retains VCV1/VCV2 compatibility, uses sequential 32 KiB read-ahead,
and expands each source pixel with paired 32-bit framebuffer writes. The
cutscene status line shows measured presentation fps followed by source fps.
ARM7 audio stop acknowledgement, source speech, subtitles and skip controls
remain intact. A source frame can still be dropped if storage misses its deadline.

The live renderer keeps its existing geometry, draw distance, collision and
simulation behavior. Texture uploads now happen before geometry submission,
with near materials taking priority over the coverage shell. A 32-pixel page
can promote to its 64-pixel source page instead of remaining permanently coarse.
A bounded original-source detail pack adds selected 128-pixel world rasters;
new detail uploads wait for the camera, vehicle and world streaming to settle.
Moving gameplay retains the normal upload budget and cached detail pages.
All sizes share the existing 144 KiB world texture budget and use matching UV
scales. Source baked lighting retains midtone contrast instead of saturating
faces after ambient addition. Lighting uses a precomputed lookup table.

The offline cutscene renderer's full original meshes do not fit the live
renderer's current scene budget. Live actor/vehicle/world meshes are unchanged.
The attempted middle car LOD could not be reduced safely enough and is not
included. This pass brings texture/material quality closer; it does not achieve
the complete cutscene appearance in free gameplay.

Build the detail pack with tools/stage_detail_textures.py against the owned
game. tools/validate_cinematics.py cross-checks every frame against the actual
C++ decoder and verifies speech/subtitle preservation against six-fps backups.
The optional revc_cinematic_probe CMake target packages the production player
with a regenerated sample and displays frame/drop/timing counters. The first
emulator probe presented all 948 frames at 30 fps with zero drops; physical
DSi timing remains unverified. Larger packages can use the launcher's ROM file
path; the matching-SD-file mount fallback also supports direct emulator boot.
No separate asset directory is required beside the ROM.

## Close geometry and moving streaming pass — October 17, 2026

Near floors and walls now use separate four-metre visibility groups. A dense
facade cannot consume the admission cost of its neighbouring pavement. The
512-group table retains material sorting and the existing polygon limits,
coverage shell, fog and draw distance. The road regression checks 27 locations
and 16 camera angles: all 4,396 previously visible floor samples survive the
normal minimum 512-face budget. At an artificial 128-face stress budget, one
sample is missing, versus 496 with the previous combined eight-metre groups.
This does not establish complete city-wide geometry coverage.

Texture residency follows the current nearest-material priorities. A recently
used distant page can yield to a higher-priority close material; it can no
longer block that upload indefinitely just because both remain in view.
Sorted material runs take a constant-time fast path in the texture planner,
and its upload prepass stops when the storage/upload budget is consumed.

Collision triangles are losslessly ordered into 128-face spatial blocks.
VCC2 requires the accompanying conservative one-metre bounds in
collision-bounds.vcb. The bounds only skip impossible blocks; the existing
per-face selection still decides membership. The old VCC1 layout remains
readable. tools/stage_collision.py produces this index automatically;
tools/index_collision.py can rebuild it from existing staged collision.
The actual-position refresh trigger is separate from the lookahead centre,
so standing still with a six-metre lead performs no repeated reads/rebuilds.
Floor-plane construction rejects walls with integer math and shares one
reciprocal, reducing ARM9 software floating-point work.

On four-megabyte DS launches, 64 existing tile slots cache decoration and 32
cache collision blocks. A small decoded floor cache protects the immediate
ground working set. On actual DSi-mode launches, an optional 1,050,624-byte
heap cache holds 128 collision/packed-terrain blocks and all 96 decoration
slots remain available. Allocation failure safely retains the smaller path.
The DSi cache route reads 572,745 bytes versus 2,433,280 on the DS path across
the same 24 road snapshots in the native test. These are host read-volume
measurements, not hardware FPS. Urgent collision refresh remains a synchronous
safety fallback and cold SD reads can still stall.

Run tools/validate_spatial_streaming.py after building the host targets to
compare selected collision byte-for-byte with the unindexed source. See
reports/renderer-rework-tests.json and reports/renderer-rework.json for this
pass's checks. Physics, missions, inputs and cutscene assets are retained.

## Moving-world tile streaming — October 17, 2026

The previous pass still waited for an entire neighbourhood before publishing
near geometry. A car could outrun that snapshot even when individual reads
were cached. Normal gameplay now publishes completed eight-metre source tiles
independently, retains useful existing tiles, and evicts complete distant tile
groups. Boot/explicit full snapshots retain the original loading path.

Ground starts first, with one scenery tile interleaved after two ground tiles.
A retarget keeps completed geometry and lets the pending scenery turn finish,
preventing scenery starvation. Car prediction now extends up to 24 metres
according to actual displacement; collision keeps its separate six-metre lead
limit and original surfaces. The moving detail work set is eight metres for
new ground and sixteen metres for scenery around that predicted point;
resident tiles persist until eviction. The distant coverage layer, 64-metre
fog limit and GPU polygon budget remain in use.

Cached region directories no longer consume an entire streaming work piece.
Small ground tiles decode, parse and publish in the same piece as their read.
Material batching uses a stable two-pass radix sort when tiles change. Local
coordinate rebasing follows current movement independently of scan completion.
Emergency COL-floor coverage checks for resident ground under the player,
instead of treating an unfinished optional scenery scan as a missing street.

The map index now allocates its actual 4,925 entries instead of reserving 16,384
entries in BSS. This saves approximately 179 KiB overall and leaves room for
the additional stream cursors and the cutscene decoder on four-megabyte DS
launches. DSi-mode caching remains optional and has the DS fallback.

revc_moving_visibility_test traverses a real 96-metre source road and a
192-metre out-and-back route with a 512-face near budget. Twenty cases cover
running, driving, turnarounds, both cache paths, and a slower-frame driving
case, limiting streaming to two work pieces per frame. All 7,116 checked
ground points under the player and two/five metres ahead remain rendered;
scenery also publishes, and stopping eventually causes zero geometry reads.
The previous renderer fails these same routes even with its ordinary larger
work-piece allowance. This is a spatial/deadline regression, not a physical
SD timing model or a city-wide playthrough. Actual hardware FPS remains to be
confirmed. Results are in reports/tile-streaming-tests.json; the comparison
output and exact release details are in reports/tile-streaming-baseline.txt
and reports/tile-streaming.json. Build and package with make revc as usual.

## Gameplay at cutscene resolution — October 18, 2026

Gameplay now uses a 128x72 3D viewport, presented as 256x144 with integer 2x
scaling and 24-pixel black bars, matching the current story movies. Horizontal
camera framing and on-screen proportions are retained; the former 4:3 view is
cropped vertically to 16:9. The bottom map, text and weapon wheel stay at their
original 256x192 resolution.

This is a separate presentation step in gameplay_display.cpp. The draw routine,
world streaming, mesh/texture assets, polygon budgets, fog, visibility rules
and gameplay are unchanged. Capture receives the raw 3D output in VRAM D;
a VBlank callback copies only the completed 18 KiB image to the unused F/G
banks and the affine background doubles it. Texture banks A/B, palette bank E
and the map's bank C retain their existing assignments. DMA channel 2 is
reserved for presentation, with no main-RAM framebuffer allocation. Capture
is suspended around menus and movies, then restored without copying their
framebuffer into gameplay. Presentation adds one display refresh of latency.

The viewport has 9,216 pixels instead of 49,152. This reduces the pixel workload,
but does not reduce CPU geometry submission, animation or SD streaming costs;
it is not a guarantee of 30 FPS on a physical DSi. The cutscenes are offline
renders with richer original geometry and textures, so matching their pixel
size/framing does not reproduce their full asset fidelity during live play.

The default CMake option REVC_GAMEPLAY_CINEMA_RESOLUTION is ON. Set it OFF in
build/revc-nds to reproduce the preceding native-resolution presentation for
comparison; no asset restaging or world-renderer changes are needed.

The packaged ROM was checked in melonDS through opening playback, running,
car entry/driving, pause/map/resume, and replaying/skipping the story after
gameplay. Pixel checks confirm exact integer scaling and matching bars for
both gameplay and movie captures. The movement (20 cases, 7,116 samples), movie
playback and render-quality native checks also pass. See
reports/gameplay-cutscene-resolution.json for the release hash and evidence.
Hardware behavior follows libnds' viewport, capture and affine-background
interfaces; viewport Y is bottom-up as documented in
https://melonds.kuribo64.net/comments.php?id=27.

## Cutscene-derived gameplay assets — October 18, 2026

After preparing the existing world/cinematic packs, run the final actor pass:

```sh
OPENBLAS_NUM_THREADS=1 python3 tools/stage_cinematic_gameplay.py "$GAME_DIR"
make revc
```

Run this pass after any older actor/fleet staging commands: the generated UVs
and new shared atlases are a matched set. The runtime expects pedestrians.atx,
fleet-shared.atx and driver2/driver3.vca in addition to the previous assets.
All are embedded by make revc. No original game installation files are edited.

Tommy's player/player2/player3 and seated/entry variants now come from the
original csplay/csplay2/csplay3 cinematic DFF/TXD pairs. Mercedes uses csmerc,
and Lance uses csbuddy. Offline simplification weights facial geometry more
strongly, but retains the existing 520-face player, 120-face driver/Mercedes
and 80-face other-NPC limits. The existing runtime animation decoder and
original gameplay IFP locomotion remain in use; extra cinematic face bones
retain their neutral pose. This is bounded conversion, not full cutscene
geometry or facial animation in the open world.

Tommy's texture area doubles from 128x128 to 256x128. Six NPC types and traffic
drivers now sample original raster textures from one 128x128 atlas instead of
using per-triangle average colors. Four cars share a 256x128 atlas: common
materials and solid colors are deduplicated, detailed rasters receive up to
36x36 samples, distant-car rasters retain their previous 32-pixel detail,
and quantization happens once. Previously a mostly empty
256x256 per-car atlas was quantized, shrunk, and quantized again. Original car
meshes, LODs, wheel/door rigs, seats and handling remain byte-identical.

Resident texture allocation remains 224 KiB at its maximum: player 32 KiB,
fleet 32 KiB, pedestrians 16 KiB, world up to 144 KiB; 32 KiB of texture VRAM
remains free as before. These actor pages load at startup/outfit changes, with
no new actor texture reads while running or driving. The 128x72 display,
world visibility/streaming, near-ground coverage and face-budget policy are
unchanged. Added NPC UV submission is bounded by those existing actor limits.
This is the resource budget for the 30 FPS target; physical DSi timing has not
been measured and a locked 30 FPS is not established.

The bake also fixes named IFP root tracks being applied to both the untagged
clump wrapper and the root bone. Female running previously displaced the
visible mesh by about four metres; converted poses now remain within their
actor bounds. Native checks exercise 16,020 samples across 13 actor variants,
original movement speeds, grounding, car poses, atlas bounds/gutters and the
existing fleet/mission/world regressions. The moving-world test still has
zero missing ground samples out of 7,116. See reports/cinematic-gameplay.json
and reports/cinematic-gameplay-assets.json for build, source and validation
details. The model comparison PNG is an offline asset preview at the same
128x72 sampling size, not an emulator performance capture.

## Equivalent-output performance pass — October 25, 2026

The current assets, 128x72 gameplay presentation, texture budgets, scene
selection and streaming admission limits remain unchanged. This pass removes
repeated CPU work:

- Map redraws reuse an already sampled scanline when adjacent screen rows
  reference the same map row, retaining halfword VRAM writes.
- Streamed geometry keeps its walkable-face classification when carried into
  the next snapshot. Expired patches are removed in one stable compaction,
  and consecutive faces sharing a source reuse their distance score.
- Frustum tests use the same six integer clip planes with common outcodes.
  Bounding-box sorting still computes the exact minimum depth of all corners.
- Collision-grid indexing replaces division by 256/512 with equivalent shifts,
  including the original floor behavior for negative cell coordinates.

The deterministic ARM9 probe in melonDS DS mode measured these mean times in
microseconds for its DS-cache workload:

| Component | Before | After | Reduction |
| --- | ---: | ---: | ---: |
| Visual streaming | 71,912 | 51,579 | 28% |
| Visibility selection | 12,603 | 9,608 | 24% |
| Collision-grid rebuild | 18,350 | 14,882 | 19% |
| Moving map redraw | 15,142 | 8,761 | 42% |

These are separate component workloads, not complete-frame FPS. Rebuilds and
map redraws do not happen every frame. Both versions consumed the same read
bytes and produced the same geometry/UV hashes. Streaming still has large
peaks; a locked 30 FPS on physical DSi has not been established.

Native before/after comparisons cover 1,516 moving frames and 154 collision
grid rebuilds, with exact equality of geometry, UVs, culling/order, walkability,
collision grids and read sizes. Separate checks compare 100,000 frustum cases
and 60 full/partial map redraws. The moving-world regression retains zero
missing floor samples out of 7,116. All 115 staged asset files retain their
pre-pass hashes.

After building the normal game, build the separate diagnostic ROM with
`cmake --build build/revc-nds --target revc_performance_probe -j4`. The result
is `build/revc-nds/revc_performance_probe.nds`; it embeds local assets just like
the gameplay ROM. See `reports/performance-preserve-visuals.json` for exact
measurements, captures and validation details.

## Mission and cutscene expansion — October 26, 2026

Start opens a pause menu with **Missions** and **Cutscenes**. Use up/down to
select, left/right to move eight entries, A to open, and B to return. The
main menu also has Cutscenes. Start skips a playing movie; Select returns from
it. Selecting the opening mission replays the opening story. A failed optional
movie falls back to the briefing or selector rather than blocking progress.

The library indexes 97 retail SCM slots. All 96 player-facing slots can be
selected. Slot 0 runs world initialization. The two opening slots share an
opening controller, followed by seven individually adapted missions. Another
65 early adaptations use up to eight original outdoor locations each, including
27 with combat or vehicle targets. Completing Four Iron leads into a 49-entry
story route ending at Keep Your Friends Close.

### Property purchases and side activities

Open **Start → Missions**. A plus sign identifies the 22 activities added in
November. Selecting an entry displays a briefing and marks its location on
the live map. The 15 properties use their original purchase prices. Walk to
the outdoor marker and press R to buy. Ownership and cash last for the current
journey. The purchase scenes play after payment. Story prerequisites, property
income and saving remain future scripting work.

For **Firefighter**, **Vigilante** and **Distribution**, go to dispatch, enter
the marked vehicle with L+R, then tap R to start. In the fire truck, stop near
each of three burning vehicles and hold R to extinguish it. In the police
cruiser, reach each of three suspects and get out to engage. In Mr. Whoopee,
stop by each of five customers and tap R to sell. Completed shifts pay again
on replay. Dispatch locations and payouts are handheld adaptations.

For each **Chopper Checkpoint**, go to the marked launch site on foot and
press R to board the Sparrow. The D-pad flies relative to the camera, R
climbs and L descends. The HUD shows aircraft and gate heights in metres.
Fly through the gates in sequence and descend at the launch site to finish.
The four courses contain 29, 17, 17 and 22 original three-dimensional gates.
Launch markers use accessible outdoor locations near the retail spawns.
Flight uses a simplified controller and the original Sparrow mesh.

### Cutscenes

The ROM contains all 70 sequences referenced by mission cutscene loads, the
scripted Ken departure and both startup movies. GSPOT plays after the last
G-Spotlight objective. FIST closes Publicity Tour. Both are reconstructed
from the successful SCM paths, including the original cameras, models, audio
and available subtitles. Character gestures, lighting and rooftop searchlight
timing are approximations. The other 68 mission scenes use the existing
DAT conversion. Mission films stream at 128×72 and 30 frames per second.

Rebuild the content from your own compatible installation:

```sh
python3 tools/stage_cutscene_library.py "$GAME_DIR" --workers 4
python3 tools/stage_campaign_library.py "$GAME_DIR"
make revc
```

Run the mission library generator **after** movie conversion so availability
flags match the embedded files. The converters retain existing VCV files;
remove only a specific locally generated file if you intentionally want to
reconvert it. Conversion failures are listed in `reports/cutscene-library.json`.
Generated data, movies, reports and ROMs remain outside the source export.

Native validation visits all **212** new checkpoints against the original COL
geometry and completes all 27 combat objectives using production placement,
line-of-sight and strike checks. It checks mission failure/retry and movie
fallbacks, as well as the original eight-mission chain. Separate movie tests
exercise the production decoder and ARM7 audio ownership. These checks do not
establish perfect original scripting or physical DSi performance.
