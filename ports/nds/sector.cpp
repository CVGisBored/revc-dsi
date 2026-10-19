// Uses reVC's directory and math code, librw's actual PC DFF decoder, and the
// DSi synchronous streaming backend. This is an asset bootstrap, not CGame.
#include "common.h"
#include "Directory.h"
#include "FileMgr.h"
#include "CdStream.h"
#include "sector.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <cmath>

namespace {
constexpr int kMaxTriangles = 1800;
RevcTriangle triangles[kMaxTriangles];
int triangleCount = 0, objectCount = 0;
bool engineStarted = false;
struct Placement { char name[24]; float position[3], scale[3], quaternion[4]; };
static_assert(sizeof(Placement) == 64, "Staged sector ABI");
static_assert(sizeof(RevcTriangle) == 20, "GPU triangle ABI");

bool addGeometry(rw::Clump* clump, const Placement& placement, float cx, float cy)
{
    rw::Quat rotation;
    rotation.x = -placement.quaternion[0];
    rotation.y = -placement.quaternion[1];
    rotation.z = -placement.quaternion[2];
    rotation.w = placement.quaternion[3];
    rw::Matrix matrix;
    matrix.setIdentity();
    rw::Matrix::makeRotation(&matrix, rotation);
    CMatrix world(&matrix, false);
    world.SetTranslateOnly(placement.position[0], placement.position[1], placement.position[2]);
    int added = 0;
    FORLIST(link, clump->atomics) {
        rw::Atomic* atomic = rw::Atomic::fromClump(link);
        if (!(atomic->object.object.flags & rw::Atomic::RENDER)) continue;
        rw::Geometry* geometry = atomic->geometry;
        if (!geometry || geometry->flags & rw::Geometry::NATIVE || geometry->numMorphTargets < 1)
            return false;
        const rw::V3d* vertices = geometry->morphTargets[0].vertices;
        if (!vertices) return false;
        CMatrix frame(atomic->getFrame()->getLTM(), false);
        for (int i = 0; i < geometry->numTriangles; i++) {
            const rw::Triangle& input = geometry->triangles[i];
            CVector points[3];
            for (int j = 0; j < 3; j++) {
                if (input.v[j] >= geometry->numVertices) return false;
                CVector local = frame * CVector(vertices[input.v[j]]);
                local.x *= placement.scale[0];
                local.y *= placement.scale[1];
                local.z *= placement.scale[2];
                points[j] = world * local;
            }
            CVector normal = CrossProduct(points[1]-points[0], points[2]-points[0]);
            if (normal.MagnitudeSqr() < 1e-10f) continue;
            normal.Normalise();
            if (triangleCount >= kMaxTriangles) return false;
            RevcTriangle& output = triangles[triangleCount];
            const float light = .5f + .5f * std::fabs(normal.x*.3f+normal.y*.4f+normal.z*.866f);
            const int r = static_cast<int>(27*light), g = static_cast<int>(25*light), b = static_cast<int>(22*light);
            output.color = r | g << 5 | b << 10;
            for (int j = 0; j < 3; j++) {
                const float coords[3] = {(points[j].x-cx)*64, points[j].z*64, -(points[j].y-cy)*64};
                for (int k = 0; k < 3; k++) {
                    if (!std::isfinite(coords[k]) || coords[k] < -32768 || coords[k] > 32767) return false;
                    output.vertex[j][k] = static_cast<int16_t>(std::lround(coords[k]));
                }
            }
            triangleCount++;
            added++;
        }
    }
    return added > 0;
}
}

// The original file manager expects a platform-supplied user directory.
const char* _psGetUserFilesFolder() { return "./userfiles"; }

int revcSectorLoad(const char* root)
{
    triangleCount = objectCount = 0;
    if (engineStarted || chdir(root) != 0) return 1;
    FILE* placements = std::fopen("sector.vcs", "rb");
    if (!placements) return 2;
    char magic[4];
    unsigned count;
    float center[2];
    if (std::fread(magic, 1, 4, placements) != 4 || std::memcmp(magic, "VCS1", 4) ||
        std::fread(&count, 4, 1, placements) != 1 || count == 0 || count > 64 ||
        std::fread(center, 4, 2, placements) != 2 || !std::isfinite(center[0]) || !std::isfinite(center[1])) {
        std::fclose(placements); return 3;
    }
    if (!rw::Engine::init()) { std::fclose(placements); return 4; }
    rw::registerMeshPlugin();
    rw::registerNativeDataPlugin();
    rw::registerAtomicRightsPlugin();
    rw::registerMaterialRightsPlugin();
    rw::EngineOpenParams params;
    if (!rw::Engine::open(&params)) { rw::Engine::term(); std::fclose(placements); return 4; }
    if (!rw::Engine::start()) {
        rw::Engine::close(); rw::Engine::term(); std::fclose(placements); return 4;
    }
    engineStarted = true;
    rw::Texture::setLoadTextures(false);
    rw::Texture::setCreateDummies(false);
    CFileMgr::Initialise();
    CdStreamInit(2);
    if (!CdStreamAddImage("models/sector.img")) { std::fclose(placements); return 5; }
    CDirectory directory(64);
    directory.ReadDirFile("models/sector.dir");
    for (unsigned i = 0; i < count; i++) {
        Placement placement;
        if (std::fread(&placement, sizeof(placement), 1, placements) != 1 ||
            !std::memchr(placement.name, 0, sizeof(placement.name))) {
            std::fclose(placements); return 6;
        }
        for (float value : placement.position) if (!std::isfinite(value)) { std::fclose(placements); return 6; }
        for (float value : placement.scale) if (!std::isfinite(value)) { std::fclose(placements); return 6; }
        float norm = 0;
        for (float value : placement.quaternion) norm += value*value;
        if (!std::isfinite(norm) || std::fabs(norm-1.f) > .01f) { std::fclose(placements); return 6; }
        uint32 offset, sectors;
        if (!directory.FindItem(placement.name, offset, sectors) || !sectors || sectors > 512 ||
            static_cast<uint64_t>(offset+sectors)*2048 > GetGTA3ImgSize()) {
            std::fclose(placements); return 7;
        }
        const size_t bytes = sectors * 2048;
        rw::uint8* buffer = static_cast<rw::uint8*>(std::malloc(bytes));
        if (!buffer) { std::fclose(placements); return 8; }
        if (CdStreamRead(0, buffer, offset, sectors) != STREAM_SUCCESS) {
            std::free(buffer); std::fclose(placements); return 9;
        }
        rw::StreamMemory stream;
        stream.open(buffer, bytes);
        rw::Clump* clump = nullptr;
        if (rw::findChunk(&stream, rw::ID_CLUMP, nullptr, nullptr)) clump = rw::Clump::streamRead(&stream);
        std::free(buffer);
        if (!clump) { std::fclose(placements); return 10; }
        const bool valid = addGeometry(clump, placement, center[0], center[1]);
        clump->destroy();
        if (!valid) { triangleCount = objectCount = 0; std::fclose(placements); return 11; }
        objectCount++;
    }
    std::fclose(placements);
    CdStreamRemoveImages();
    return 0;
}
void revcSectorShutdown(void)
{
    CdStreamShutdown();
    if (engineStarted) {
        rw::Engine::stop(); rw::Engine::close(); rw::Engine::term(); engineStarted = false;
    }
    triangleCount = objectCount = 0;
}
const RevcTriangle* revcSectorTriangles(void) { return triangles; }
int revcSectorTriangleCount(void) { return triangleCount; }
int revcSectorObjectCount(void) { return objectCount; }
