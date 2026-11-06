#!/usr/bin/env python3
"""LiteralMap<T> (game/stagelist.h) against the ROM's three instances (offline).

python literalmapcmp.py [scripts] [ops]

Plays random scripts of insert, find and balance on the ROM's own code (eerun.py) and on the host's
template in a small exe (work/literalmapcmp/). The keys are the names the game really uses plus random
ones, repeats and values of 0 (find answers null for those) included. After each op both print what it
returned and the whole tree: value and key of every node in pre-order with its shape, then the
insertion chain and the count. The first differences and a count go out. The specials' instance has
no balance in the ROM, so its scripts have none.
"""
import os
import random
import subprocess
import sys
from pathlib import Path

import eerun
import yamipan

HERE = Path(__file__).parent
WORK = HERE / 'work' / 'literalmapcmp'
SRC = yamipan.REPO / 'src'

# ctor, dtor, insert, find, balance
INSTANCES = {
    'links': (0x1eee30, 0x1eee50, 0x1eef90, 0x1ef260, 0x1ef990),
    'conditions': (0x1ef040, 0x1ef060, 0x1ef0f0, 0x1ef590, 0x1ef850),
    'specials': (0x1eeee0, 0x1eef00, 0x281730, 0x1d1280, None),
}

NAMES = ('Always Squat BtOn GunOn Turn Move Level LevelLift Ladder Jump Door PutItem PickupItem TrapSet '
         'TrapCancel Throw Showt Reload QuickTurn Jack Bite LadderMove Lock BtAttack BtAtkCombo BtMove '
         'GunAttack Snipe GunMove Walk Pickup Up Fall Battle Gun DiveFall DiveLand TrapHit Bitten Damage '
         'DamageLand DamageFatal DamageDead Revive ApproachCar ApproachOut Drive SpWait NPCReaction '
         'ApproachBite SigLamp NumLock SevenKey SpFear SpRing SpRobo SpDrop SpPadlock SpNumLock '
         'BossAttack BossIdle').split()

HOST = r'''
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "game/stagelist.h"
typedef LiteralMap<int> Map;
static void tree(const Map::Node *n)
{
    if (n == NULL) { printf(" ."); return; }
    printf(" (%d %s", n->value, n->key);
    tree(n->left);
    tree(n->right);
    printf(")");
}
static void dump(const Map *m)
{
    printf(" |");
    tree(m->root);
    printf(" |");
    for (const Map::Node *n = m->first; n != NULL; n = n->next)
        printf(" %s", n->key);
    printf(" | %d %s\n", m->count, m->last != NULL ? m->last->key : "-");
}
int main(int, char **argv)
{
    FILE *ops = fopen(argv[1], "r");
    char op[8];
    Map *m = new Map;
    while (fscanf(ops, "%7s", op) == 1) {
        if (!strcmp(op, "new")) {
            delete m;
            m = new Map;
            printf("new");
            dump(m);
        } else if (!strcmp(op, "ins")) {
            char *key = (char *)malloc(32);
            int value;
            fscanf(ops, "%31s %d", key, &value);
            Map::Node *n = m->insert(key, value);
            printf("ins %s", n != NULL ? n->key : "-");
            dump(m);
        } else if (!strcmp(op, "fnd")) {
            char key[32];
            fscanf(ops, "%31s", key);
            Map::Node *n = m->find(key);
            printf("fnd %d", n != NULL ? n->value : -1);
            dump(m);
        } else {
            m->balance();
            printf("bal");
            dump(m);
        }
    }
    return 0;
}
'''


def build():
    WORK.mkdir(parents=True, exist_ok=True)
    (WORK / 'host.cpp').write_text(HOST)
    exe = WORK / 'host.exe'
    os.environ['PATH'] = yamipan.MINGW_BIN + os.pathsep + os.environ['PATH']
    subprocess.run([str(Path(yamipan.MINGW_BIN) / 'g++.exe'), '-std=c++17', '-O1', '-w', '-I', str(SRC),
                    str(WORK / 'host.cpp'), '-o', str(exe)], check=True)
    return exe


