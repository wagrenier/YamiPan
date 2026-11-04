/* ==========================================================================
 *  game/scenerender.h
 *
 *  The stage's lighting (game/scenerender.cpp, 0x1f6ac0..0x1fa900;
 *  StageScene::render). Twenty light sets, each 36 keyed tracks: the map's
 *  three lights and ambient, the models' three lights and two ambients, the
 *  two kinds of fog, the clear colour and the camera's range. The ENV_CB box
 *  the view (and each lit model) stands in picks the set, by its number; a
 *  change of set fades over the set's own transition frames.
 *
 *  The tracks' keys come only from the stage's scripts: natives 0x1b4..0x1c3
 *  of ScriptRunner (architecture.md "The lights of a stage"). Colours are
 *  script integers / 127.5, so 255 is 2.0.
 *
 *  The ROM's class sits on two of rena's light records (0x2e04e0..0x2e0840):
 *  the scene's point light is its base, its spot a member at +0x80 that
 *  every lit model points at (ModelInstance::spot).
 * ======================================================================== */
#ifndef _GAME_SCENERENDER_H
#define _GAME_SCENERENDER_H

#include "game/gamehooks.h"
#include "sdk/libvu0.h"     /* sceVu0FVECTOR */

class AreaBox;                  // game/areagrid.h
struct AreaGrid;                // game/areagrid.h
class Tim2Texture;              // game/gsutil.h
class PointLight;               // game/stagelight.h
class SpotLight;                // game/stagelight.h

constexpr int kSceneLightSets    = 20;
constexpr int kSceneLights       = 3;   // as DrawSysLights::kLightCount
constexpr int kSceneFarFogs      = 4;   // as kDrawSysFarPasses
constexpr int kSceneSpotTextures = 8;

// The transition track's lanes.
enum SceneTransitionLane
{
    SceneTransition_CameraRange = 0,    // not frames: every camera's range (StageCamera::setRange)
    SceneTransition_Frames      = 1,    // frames a change of set takes
    SceneTransition_SbFrames    = 2,    // the same for a model inside an ENV_SB box
};

// What rena's light records share (vtable 0x31a780). layout: host -- EE 0x70.
struct RenaLight
{
    RenaLight();
    // FS2 0x001f8600
    void setActive(bool on) { active = on; }
    // FS2 0x001f8610
    bool isActive() const { return active != 0; }
    // FS2 0x001f85f0
    const sceVu0FVECTOR *getMatrix() const { return matrix; }
    void setMatrix(const sceVu0FVECTOR *from);                           // FS2 0x002e0810
    void setColor(const sceVu0FVECTOR from);                             // FS2 0x002e07c0
    void setShade(const sceVu0FVECTOR from);                             // FS2 0x002e07b0

    u_char        active;       // FS2 +0x04
    sceVu0FMATRIX matrix;       // FS2 +0x10: unit from the ctor
    sceVu0FVECTOR color;        // FS2 +0x50: (1, 1, 1, 0) from the ctor
    sceVu0FVECTOR shade;        // FS2 +0x60: the colour times SceneRender::m_shadeScale
};

// vtable 0x31a750. layout: host -- EE 0x80.
struct RenaPointLight : RenaLight
{
    RenaPointLight();                                                    // FS2 0x002e0730

    float radius;               // FS2 +0x70: 1200 from the ctor
    float weight;               // FS2 +0x74: 4 from the ctor
};

// vtable 0x31a720. layout: host -- EE 0xa0.
struct RenaSpotLight : RenaLight
{
    RenaSpotLight();                                                     // FS2 0x002e04e0

    float radius;               // FS2 +0x70: 1200 from the ctor
    float reach;                // FS2 +0x74: 1200 from the ctor
    float tangent;              // FS2 +0x78: tan(angle)
    float angle;                // FS2 +0x7c: radians
    float intensity;            // FS2 +0x80: SpotLight::m_intensityPercent / 100
    sceVu0FVECTOR _90;          // FS2 +0x90: (0.8, 3, 4, -0.13) from data 0x2fd9f0; unread
};

