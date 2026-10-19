#pragma once
#include <stdint.h>
struct RevcUV { int16_t uv[3][2]; };
struct RevcTriangle { uint16_t color; int16_t vertex[3][3]; };
#ifdef __cplusplus
extern "C" {
#endif
int revcSectorLoad(const char* root);
void revcSectorShutdown(void);
const RevcTriangle* revcSectorTriangles(void);
int revcSectorTriangleCount(void);
int revcSectorObjectCount(void);
void revcSetKeys(unsigned keys);
#ifdef __cplusplus
}
#endif
