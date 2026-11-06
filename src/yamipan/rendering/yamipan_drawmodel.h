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

/* The scene's point light as VU1 takes it at addresses 176..179 (0x2e05f0). It adds to a vertex's colour, in the
 * colour's own units, before the lights multiply it: kernel 0x545. */
struct YamiPanModelPointLight
{
    int     active;
    float   position[3];
    float   color[3];           /* times how far the vertex faces it */
    float   shade[3];           /* whatever way the vertex faces */
    float   weight;             /* the strength at the light: weight + slope * distance, above 0 */
    float   slope;              /* -weight / radius */
};

/* The scene's spot: the picture a lamp throws, a second pass over the lit vertices (VU1 0x2eb, addresses 171,
 * 201..204, 210, 211; 0x2e00f0). */
struct YamiPanModelSpotLight
{
    int     active;
    const void *texture;        /* a Tim2Texture */
    float   worldToLight[4][4]; /* sceVu0 layout; the light's x, up, ahead */
    float   scaleX;             /* how much of the picture a unit of x / ahead crosses */
    float   scaleY;
    float   falloff[3];         /* reach = 255 / (0 + 1 * ahead + 2 * ahead * ahead), 255 at most */
    float   limit;              /* no reach at this distance ahead, or past it */
};

/* The frame's shadows: one picture laid over the models that take them, black with the picture's alpha, a pass per
 * shadow (VU1 0x393, addresses 64.. a shadow's 8 quadwords, 150 the count; 0x2d7a60 builds the matrices). */
enum { kYamiPanModelMaxShadows = 7 };

struct YamiPanModelShadows
{
    int     count;              /* 0: this pass lays none */
    const void *texture;        /* a Tim2Texture */
    float   projection[kYamiPanModelMaxShadows][4][4]; /* sceVu0 layout: x, y the picture's 0..1, z and w the fade */
    float   clip[kYamiPanModelMaxShadows][4][4];       /* sceVu0 layout: the shadow's box is -1..1 */
};

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
    float   fogNearValue;   /* the value stays between these two (VU address 24 y, z) */
    float   fogFarValue;
    int     clearDepth;
    float   eye[3];             /* the camera in the world: VU address 4, what culling measures from */
    YamiPanModelPointLight point;   /* only the main pass has one */
    YamiPanModelSpotLight  spot;
    YamiPanModelShadows    shadows;
};

/* A material as the VU1 program and the GS register set would take it. */
struct YamiPanModelMaterial
{
    const void *texture;        /* a Tim2Texture; null: untextured */
    int     scrollU;            /* 0..0xfff: VIF's row, added to every UV (4096 = one repeat) */
    int     scrollV;
    int     blended;            /* over what is drawn, no depth written; otherwise opaque with an alpha test */
    int     unlit;              /* the vertex colours as they are; otherwise multiplied by the light */
    int     pointFlat;          /* the point light ignores which way a vertex faces (material flag 0x10, 0x59a) */
    int     noSpot;             /* the spot's pass skips it (material flag 0x20, 0x2f1) */
    int     noShadow;           /* takes no shadow (material flags 1 and 2, 0x39d) */
};

/* What lights a model: colours 0..1 and each light's direction. */
struct YamiPanModelLights
{
    float   ambient[3];
    float   colors[3][3];
    float   directions[3][3];
    int     takesPoint;         /* ModelFlag_TakesPoint */
    int     takesSpot;          /* ModelFlag_TakesSpot, and the model's spot is on (VU address 200) */
    int     takesShadow;        /* ModelFlag_TakesShadow */
    int     cullBack;           /* ModelFlag_TagC: faces turned from the eye are not drawn (VU1 0x128) */
};

/* One batch, in place in the model file: all `count` long. With a palette the positions and normals go
 * through its matrices (`world` is unit, the palette already takes them to the world): VU1 0x07f. */
struct YamiPanModelBatch
{
    const float   *positions;   /* x y z */
    const float   *normals;     /* x y z */
    const uint8_t *colors;      /* r g b a, 0x80 = 1.0 */
    const int16_t *uvs;         /* u v, 4096 = 1.0; null: the model has none for this batch */
    int            count;
    int            strip;       /* a strip; otherwise a list of triangles */
    const float   *palette;     /* null: rigid; otherwise 4 x 4 floats per palette bone (sceVu0 layout) */
    const int8_t  *boneIds;     /* bonesPerVertex per vertex: VU quadwords into the palette, 4 per matrix; -1 unused */
    const float   *weights;     /* bonesPerVertex per vertex; not read with one bone */
    int            bonesPerVertex; /* 1..4 */
    int            firstBoneOnly;  /* ModelFlag_Distant: every vertex takes its first bone, unweighted */
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
