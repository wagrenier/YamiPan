// rena/rena_2d_texture_vram.cpp -- rena's texture VRAM regions (GsRegionAlloc);
// the file name is a guess. rena_2d_texture_vram.h has the classes.
//
// PC: the region bookkeeping runs as in the ROM; the packets bind and upload
// write are the ROM's, and nothing the host draws reads them (the host
// module binds a texture by its owner record).

#include "rena/rena_2d_texture_vram.h"

#include "rena/rena_2d_texture_tim2.h"
#include "sdk/libgraph.h"

// bind's VU1 packet: DMA CNT of 2 qwords; VIF FLUSH, UNPACK V4-32 of 2 qwords
// to VU1 0x26 (the micro program's TEX0 pair).
constexpr u_long kVramPktVifFlushUnpackTex0 = 0x6c02002611000000ull;
// The second CLUT (Tim2Texture::m_altClut) sits 4 blocks after the first.
constexpr int kAltClutBlocks = 4;

// ──────────────────────────────────────────────────────────────────────
// FS2 0x002e0840

void GsRegion::release()
{
    m_state = GsRegion_Released;
    m_owner = nullptr;
}

// ──────────────────────────────────────────────────────────────────────
// The ROM tests texture->m_region without testing texture.
// FS2 0x002e0850