// One set's tracks, in the order of the block new[] 0x5a10 gives them: the
// ROM keeps a pointer table per field (+0x12c..+0xc1c, set * 12 + light * 4
// or set * 4 or set * 16 + fog * 4). layout: host -- EE 0x240.
struct SceneLightSet
{
    VectorTrack direction[kSceneLights];        // FS2 +0x12c: from angles
    VectorTrack angles[kSceneLights];           // FS2 +0x21c: degrees; not stepped
    VectorTrack color[kSceneLights];            // FS2 +0x30c
    VectorTrack ambient;                        // FS2 +0x3fc
    VectorTrack modelDirection[kSceneLights];   // FS2 +0x44c: from modelAngles
    VectorTrack modelAngles[kSceneLights];      // FS2 +0x53c: degrees; not stepped
    VectorTrack modelColor[kSceneLights];       // FS2 +0x62c
    VectorTrack modelColorSb[kSceneLights];     // FS2 +0x71c: inside an ENV_SB box
    VectorTrack modelAmbient;                   // FS2 +0x80c
    VectorTrack modelAmbientSb;                 // FS2 +0x85c
    VectorTrack fog;                            // FS2 +0x8ac: near, far, near value, far value
    VectorTrack fogColor;                       // FS2 +0x8fc
    VectorTrack farFog[kSceneFarFogs];          // FS2 +0x94c: the same for far_ .. far_3
    VectorTrack farFogColor[kSceneFarFogs];     // FS2 +0xa8c
    VectorTrack clearColor;                     // FS2 +0xbcc: 0..255
    VectorTrack transition;                     // FS2 +0xc1c: SceneTransitionLane

    // Each track of the set, the angles too (the ROM spells out every one).
    template <class Fn> void forEachTrack(Fn fn);
    // Each track but the angles: what a frame steps.
    template <class Fn> void forEachPlayed(Fn fn);
};

// layout: host -- EE 0xe70.
struct SceneRender : RenaPointLight
{
    SceneRender();                                                       // FS2 0x001fa4a0
    ~SceneRender();                                                      // FS2 0x001fa230: deleting

    // The map's ENV_CB boxes pick the set; ENV_SB boxes the models' second
    // colour and ambient.
    void addCbBox(AreaBox *box);                                         // FS2 0x001f6ad0
    void addSbBox(AreaBox *box);                                         // FS2 0x001f6ac0
    // loadCommon's spotlight and shadow_00.
    void setSpotlightTexture(Tim2Texture *texture);                      // FS2 0x001fa1c0
    void setShadowTexture(Tim2Texture *texture);                         // FS2 0x001fa150
    // The frame: the tracks, the set, the scene's point and spot, the
    // record's lights, the fogs and the cameras' range.
    void update();                                                       // FS2 0x001f73c0
    // The lit models' own lights and the characters' shadow light.
    void updateLights();                                                 // FS2 0x001f8620
    // gfxSetClearColor with the scene's colour.
    void applyClearColor();                                              // FS2 0x001f7380
    // yami_pl / demo_pl, the grids, the bn_ links, the spots' textures;
    // once, in StageScene::setupCameras.
    void setup();                                                        // FS2 0x001f94c0

    // The script's: native 0x1c3 every track's loop byte; native 0x1c2 one
    // new key at time in each track of the set (the transition's is (1600,
    // 30, 30)); the two direction keys from their angles' key.
    void setSetLoop(int set, bool loop);                                 // FS2 0x001f6ae0
    void addSetKeys(int set, int time);                                  // FS2 0x001f6c30
    void updateModelDirection(int set, int light, int key);              // FS2 0x001f7020
    void updateDirection(int set, int light, int key);                   // FS2 0x001f71d0

    // The tracks of a set, for the natives.
    SceneLightSet &getSet(int set) { return m_sets[set]; }

