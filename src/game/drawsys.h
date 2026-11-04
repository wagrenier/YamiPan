/* ==========================================================================
 *  game/drawsys.h
 *
 *  The draw system (game/drawsys.cpp, 0x2d46a0..0x2dac70): what the stage
 *  hands rena each frame. One record (gp-0x7994) holds the models to draw,
 *  the camera's matrices and the lights; drawSysRender runs the passes over
 *  it. No file string and no RTTI names: DrawSys* are the port's names,
 *  from the callers (StageScene::draw, the "rena draw" timing mark).
 *
 *  Every class here sits on five or six empty bases and up to four vptrs in
 *  the ROM (the base ctors and dtors are the addresses listed in the .cpp);
 *  nothing but the leaf classes is ever built, so the host keeps the leaves.
 * ======================================================================== */
#ifndef _GAME_DRAWSYS_H
#define _GAME_DRAWSYS_H

#include <vector>

#include "sdk/libgraph.h"   /* sceGsDrawEnv1 */
#include "sdk/libvifpk.h"
#include "sdk/libvu0.h"     /* sceVu0FVECTOR */
#include "sdk/scetypes.h"

class GamePacketManager;        // game/gamepacket.h
class GsRegionAlloc;            // game/gfxtarget.h
class Tim2Texture;              // game/gsutil.h
struct ModelInstance;           // game/gamehooks.h
struct ModelSurface;            // game/gamehooks.h
struct SceneRender;             // game/gamehooks.h

// Which pass draws a model (ModelInstance::drawGroup, one bit). The far_
// models are the stage's backdrop, drawn at 1 / 20 scale with a fog each.
enum DrawSysGroup
{
    DrawSysGroup_Main   = 0x01, // every other model
    DrawSysGroup_Far0   = 0x02, // the model called far_
    DrawSysGroup_Far1   = 0x04, // far_1
    DrawSysGroup_Far2   = 0x08, // far_2
    DrawSysGroup_Far3   = 0x10, // far_3
    DrawSysGroup_Target = 0x20, // what the view target looks at (StageScene::prepareDrawFlags)
    DrawSysGroup_BossShadow = 0x40, // the boss, for its shadow's draw only
};

constexpr int kDrawSysFogCount    = 10;
constexpr int kDrawSysMaxShadows  = 7;      // sdata 0x31ad0c; also VU1's limit (renaSetShadowMatrices)
constexpr int kDrawSysFarPasses   = 4;      // far_ .. far_3: the fogs of drawSysGetFog 0..3
constexpr int kDrawSysRecordSpace = 200;    // the record's reserved models

// The record's camera: the matrices the stage's camera hands over, and
// view times screen. EE 0xe0: +0x10..+0xd4 are a rena base built by
// 0x2e2970 (its own default screen matrix is replaced at once by
// drawSysInit). layout: host.
class DrawSysCamera
{
public:
    DrawSysCamera();                                                     // FS2 0x002e2970
    virtual ~DrawSysCamera() {}                                          // FS2 0x002d9f30

    const sceVu0FVECTOR *getViewScreen() const { return m_viewScreen; }  // FS2 0x002d9fc0
    const sceVu0FVECTOR *getView() const { return m_view; }              // FS2 0x002d9fd0
    const sceVu0FVECTOR *getScreen() const { return m_screen; }          // FS2 0x002d9fe0
    void setViewMatrix(const void *matrix);                              // FS2 0x002d9ff0
    void setScreenMatrix(const void *matrix);                            // FS2 0x002da060

private:
    void updateViewScreen();

    sceVu0FMATRIX m_screen;     // FS2 +0x10
    sceVu0FMATRIX m_view;       // FS2 +0x50
    sceVu0FMATRIX m_viewScreen; // FS2 +0x90
};

// The directional light (EE 0x40). layout: host.
class DrawSysDirLight
{
public:
    DrawSysDirLight();                                                   // FS2 0x002d91e0
    virtual ~DrawSysDirLight() {}                                        // FS2 0x002d9e00

    void setColor(const float *color);                                   // FS2 0x002d91a0
    const float *getColor() const { return m_color; }                    // FS2 0x002da200
    void setDirection(const float *direction);                           // FS2 0x002d9190
    const float *getDirection() const { return m_direction; }            // FS2 0x002da1f0
    void setEnabled(bool on) { m_enabled = on; }                         // FS2 0x002d91d0
    bool isEnabled() const { return m_enabled != 0; }                    // FS2 0x002da1d0

private:
    sceVu0FVECTOR m_color;      // FS2 +0x10
    sceVu0FVECTOR m_direction;  // FS2 +0x20
    u_char        m_enabled = 0; // FS2 +0x30
};

// The ambient colour (EE 0x30). layout: host.
class DrawSysAmbient
{
public:
    DrawSysAmbient();                                                    // FS2 0x002d8dd0
    virtual ~DrawSysAmbient() {}                                         // FS2 0x002d9d60

