// game/vcmtable.cpp -- the VCM table of a model (game/vcmtable.h).

#include "game/vcmtable.h"

#include <string.h>

#include <algorithm>
#include <map>
#include <set>
#include <string>

#include "game/gamehooks.h"
#include "game/stagelight.h"

// The token names compared as C strings (the instances' strcmp less).
// layout: host -- no data.
struct VcmCStrLess
{
    bool operator()(const char *a, const char *b) const
    {
        return strcmp(a, b) < 0;
    }
};

// What vcmCollectTokens gathers: the distinct tokens as C strings it
// allocated (their owner is the VcmTable, not the set).
typedef std::set<const char *, VcmCStrLess> VcmNameSet;
// One tag's tokens.
typedef std::set<std::string> VcmTokenSet;

// The constructor's map value for a tag: its state's index (0 until the
// states are numbered: the sdata zero 0x31ac30) and its token mask.
// layout: host -- EE 8, after the key string (0xc) in the map's pair.
struct VcmStateRef
{
    int   index;                // FS2 +0x0 (the pair's +0xc)
    u_int mask;                 // FS2 +0x4 (the pair's +0x10)
};
typedef std::map<std::string, VcmStateRef> VcmStateMap;

// newlib's strncasecmp 0x1285d0 (ASCII: the ctype table's upper case
// lowered by 0x20); not in every host libc.
static int vcmStrncasecmp(const char *a, const char *b, size_t n)
{
    for (; n != 0; n--, a++, b++) {
        int ca = (*a >= 'A' && *a <= 'Z') ? *a + 0x20 : *a;
        int cb = (*b >= 'A' && *b <= 'Z') ? *b + 0x20 : *b;
        if (ca != cb || ca == 0) {
            return (u_char)ca - (u_char)cb;
        }
    }
    return 0;
}

// The name a part is tagged by, null for an untagged part (the callers
// test the geometry's material before asking for the tag). Inline in the
// ROM.
static const ModelMaterial *vcmPartMaterial(const ModelInstance *model, int part)
{
    const ModelMeshParts *geometry = model->node->meshes.get()[part].parts;
    if (geometry == NULL) {
        return NULL;
    }
    return geometry->material;
}

// ──────────────────────────────────────────────────────────────────────
// Every token after a "__" up to the next "__" or the end. Each token
// string takes one byte more than the run: the first '_' of the next
// separator, or the terminating 0 of the last token (kept as the ROM does
// it: one-token tags, the only kind on the disc, compare equal as C
// strings). No "__": no tokens.
// callcheck: VcmTokenSet, VcmTokenSet_copy, destroyTree, string_cstr_n, ~string (the set and token string instances: host std::set / std::string)
// FS2 0x00297aa0

static VcmTokenSet vcmSplitTokens(const char *name)
{
    VcmTokenSet tokens;
    const char *sep = strstr(name, VCM_TOKEN_SEP);
    if (sep != NULL) {
        const char *end = strchr(name, 0);
        while (*sep != 0) {
            const char *start = sep + 2;
            const char *next = strstr(start, VCM_TOKEN_SEP);
            if (next == NULL) {
                next = end;
            }
            tokens.insert(std::string(start, next - start + 1));
            sep = next;
        }
    }
    return tokens;
}

// ──────────────────────────────────────────────────────────────────────
// A tagged part's tag: its material's name when that holds tokens, else
// the first part named "VCM__..." (case ignored) from the part itself up
// its group chain, else "VCM__" itself (no tokens).
// callcheck: strncasecmp, destroyTree (vcmStrncasecmp; the token set's destructor)
// FS2 0x00297950

static const char *vcmPartTag(const ModelInstance *model, int part)
{
    const char *name = vcmPartMaterial(model, part)->name;
    if (!vcmSplitTokens(name).empty()) {
        return name;
    }
    for (const ModelPartRec *rec = &model->partRecs[part]; rec != NULL;
         rec = model->findGroup(rec)) {
        const char *partName = model->node->meshes.get()[rec - model->partRecs].name;
        if (vcmStrncasecmp(partName, VCM_TAG_PREFIX, strlen(VCM_TAG_PREFIX)) == 0) {
            return partName;
        }
    }
    return VCM_TAG_PREFIX;
}

// ──────────────────────────────────────────────────────────────────────
// Every distinct token of the tagged parts' tags, each copied once.
// callcheck: destroyTree, mslTreeIncrement, operator_new[] (the token set's walk and destructor; new char[])
// FS2 0x00297790