    u_char m_snap;              // FS2 +0xe24: the next values come at once: set by the ctor and a view change, cleared each frame by StageScene::update
    int    m_forcedSet;         // FS2 +0xe28: sceneRenderGroup(0) each frame: -1 the boxes choose

private:
    // The model's ambient: of its set and box, plus extra (null none), more
    // of it on an item lying free.
    void getModelAmbient(StageObject *object, sceVu0FVECTOR out, const float *extra); // FS2 0x001f9ca0
    // The end of setup: the tracks rewound, the values cleared.
    void start();                                                        // FS2 0x001f9ee0
    // PC: the current set has keys. Nothing but a script makes them, and no
    // script runs yet.
    bool isKeyed() const;
    // One object of updateLights' walk.
    void lightObject(StageObject *object);
    // The point light's and the spot's lines of update.
    void updatePointLight(StageLight *point, bool wasActive);
    void updateSpotLight(SpotLight *spot);

    RenaSpotLight m_spot;       // FS2 +0x80
    PointLight   *m_playerLight; // FS2 +0x120: yami_pl, hung under the player's character
    StageLight   *m_lastPoint;  // FS2 +0x124: the point light of the last frame
    SceneLightSet *m_sets;      // FS2 +0x128: the block

    u_char m_fogEnabled;        // FS2 +0xc6c
    u_char m_clearEnabled;      // FS2 +0xc6d
    int    m_set;               // FS2 +0xc70: -1 until the first
    sceVu0FVECTOR m_direction[kSceneLights];  // FS2 +0xc80
    sceVu0FVECTOR m_lightColor[kSceneLights]; // FS2 +0xcb0
    sceVu0FVECTOR m_ambient;    // FS2 +0xce0
    sceVu0FVECTOR m_fog;        // FS2 +0xcf0
    sceVu0FVECTOR m_fogColor;   // FS2 +0xd00
    sceVu0FVECTOR m_farFog[kSceneFarFogs];      // FS2 +0xd10
    sceVu0FVECTOR m_farFogColor[kSceneFarFogs]; // FS2 +0xd50
    sceVu0FVECTOR m_clearColor; // FS2 +0xd90
    sceVu0FVECTOR m_transition; // FS2 +0xda0

    // The ROM news each one.
    VectorFade m_fadeDirection[kSceneLights];  // FS2 +0xdb0
    VectorFade m_fadeColor[kSceneLights];      // FS2 +0xdbc
    VectorFade m_fadeAmbient;   // FS2 +0xdc8
    VectorFade m_fadeFog;       // FS2 +0xdcc
    VectorFade m_fadeFogColor;  // FS2 +0xdd0
    VectorFade m_fadeFarFog[kSceneFarFogs];      // FS2 +0xdd4
    VectorFade m_fadeFarFogColor[kSceneFarFogs]; // FS2 +0xde4
    VectorFade m_fadeClearColor; // FS2 +0xdf4
    VectorFade m_fadeTransition; // FS2 +0xdf8

    AreaGrid *m_cbGrid;         // FS2 +0xdfc
    AreaGrid *m_sbGrid;         // FS2 +0xe00
    sceVu0FVECTOR m_e10;        // FS2 +0xe10: (160, 160, 160, 0) / 255 from start; unread
    float  m_shadeScale;        // FS2 +0xe20: 0.3 from start
    Tim2Texture *m_spotlightTexture;  // FS2 +0xe2c: the common spotlight
    Tim2Texture *m_spotTextures[kSceneSpotTextures]; // FS2 +0xe30: <light>.tm2, owned
    int    m_spotlightBlock;    // FS2 +0xe50: of the texture area
    int    m_spotlightBlocks;   // FS2 +0xe54
    Tim2Texture *m_shadowTexture; // FS2 +0xe58
    int    m_shadowBlock;       // FS2 +0xe5c
    int    m_shadowBlocks;      // FS2 +0xe60
};

// The groups whose objects have lit models.
bool sceneGroupIsLit(int group);                                         // FS2 0x001f9490
// 2 -> 0x12, 1 -> 0x13, else -1.
int sceneRenderGroup(int mode);                                          // FS2 0x001f9c70

#endif /* _GAME_SCENERENDER_H */
