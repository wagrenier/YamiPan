/* ========================================================================
 *  game/gadgetview.h -- the stage's gadget views (0x2a7b10..0x2a9c68).
 *
 *  A gadget is something the player walks up to and looks at through its
 *  own camera: the signal lamp (SigLamp, the `effect` target's texture
 *  "siglamp"), the number lock (oMK21_06 / oSJ08_07) and the seven-key pad
 *  (0x2b2c80, run unassigned@002b22f0). StageScene keeps one of each in
 *  `owned[3]` (+0x8c..+0x94).
 *
 *  Each is a handle and a body. The handle (`SceneOwnedObject`, 8 bytes:
 *  vptr, body) is what the scene, the scripts and GadgetLink hold; every one
 *  of its 15 slots after the dtor is a two-instruction jump to the body's
 *  slot at the same offset (0x2a9a00..0x2a9bc0, shared by all three
 *  handles' vtables). The body (`GadgetView`) keeps the pairs a script named
 *  (an object of group 1 and a camera of group 0), the state of a look and
 *  three hooks of its own (frame, enter, leave).
 *
 *  A look: the special's text picks a pair (`select`), the motion's start
 *  gives the user (`begin`), then `update` fades to black, swaps to the
 *  pair's camera (`enterView`: StageScene::pushView at priority 4, the
 *  object's children hidden), runs `frame` each frame until `cancel`, fades
 *  to black again, swaps back (`leaveView`) and fades in.
 *
 *  The file is the port's name: no source string says what the ROM called
 *  it. The class names but SigLamp (the stage script's) are the port's.
 * ======================================================================== */
#ifndef GAME_GADGETVIEW_H
#define GAME_GADGETVIEW_H

#include "game/gamehooks.h"

class StageCamera;
class StageParticle;
class SpotLight;
class PointLight;
class Tim2Texture;

// StageScene::owned's index of each gadget.
enum GadgetSlot
{
    GadgetSlot_SigLamp  = 0,    // +0x8c: StageFiles' effect target
    GadgetSlot_NumLock  = 1,    // +0x90: createMapObject, with the first lock
    GadgetSlot_SevenKey = 2,    // +0x94: createMapObject, with the first pad
};

// The root both halves derive from: one slot, the deleting dtor.
// layout: host -- EE: vptr only (vtable 0x318b90).
class GadgetRoot
{
public:
    virtual ~GadgetRoot() {}                                             // FS2 0x002a7c90: deleting
};

// GadgetView::m_state: the steps of a look.
enum GadgetState
{
    GadgetState_Idle     = 0,
    GadgetState_Selected = 1,   // a pair is picked; waits for its user (begin)
    GadgetState_FadeOut  = 2,   // to black before the gadget's camera
    GadgetState_Looking  = 3,   // through the camera: frame() each frame
    GadgetState_FadeBack = 4,   // to black before the scene's camera, then in
};

// The body: the named pairs and the look.
// layout: host -- EE 0x28 (vtable 0x318c40), offsets in the comments.
class GadgetView : public GadgetRoot
{
public:
    // capacity pairs, all empty.
    explicit GadgetView(int capacity);                                   // FS2 0x002a9940
    ~GadgetView() override;                                              // FS2 0x002a98c0: slot 0, deleting

    // Slot 1: once, before the first frame (StageScene::setupCameras).
    virtual void prepare() = 0;                                          // FS2 vtable +0xc
    // Slot 2: a script names a pair: an object of group 1 and a camera of
    // group 0. Refused while a look runs, when full or the object is listed.
    virtual bool addPair(const char *objectName, const char *cameraName); // FS2 0x002a9700
    // Slot 3: the pair of that object goes, unless a look runs.
    virtual bool removePair(const char *objectName);                     // FS2 0x002a9610
    // Slot 4: a pair names that object.
    virtual bool hasObject(const char *objectName);                      // FS2 0x002a9570
    // Slot 5: idle, a pair whose object is active, no fade running.
    virtual bool canUse();                                               // FS2 0x002a94c0
    // Slot 6: the special's text picks the pair; the look is Selected.
    virtual bool takeMotionText(const char *objectName);                 // FS2 0x002a93d0
    // Slot 7: a look runs.
    // FS2 0x002a7b70
    virtual bool isActive() { return m_state != GadgetState_Idle; }
    // Slot 8: the character whose motion started; the look may go on.
    virtual bool takeMotionUser(StageObject *user);                      // FS2 0x002a93a0
    // Slot 9: asks the running look to end.
    virtual bool cancel();                                               // FS2 0x002a9380
    // Slot 10: the look's steps and fades.
    virtual void update();                                               // FS2 0x002a9050
    // Slot 11: over the frame while in view.
    virtual void draw() = 0;                                             // FS2 vtable +0x34
    // Slot 12: who looks.
    // FS2 0x002a7b80
    virtual StageObject *getUser() { return m_user; }
    // Slot 13: holds the player's actions: fading out, or in the view.
    // FS2 0x002a7b90
    virtual bool isBusy() { return m_state == GadgetState_FadeOut || isInView(); }
    // Slot 14: the gadget's camera is the view.
    // FS2 0x002a7bd0
    virtual bool isInView() { return m_inView != 0; }
    // Slot 15: looking (not fading).
    // FS2 0x002a7be0
    virtual bool isLooking() { return m_state == GadgetState_Looking; }

