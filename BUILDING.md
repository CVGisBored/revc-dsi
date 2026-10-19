# Building from this source export

Run commands from the repository root. Linux is the tested build environment.
The folder is deliberately not a ready-to-run game; game-derived inputs are
generated locally before compiling.

## 1. Tools

Install Git, Make, CMake, a host C/C++ compiler with `cpp`, and Python 3.11 or
newer. Install devkitPro's DS toolchain with devkitARM, libnds, Calico, libfat
and ndstool. The default SDK prefix is `/opt/devkitpro`; use
`make revc DEVKITPRO=/your/sdk/prefix` for a different installation.

Cinematic conversion needs FFmpeg/ffprobe and an EGL OpenGL compatibility
context with GLU. Mesa's software EGL implementation is suitable for a
headless Linux setup. These binaries and SDKs are not included.

```sh
python3 -m venv .venv
. .venv/bin/activate
python3 -m pip install -r requirements.txt
```

## 2. Fetch the pinned dependencies and apply patches

Read [THIRD_PARTY.md](THIRD_PARTY.md) and the upstream notices first. These
commands obtain dependencies; no dependency download occurs automatically.

```sh
mkdir -p external
git clone https://github.com/mrxenginner/reVC.git external/reVC
git -C external/reVC checkout --detach a26fff4af555a8a425ac9f17d1dd718d00d1f942
git -C external/reVC submodule update --init vendor/librw
git -C external/reVC/vendor/librw rev-parse HEAD
# Expected: 0ad67ee226ea51beb3f465cb8857195e4db16029

git -C external/reVC apply --check ../../ports/nds/patches/revc.patch
git -C external/reVC apply ../../ports/nds/patches/revc.patch
git -C external/reVC/vendor/librw apply --check ../../../../ports/nds/patches/librw.patch
git -C external/reVC/vendor/librw apply ../../../../ports/nds/patches/librw.patch
```

Apply patches once to fresh checkouts. They are source changes for those exact
revisions. If a dependency is unavailable, obtain the same revision through an
authorized source; this export does not supply an alternative code archive.

## 3. Supply your own PC game files

Set the path to an extracted, compatible original PC installation, including
`gta-vc.exe`, `models/`, `data/`, `TEXT/`, `anim/`, `audio/` and `movies/`.
The Definitive Edition is not an interchangeable input. The mission adapters
use reviewed offsets and require this SHA-256 for `data/main.scm`:

```text
0fb3fd45b6a6dc21870cb48d9abe983510284b2b09b0c3d0eeac51d5dff338c7
```

```sh
export GAME_DIR='/absolute/path/to/Grand Theft Auto Vice City'
sha256sum "$GAME_DIR/data/main.scm"
```

Do not bypass the SCM hash checks for a different version. Game paths are only
read; converted files are written into ignored folders in this project.

## 4. Generate the data locally

```sh
# Inspect the complete conversion order without generating anything:
python3 tools/stage_all.py "$GAME_DIR" --plan

# Run the complete conversion:
OPENBLAS_NUM_THREADS=1 python3 tools/stage_all.py "$GAME_DIR"
```

Full city and cinematic conversion is substantial offline work. Allow several
gigabytes of free disk space and do not treat a quiet long conversion as a
hang. Stop on errors and inspect the failed step. To resume after fixing a
dependency, use its name from `--plan`, for example:

```sh
python3 tools/stage_all.py "$GAME_DIR" --from-step stage_cutscenes
```

Resume only when all preceding steps completed. `compact_assets` consumes
uncompressed outputs from earlier stages; rerun its prerequisites before
repeating that step. Logo compaction retains a local backup, so remove that
specific ignored backup if deliberately changing the source logo movies.

The ordered recipe generates mission headers directly from local SCM/GXT data,
builds world/physics/material packs, converts 15 story/mission scenes to 30 FPS
VCV3 at 128×72, and prepares the two six-FPS startup movies. The final
`stage_cinematic_gameplay` step must run after older actor/fleet conversions:
the shared atlases and their model UVs are a matched set.

Outputs live in `generated/`, `sdcard/revc/`, `reports/` and `build/`. Reports
may contain extracted text, source paths or screenshots. None belongs in the
source repository.

## 5. Compile and validate

```sh
make test
make revc
make revc-test GAME_DIR="$GAME_DIR"
```

`make test` runs synthetic Python tests without game assets or reVC.
`make revc-test` requires completed conversion and the patched dependencies;
it builds and runs native asset/physics/animation/mission/codec checks.
`make revc` embeds the generated assets and checks the ROM header plus an exact
NitroFS file comparison. The local result is:

```text
build/revc-nds/revc_dsi.nds
```

For release-style gameplay, retain `REVC_GAMEPLAY_CINEMA_RESOLUTION=ON` and
`REVC_INSPECT_OCEAN_VIEW=OFF`, `REVC_INSPECT_KEN_SPAWN=OFF` in the CMake cache.
The 128×72 scene is presented at 256×144; the bottom screen stays native size.
Host checks do not establish a locked 30 FPS or physical DSi compatibility.

## 6. Local device and emulator checks

The current hardware workflow uses TWiLight Menu++ from a DSi SD card. Copy
your locally built ROM to that card. It contains game-derived assets, so keep
the result local rather than attaching it to this source repository's releases.

melonDS DS-mode smoke checks were used during development. Configure homebrew
SD/DLDI access to the same ROM so NitroFS can read its embedded data. Emulator
timing is not a substitute for DSi timing. The optional `emulator_session.py`
and `emulator_control.py` helpers target an isolated Xvfb display and expect a
workspace-local emulator/tool installation under `build/emulator/`; they are
not installers and are not needed to compile. No BIOS, NAND or firmware files
are supplied.

