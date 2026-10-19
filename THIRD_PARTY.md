# Dependency provenance

The dependencies below are fetched locally into ignored `external/` folders.
Their complete source trees and Git histories are not included in this export.

| Dependency | Repository | Tested revision | Local patch |
| --- | --- | --- | --- |
| reVC | https://github.com/mrxenginner/reVC | `a26fff4af555a8a425ac9f17d1dd718d00d1f942` | `ports/nds/patches/revc.patch` |
| librw | https://github.com/mrxenginner/librw | `0ad67ee226ea51beb3f465cb8857195e4db16029` | `ports/nds/patches/librw.patch` |

The patches exactly match the source changes used by the development build.
They adapt input declarations, fixed-width types, file handling and handheld
build options. Patch context originates in the corresponding upstream project.

Verbatim notices from these revisions are retained in
[licenses/reVC-LICENSE.md](licenses/reVC-LICENSE.md) and
[licenses/librw-LICENSE.txt](licenses/librw-LICENSE.txt). The reVC notice is
titled “MIT License with Proof-of-Usage Condition (MIT-PoU)”; do not label it
as ordinary MIT. No replacement license or project-wide SPDX designation has
been assigned by this export.

Other tools are installed separately: devkitARM, libnds, Calico, libfat,
ndstool, Python, NumPy, Pillow, pefile, PyOpenGL, Mesa/OpenGL/GLU and FFmpeg.
Their own distributions retain their respective notices. Optional emulator
helpers expect locally installed programs; no emulator or firmware is bundled.

Grand Theft Auto and Vice City names identify the compatible game. Game assets,
script text, music, speech, artwork and generated representations are excluded
from this source export. The dependency notices do not grant rights to those
assets. This folder has not been published to any remote service.
