/*
 *  game/stagefog.h -- the fog that goes with a character's head.
 *
 *  Two of a stage's effects hang from bn_head and change the light set while
 *  the view is theirs. A stage that has a smoke candle (I_HATUEN) makes
 *  hatsu_fog for up to eight characters: one in reach of a burning flare is
 *  in smoke, and the camera's character sees it. A stage with a character of
 *  a glowing kind makes koh_fog for the player, which runs while such a
 *  character is near. The file name is the port's: no source string
 *  (architecture.md "The fog around a character").
 */
#ifndef GAME_STAGEFOG_H
#define GAME_STAGEFOG_H

#include "sdk/scetypes.h"

class StageCharacter;
class StageObject;
class StageObjectList;
class StageParticle;
class StageThrownItem;
struct SceneRender;
struct StageNodeTree;

constexpr int kSmokeFogs    = 8;
constexpr int kSmokeSources = 5;

// One effect on one character. layout: host (EE 0xc).
struct CharacterFog
{
    // The effect hangs from the head of who, who is the owner whatever the
    // head says. Also the player's fog, each frame.
    void attachTo(StageObject *who);                                     // FS2 0x002cebd0

    StageParticle *m_effect = nullptr;  // FS2 +0x0
    u_char m_shown = 1;                 // FS2 +0x4: the view's, at the end of the last frame
    StageObject *m_owner = nullptr;     // FS2 +0x8
};

// StageScene +0x98, the player's koh_fog. layout: host (EE 0x14).
struct GlowFog : CharacterFog
{
    // A new fog when owner is of a glowing kind, else null.
    static GlowFog *createFor(StageObject *owner, StageNodeTree *tree);  // FS2 0x002ce9d0
    // A character's end of frame: a glowing one, up and about, burns while
    // it has endurance and counts for the player's nearest.
    void updateFor(StageCharacter *who, StageCharacter *player);         // FS2 0x002ce820
    // A change of on: the effect shown or hidden, render->m_snap set.
    void setActive(bool on, SceneRender *render);                        // FS2 0x002ceb70
    // It runs while someone was measured this frame within reach, and the
    // light set is then the glow's.
    void updateVisible(SceneRender *render);                             // FS2 0x002ce790

    u_char m_unmeasured = 1;    // FS2 +0xc: set each frame by StageScene::update
    float  m_nearest = 0.0f;    // FS2 +0x10: read only with m_unmeasured clear
};

// StageScene +0x9c. layout: host (EE 0x38).
struct SmokeFog
{
    ~SmokeFog();                                                         // FS2 0x002ce5a0: deleting
    // A new set when owner is I_HATUEN, else null.
    static SmokeFog *createFor(StageObject *owner, StageNodeTree *tree); // FS2 0x002ce620
    // A flare that burns in a smoke area joins this frame's list.
    void addObject(StageThrownItem *item);                               // FS2 0x002ce360
    // Once: the characters take the eight fogs, in the list's order.
    void attachCharacters(StageObjectList *objects);                     // FS2 0x002ce3e0
    // Each fog whose owner became or stopped being the view's: shown or
    // hidden, render->m_snap set.
    void setActiveFor(StageObject *linked, SceneRender *render);         // FS2 0x002ce0c0
    // A fog runs while its owner is within range of a flare, and the view's
    // shows; the light set is then the smoke's.
    void updateRange(float range, StageObject *linked, SceneRender *render); // FS2 0x002ce1c0
    // Each fog's m_shown = its owner is the view's.
    void storeActive(StageObject *linked);                               // FS2 0x002ce180

    StageThrownItem *m_sources[kSmokeSources]; // FS2 +0x0
    int m_sourceCount = 0;                     // FS2 +0x14: zeroed each frame by StageScene::update
    CharacterFog *m_fogs[kSmokeFogs];          // FS2 +0x18
};

#endif /* GAME_STAGEFOG_H */
