/* ==========================================================================
 *  game/stagelight.h
 *
 *  The stage's lights: StageLight and its two classes, SpotLight (the map's
 *  ENV_CL / ENV_SL / YAM_CL / YAM_SL) and PointLight (ENV_PL / YAM_PL, the
 *  muzzle flash "flash"). A light is an object of group 5, an entry of the
 *  action lists (list 2; sideHook2 is its frame), a body of the collision
 *  world on a stage (a bullet breaks it) and, by name, the owner of an effect
 *  and of model parts. SceneRender reads the rest: the light and shadow it
 *  throws on a point (lightAt, shadowAt). VcmTable takes getWeight as the
 *  fade of a model's vertex-colour layers: its "controllers" are lights.
 * ======================================================================== */
#ifndef _GAME_STAGELIGHT_H
#define _GAME_STAGELIGHT_H

#include "game/gamehooks.h"

class AreaBox;

// StageLight::m_mode: ItemLamp's lights are Hard, nothing sets Keyed so far.
// The names say what the checks do.
enum StageLightMode
{
    StageLight_Soft  = 0,
    StageLight_Hard  = 1,   // reaches kHardLightRangeSq at most; no falloff in shadowAt
    StageLight_Keyed = 4,   // gets a colour track though it is no ENV_ light
};

// The ScenePairTable list of the lights.
constexpr int kScenePairLights = 2;
// fade frames until a switched light is full.
constexpr int kLightFadeFrames = 10;
// StageLight_Hard's reach: 800 units, squared.
constexpr float kHardLightRangeSq = 640000.0f;

// The three light classes' base: vtables 0x314490 / 0x3144b4 / 0x3144cc /
// 0x3146dc, abstract (the slots lightAt .. are empty there). Its ROM layout
// is the object's to +0x200, then the CollisionOwner.
//
// CodeWarrior repeats the CollisionOwner's five slots and then the class's
// own after the table at +0x204: +0x26c lightAt, +0x270 inReach, +0x274
// shadowAt, +0x278 sideHook2, the five again (+0x27c..), then the cmd* below
// in order, then the side base's hooks 0, 1, 3. Most cmd* are also the
// script's commands: the runner's slots are thunks to them (slot numbers in
// the comments).
// layout: host -- EE offsets in the comments; EE 0x2c0.
class StageLight : public StageObject, public CollisionOwner
{
    friend class ItemLamp;          // game/stageitem.cpp
    friend class StageItemE;
    friend struct SceneRender;      // game/scenerender.cpp
    friend class StageWeaponItem;

public:
    StageLight();                                                        // FS2 0x001866f0
    ~StageLight() override;                                              // FS2 0x00186380

    // FS2 0x00185fa0
    int getKind() override { return StageObject_Light; }

    // What a point gets from the light: colour weight, 0 none.
    virtual float lightAt(const sceVu0FVECTOR point) = 0;
    // The point is within reach.
    virtual bool  inReach(const sceVu0FVECTOR point) = 0;
    // How strongly the point's owner throws a shadow from it, 0 none.
    virtual float shadowAt(const sceVu0FVECTOR point) = 0;

    // The frame: the colour track, the fade, the effect and parts, a
    // temporary light's time.
    void sideHook2() override;                                           // FS2 0x00185830

    // CollisionOwner.
    // FS2 0x001857c0
    int  collisionHook0() override { return 0; }
    int  onCollide(CollisionContact *contact, const float *sweep, StageCollisionFace *face,
                   float t, float distance) override;                    // FS2 0x00185710
    void syncBody(CollisionBody *body) override;                         // FS2 0x001857e0
    // FS2 0x001857d0
    void collisionHook3(CollisionBody *) override {}
    // FS2 0x00184bc0
    StageObject *getCollisionObject() override { return this; }

    // Runner slot 22: switched on or off (the fade starts).
    // FS2 0x00184a30
    void setScriptFlag(bool on) override { m_switchedOn = on; }
    // Runner slot 24: broken, no light at all.
    // FS2 0x00184a10
    void setScriptFlag2(bool on) override { m_broken = on; }

    // The light shows: not broken, some brightness, on or still fading.
    bool  isLit() const;                                                 // FS2 0x00185fd0
    // isLit times the fade (or the track's weight), times the brightness.
    float getWeight() const;                                             // FS2 0x00185af0

