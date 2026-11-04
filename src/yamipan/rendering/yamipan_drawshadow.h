/* ==========================================================================
 *  yamipan_drawshadow.h
 *
 *  The boss stage's shadow target, the native side of what
 *  drawSysBuildBossShadowTexture (game/drawsys.cpp) does with GS render
 *  targets: the boss's silhouette on white, seen from above, blurred down
 *  and up, then read through an alpha ramp as the frame's shadow texture.
 *  The game keeps every decision (sizes, order, taps, ramp); the module owns
 *  the targets (no GS memory, no page aliasing).
 *
 *  `list` is the packet the game draws into. renaDrawModels between
 *  YamiPan_ShadowBegin and the first blit draws into the target.
 *
 *  Declarations with empty bodies in seams.cpp until the 3D host module
 *  exists (open-items.md "Next up").
 * ======================================================================== */
#ifndef YAMIPAN_DRAWSHADOW_H
#define YAMIPAN_DRAWSHADOW_H

enum YamiPanShadowAxis
{
    YamiPanShadowAxis_U,
    YamiPanShadowAxis_V,
};

constexpr int kYamiPanShadowTaps = 3;

/* One step of the chain: the last result (srcSize square) into a new target
 * (dstSize square), three passes of the source shifted -1, 0, +1 source texels
 * along axis, each blended over the last with its alpha in tapRgba (0x80 is
 * 1.0, so 1, 1/2, 1/3: the mean of the three). offsetU / offsetV shift the
 * source by that many of its texels. */
struct YamiPanShadowBlit
{
    int          srcSize;
    int          dstSize;
    YamiPanShadowAxis axis;
    float        offsetU;
    float        offsetV;
    const unsigned int *tapRgba; /* kYamiPanShadowTaps */
};

/* A new size-square target, cleared to white. */
void YamiPan_ShadowBegin(const void *list, int size);
void YamiPan_ShadowBlit(const void *list, const YamiPanShadowBlit *blit);
/* The result becomes the frame's shadow texture: its alpha is alphaRamp
 * (0x80 is 1.0) at the result's brightness (0..255). */
void YamiPan_ShadowEnd(const void *list, const unsigned char *alphaRamp);

#endif /* YAMIPAN_DRAWSHADOW_H */