    void setColor(const float *color);                                   // FS2 0x002d8db0
    const float *getColor() const { return m_color; }                    // FS2 0x002da210
    void setEnabled(bool on) { m_enabled = on; }                         // FS2 0x002d8dc0
    bool isEnabled() const { return m_enabled != 0; }                    // FS2 0x002da1e0

private:
    sceVu0FVECTOR m_color;      // FS2 +0x10
    u_char        m_enabled = 0; // FS2 +0x20
};

// Fog (EE 0x40): the value A + B / z through (near, nearValue) and
// (far, farValue), which is linear in 1 / z as the GS's Q is; 255 is no fog.
// layout: host.
class DrawSysFog
{
public:
    DrawSysFog();
    virtual ~DrawSysFog() {}                                             // FS2 0x002d9ac0

    void  setNear(float value) { m_near = value; }                       // FS2 0x001f85d0
    float getNear() const { return m_near; }                             // FS2 0x002d9ba0
    void  setFar(float value) { m_far = value; }                         // FS2 0x001f85c0
    float getFar() const { return m_far; }                               // FS2 0x002d9bb0
    void  setNearValue(float value) { m_nearValue = value; }             // FS2 0x001f85b0
    float getNearValue() const { return m_nearValue; }                   // FS2 0x002d9bc0
    void  setFarValue(float value) { m_farValue = value; }               // FS2 0x001f85a0
    float getFarValue() const { return m_farValue; }                     // FS2 0x002d9bd0
    float getCoefficientA() const;                                       // FS2 0x002d9be0
    float getCoefficientB() const;                                       // FS2 0x002d9c30
    int   getFogValue(float z) const;                                    // FS2 0x002d9c70
    void  setColor(const float *color);                                  // FS2 0x001f8590
    const float *getColor() const { return m_color; }                    // FS2 0x002d9d40
    void  setEnabled(bool on) { m_enabled = on; }                        // FS2 0x001f85e0
    bool  isEnabled() const { return m_enabled != 0; }                   // FS2 0x002d9d50

private:
    sceVu0FVECTOR m_color;           // FS2 +0x10
    float         m_near = 0.0f;     // FS2 +0x20
    float         m_far = 0.0f;      // FS2 +0x24
    float         m_nearValue = 0.0f; // FS2 +0x28
    float         m_farValue = 0.0f;  // FS2 +0x2c
    u_char        m_enabled = 0;     // FS2 +0x30
};

// The record's lights (EE 0x1c, vptrs +0 and +0x18). layout: host.
class DrawSysLights
{
public:
    static constexpr int kLightCount = 3;

    virtual ~DrawSysLights() {}                                          // FS2 0x002da0d0

    DrawSysAmbient *getAmbient() const { return m_ambient; }             // FS2 0x002da160
    DrawSysAmbient *setAmbient(DrawSysAmbient *ambient);                 // FS2 0x002da170
    DrawSysDirLight *getLight(int index) const { return m_lights[index]; } // FS2 0x002da180
    void setLight(int index, DrawSysDirLight *light) { m_lights[index] = light; } // FS2 0x002da190
    DrawSysFog *getFog() const { return m_fog; }                         // FS2 0x002da1b0
    DrawSysFog *setFog(DrawSysFog *fog);                                 // FS2 0x002da1c0

private:
    DrawSysAmbient  *m_ambient = nullptr;                // FS2 +0x04
    DrawSysDirLight *m_lights[kLightCount] = {};         // FS2 +0x08
    DrawSysFog      *m_fog = nullptr;                    // FS2 +0x14
};

// The draw system's record (EE 0x14): a sub-scene pushes its own, sharing
// the camera and lights. layout: host.
struct DrawSysRecord
{
    DrawSysRecord() { models.reserve(kDrawSysRecordSpace); }             // FS2 0x00241da0: the reserve

    std::vector<ModelInstance *> models; // FS2 +0x00: capacity, +0x04 count, +0x08 data
    DrawSysCamera *camera = nullptr;     // FS2 +0x0c
    DrawSysLights *lights = nullptr;     // FS2 +0x10
};

// A shadow of the frame (0x50, seven at 0x59ae90): its box's matrix, the
// box's size.
struct DrawSysShadow
{
    sceVu0FMATRIX matrix;       // FS2 +0x00
    sceVu0FVECTOR size;         // FS2 +0x40
};
static_assert(sizeof(DrawSysShadow) == 0x50, "DrawSysShadow: EE 0x50");
extern DrawSysShadow gDrawSysShadows[kDrawSysMaxShadows];   /* bss 0x59ae90 */

