/* ==========================================================================
 *  yamipan_drawmodel.h
 *
 *  The native draw module of the 3D models (rendering::ModelRenderer). rena/rena_draw.cpp keeps what
 *  the ROM decides on the EE -- which models, parts and materials, the order, the matrices, the lights
 *  -- and hands over each batch of a model file read in place, where the ROM would chain it to VU1. What
 *  the VU1 program does with one (transform, light, fog, clip, kick) is the module's. No VU1, no GS memory.
 *
 *  `list` is the packet the game drew into: a pass is part of that layer, in the game's order.
 * ======================================================================== */
#ifndef YAMIPAN_DRAWMODEL_H
#define YAMIPAN_DRAWMODEL_H

#include <stdint.h>

/* One pass: the frame's world -> screen matrix (sceVu0 layout, screen units around 2048), its fog as
 * (A + B / z) / 255 with 1 no fog, and whether the pass starts on a fresh depth buffer. */
struct YamiPanModelPass
{
    float   viewScreen[4][4];
    float   screenCenterX;
    float   screenCenterY;
    float   screenHalfWidth;
    float   screenHalfHeight;
    float   depthMin;           /* the GS's Z at the far plane and at the near one */
    float   depthMax;
    float   fogColor[3];        /* 0..1 */
    float   fogA;
    float   fogB;
    int     clearDepth;
};

/* A material as the VU1 program and the GS register set would take it. */
struct YamiPanModelMaterial
{
    const void *texture;        /* a Tim2Texture; null: untextured */
    int     scrollU;            /* 0..0xfff: VIF's row, added to every UV (4096 = one repeat) */
    int     scrollV;
    int     blended;            /* over what is drawn, no depth written; otherwise opaque with an alpha test */
    int     unlit;              /* the vertex colours as they are; otherwise multiplied by the light */
};

/* What lights a model: colours 0..1 and each light's direction. */
struct YamiPanModelLights
{
    float   ambient[3];
    float   colors[3][3];
    float   directions[3][3];
};

/* One batch, in place in the model file: all `count` long. */
struct YamiPanModelBatch
{
    const float   *positions;   /* x y z */
    const float   *normals;     /* x y z */
    const uint8_t *colors;      /* r g b a, 0x80 = 1.0 */
    const int16_t *uvs;         /* u v, 4096 = 1.0 */
    int            count;
    int            strip;       /* a strip; otherwise a list of triangles */
};

void YamiPan_ModelBeginPass(const void *list, const YamiPanModelPass *pass);
void YamiPan_ModelSetMaterial(const void *list, const YamiPanModelMaterial *material);
void YamiPan_ModelSetLights(const void *list, const YamiPanModelLights *lights);
/* `world` takes the batch's positions to the world (sceVu0 layout). */
void YamiPan_ModelDrawBatch(const void *list, const float world[4][4], const YamiPanModelBatch *batch);
void YamiPan_ModelEndPass(const void *list);

/* A model took a texture: the module's lookups by texture object start over. */
void YamiPan_ModelForgetTextures(void);

#endif /* YAMIPAN_DRAWMODEL_H */
