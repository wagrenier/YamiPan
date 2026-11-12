/* ==========================================================================
 *  sdpr2/sdpr2single.h
 *
 *  SDPR2's one-voice type and the pan maths (sdpr2/sdpr2single.c
 *  0x16e498..0x170088, data 0x2f5fb8..0x2f7a58, bss 0x537cc0..0x537d30).
 *  architecture.md "SDPR2".
 *
 *  Angles are degrees, 0 ahead, clockwise (90 right). The pan is a
 *  constant-power pair of half-degree tables read at (angle + 90): gains
 *  2048 = 1.0, a negative gain is a phase sign -1 (the SPU2's phase
 *  inversion, heard as "behind"). So the phase does not jump when a sound
 *  crosses the back, the pan angle is unwrapped over two turns (0..719,
 *  sdpr2UnwrapAngle) and the tables cover 720 half degrees.
 * ======================================================================== */
#ifndef _SDPR2_SDPR2SINGLE_H
#define _SDPR2_SDPR2SINGLE_H

#include "sdpr2/sdpr2.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SDPR2_GAIN_ONE          2048        /* the tables' 1.0 (>> 11)              */
#define SDPR2_GAIN_NEG_ZERO     (-0x1000)   /* a table's 0 with phase sign -1       */
#define SDPR2_PAN_TABLE_SIZE    720         /* half degrees, also the unwrapped range */
#define SDPR2_SPREAD_TABLE_SIZE (SDPR2_SPREAD_MAX + 1)

/* sdpr2CalcLevels' five outputs: speakers at 0, 330, 30, 240, 120 deg. */
enum Sdpr2Level
{
    SDPR2_LEVEL_CENTER = 0,     /*   0 deg */
    SDPR2_LEVEL_LEFT,           /* 330 deg */
    SDPR2_LEVEL_RIGHT,          /*  30 deg */
    SDPR2_LEVEL_REAR_LEFT,      /* 240 deg */
    SDPR2_LEVEL_REAR_RIGHT,     /* 120 deg */
    SDPR2_LEVEL_MAX
};

/* sdpr2CalcLevels' voice: the sound's own, or a pair's second voice at
 * angle + 180 (each scaled by its spread table). */
#define SDPR2_LEVELS_MAIN   0
#define SDPR2_LEVELS_BACK   1

/* The tables (data; the pair reads them too). */
extern const int sdpr2SinData[SDPR2_PAN_TABLE_SIZE];                    /* FS2 0x002f5fb8 */
extern const int sdpr2CosData[SDPR2_PAN_TABLE_SIZE];                    /* FS2 0x002f6af8 */
extern const int sdpr2SpreadMainData[SDPR2_SPREAD_TABLE_SIZE];          /* FS2 0x002f7638 */
extern const int sdpr2SpreadBackData[SDPR2_SPREAD_TABLE_SIZE];          /* FS2 0x002f7840 */
extern const int *sdpr2CosTable;            /* FS2 0x002f7a48: sdpr2CosData, the left gain  */
extern const int *sdpr2SinTable;            /* FS2 0x002f7a4c: sdpr2SinData, the right gain */
extern const int *sdpr2SpreadMainTable;     /* FS2 0x002f7a50: sdpr2SpreadMainData          */
extern const int *sdpr2SpreadBackTable;     /* FS2 0x002f7a54: sdpr2SpreadBackData          */

/* Registers SDPR2_TYPE_SINGLE; 0, or -1. */
int  sdpr2SingleInit(void);                                             /* FS2 0x0016f808 */
int  sdpr2SingleExit(void);                                             /* FS2 0x0016f960 */

/* The gains (0..0x80) of angle (unwrapped, 0..719; 0x80 = 1.0) with their
 * phase signs applied (-0x80..0x80). */
void sdpr2CalcPan(int angle, int *volL, int *volR);                     /* FS2 0x0016f978 */

/* The left / right gains (-0x80..0x80, phase sign applied) of angle deg;
 * state carries the previous unwrapped angle between calls (0 to start).
 * The new state. */
int  sdpr2CalcPanDeg(int deg, int state, int *volL, int *volR);         /* FS2 0x0016f9e0 */

/* volume (0..0x80) panned to angle (unwrapped, 0..719): the gains
 * (volume * |table| / 2048) and their phase signs (+1 / -1). */
void sdpr2CalcPanVolumes(int volume, int angle, int *volL, int *volR,
                         int *signL, int *signR);                       /* FS2 0x0016fa78 */

/* deg (0..359) moved to the unwrapped angle (0..719) nearest prev. */
int  sdpr2UnwrapAngle(int deg, int prev);                               /* FS2 0x0016fb68 */

/* volume at deg (0..359) between the two speakers around it, scaled by
 * the spread table of which (SDPR2_LEVELS_*) at spread: writes those two
 * of levels[SDPR2_LEVEL_MAX], leaves the others. */
void sdpr2CalcLevels(int volume, int deg, int spread, int which,
                     int *levels);                                      /* FS2 0x0016fbd0 */

#ifdef __cplusplus
}
#endif

#endif /* _SDPR2_SDPR2SINGLE_H */
