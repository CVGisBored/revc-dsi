#include "sector.h"
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cmath>

int main(int argc, char** argv)
{
    if (argc != 3) return 1;
    FILE* expected = std::fopen(argv[2], "rb");
    if (!expected) return 2;
    int result = revcSectorLoad(argv[1]);
    if (result) { std::fprintf(stderr, "Loader failed: %d\n", result); return 3; }
    uint32_t count;
    if (std::fread(&count, sizeof(count), 1, expected) != 1 ||
        count != static_cast<unsigned>(revcSectorTriangleCount())) return 4;
    const RevcTriangle* triangles = revcSectorTriangles();
    int maxError = 0;
    for (uint32_t i = 0; i < count; i++) {
        int16_t vertices[3][3];
        if (std::fread(vertices, sizeof(vertices), 1, expected) != 1) return 5;
        for (int j = 0; j < 3; j++) for (int k = 0; k < 3; k++) {
            const int delta = std::abs(static_cast<int>(vertices[j][k]) - triangles[i].vertex[j][k]);
            if (delta > maxError) maxError = delta;
            if (delta > 2) {
                std::fprintf(stderr, "Geometry mismatch: triangle %u vertex %d axis %d (%d vs %d)\n",
                             i, j, k, vertices[j][k], triangles[i].vertex[j][k]);
                return 6;
            }
        }
    }
    std::fclose(expected);
    std::printf("Upstream reVC/librw loader: %d objects, %u triangles; max coordinate error %d/64 world unit\n",
                revcSectorObjectCount(), count, maxError);
    revcSectorShutdown();
    return 0;
}