// What the passes read from memory (bss 0x59b0c0): rena's draw of the
// models takes its matrices, fog and draw env from here. layout: host.
struct DrawSysState
{
    sceVu0FMATRIX viewScreen;   // FS2 +0x00: 0x59b0c0
    sceVu0FMATRIX screen;       // FS2 +0x40
    sceVu0FMATRIX view;         // FS2 +0x80
    SceneRender  *render;       // FS2 +0xc0: the main pass's; null in the others
    DrawSysFog   *fog;          // FS2 +0xc4: the pass's
    int           width;        // FS2 +0xc8
    int           height;       // FS2 +0xcc
    float         _d0;          // FS2 +0xd0: 1 (unread)
    float         _d4;          // FS2 +0xd4: 1 (unread)
    float         _d8;          // FS2 +0xd8: 7 (unread)
    float         drawDistance; // FS2 +0xdc
    float         centerX;      // FS2 +0xe0: 2048, the draw env's centre
    float         centerY;      // FS2 +0xe4
    sceGsDrawEnv1 env;          // FS2 +0xf0: the frame's
    // What drawSysBuildDepthClear's packet would cover. PC: the ROM keeps a
    // GIF packet instead (drawsys.cpp).
    int           clearX;
    int           clearY;
    int           clearWidth;
    int           clearHeight;
};
extern DrawSysState gDrawSysState;  /* bss 0x59b0c0 */

// The frame's draw target (bss 0x5a52d0, built on first use). layout: host.
struct DrawTarget
{
    int   _00 = 100;            // FS2 +0x00 (unread)
    int   _04 = 100;            // FS2 +0x04 (unread)
    int   _08 = 100;            // FS2 +0x08 (unread)
    float _10 = 1.0f;           // FS2 +0x10 (unread)
    const sceGsDrawEnv1 *env = nullptr; // FS2 +0x14: the frame's
    int   width = 0;            // FS2 +0x18
    int   height = 0;           // FS2 +0x1c
    int   originX = 0;          // FS2 +0x20
    int   originY = 0;          // FS2 +0x24
};
// FS2 0x002ec110
inline DrawTarget *getDrawTarget()
{
    static DrawTarget target;
    return &target;
}

// Builds the draw system on the screen origin and size with the texture
// region allocator: gfxInitRenderTargets calls it once, last.
void drawSysInit(int originX, int originY, int width, int height,
                 GsRegionAlloc *alloc);                                  // FS2 0x002d7cf0
// The record stack: a sub-scene draws into its own record.
void drawSysPush();                                                      // FS2 0x002d7c70
void drawSysPop();                                                       // FS2 0x002d7c00
DrawSysRecord *drawSysCurrent();                                         // FS2 0x002d4fd0
// The frame's 3D draw ("rena draw"); env is the frame's, flags 0 here.
void drawSysRender(GamePacketManager *packets, const sceGsDrawEnv1 *env, int flags); // FS2 0x002d5560
// Advances every model's texture animations by ticks of 1 / 300 s.
void drawSysTick(int ticks);                                             // FS2 0x002d4ef0
// The flag beside the view target's model mark (prepareDrawFlags).
void drawSysClearTargetView();                                           // FS2 0x002d4780
void drawSysSetTargetView();                                             // FS2 0x002d4790
float getDrawDistance();                                                 // FS2 0x002d9aa0
void  setDrawDistance(float distance);                                   // FS2 0x002d9ab0
// The index-th of the ten fogs (SceneRender::update sets them up).
DrawSysFog *drawSysGetFog(int index);                                    // FS2 0x002d47a0
// SceneRender's textures, with the GS block each sits at.
void drawSysSetShadowTexture(Tim2Texture *texture, int block);           // FS2 0x002d4ec0
void drawSysSetSpotlightTexture(Tim2Texture *texture, int block);        // FS2 0x002d4ed0
void drawSysSetSceneRender(SceneRender *render);                         // FS2 0x002d4ee0
// Hands a new model to the current record: its draw group and the flags its
// name asks for (StageModel::init).
void drawSysAddModel(ModelInstance *model);                              // FS2 0x002d5140
// The surface of the model's first mesh material of kind 1 (StageModel::init).
ModelSurface *drawSysFindKind1Surface(ModelInstance *model);             // FS2 0x002d46a0
// Takes model out of the current record (StageModel's dtor).
void drawSysRemoveModel(ModelInstance *model);                           // FS2 0x002d4fe0
// The boss stage's one shadow; answers how many.
int  drawSysBuildBossShadow();                                           // FS2 0x002d4cf0
// Each shadow's matrices into rena.
void drawSysUploadShadows(sceVif1Packet *packet);                        // FS2 0x002d7a60
// What the boss stage draws instead of binding the shadow texture: the boss
// from above, blurred, as the frame's shadow texture.
void drawSysBuildBossShadowTexture(sceVif1Packet *packet);               // FS2 0x002d5e10

#endif /* _GAME_DRAWSYS_H */