    // The pair's object by name, null none.
    StageObject *findObject(const char *objectName);                     // FS2 0x002a9810

protected:
    // Slot 16: each frame while looking.
    virtual void frame() = 0;                                            // FS2 vtable +0x48
    // Slot 17: the swap to the camera, at full black.
    virtual void enterView() = 0;                                        // FS2 vtable +0x4c
    // Slot 18: the swap back, at full black.
    virtual void leaveView() = 0;                                        // FS2 vtable +0x50

    // The camera becomes the view and the object's children hide.
    void pushCamera();                                                   // FS2 0x002a9300
    void popCamera();                                                    // FS2 0x002a9290
    // Back to idle, nothing picked.
    void reset();                                                        // inline in update

    // The index of the pair naming that object; the capacity none.
    int findPair(const char *objectName) const;                          // inline 4 times

    StageObject  **m_objects;   // FS2 +0x04: new[] capacity; null an empty pair
    StageCamera  **m_cameras;   // FS2 +0x08: new[] capacity
    int     m_count = 0;        // FS2 +0x0c: pairs set
    int     m_capacity;         // FS2 +0x10
    int     m_state = GadgetState_Idle; // FS2 +0x14: GadgetState
    u_char  m_inView = 0;       // FS2 +0x18: pushCamera .. popCamera
    u_char  m_userSet = 0;      // FS2 +0x19: begin came
    u_char  m_cancel = 0;       // FS2 +0x1a: cancel came
    StageObject *m_user = nullptr;      // FS2 +0x1c
    StageObject *m_object = nullptr;    // FS2 +0x20: the picked pair's
    StageCamera *m_camera = nullptr;    // FS2 +0x24: the picked pair's
};

// The signal lamp's body: a spot and a point light on the camera, turned
// by the left stick, lit while the Lamp button is held, the beam's end
// where the ray from the lamp hits; the "siglamp" texture over the frame.
// layout: host -- EE 0x60 in a block aligned 0x40 (vtable 0x318b30).
class SigLampView : public GadgetView
{
public:
    explicit SigLampView(Tim2Texture *texture);                          // inline in SigLamp's ctor
    ~SigLampView() override;                                             // FS2 0x002a7b10: deleting

    void prepare() override;                                             // FS2 0x002a8650
    void draw() override;                                                // FS2 0x002a7ce0

protected:
    void frame() override;                                               // FS2 0x002a8380
    void enterView() override;                                           // FS2 0x002a8180
    void leaveView() override;                                           // FS2 0x002a8050

private:
    Tim2Texture *m_texture;     // FS2 +0x28: "siglamp", a quarter of the frame's mask
    SpotLight   *m_spot = nullptr;  // FS2 +0x2c: "SigLamp_sl"
    PointLight  *m_point = nullptr; // FS2 +0x30: "SigLamp_pl", at the beam's end
    sceVu0FVECTOR m_cameraRotation = {}; // FS2 +0x40: the camera's, from enterView
    float   m_cameraScrZ = 450.0f;  // FS2 +0x50: the camera's, from enterView
    float   m_beamLength = 0.0f;    // FS2 +0x54: the point light's y, from enterView
};

// The number lock's body: the stick's left and right pick a dial, up and
// down turn it, Use tries the code, Squat leaves; "numlock" sparkles on
// bn_key while in view.
// layout: host -- EE 0x34 in a block aligned 0x40 (vtable 0x318be0).
class NumLockView : public GadgetView
{
public:
    NumLockView() : GadgetView(1) {}                                     // inline in the handle's ctor
    ~NumLockView() override;                                             // FS2 0x002a88b0: deleting