class Rom:
    def __init__(self, instance):
        self.ctor, self.dtor, self.ins, self.fnd, self.bal = INSTANCES[instance]
        ee = self.ee = eerun.Ee()
        ee.hook('strcmp', self.strcmp)      # the libc's use MMI ops eerun does not have
        self.map = ee.alloc(0x10)
        ee.call(self.ctor, self.map)
        self.box = ee.alloc(4)

    @staticmethod
    def strcmp(ee):
        a, b = ee.g('a0'), ee.g('a1')
        while True:
            x, y = ee.rd(a, 1), ee.rd(b, 1)
            if x != y or x == 0:
                return (x - y) & 0xffffffffffffffff     # a full register: the ROM compares 64 bits
            a += 1
            b += 1

    def text(self, addr):
        out = bytearray()
        while True:
            c = self.ee.rd(addr, 1)
            if c == 0:
                return out.decode()
            out.append(c)
            addr += 1

    def alloc_text(self, text):
        at = self.ee.alloc(32)
        self.ee.write(at, text.encode() + b'\0')
        return at

    def tree(self, node):
        if node == 0:
            return ' .'
        ee = self.ee
        return (f' ({eerun.s32(ee.u32(node))} {self.text(ee.u32(node + 4))}'
                f'{self.tree(ee.u32(node + 8))}{self.tree(ee.u32(node + 0xc))})')

    def dump(self):
        ee = self.ee
        row = ' |' + self.tree(ee.u32(self.map)) + ' |'
        node = ee.u32(self.map + 4)
        while node:
            row += ' ' + self.text(ee.u32(node + 4))
            node = ee.u32(node + 0x10)
        last = ee.u32(self.map + 8)
        return row + f' | {eerun.s32(ee.u32(self.map + 0xc))} {self.text(ee.u32(last + 4)) if last else "-"}'

    def run(self, op):
        ee = self.ee
        w = op.split()
        if w[0] == 'new':
            ee.call(self.dtor, self.map, 0)
            # The ROM's dtor keeps count; a new map is the ctor's.
            ee.call(self.ctor, self.map)
            head = 'new'
        elif w[0] == 'ins':
            ee.wr(self.box, 4, int(w[2]))
            n = ee.call(self.ins, self.map, self.alloc_text(w[1]), self.box)
            head = f'ins {self.text(ee.u32(n + 4)) if n else "-"}'
        elif w[0] == 'fnd':
            n = ee.call(self.fnd, ee.u32(self.map), self.alloc_text(w[1]))
            head = f'fnd {eerun.s32(ee.u32(n)) if n else -1}'
        else:
            root = ee.u32(self.map)
            if root:
                ee.wr(self.map, 4, ee.call(self.bal, root))
            head = 'bal'
        return head + self.dump()


def script(rng, ops, balances):
    keys = rng.sample(NAMES, rng.randint(1, len(NAMES))) + [
        ''.join(rng.choice('abAB_') for _ in range(rng.randint(1, 6))) for _ in range(rng.randint(0, 8))]
    if rng.random() < 0.3:
        keys.sort()                 # a list in order: the worst tree, the most rotations
    out = ['new']
    at = 0
    for _ in range(ops):
        roll = rng.random()
        if roll < 0.6:
            key = keys[at % len(keys)] if rng.random() < 0.8 else rng.choice(keys)
            at += 1
            out.append(f'ins {key} {0 if rng.random() < 0.05 else rng.randint(1, 1000)}')
        elif roll < 0.92 or not balances:
            out.append(f'fnd {rng.choice(keys)}')
        else:
            out.append('bal')
    return out


def main():
    scripts = int(sys.argv[1]) if len(sys.argv) > 1 else 30
    ops = int(sys.argv[2]) if len(sys.argv) > 2 else 120
    exe = build()
    total = differ = 0
    for instance, row in INSTANCES.items():
        rng = random.Random(instance)
        rom = Rom(instance)
        lines = []
        for _ in range(scripts):
            lines += script(rng, ops, row[4] is not None)
        path = WORK / f'{instance}.txt'
        path.write_text('\n'.join(lines) + '\n')
        host = subprocess.run([str(exe), str(path)], capture_output=True, text=True, check=True).stdout.split('\n')
        seen = 0
        for op, theirs in zip(lines, host):
            mine = rom.run(op)
            total += 1
            if mine != theirs:
                differ += 1
                seen += 1
                if seen <= 3:
                    print(f'{instance}: {op}\n  rom  {mine[:400]}\n  host {theirs[:400]}')
        print(f'{instance}: {len(lines)} ops')
    print(f'{total} ops, {differ} differ')
    return 1 if differ else 0


if __name__ == '__main__':
    sys.exit(main())
