/* ==========================================================================
 *  game/layoutpaths.h
 *
 *  The paths of the event movies and of the menu layouts
 *  (game/layoutpaths.cpp, 0x2ec160..0x2ec8e0): each answers a static buffer
 *  holding the file's name in the current language, so a second call to the
 *  same function overwrites the first answer.
 * ======================================================================== */
#ifndef _GAME_LAYOUTPATHS_H
#define _GAME_LAYOUTPATHS_H

// The event movies stageMoviePath's table names (game/gamepaths.cpp):
// movieEU/ev<id><lang>.d2b.
const char *eventMoviePath0102();                                        // FS2 0x002ec520
const char *eventMoviePath0202();                                        // FS2 0x002ec4d0
const char *eventMoviePath1206();                                        // FS2 0x002ec480
const char *eventMoviePath1404();                                        // FS2 0x002ec430
const char *eventMoviePath1405();                                        // FS2 0x002ec3e0
const char *eventMoviePath1505();                                        // FS2 0x002ec390
const char *eventMoviePath1506();                                        // FS2 0x002ec340
const char *eventMoviePath1608();                                        // FS2 0x002ec2f0
const char *eventMoviePath2103();                                        // FS2 0x002ec2a0
const char *eventMoviePath2205();                                        // FS2 0x002ec250
const char *eventMoviePath2301();                                        // FS2 0x002ec200
const char *eventMoviePath2502();                                        // FS2 0x002ec1b0
const char *eventMoviePath2506();                                        // FS2 0x002ec160

// The menu layouts: 2dEU/<name><lang>.d2b.
const char *naviLayoutPath();                                            // FS2 0x002ec570
const char *naviTuLayoutPath();                                          // FS2 0x002ec5c0
const char *mcBaseLayoutPath();                                          // FS2 0x002ec610
const char *optionLayoutPath();                                          // FS2 0x002ec660
const char *memAltLayoutPath();                                          // FS2 0x002ec6b0
const char *newGameLayoutPath();                                         // FS2 0x002ec700
const char *sceLogoLayoutPath();                                         // FS2 0x002ec750
const char *topMenuLayoutPath();                                         // FS2 0x002ec7a0
const char *archiveLayoutPath();                                         // FS2 0x002ec7f0
const char *helpLayoutPath();                                            // FS2 0x002ec840
const char *titleLayoutPath();                                           // FS2 0x002ec890

#endif // _GAME_LAYOUTPATHS_H