void GsRegionAlloc::bind(u_long **cursor, Tim2Texture *texture, u_int flags)
{
    if (m_bound == texture)
        return;
    GsRegion *region = texture->m_region;
    if (region == nullptr)
        return;

    if (flags & GsBind_Tex0_1)
        texture->putTex0(cursor, region->m_block, SCE_GS_TEX0_1);
    if (flags & GsBind_Tex0_2)
        texture->putTex0(cursor, region->m_block, SCE_GS_TEX0_2);
    if (flags & GsBind_Vu1) {
        sceGsTex0 tex0;
        texture->getTex0(&tex0, region->m_block);
        sceGsTex0 altTex0 = tex0;
        altTex0.CBP = tex0.CBP + kAltClutBlocks;        // 14 bits, wraps as the ROM's mask

        u_long *p = *cursor;
        p[0] = kTexPktDmaCnt2;
        p[1] = kVramPktVifFlushUnpackTex0;
        p[2] = *(u_long *)&tex0;
        p[3] = 0;
        p[4] = *(u_long *)&altTex0;
        p[5] = 0;
        *cursor = p + 6;
    }
    m_bound = texture;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x002e09b0

void GsRegionAlloc::upload(u_long **cursor, Tim2Texture *texture)
{
    if (texture == nullptr)
        return;
    GsRegion *region = texture->m_region;
    if (region != nullptr && region->m_state == GsRegion_ToSend) {
        texture->upload(cursor, region->m_block);
        region->m_state = GsRegion_Sent;
    }
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x002e0a10

void GsRegionAlloc::assign(Tim2Texture *texture, short pinned)
{
    if (texture == nullptr || texture->m_region != nullptr)
        return;
    GsRegion *region = alloc(texture->getVramBlocks());
    region->m_owner = texture;
    texture->m_region = region;
    region->m_pinned = pinned;
    region->m_state = GsRegion_ToSend;
}

// ──────────────────────────────────────────────────────────────────────
// The smallest free region of `blocks` or more. A region whose lower
// neighbour was released frees that one first (merging the two) and the walk
// starts again. The ROM makes the released test twice on the lower neighbour;
// the higher one is never tested here.
// FS2 0x002e0a90

GsRegion *GsRegionAlloc::findFree(u_int blocks)
{
    GsRegion *best = nullptr;
    u_int bestSize = m_size + 1;
    GsPoolList<GsRegion *, kRegions>::Node *node = m_free.head();
    while (node != nullptr) {
        GsRegion *region = node->value;
        GsRegion *lower = region->m_lower;
        if (lower != nullptr && lower->m_state == GsRegion_Released) {
            freeRegion(lower);
            node = m_free.head();
            continue;
        }
        if (region->m_size >= blocks && region->m_size < bestSize) {
            bestSize = region->m_size;
            best = region;
        }
        node = node->next;
    }
    return best;
}

// ──────────────────────────────────────────────────────────────────────
// A region in use goes back to the free list and swallows its free
// neighbours, lower first; a released neighbour is freed in its place (which
// merges it with this one) and that call finishes the work.
// FS2 0x002e0b80

void GsRegionAlloc::freeRegion(GsRegion *region)
{
    if (m_bound == region->m_owner)
        m_bound = nullptr;
    m_used.remove(region);
    region->m_state = GsRegion_Free;
    if (region->m_owner != nullptr) {
        region->m_owner->m_region = nullptr;
        region->m_owner = nullptr;
    }
    m_free.push_back(region);

    GsRegion *higher = region->m_higher;
    if (higher != nullptr && higher->m_state == GsRegion_Released) {
        freeRegion(higher);
        return;
    }

    for (GsRegion *lower = region->m_lower; lower != nullptr; lower = region->m_lower) {
        if (lower->m_state != GsRegion_Free && lower->m_state != GsRegion_Released)
            break;
        if (lower->m_state == GsRegion_Released) {
            freeRegion(lower);
            return;
        }
        region->m_size += lower->m_size;
        region->m_block = lower->m_block;
        region->m_lower = lower->m_lower;
        if (lower->m_lower != nullptr)
            lower->m_lower->m_higher = region;
        m_free.remove(lower);
        putRecord(lower);
    }

    for (higher = region->m_higher; higher != nullptr; higher = region->m_higher) {
        if (higher->m_state != GsRegion_Free && higher->m_state != GsRegion_Released)
            return;
        if (higher->m_state == GsRegion_Released) {
            freeRegion(higher);
            return;
        }
        region->m_size += higher->m_size;
        region->m_higher = higher->m_higher;
        if (higher->m_higher != nullptr)
            higher->m_higher->m_lower = region;
        m_free.remove(higher);
        putRecord(higher);
    }
}

// ──────────────────────────────────────────────────────────────────────
// Best fit; when nothing fits, the oldest unpinned region in use is freed
// and the search starts again. With every region in use pinned and nothing
// free that fits, the ROM loops here for ever; so does this.
// FS2 0x002e10c0

GsRegion *GsRegionAlloc::alloc(u_int blocks)
{
    GsRegion *region;
    while ((region = findFree(blocks)) == nullptr) {
        for (u_int i = 0; i < m_used.size(); i++) {
            GsRegion *used = m_used.at(i);
            if (used->m_pinned == 0) {
                freeRegion(used);
                break;
            }
        }
    }

    if (region->m_size != blocks) {
        // The rest becomes a free region just above.
        GsRegion *rest = takeRecord();
        rest->m_lower = region;
        rest->m_higher = region->m_higher;
        if (region->m_higher != nullptr)
            region->m_higher->m_lower = rest;
        rest->m_block = region->m_block + blocks;
        rest->m_size = region->m_size - blocks;
        rest->m_owner = nullptr;
        rest->m_pinned = 0;
        rest->m_state = GsRegion_Free;
        m_free.push_back(rest);
        region->m_higher = rest;
        region->m_size = blocks;
    }
    m_free.remove(region);
    m_used.push_back(region);
    return region;
}

// ──────────────────────────────────────────────────────────────────────
// The textures keep their regions' blocks no longer.
// FS2 0x002e1440
// callcheck: operator_delete (the deleting dtor's flag: the host compiler's own)

GsRegionAlloc::~GsRegionAlloc()
{
    for (GsPoolList<GsRegion *, kRegions>::Node *node = m_used.head(); node != nullptr; node = node->next) {
        if (node->value->m_owner != nullptr)
            node->value->m_owner->m_region = nullptr;
    }
}

// ──────────────────────────────────────────────────────────────────────
// The lists are made by their ctors, then cleared again (the ROM's inlined
// reset); every region record spare; one free region over the whole range.
// FS2 0x002e14d0

GsRegionAlloc::GsRegionAlloc(int base, int size)
{
    m_base = base;
    m_size = size;
    m_used.clear();
    m_free.clear();

    m_spareRegions = nullptr;
    for (int i = 0; i < kRegions; i++)
        putRecord(&m_regions[i]);

    m_root = takeRecord();
    m_root->m_lower = nullptr;
    m_root->m_higher = nullptr;
    m_root->m_block = base;
    m_root->m_size = size;
    m_root->m_pinned = 0;
    m_root->m_state = GsRegion_Free;
    m_root->m_owner = nullptr;
    m_free.push_back(m_root);
    m_bound = nullptr;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x002e16c0

float Tim2Texture::getLodScale()
{
    return *(const float *)((const char *)m_image + 8);
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x002e16d0

Tim2Texture *Tim2Texture::getMip(int level)
{
    if (level >= m_mipCount || m_mips == nullptr)
        return nullptr;
    return m_mips[level];
}