static void vcmCollectTokens(VcmNameSet &names, const ModelInstance *model)
{
    for (int part = 0; part < model->node->meshCount; part++) {
        if (vcmPartMaterial(model, part) == NULL) {
            continue;
        }
        VcmTokenSet tokens = vcmSplitTokens(vcmPartTag(model, part));
        for (VcmTokenSet::const_iterator it = tokens.begin(); it != tokens.end(); ++it) {
            if (names.find(it->c_str()) == names.end()) {
                char *copy = new char[it->size() + 1];
                strcpy(copy, it->c_str());
                names.insert(copy);
            }
        }
    }
}

// ──────────────────────────────────────────────────────────────────────
// The bit of each of the tag's tokens, by its place in the sorted names.
// The ROM takes the lower bound without testing for equality: every token
// is among the names (vcmCollectTokens put it there).
// callcheck: destroyTree, mslTreeIncrement, vcmLowerBound (the token set's walk and destructor; std::lower_bound)
// FS2 0x002976a0

static u_int vcmTokenMask(const char *tag, const char **names, int count)
{
    u_int mask = 0;
    const char **end = names + count;
    VcmTokenSet tokens = vcmSplitTokens(tag);
    for (VcmTokenSet::const_iterator it = tokens.begin(); it != tokens.end(); ++it) {
        const char **found = std::lower_bound(names, end, it->c_str(), VcmCStrLess());
        if (found != end) {
            mask |= 1u << ((found - names) & 31);
        }
    }
    return mask;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00296d60

void VcmTable::update()
{
    // The ROM keeps the alpha in a register across the states: a state
    // without tokens stores the previous state's (the first one garbage;
    // PC: 0).
    int alpha = 0;
    for (int s = 0; s < m_stateCount; s++) {
        VcmState &state = m_states[s];
        u_int fade = 0;
        u_int off = 0;
        u_int bit = 1;
        for (int token = 0; token < m_tokenCount; token++) {
            if ((state.mask & (1u << token)) == 0) {
                continue;
            }
            StageLight *controller = m_controllers[token];
            float weight = (controller == NULL || !controller->m_active)
                               ? 0.0f
                               : controller->getWeight();
            if (weight < 1.0f) {
                if (weight > 0.0f) {
                    fade |= bit;
                    alpha = (int)(weight * kVcmAlphaScale);
                } else {
                    fade |= bit;
                    off |= bit;
                    alpha = 0;
                }
            } else {
                alpha = 0;
            }
            bit <<= 1;
        }
        state.fadeBits = (u_char)fade;
        state.offBits = (u_char)off;
        state.alpha = (u_char)alpha;
    }

    // Unrolled eight times in the ROM.
    for (VcmPartLink *link = m_links; link < m_links + m_linkCount; link++) {
        const VcmState &state = m_states[link->state];
        link->rec->vcmFadeBits = state.fadeBits;
        link->rec->vcmOffBits = state.offBits;
        link->rec->vcmAlpha = state.alpha;
    }
}

// ──────────────────────────────────────────────────────────────────────
// callcheck: vcmLowerBound (std::lower_bound)
// FS2 0x00297150

void VcmTable::attach(StageLight *obj)
{
    const char **end = m_tokens + m_tokenCount;
    const char **found = std::lower_bound(m_tokens, end, (const char *)obj->m_name,
                                          VcmCStrLess());
    if (found != end && strcmp(obj->m_name, *found) == 0) {
        m_controllers[found - m_tokens] = obj;
    }
}

// ──────────────────────────────────────────────────────────────────────
// The ROM frees the token names with operator delete (they come from
// new[]); PC: delete[].
// callcheck: operator_delete, operator_delete[] (the delete-expressions)
// FS2 0x00297210

VcmTable::~VcmTable()
{
    for (int i = 0; i < m_tokenCount; i++) {
        delete[] m_tokens[i];
    }
    delete[] m_tokens;
    delete[] m_controllers;
    delete[] m_states;
    delete[] m_links;
}

// ──────────────────────────────────────────────────────────────────────
// The tokens first (their order gives the mask bits), then one state per
// distinct tag in the tags' order, then the parts' links.
// callcheck: VcmNameSet, VcmStateMap, destroyTree, mslTreeIncrement, operator_index, operator_new[], string_cstr, ~string (the set / map / string instances: host STL; the new-expressions)
// FS2 0x002972d0

VcmTable::VcmTable(ModelInstance *model)
{
    VcmNameSet names;
    vcmCollectTokens(names, model);
    m_tokenCount = (int)names.size();
    m_tokens = new const char *[m_tokenCount];
    m_controllers = new StageLight *[m_tokenCount];
    VcmNameSet::const_iterator name = names.begin();
    for (int i = 0; i < m_tokenCount; i++, ++name) {
        m_tokens[i] = *name;
        m_controllers[i] = NULL;
    }

    VcmStateMap tags;
    for (int part = 0; part < model->node->meshCount; part++) {
        if (vcmPartMaterial(model, part) == NULL) {
            continue;
        }
        const char *tag = vcmPartTag(model, part);
        if (tags.find(tag) != tags.end()) {
            continue;
        }
        u_int mask = vcmTokenMask(tag, m_tokens, m_tokenCount);
        VcmStateRef &ref = tags[tag];
        ref.index = 0;
        ref.mask = mask;
    }

    // PC: the states are zeroed (the ROM leaves the bytes after the mask
    // to the first update).
    m_stateCount = (int)tags.size();
    m_states = new VcmState[m_stateCount]();
    int index = 0;
    for (VcmStateMap::iterator it = tags.begin(); it != tags.end(); ++it, index++) {
        m_states[index].mask = it->second.mask;
        it->second.index = index;
    }

    m_linkCount = 0;
    for (int part = 0; part < model->node->meshCount; part++) {
        if (vcmPartMaterial(model, part) != NULL) {
            m_linkCount++;
        }
    }
    m_links = new VcmPartLink[m_linkCount];
    VcmPartLink *link = m_links;
    for (int part = 0; part < model->node->meshCount; part++) {
        if (vcmPartMaterial(model, part) == NULL) {
            continue;
        }
        const char *tag = vcmPartTag(model, part);
        link->rec = &model->partRecs[part];
        link->state = tags[tag].index;
        link++;
    }
}

// std::lower_bound over the sorted names (VcmTable::attach, vcmTokenMask),
// then the file's MSL string / set / map instances, out of line in the ROM
// after the code above; the host <algorithm> / <string> / <set> / <map>
// provide them. The string ones are the binary's only copies (every
// std::string in the game calls them).
//   FS2 0x00297bd0  vcmLowerBound (std::lower_bound, strcmp)
//   FS2 0x00297c90  VcmNameSet::find
//   FS2 0x00297d50  VcmStateMap::operator[]
//   FS2 0x00297e80  VcmStateMap::destroyPair (pair dtor)
//   FS2 0x00297f00  VcmStateMap::find
//   FS2 0x00297fb0  std::string::~string
//   FS2 0x00298030  std::string::string (const char *)
//   FS2 0x002980b0  std::string::string (const char *, n)
//   FS2 0x00298120  VcmTokenSet::destroyTree
//   FS2 0x00298390  VcmTokenSet::deallocate
//   FS2 0x002983a0  VcmTokenSet::nodeAddress
//   FS2 0x002983b0  VcmTokenSet::destroyValue
//   FS2 0x002983f0  VcmTokenSet::identity
//   FS2 0x00298400  VcmTokenSet::insert
//   FS2 0x00298510  VcmTokenSet::VcmTokenSet (copy)
//   FS2 0x002985c0  VcmTokenSet::VcmTokenSet
//   FS2 0x002985e0  VcmNameSet::destroyTree
//   FS2 0x00298800  VcmNameSet::deallocate
//   FS2 0x00298810  VcmNameSet::nodeAddress
//   FS2 0x00298820  VcmNameSet::destroyValue
//   FS2 0x00298830  VcmNameSet::identity
//   FS2 0x00298840  VcmNameSet::insert
//   FS2 0x00298950  VcmNameSet::VcmNameSet
//   FS2 0x00298970  VcmStateMap::destroyTree
//   FS2 0x00298be0  VcmStateMap::deallocate
//   FS2 0x00298bf0  VcmStateMap::nodeAddress
//   FS2 0x00298c00  VcmStateMap::destroyValue
//   FS2 0x00298c40  VcmStateMap::identity
//   FS2 0x00298c50  VcmStateMap::VcmStateMap
//   FS2 0x00298c70  std::string::replace (pos, len, first, last)
//   FS2 0x00299060  std::string::compare (const string &)
//   FS2 0x002990c0  std::string::reserve
//   FS2 0x00299320  std::string::string (copy)
//   FS2 0x002993c0  VcmTokenSet::insertNode
//   FS2 0x00299550  VcmTokenSet::copyTree
//   FS2 0x00299800  VcmTokenSet::setNodeColor
//   FS2 0x00299830  VcmTokenSet::nodeColor
//   FS2 0x00299840  VcmTokenSet::constructValue
//   FS2 0x002998d0  VcmTokenSet::setParent
//   FS2 0x002998f0  VcmTokenSet::allocateNodes
//   FS2 0x00299950  VcmNameSet::insertNode
//   FS2 0x00299a80  VcmStateMap::insertNode
//   FS2 0x00299c20  std::string::compare (pos, n, s, n2)
//   FS2 0x00299d40  std::string::assign (first, last)