    void  setRadiusSquared(float radiusSquared);                         // FS2 0x00185bb0, 0x00184aa0
    // The colour; the map's ENV_ lights all pass (1, 0, 0), a colour track
    // replaces it then.
    void  setVector(float x, float y, float z);                          // FS2 0x00185c40
    // +0x200 and +0x36. Off: the world's body stays, what it was for is unread.
    void  setEnabled(bool on);                                           // FS2 0x00185fb0, 0x00184980
    // A temporary light: seconds until it goes, and the dtor then takes it
    // out of the scene (the lights of the map stay until the end).
    void  setLifetime(float seconds);                                    // FS2 0x00185a50
    // withEffect: the map's and the flash's calls all pass 1.
    void  addToScene(const char *name, bool withEffect);                 // FS2 0x001861b0: StageObjectList::add5
    // A sub-scene's light shows what the parent's light of its name shows.
    void  copyShownFrom(StageLight *other);                              // FS2 0x00186160
    // The two model switches, when model has parts named after the light:
    // bn_<name>_0.. (lit, dark) and bn_<name>_f_0.. (its other pieces).
    void  bindParts(ModelInstance *model);                               // FS2 0x00186030
    // No boxes, or point in one of them.
    bool  inBoxes(const sceVu0FVECTOR point);                            // FS2 0x00185a60
    // Where the body is (+0x90 of it); a stage's lights only.
    const float *getBodyPosition();                                      // FS2 0x00185ae0

    // The commands (the class's slots after the repeated five).
    // The colour as a vector. Runner slot 16.
    // FS2 0x00184bb0
    void cmdSetColor(const float *color) override { memcpy(m_color, color, sizeof(m_color)); }
    // Runner slot 19: 0x184aa0, the copy of setRadiusSquared.
    void cmdSetRadiusSquared(float radiusSquared) override { setRadiusSquared(radiusSquared); }
    // The tree's setScriptFlag. Runner slot 23.
    // FS2 0x00184a20
    // callcheck: setScriptFlag
    void cmdSetTreeSwitchedOn(bool on) override { m_tree->setScriptFlag(on); }
    // Runner slot 86.
    // FS2 0x00184920
    // callcheck: isLit (inline in the ROM)
    virtual bool cmdIsLit() { return isLit(); }
    // The tree's setScriptFlag2. Runner slot 25.
    // FS2 0x00184a00
    // callcheck: setScriptFlag2
    void cmdSetTreeBroken(bool on) override { m_tree->setScriptFlag2(on); }
    // Runner slot 26.
    // FS2 0x001849f0
    void cmdSetFadeFrames(int frames) override { m_fadeFrames = frames; }
    // Runner slot 27.
    // FS2 0x001849e0
    void cmdSetFlag240(bool on) override { m_modelFade = on; }
    // Runner slot 28.
    // FS2 0x001849d0
    void cmdSetValue244(int value) override { m_modelFadeFrames = value; }
    // Runner slot 29.
    // FS2 0x001849c0
    void cmdSetFlag248(bool on) override { m_lightsModels = on; }
    // Runner slot 30.
    // FS2 0x001849b0
    void cmdSetColor250(const float *color) override { memcpy(m_modelColor, color, sizeof(m_modelColor)); }
    // Runner slot 31.
    // FS2 0x001849a0
    void cmdSetColor260(const float *color) override { memcpy(m_modelAmbient, color, sizeof(m_modelAmbient)); }
    // Runner slot 32.
    // FS2 0x00184990
    void cmdSetCastsShadow(bool on) override { m_castsShadow = on; }
    // Runner slot 33: 0x184980, the copy of setEnabled.
    void cmdSetEnabled(bool on) override { setEnabled(on); }
    // A key of the colour track, frames in from the start. Runner slot 17.
    void cmdAddColorKey(int frames, const float *color) override;         // FS2 0x00184b40
    // Runner slot 18.
    // FS2 0x00184b30
    void cmdSetColorLoop(u_char loop) override { m_track->loop = loop; }

    u_char m_envLight;          // FS2 +0x271: ENV_ lights
    StageList<AreaBox *> m_boxes; // FS2 +0x278: the boxes named after the light; it only works inside them
    u_char m_yamLight;          // FS2 +0x2a1: YAM_ lights; off at the start

protected:
    // point - position in out, the squared distance answered (the classes'
    // shared first lines; inline in the ROM).
    float offsetTo(sceVu0FVECTOR out, const sceVu0FVECTOR point);
    // The box around the radius (inline in the ROM).
    void  updateBounds();
    // The effect the name asks for: TORCH / SigLamp on a spot, the effect
    // of that name, the lpt file's record.
    void  attachEffect();                                                // FS2 0x00185c60