    void prepare() override;                                             // FS2 0x002a8f80
    // FS2 0x002a8a20
    void draw() override {}

    int     m_dial = 0;         // FS2 +0x28: 0..3, the one the stick turns
    StageParticle *m_sparkle = nullptr; // FS2 +0x2c: "numlock", on bn_key while in view
    u_char  m_tried = 0;        // FS2 +0x30: Use left the view (the script asks)
    u_char  m_recordFlag = 0;   // FS2 +0x31: SpNumLockLink's (native 0x220)

protected:
    void frame() override;                                               // FS2 0x002a8d00
    void enterView() override;                                           // FS2 0x002a8b60
    void leaveView() override;                                           // FS2 0x002a8a30
};

// The handle the scene holds. Its slots forward to the body's.
// layout: host -- EE 8: vptr (vtable 0x318ca0; the gadgets' 0x314c40,
// 0x314c90, 0x314ce0), the body at +4.
class SceneOwnedObject : public GadgetRoot
{
public:
    ~SceneOwnedObject() override;                                        // FS2 0x002a9be0: deleting
    // The deleting dtor as the scene calls it.
    void destroy() { delete this; }

    // FS2 0x002a9bc0
    virtual void prepare() { m_view->prepare(); }
    // FS2 0x002a9ba0
    virtual bool addPair(const char *objectName, const char *cameraName) { return m_view->addPair(objectName, cameraName); }
    // FS2 0x002a9b80
    virtual bool removePair(const char *objectName) { return m_view->removePair(objectName); }
    // FS2 0x002a9b60
    virtual bool hasObject(const char *objectName) { return m_view->hasObject(objectName); }
    // FS2 0x002a9b40
    virtual bool canUse() { return m_view->canUse(); }
    // FS2 0x002a9b20
    virtual bool takeMotionText(const char *objectName) { return m_view->takeMotionText(objectName); }
    // FS2 0x002a9b00
    virtual bool isActive() { return m_view->isActive(); }
    // FS2 0x002a9ae0
    virtual bool takeMotionUser(StageObject *user) { return m_view->takeMotionUser(user); }
    // FS2 0x002a9ac0
    virtual bool cancel() { return m_view->cancel(); }
    // FS2 0x002a9aa0
    virtual void update() { m_view->update(); }
    // FS2 0x002a9a80
    virtual void draw() { m_view->draw(); }
    // FS2 0x002a9a60
    virtual StageObject *getUser() { return m_view->getUser(); }
    // FS2 0x002a9a40
    virtual bool isBusy() { return m_view->isBusy(); }
    // FS2 0x002a9a20
    virtual bool isInView() { return m_view->isInView(); }
    // FS2 0x002a9a00
    virtual bool isLooking() { return m_view->isLooking(); }

    // The number lock's handle only (a0 is the scene's +0x90).
    bool getRecordFlag31();                                              // FS2 0x002a8920
    void setRecordFlag31(bool on);                                       // FS2 0x002a8910
    bool wasTried();                                                     // FS2 0x002a8930
    // The pair's object of that name is a number lock with the code set.
    bool isLockOpen(const char *objectName);                             // FS2 0x002a8940

    // The seven-key pad's (native 0x205): the object of that name, when it
    // is of kind 6, takes the two pairs (0x2b2420, 0x2b23e0, unread).
    void setSevenKeyNumbers(const char *name, int a, int b, int c, int d); // FS2 0x002b2b70
    // The seven-key pad's: the object of that name is of kind 6 and its
    // keys stand at the code (0x2b2620).
    bool isSevenKeyOpen(const char *objectName);                         // FS2 0x002b2c10

protected:
    explicit SceneOwnedObject(GadgetView *view) : m_view(view) {}

    GadgetView *m_view;         // FS2 +0x04
};

// +0x8c, built by the `effect` target from the texture "siglamp".
// layout: host -- EE 8 (vtable 0x314c40).
class SigLamp : public SceneOwnedObject
{
public:
    explicit SigLamp(Tim2Texture *texture);                              // FS2 0x002a7bf0
};

// +0x90, built by createMapObject with the first number lock.
// layout: host -- EE 8 (vtable 0x314c90).
class NumLockGadget : public SceneOwnedObject
{
public:
    NumLockGadget();                                                     // FS2 0x002a89b0
};

#endif // GAME_GADGETVIEW_H
