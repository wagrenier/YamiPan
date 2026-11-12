// game/sceneeffects.cpp -- the effects scripts start, by handle (0x23a870..0x23adb0).
//
// SceneEffectList is the stage scene's +0xdc. The file name and the class name are the port's; no
// string names them. The effects themselves are StageNodeTree's.

#include <string.h>

#include "game/gamehooks.h"
#include "game/stageparticle.h"
#include "game/stagescene.h"

static const char kRainName[] = "rain";     // data 0x307340

// FS2 0x0023ad60

SceneEffectList::SceneEffectList()
{
}

// FS2 0x0023ad10: deleting

SceneEffectList::~SceneEffectList()
{
}

// Entries whose effect the tree has let go are free again.
// FS2 0x0023aca0
// callcheck: isLive

void SceneEffectList::update()
{
    for (Entry &entry : m_entries) {
        if (entry.effect != nullptr && !gStageScene->nodeTree->isLive(entry.effect))
            entry.effect = nullptr;
    }
}

// The ROM copies a name that is not word-aligned first, for the tree's
// sake; the host's has no such need.
// FS2 0x0023aad0
// callcheck: spawn, strcmp

int SceneEffectList::spawn(const char *name, StageNodeBase *parent, const float *pos, const float *rot)
{
    Entry *free = nullptr;
    for (Entry &entry : m_entries) {
        if (entry.effect == nullptr) {
            free = &entry;
            break;
        }
    }
    if (free == nullptr)
        return -1;

    // The next handle no live entry holds.
    bool taken;
    do {
        m_lastHandle++;
        if (m_lastHandle < 0)
            m_lastHandle = 0;
        taken = false;
        for (const Entry &entry : m_entries) {
            if (entry.effect != nullptr && entry.handle == m_lastHandle)
                taken = true;
        }
    } while (taken);

    free->effect = gStageScene->nodeTree->spawn(name, parent, pos, rot, gStageScene->isSubScene != 0, 0);
    free->handle = m_lastHandle;
    free->isRain = name[0] == 's' && strcmp(name + 3, kRainName) == 0;
    return free->effect == nullptr ? -1 : free->handle;
}

// FS2 0x0023a910
// callcheck: isLive

StageParticle *SceneEffectList::find(int handle)
{
    for (Entry &entry : m_entries) {
        if (entry.effect == nullptr || entry.handle != handle)
            continue;
        if (gStageScene->nodeTree->isLive(entry.effect))
            return entry.effect;
        entry.effect = nullptr;
        return nullptr;
    }
    return nullptr;
}

// FS2 0x0023a9f0
// callcheck: isLive, setStopFrames

bool SceneEffectList::stop(int handle)
{
    StageParticle *effect = find(handle);
    if (effect != nullptr)
        effect->setStopFrames(1);
    return effect != nullptr;
}

// FS2 0x0023a870
// callcheck: isLive, setEmitting

void SceneEffectList::setRainEmitting(bool on)
{
    for (Entry &entry : m_entries) {
        if (entry.effect != nullptr && entry.isRain != 0 && gStageScene->nodeTree->isLive(entry.effect))
            entry.effect->setEmitting(on);
    }
}