    StageParticle *m_effect;    // FS2 +0x208
    PartSwitch  *m_parts;       // FS2 +0x20c: bn_<name>_0, _1: part 1 while on, part 0 while off, none broken
    PartSwitch  *m_offParts;    // FS2 +0x210: bn_<name>_f_0: shows while off
    u_char m_switchedOn;        // FS2 +0x214: 1 from the ctor
    sceVu0FVECTOR m_color;      // FS2 +0x220: w 1
    float  m_radiusSq;          // FS2 +0x230
    float  m_radius;            // FS2 +0x234
    int    m_fadeFrames;        // FS2 +0x238: the fade's length
    int    m_fade;              // FS2 +0x23c: 0..m_fadeFrames; -1 at the start: the first frame jumps to the end
    u_char m_modelFade;         // FS2 +0x240: a model fades when it takes or loses this light
    int    m_modelFadeFrames;   // FS2 +0x244: over so many frames; 30 from the ctor
    u_char m_lightsModels;      // FS2 +0x248: models may pick it; 1 from the ctor
    sceVu0FVECTOR m_modelColor; // FS2 +0x250: light 0 of a model; (42, 46, 54) / 255
    sceVu0FVECTOR m_modelAmbient; // FS2 +0x260: added to its ambient; (40, 44, 46) / 255
    u_char m_usable;            // FS2 +0x270: SceneRender::updateLights skips the light when clear
    u_char m_broken;            // FS2 +0x272
    u_char _273;                // FS2 +0x273: 0 from the ctor
    CollisionBody *m_body;      // FS2 +0x274: a stage's; bucket 2, a sphere of 20
    sceVu0FVECTOR m_bounds[2];  // FS2 +0x280: low and high corner (ENV_ / YAM_ lights)
    u_char m_castsShadow;       // FS2 +0x2a0: 1 from the ctor
    // +0x2a2 StageObject::m_keepFlags: set, the light is temporary.
    float  m_lifetime;          // FS2 +0x2a4: seconds left of a temporary light
    float  m_brightness;        // FS2 +0x2a8: 1 from the ctor
    VectorTrack *m_track;       // FS2 +0x2ac: the colour's keys (ENV_ and Keyed lights)
    int    m_priority;          // FS2 +0x2b0: the scene's own point and spot take the highest; -1 from the ctor: never
    int    m_mode;              // FS2 +0x2b4: StageLightMode
    StageObject *m_shadowOwner; // FS2 +0x2b8: null any; SceneRender only takes the shadow while this is the player
};

// ENV_CL, ENV_SL, YAM_CL, YAM_SL: a cone along row 1 of the matrix. Vtables
// 0x313e30 / 0x313e54 / 0x313e6c / 0x31407c.
// layout: host -- EE 0x300.
class SpotLight : public StageLight
{
    friend class ItemLamp;          // game/stageitem.cpp
    friend struct SceneRender;      // game/scenerender.cpp

public:
    SpotLight();                                                         // FS2 0x00185260
    ~SpotLight() override;                                               // FS2 0x00184860: deleting

    // FS2 0x001848f0
    int getKind() override { return StageObject_SpotLight; }

    float lightAt(const sceVu0FVECTOR point) override;                   // FS2 0x00185010
    bool  inReach(const sceVu0FVECTOR point) override;                   // FS2 0x00184f00
    float shadowAt(const sceVu0FVECTOR point) override;                  // FS2 0x00184d50

    // FS2 0x00184910
    void sideHook0() override {}
    // FS2 0x00184900
    void sideHook1() override {}
    // With m_limitByMap the reach ends at the map along the axis.
    void sideHook3() override;                                           // FS2 0x00184bd0

    void setConeAngle(int degrees);                                      // FS2 0x00185210, 0x00184a50: half the cone
    // Runner slot 20: 0x184a50, the copy of setConeAngle.
    void cmdSetConeAngle(int degrees) override { setConeAngle(degrees); }
    // Runner slot 21.
    // FS2 0x00184a40
    void cmdSetValue2d8(int value) override { m_intensityPercent = value; }

private:
    int    m_coneDegrees;       // FS2 +0x2c0
    float  m_coneCos;           // FS2 +0x2c4: the cone's edge
    u_char m_limitByMap;        // FS2 +0x2c8: ItemLamp's first spot
    float  m_reach;             // FS2 +0x2cc: the radius, or to the map
    float  m_reachSq;           // FS2 +0x2d0: PC: 0 from the ctor (the ROM leaves it)
    Tim2Texture *m_texture;     // FS2 +0x2d4: <name>.tm2 (SceneRender::setup); null: the common spotlight
    int    m_intensityPercent;  // FS2 +0x2d8: 100 from the ctor
};

// ENV_PL, YAM_PL, the flash. Vtables 0x3141b0 / 0x3141d4 / 0x3141ec /
// 0x3143fc.
// layout: host -- EE 0x2c0.
class PointLight : public StageLight
{
public:
    PointLight();                                                        // FS2 0x001856b0
    ~PointLight() override;                                              // FS2 0x00186a40: deleting

    // FS2 0x00186ad0
    int getKind() override { return StageObject_PointLight; }

    float lightAt(const sceVu0FVECTOR point) override;                   // FS2 0x00185510
    bool  inReach(const sceVu0FVECTOR point) override;                   // FS2 0x00185430
    float shadowAt(const sceVu0FVECTOR point) override;                  // FS2 0x001852e0

    // FS2 0x00186b00
    void sideHook0() override {}
    // FS2 0x00186af0
    void sideHook1() override {}
    // FS2 0x00186ae0
    void sideHook3() override {}
};

#endif /* _GAME_STAGELIGHT_H */
