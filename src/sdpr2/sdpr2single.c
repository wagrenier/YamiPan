/* ==========================================================================
 *  sdpr2/sdpr2single.c
 *
 *  SDPR2's one-voice type (registry SDPR2_TYPE_SINGLE) and the pan maths
 *  both types share (FS2 0x16e498..0x170088, data 0x2f5fb8..0x2f7a58, bss
 *  0x537cc0..0x537d30). See sdpr2single.h.
 *
 *  The type plays one SE voice per handle (Sdpr2Slot.voice[0]; voice[1]
 *  stays -1), panned by sdpr2CalcPanVolumes. It ignores spread (always
 *  0x80) and keeps three angles: angle and levelAngle as given, panAngle
 *  unwrapped (sdpr2UnwrapAngle) so the phase sign changes smoothly.
 *
 *  FS2-only middleware, named by reading (batch 011). The handle checks
 *  are sdpr2check.h's, with this file's own message copies in the ROM
 *  (0x302a88..); every failed call is made a second time for its answer
 *  (SDPR2_TRY's shape), so a failed driver play plays twice: kept.
 * ======================================================================== */
#include "sdpr2/sdpr2single.h"

#include <stddef.h>

#include "sdpr2/sdpr2.h"
#include "sdpr2/sdpr2check.h"
#include "sdpr2/sdpr2drv.h"

/* sdpr2DrvGetConfig's 8 words: [0] 1 = sdpr2SingleUpdate stops a voice
 * that ended (the game passes 0). */
#define SDPR2_CONFIG_WORDS      8
#define SDPR2_CONFIG_STOP_ENDED 0

#define SDPR2_STOP_ENDED_MODE   1   /* sdpr2DrvStop's mode for that stop */

/* ---- data ------------------------------------------------------------- */

/* data 0x2f5fb8: 2048 sin(i / 2 deg); -4096 at 180 deg */
const int sdpr2SinData[720] =
{
        0,    17,    35,    53,    71,    89,   107,   125,   142,   160,   178,   196,  /*   0 */
      214,   231,   249,   267,   285,   302,   320,   338,   355,   373,   390,   408,  /*  12 */
      425,   443,   460,   478,   495,   512,   530,   547,   564,   581,   598,   615,  /*  24 */
      632,   649,   666,   683,   700,   717,   733,   750,   767,   783,   800,   816,  /*  36 */
      832,   849,   865,   881,   897,   913,   929,   945,   961,   977,   992,  1008,  /*  48 */
     1023,  1039,  1054,  1070,  1085,  1100,  1115,  1130,  1145,  1159,  1174,  1189,  /*  60 */
     1203,  1218,  1232,  1246,  1260,  1274,  1288,  1302,  1316,  1330,  1343,  1357,  /*  72 */
     1370,  1383,  1396,  1409,  1422,  1435,  1448,  1460,  1473,  1485,  1497,  1509,  /*  84 */
     1521,  1533,  1545,  1557,  1568,  1580,  1591,  1602,  1613,  1624,  1635,  1646,  /*  96 */
     1656,  1667,  1677,  1687,  1697,  1707,  1717,  1727,  1736,  1746,  1755,  1764,  /* 108 */
     1773,  1782,  1791,  1799,  1808,  1816,  1824,  1832,  1840,  1848,  1856,  1863,  /* 120 */
     1870,  1878,  1885,  1892,  1898,  1905,  1911,  1918,  1924,  1930,  1936,  1942,  /* 132 */
     1947,  1953,  1958,  1963,  1968,  1973,  1978,  1982,  1987,  1991,  1995,  1999,  /* 144 */
     2003,  2006,  2010,  2013,  2016,  2019,  2022,  2025,  2028,  2030,  2032,  2034,  /* 156 */
     2036,  2038,  2040,  2041,  2043,  2044,  2045,  2046,  2046,  2047,  2047,  2047,  /* 168 */
     2048,  2047,  2047,  2047,  2046,  2046,  2045,  2044,  2043,  2041,  2040,  2038,  /* 180 */
     2036,  2034,  2032,  2030,  2028,  2025,  2022,  2019,  2016,  2013,  2010,  2006,  /* 192 */
     2003,  1999,  1995,  1991,  1987,  1982,  1978,  1973,  1968,  1963,  1958,  1953,  /* 204 */
     1947,  1942,  1936,  1930,  1924,  1918,  1911,  1905,  1898,  1892,  1885,  1878,  /* 216 */
     1870,  1863,  1856,  1848,  1840,  1832,  1824,  1816,  1808,  1799,  1791,  1782,  /* 228 */
     1773,  1764,  1755,  1746,  1736,  1727,  1717,  1707,  1697,  1687,  1677,  1667,  /* 240 */
     1656,  1646,  1635,  1624,  1613,  1602,  1591,  1580,  1568,  1557,  1545,  1533,  /* 252 */
     1521,  1509,  1497,  1485,  1473,  1460,  1448,  1435,  1422,  1409,  1396,  1383,  /* 264 */
     1370,  1357,  1343,  1330,  1316,  1302,  1288,  1274,  1260,  1246,  1232,  1218,  /* 276 */
     1203,  1189,  1174,  1159,  1145,  1130,  1115,  1100,  1085,  1070,  1054,  1039,  /* 288 */
     1024,  1008,   992,   977,   961,   945,   929,   913,   897,   881,   865,   849,  /* 300 */
      832,   816,   800,   783,   767,   750,   733,   717,   700,   683,   666,   649,  /* 312 */
      632,   615,   598,   581,   564,   547,   530,   512,   495,   478,   460,   443,  /* 324 */
      425,   408,   390,   373,   355,   338,   320,   302,   285,   267,   249,   231,  /* 336 */
      214,   196,   178,   160,   142,   125,   107,    89,    71,    53,    35,    17,  /* 348 */
    -4096,   -17,   -35,   -53,   -71,   -89,  -107,  -125,  -142,  -160,  -178,  -196,  /* 360 */
     -214,  -231,  -249,  -267,  -285,  -302,  -320,  -338,  -355,  -373,  -390,  -408,  /* 372 */
     -425,  -443,  -460,  -478,  -495,  -512,  -530,  -547,  -564,  -581,  -598,  -615,  /* 384 */
     -632,  -649,  -666,  -683,  -700,  -717,  -733,  -750,  -767,  -783,  -800,  -816,  /* 396 */
     -832,  -849,  -865,  -881,  -897,  -913,  -929,  -945,  -961,  -977,  -992, -1008,  /* 408 */
    -1023, -1039, -1054, -1070, -1085, -1100, -1115, -1130, -1145, -1159, -1174, -1189,  /* 420 */
    -1203, -1218, -1232, -1246, -1260, -1274, -1288, -1302, -1316, -1330, -1343, -1357,  /* 432 */
    -1370, -1383, -1396, -1409, -1422, -1435, -1448, -1460, -1473, -1485, -1497, -1509,  /* 444 */
    -1521, -1533, -1545, -1557, -1568, -1580, -1591, -1602, -1613, -1624, -1635, -1646,  /* 456 */
    -1656, -1667, -1677, -1687, -1697, -1707, -1717, -1727, -1736, -1746, -1755, -1764,  /* 468 */
    -1773, -1782, -1791, -1799, -1808, -1816, -1824, -1832, -1840, -1848, -1856, -1863,  /* 480 */
    -1870, -1878, -1885, -1892, -1898, -1905, -1911, -1918, -1924, -1930, -1936, -1942,  /* 492 */
    -1947, -1953, -1958, -1963, -1968, -1973, -1978, -1982, -1987, -1991, -1995, -1999,  /* 504 */
    -2003, -2006, -2010, -2013, -2016, -2019, -2022, -2025, -2028, -2030, -2032, -2034,  /* 516 */
    -2036, -2038, -2040, -2041, -2043, -2044, -2045, -2046, -2046, -2047, -2047, -2047,  /* 528 */
    -2048, -2047, -2047, -2047, -2046, -2046, -2045, -2044, -2043, -2041, -2040, -2038,  /* 540 */
    -2036, -2034, -2032, -2030, -2028, -2025, -2022, -2019, -2016, -2013, -2010, -2006,  /* 552 */
    -2003, -1999, -1995, -1991, -1987, -1982, -1978, -1973, -1968, -1963, -1958, -1953,  /* 564 */
    -1947, -1942, -1936, -1930, -1924, -1918, -1911, -1905, -1898, -1892, -1885, -1878,  /* 576 */
    -1870, -1863, -1856, -1848, -1840, -1832, -1824, -1816, -1808, -1799, -1791, -1782,  /* 588 */
    -1773, -1764, -1755, -1746, -1736, -1727, -1717, -1707, -1697, -1687, -1677, -1667,  /* 600 */
    -1656, -1646, -1635, -1624, -1613, -1602, -1591, -1580, -1568, -1557, -1545, -1533,  /* 612 */
    -1521, -1509, -1497, -1485, -1473, -1460, -1448, -1435, -1422, -1409, -1396, -1383,  /* 624 */
    -1370, -1357, -1343, -1330, -1316, -1302, -1288, -1274, -1260, -1246, -1232, -1218,  /* 636 */
    -1203, -1189, -1174, -1159, -1145, -1130, -1115, -1100, -1085, -1070, -1054, -1039,  /* 648 */
    -1024, -1008,  -992,  -977,  -961,  -945,  -929,  -913,  -897,  -881,  -865,  -849,  /* 660 */
     -832,  -816,  -800,  -783,  -767,  -750,  -733,  -717,  -700,  -683,  -666,  -649,  /* 672 */
     -632,  -615,  -598,  -581,  -564,  -547,  -530,  -512,  -495,  -478,  -460,  -443,  /* 684 */
     -425,  -408,  -390,  -373,  -355,  -338,  -320,  -302,  -285,  -267,  -249,  -231,  /* 696 */
     -214,  -196,  -178,  -160,  -142,  -125,  -107,   -89,   -71,   -53,   -35,   -17,  /* 708 */
};

/* data 0x2f6af8: 2048 cos(i / 2 deg); -4096 at 90 deg */
const int sdpr2CosData[720] =
{
     2048,  2047,  2047,  2047,  2046,  2046,  2045,  2044,  2043,  2041,  2040,  2038,  /*   0 */
     2036,  2034,  2032,  2030,  2028,  2025,  2022,  2019,  2016,  2013,  2010,  2006,  /*  12 */
     2003,  1999,  1995,  1991,  1987,  1982,  1978,  1973,  1968,  1963,  1958,  1953,  /*  24 */
     1947,  1942,  1936,  1930,  1924,  1918,  1911,  1905,  1898,  1892,  1885,  1878,  /*  36 */
     1870,  1863,  1856,  1848,  1840,  1832,  1824,  1816,  1808,  1799,  1791,  1782,  /*  48 */
     1773,  1764,  1755,  1746,  1736,  1727,  1717,  1707,  1697,  1687,  1677,  1667,  /*  60 */
     1656,  1646,  1635,  1624,  1613,  1602,  1591,  1580,  1568,  1557,  1545,  1533,  /*  72 */
     1521,  1509,  1497,  1485,  1473,  1460,  1448,  1435,  1422,  1409,  1396,  1383,  /*  84 */
     1370,  1357,  1343,  1330,  1316,  1302,  1288,  1274,  1260,  1246,  1232,  1218,  /*  96 */
     1203,  1189,  1174,  1159,  1145,  1130,  1115,  1100,  1085,  1070,  1054,  1039,  /* 108 */
     1024,  1008,   992,   977,   961,   945,   929,   913,   897,   881,   865,   849,  /* 120 */
      832,   816,   800,   783,   767,   750,   733,   717,   700,   683,   666,   649,  /* 132 */
      632,   615,   598,   581,   564,   547,   530,   512,   495,   478,   460,   443,  /* 144 */
      425,   408,   390,   373,   355,   338,   320,   302,   285,   267,   249,   231,  /* 156 */
      214,   196,   178,   160,   142,   125,   107,    89,    71,    53,    35,    17,  /* 168 */
    -4096,   -17,   -35,   -53,   -71,   -89,  -107,  -125,  -142,  -160,  -178,  -196,  /* 180 */
     -214,  -231,  -249,  -267,  -285,  -302,  -320,  -338,  -355,  -373,  -390,  -408,  /* 192 */
     -425,  -443,  -460,  -478,  -495,  -512,  -530,  -547,  -564,  -581,  -598,  -615,  /* 204 */
     -632,  -649,  -666,  -683,  -700,  -717,  -733,  -750,  -767,  -783,  -800,  -816,  /* 216 */
     -832,  -849,  -865,  -881,  -897,  -913,  -929,  -945,  -961,  -977,  -992, -1008,  /* 228 */
    -1023, -1039, -1054, -1070, -1085, -1100, -1115, -1130, -1145, -1159, -1174, -1189,  /* 240 */
    -1203, -1218, -1232, -1246, -1260, -1274, -1288, -1302, -1316, -1330, -1343, -1357,  /* 252 */
    -1370, -1383, -1396, -1409, -1422, -1435, -1448, -1460, -1473, -1485, -1497, -1509,  /* 264 */
    -1521, -1533, -1545, -1557, -1568, -1580, -1591, -1602, -1613, -1624, -1635, -1646,  /* 276 */
    -1656, -1667, -1677, -1687, -1697, -1707, -1717, -1727, -1736, -1746, -1755, -1764,  /* 288 */
    -1773, -1782, -1791, -1799, -1808, -1816, -1824, -1832, -1840, -1848, -1856, -1863,  /* 300 */
    -1870, -1878, -1885, -1892, -1898, -1905, -1911, -1918, -1924, -1930, -1936, -1942,  /* 312 */
    -1947, -1953, -1958, -1963, -1968, -1973, -1978, -1982, -1987, -1991, -1995, -1999,  /* 324 */
    -2003, -2006, -2010, -2013, -2016, -2019, -2022, -2025, -2028, -2030, -2032, -2034,  /* 336 */
    -2036, -2038, -2040, -2041, -2043, -2044, -2045, -2046, -2046, -2047, -2047, -2047,  /* 348 */
    -2048, -2047, -2047, -2047, -2046, -2046, -2045, -2044, -2043, -2041, -2040, -2038,  /* 360 */
    -2036, -2034, -2032, -2030, -2028, -2025, -2022, -2019, -2016, -2013, -2010, -2006,  /* 372 */
    -2003, -1999, -1995, -1991, -1987, -1982, -1978, -1973, -1968, -1963, -1958, -1953,  /* 384 */
    -1947, -1942, -1936, -1930, -1924, -1918, -1911, -1905, -1898, -1892, -1885, -1878,  /* 396 */
    -1870, -1863, -1856, -1848, -1840, -1832, -1824, -1816, -1808, -1799, -1791, -1782,  /* 408 */
    -1773, -1764, -1755, -1746, -1736, -1727, -1717, -1707, -1697, -1687, -1677, -1667,  /* 420 */
    -1656, -1646, -1635, -1624, -1613, -1602, -1591, -1580, -1568, -1557, -1545, -1533,  /* 432 */
    -1521, -1509, -1497, -1485, -1473, -1460, -1448, -1435, -1422, -1409, -1396, -1383,  /* 444 */
    -1370, -1357, -1343, -1330, -1316, -1302, -1288, -1274, -1260, -1246, -1232, -1218,  /* 456 */
    -1203, -1189, -1174, -1159, -1145, -1130, -1115, -1100, -1085, -1070, -1054, -1039,  /* 468 */
    -1024, -1008,  -992,  -977,  -961,  -945,  -929,  -913,  -897,  -881,  -865,  -849,  /* 480 */
     -832,  -816,  -800,  -783,  -767,  -750,  -733,  -717,  -700,  -683,  -666,  -649,  /* 492 */
     -632,  -615,  -598,  -581,  -564,  -547,  -530,  -512,  -495,  -478,  -460,  -443,  /* 504 */
     -425,  -408,  -390,  -373,  -355,  -338,  -320,  -302,  -285,  -267,  -249,  -231,  /* 516 */
     -214,  -196,  -178,  -160,  -142,  -125,  -107,   -89,   -71,   -53,   -35,   -17,  /* 528 */
        0,    17,    35,    53,    71,    89,   107,   125,   142,   160,   178,   196,  /* 540 */
      214,   231,   249,   267,   285,   302,   320,   338,   355,   373,   390,   408,  /* 552 */
      425,   443,   460,   478,   495,   512,   530,   547,   564,   581,   598,   615,  /* 564 */
      632,   649,   666,   683,   700,   717,   733,   750,   767,   783,   800,   816,  /* 576 */
      832,   849,   865,   881,   897,   913,   929,   945,   961,   977,   992,  1008,  /* 588 */
     1023,  1039,  1054,  1070,  1085,  1100,  1115,  1130,  1145,  1159,  1174,  1189,  /* 600 */
     1203,  1218,  1232,  1246,  1260,  1274,  1288,  1302,  1316,  1330,  1343,  1357,  /* 612 */
     1370,  1383,  1396,  1409,  1422,  1435,  1448,  1460,  1473,  1485,  1497,  1509,  /* 624 */
     1521,  1533,  1545,  1557,  1568,  1580,  1591,  1602,  1613,  1624,  1635,  1646,  /* 636 */
     1656,  1667,  1677,  1687,  1697,  1707,  1717,  1727,  1736,  1746,  1755,  1764,  /* 648 */
     1773,  1782,  1791,  1799,  1808,  1816,  1824,  1832,  1840,  1848,  1856,  1863,  /* 660 */
     1870,  1878,  1885,  1892,  1898,  1905,  1911,  1918,  1924,  1930,  1936,  1942,  /* 672 */
     1947,  1953,  1958,  1963,  1968,  1973,  1978,  1982,  1987,  1991,  1995,  1999,  /* 684 */
     2003,  2006,  2010,  2013,  2016,  2019,  2022,  2025,  2028,  2030,  2032,  2034,  /* 696 */
     2036,  2038,  2040,  2041,  2043,  2044,  2045,  2046,  2046,  2047,  2047,  2047,  /* 708 */
};

/* data 0x2f7638: by spread 0..0x80: 1448 (cos 45) up to 2048 */
const int sdpr2SpreadMainData[129] =
{
     1448,  1457,  1465,  1474,  1483,  1491,  1500,  1509,  1517,  1525,  1534,  1542,  /*   0 */
     1550,  1558,  1567,  1575,  1583,  1591,  1598,  1606,  1614,  1622,  1629,  1637,  /*  12 */
     1644,  1652,  1659,  1667,  1674,  1681,  1688,  1695,  1702,  1709,  1716,  1723,  /*  24 */
     1730,  1736,  1743,  1750,  1756,  1763,  1769,  1775,  1781,  1788,  1794,  1800,  /*  36 */
     1806,  1812,  1817,  1823,  1829,  1834,  1840,  1845,  1851,  1856,  1861,  1867,  /*  48 */
     1872,  1877,  1882,  1887,  1892,  1896,  1901,  1906,  1910,  1915,  1919,  1924,  /*  60 */
     1928,  1932,  1936,  1940,  1944,  1948,  1952,  1956,  1959,  1963,  1966,  1970,  /*  72 */
     1973,  1977,  1980,  1983,  1986,  1989,  1992,  1995,  1998,  2000,  2003,  2006,  /*  84 */
     2008,  2011,  2013,  2015,  2017,  2019,  2021,  2023,  2025,  2027,  2029,  2031,  /*  96 */
     2032,  2034,  2035,  2036,  2038,  2039,  2040,  2041,  2042,  2043,  2044,  2044,  /* 108 */
     2045,  2046,  2046,  2047,  2047,  2047,  2047,  2047,  2048,  /* 120 */
};

/* data 0x2f7840: by spread 0..0x80: 1448 (cos 45) down to 0 */
const int sdpr2SpreadBackData[129] =
{
     1448,  1439,  1430,  1421,  1412,  1403,  1393,  1384,  1375,  1366,  1356,  1347,  /*   0 */
     1337,  1328,  1318,  1308,  1299,  1289,  1279,  1269,  1259,  1250,  1240,  1230,  /*  12 */
     1219,  1209,  1199,  1189,  1179,  1168,  1158,  1148,  1137,  1127,  1116,  1106,  /*  24 */
     1095,  1085,  1074,  1063,  1052,  1042,  1031,  1020,  1009,   998,   987,   976,  /*  36 */
      965,   954,   943,   932,   920,   909,   898,   886,   875,   864,   852,   841,  /*  48 */
      829,   818,   806,   795,   783,   772,   760,   748,   737,   725,   713,   701,  /*  60 */
      689,   678,   666,   654,   642,   630,   618,   606,   594,   582,   570,   558,  /*  72 */
      546,   534,   521,   509,   497,   485,   473,   460,   448,   436,   424,   411,  /*  84 */
      399,   387,   374,   362,   350,   337,   325,   312,   300,   288,   275,   263,  /*  96 */
      250,   238,   225,   213,   200,   188,   175,   163,   150,   138,   125,   113,  /* 108 */
      100,    87,    75,    62,    50,    37,    25,    12,     0,  /* 120 */
};

/* data 0x2f7a44: one zero word (padding), then the tables' pointers. */
const int *sdpr2CosTable = sdpr2CosData;                /* data 0x2f7a48 */
const int *sdpr2SinTable = sdpr2SinData;                /* data 0x2f7a4c */
const int *sdpr2SpreadMainTable = sdpr2SpreadMainData;  /* data 0x2f7a50 */
const int *sdpr2SpreadBackTable = sdpr2SpreadBackData;  /* data 0x2f7a54 */

/* ---- bss -------------------------------------------------------------- */

static Sdpr2Type s_type;                        /* bss 0x537cc0: filled by sdpr2SingleInit */
static int s_config[SDPR2_CONFIG_WORDS];        /* bss 0x537d10 */

// ──────────────────────────────────────────────────────────────────────
// Pans an SE voice: the gains of angle (unwrapped) at volume, both signs
// forced to +1 without surround.
// FS2 0x0016e498

static int sdpr2SingleSetPan(int voice, int volume, int angle)
{
    int volL, volR, signL, signR;

    sdpr2CalcPanVolumes(volume, angle, &volL, &volR, &signL, &signR);
    if (sdpr2GetSurround() == 0)
    {
        signL = 1;
        signR = 1;
    }
    SDPR2_TRY(sdpr2DrvSetVolume(voice, volL, volR, signL, signR));
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// A free slot, the voice played at volume and angle deg (0..359, also the
// first unwrapped angle); the slot's fields set; its index. spread is
// not used (stored 0x80).
// FS2 0x0016e538

static int sdpr2SinglePlayPan(int xvab, int prog, int note, int volume,
                              int deg, int spread)
{
    Sdpr2Slot *slot;
    int handle, voice;
    int volL, volR, signL, signR;

    (void)spread;
    handle = sdpr2FindFreeSlot();
    if (handle == SDPR2_ERR)
        return sdpr2FindFreeSlot();
    slot = sdpr2GetSlot(handle);
    if (slot == NULL)
        return -1;
    sdpr2CalcPanVolumes(volume, deg, &volL, &volR, &signL, &signR);
    if (sdpr2GetSurround() == 0)
    {
        signL = 1;
        signR = 1;
    }
    voice = sdpr2DrvPlay(xvab, prog, note, volL, volR, signL, signR);
    if (voice == SDPR2_ERR)
        return sdpr2DrvPlay(xvab, prog, note, volL, volR, signL, signR);
    slot->voice[1] = SDPR2_VOICE_FREE;
    slot->panAngle = deg;
    slot->volume = volume;
    slot->pitch = SDPR2_PITCH_ONE;
    slot->spread = SDPR2_SPREAD_MAX;
    slot->state = 0;
    slot->flip = 0;
    slot->levelAngle = deg;
    slot->angle = deg;
    slot->voice[0] = voice;
    return handle;
}

// ──────────────────────────────────────────────────────────────────────
// Full volume, ahead, full spread.
// FS2 0x0016e690

static int sdpr2SinglePlay(int xvab, int prog, int note)
{
    return sdpr2SinglePlayPan(xvab, prog, note, SDPR2_VOLUME_MAX, 0,
                              SDPR2_SPREAD_MAX);
}

// ──────────────────────────────────────────────────────────────────────
// sdpr2SinglePlayPan with an SE parameter set (sdpr2DrvPlayEx).
// FS2 0x0016e6b0

static int sdpr2SinglePlayPanEx(int xvab, int prog, int note, int volume,
                                int deg, int spread, int preset)
{
    Sdpr2Slot *slot;
    int handle, voice;
    int volL, volR, signL, signR;

    (void)spread;
    handle = sdpr2FindFreeSlot();
    if (handle == SDPR2_ERR)
        return sdpr2FindFreeSlot();
    slot = sdpr2GetSlot(handle);
    if (slot == NULL)
        return -1;
    sdpr2CalcPanVolumes(volume, deg, &volL, &volR, &signL, &signR);
    if (sdpr2GetSurround() == 0)
    {
        signL = 1;
        signR = 1;
    }
    voice = sdpr2DrvPlayEx(xvab, prog, note, volL, volR, signL, signR, preset);
    if (voice == SDPR2_ERR)
        return sdpr2DrvPlayEx(xvab, prog, note, volL, volR, signL, signR,
                              preset);
    slot->voice[1] = SDPR2_VOICE_FREE;
    slot->panAngle = deg;
    slot->volume = volume;
    slot->pitch = SDPR2_PITCH_ONE;
    slot->spread = SDPR2_SPREAD_MAX;
    slot->state = 0;
    slot->flip = 0;
    slot->levelAngle = deg;
    slot->angle = deg;
    slot->voice[0] = voice;
    return handle;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x0016e820

static int sdpr2SinglePlayEx(int xvab, int prog, int note, int preset)
{
    return sdpr2SinglePlayPanEx(xvab, prog, note, SDPR2_VOLUME_MAX, 0,
                                SDPR2_SPREAD_MAX, preset);
}

// ──────────────────────────────────────────────────────────────────────
// The voice stopped and the slot freed.
// FS2 0x0016e848

static int sdpr2SingleStop(int handle, int mode)
{
    Sdpr2Slot *slot;

    // callcheck: sdpr2GetDebug, rxSysPrintf (sdpr2CheckRange, inline)
    SDPR2_TRY(sdpr2CheckRange(handle, SDPR2_SLOT_MAX));
    slot = sdpr2GetSlot(handle);
    if (slot == NULL)
        return -1;
    if (slot->voice[0] == SDPR2_VOICE_FREE)
        return -1;
    SDPR2_TRY(sdpr2DrvStop(slot->voice[0], mode));
    slot->voice[1] = SDPR2_VOICE_FREE;
    slot->state = 0;
    slot->voice[0] = SDPR2_VOICE_FREE;
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// Every playing slot of this type; the first failed stop ends the walk.
// FS2 0x0016e9d0

static int sdpr2SingleStopAll(int mode)
{
    Sdpr2Slot *slot;
    int i;

    i = 0;
    do
    {
        slot = sdpr2GetSlot(i);
        if (slot == NULL)
            return -1;
        if (slot->type == &s_type && slot->voice[0] != SDPR2_VOICE_FREE)
        {
            SDPR2_TRY(sdpr2DrvStop(slot->voice[0], mode));
            slot->voice[1] = SDPR2_VOICE_FREE;
            slot->state = 0;
            slot->voice[0] = SDPR2_VOICE_FREE;
        }
        i++;
    } while (i < SDPR2_SLOT_MAX);
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x0016ea90

static int sdpr2SinglePause(int handle, int on)
{
    Sdpr2Slot *slot;

    // callcheck: sdpr2GetDebug, rxSysPrintf (sdpr2CheckRange, inline)
    SDPR2_TRY(sdpr2CheckRange(handle, SDPR2_SLOT_MAX));
    slot = sdpr2GetSlot(handle);
    if (slot == NULL)
        return -1;
    if (slot->voice[0] == SDPR2_VOICE_FREE)
        return -1;
    SDPR2_TRY(sdpr2DrvPause(slot->voice[0], on));
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// pitch: SDPR2_PITCH_ONE = 1.0.
// FS2 0x0016ec10

static int sdpr2SingleSetPitch(int handle, int pitch)
{
    Sdpr2Slot *slot;

    // callcheck: sdpr2GetDebug, rxSysPrintf (sdpr2CheckRange, inline)
    SDPR2_TRY(sdpr2CheckRange(handle, SDPR2_SLOT_MAX));
    slot = sdpr2GetSlot(handle);
    if (slot == NULL)
        return -1;
    if (slot->voice[0] == SDPR2_VOICE_FREE)
        return -1;
    SDPR2_TRY(sdpr2DrvSetPitch(slot->voice[0], pitch));
    slot->pitch = pitch;
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// The slot keeps the pitch as fixed point (truncated).
// FS2 0x0016ed90

static int sdpr2SingleSetPitchF(int handle, float pitch)
{
    Sdpr2Slot *slot;

    // callcheck: sdpr2GetDebug, rxSysPrintf (sdpr2CheckRange, inline)
    SDPR2_TRY(sdpr2CheckRange(handle, SDPR2_SLOT_MAX));
    slot = sdpr2GetSlot(handle);
    if (slot == NULL)
        return -1;
    if (slot->voice[0] == SDPR2_VOICE_FREE)
        return -1;
    SDPR2_TRY(sdpr2DrvSetPitchF(slot->voice[0], pitch));
    slot->pitch = (int)(pitch * (float)SDPR2_PITCH_ONE);
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// The pan angle moves to deg's nearest unwrapped angle, so the phase
// sign follows the sound round the back.
// FS2 0x0016ef20

static int sdpr2SingleSetAngle(int handle, int deg)
{
    Sdpr2Slot *slot;
    int panAngle;

    // callcheck: sdpr2GetDebug, rxSysPrintf (sdpr2CheckRange, inline)
    SDPR2_TRY(sdpr2CheckRange(handle, SDPR2_SLOT_MAX));
    slot = sdpr2GetSlot(handle);
    if (slot == NULL)
        return -1;
    if (slot->voice[0] == SDPR2_VOICE_FREE)
        return -1;
    panAngle = sdpr2UnwrapAngle(deg, slot->panAngle);
    SDPR2_TRY(sdpr2SingleSetPan(slot->voice[0], slot->volume, panAngle));
    slot->panAngle = panAngle;
    slot->angle = deg;
    slot->levelAngle = deg;
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x0016f0c0

static int sdpr2SingleSetVolume(int handle, int volume)
{
    Sdpr2Slot *slot;

    // callcheck: sdpr2GetDebug, rxSysPrintf (sdpr2CheckRange, inline)
    SDPR2_TRY(sdpr2CheckRange(handle, SDPR2_SLOT_MAX));
    slot = sdpr2GetSlot(handle);
    if (slot == NULL)
        return -1;
    if (slot->voice[0] == SDPR2_VOICE_FREE)
        return -1;
    SDPR2_TRY(sdpr2SingleSetPan(slot->voice[0], volume, slot->panAngle));
    slot->volume = volume;
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// One voice has no spread: the slot keeps the full spread, whatever is
// asked.
// FS2 0x0016f248

static int sdpr2SingleSetSpread(int handle, int spread)
{
    Sdpr2Slot *slot;

    (void)spread;
    // callcheck: sdpr2GetDebug, rxSysPrintf (sdpr2CheckRange, inline)
    SDPR2_TRY(sdpr2CheckRange(handle, SDPR2_SLOT_MAX));
    slot = sdpr2GetSlot(handle);
    if (slot == NULL)
        return -1;
    if (slot->voice[0] == SDPR2_VOICE_FREE)
        return -1;
    slot->spread = SDPR2_SPREAD_MAX;
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x0016f388

static int sdpr2SingleSetAttr(int handle, int attr)
{
    Sdpr2Slot *slot;

    // callcheck: sdpr2GetDebug, rxSysPrintf (sdpr2CheckRange, inline)
    SDPR2_TRY(sdpr2CheckRange(handle, SDPR2_SLOT_MAX));
    slot = sdpr2GetSlot(handle);
    if (slot == NULL)
        return -1;
    if (slot->voice[0] == SDPR2_VOICE_FREE)
        return -1;
    SDPR2_TRY(sdpr2DrvSetAttr(slot->voice[0], attr));
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// levels[SDPR2_LEVEL_MAX]: the sound's volume on the five speakers.
// FS2 0x0016f508

static int sdpr2SingleGetLevels(int handle, int *levels)
{
    Sdpr2Slot *slot;

    // callcheck: sdpr2GetDebug, rxSysPrintf (sdpr2CheckRange, inline)
    SDPR2_TRY(sdpr2CheckRange(handle, SDPR2_SLOT_MAX));
    slot = sdpr2GetSlot(handle);
    if (slot == NULL)
        return -1;
    if (slot->voice[0] == SDPR2_VOICE_FREE)
        return -1;
    levels[SDPR2_LEVEL_CENTER] = 0;
    levels[SDPR2_LEVEL_LEFT] = 0;
    levels[SDPR2_LEVEL_RIGHT] = 0;
    levels[SDPR2_LEVEL_REAR_LEFT] = 0;
    levels[SDPR2_LEVEL_REAR_RIGHT] = 0;
    sdpr2CalcLevels(slot->volume, slot->levelAngle, SDPR2_SPREAD_MAX,
                    SDPR2_LEVELS_MAIN, levels);
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// The driver's status; 0 for a free slot. The only handle check here
// that prints nothing.
// FS2 0x0016f678

static int sdpr2SingleGetStatus(int handle)
{
    Sdpr2Slot *slot;

    if (handle < 0 || handle >= SDPR2_SLOT_MAX)
        return -1;
    slot = sdpr2GetSlot(handle);
    if (slot == NULL)
        return -1;
    if (slot->voice[0] == SDPR2_VOICE_FREE)
        return 0;
    return sdpr2DrvGetStatus(slot->voice[0]);
}

// ──────────────────────────────────────────────────────────────────────
// Frees the slots whose voice ended (stopping it first when the config
// says so).
// FS2 0x0016f710

static int sdpr2SingleUpdate(void)
{
    Sdpr2Slot *slot;
    int i;

    i = 0;
    do
    {
        slot = sdpr2GetSlot(i);
        if (slot == NULL)
            return -1;
        if (slot->type == &s_type && slot->voice[0] != SDPR2_VOICE_FREE
            && sdpr2SingleGetStatus(i) == 0)
        {
            if (s_config[SDPR2_CONFIG_STOP_ENDED] == 1)
                SDPR2_TRY(sdpr2SingleStop(i, SDPR2_STOP_ENDED_MODE));
            slot->state = 0;
            slot->voice[0] = SDPR2_VOICE_FREE;
            slot->voice[1] = SDPR2_VOICE_FREE;
        }
        i++;
    } while (i < SDPR2_SLOT_MAX);
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// The table (no extensions), the driver's config, then the registry.
// FS2 0x0016f808

int sdpr2SingleInit(void)
{
    s_type.play = sdpr2SinglePlay;
    s_type.playPanEx = sdpr2SinglePlayPanEx;
    s_type.playPan = sdpr2SinglePlayPan;
    s_type.playEx = sdpr2SinglePlayEx;
    s_type.stop = sdpr2SingleStop;
    s_type.stopAll = sdpr2SingleStopAll;
    s_type.pause = sdpr2SinglePause;
    s_type.setPitch = sdpr2SingleSetPitch;
    s_type.setPitchF = sdpr2SingleSetPitchF;
    s_type.setAngle = sdpr2SingleSetAngle;
    s_type.setVolume = sdpr2SingleSetVolume;
    s_type.setSpread = sdpr2SingleSetSpread;
    s_type.setAttr = sdpr2SingleSetAttr;
    s_type.getLevels = sdpr2SingleGetLevels;
    s_type.getStatus = sdpr2SingleGetStatus;
    s_type.update = sdpr2SingleUpdate;
    s_type.ext40 = NULL;
    s_type.ext44 = NULL;
    s_type.ext48 = NULL;
    SDPR2_TRY(sdpr2DrvGetConfig(s_config));
    return sdpr2RegisterType(SDPR2_TYPE_SINGLE, &s_type);
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x0016f960

int sdpr2SingleExit(void)
{
    return sdpr2UnregisterType(SDPR2_TYPE_SINGLE);
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x0016f978

void sdpr2CalcPan(int angle, int *volL, int *volR)
{
    int signL, signR;

    sdpr2CalcPanVolumes(SDPR2_VOLUME_MAX, angle, volL, volR, &signL, &signR);
    *volL = *volL * signL;
    *volR = *volR * signR;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x0016f9e0

int sdpr2CalcPanDeg(int deg, int state, int *volL, int *volR)
{
    int angle, signL, signR;

    angle = sdpr2UnwrapAngle(deg % SDPR2_ANGLE_RANGE, state);
    sdpr2CalcPanVolumes(SDPR2_VOLUME_MAX, angle, volL, volR, &signL, &signR);
    *volL = *volL * signL;
    *volR = *volR * signR;
    return angle;
}

// ──────────────────────────────────────────────────────────────────────
// The tables are read at angle + 90 (0 ahead = cos / sin 45). A negative
// entry is a gain with phase sign -1; SDPR2_GAIN_NEG_ZERO is its zero.
// FS2 0x0016fa78

void sdpr2CalcPanVolumes(int volume, int angle, int *volL, int *volR,
                         int *signL, int *signR)
{
    int index, gainL, gainR;

    index = (angle + 90) % SDPR2_PAN_TABLE_SIZE;
    gainL = sdpr2CosTable[index];
    gainR = sdpr2SinTable[index];
    if (gainL < 0)
    {
        *signL = -1;
        gainL = (gainL == SDPR2_GAIN_NEG_ZERO) ? 0 : -gainL;
    }
    else
        *signL = 1;
    if (gainR < 0)
    {
        gainR = (gainR == SDPR2_GAIN_NEG_ZERO) ? 0 : -gainR;
        *signR = -1;
    }
    else
        *signR = 1;
    *volL = volume * gainL;
    *volR = volume * gainR;
    *volL = *volL / SDPR2_GAIN_ONE;
    *volR = *volR / SDPR2_GAIN_ONE;
}

// ──────────────────────────────────────────────────────────────────────
// prev % 360 against deg: a step of more than half a turn goes the other
// way round, so the result stays next to prev on the two-turn circle.
// FS2 0x0016fb68

int sdpr2UnwrapAngle(int deg, int prev)
{
    int delta;

    delta = deg - prev % SDPR2_ANGLE_RANGE;
    if ((delta < 0 ? -delta : delta) > SDPR2_ANGLE_RANGE / 2)
    {
        if (delta > 0)
            delta -= SDPR2_ANGLE_RANGE;
        else if (delta < 0)
            delta += SDPR2_ANGLE_RANGE;
    }
    return (prev + delta + SDPR2_PAN_TABLE_SIZE) % SDPR2_PAN_TABLE_SIZE;
}

// ──────────────────────────────────────────────────────────────────────
// One sector of sdpr2CalcLevels (unrolled five times in the ROM): t
// (0..180 half degrees, a quarter turn of cos across the sector) shares
// volume between the speaker the sector starts at and the one it ends
// at, each scaled by gain.

static inline int sdpr2LevelGain(int t, int volume, int gain)
{
    int level;

    level = sdpr2CosData[t];
    if (level == SDPR2_GAIN_NEG_ZERO)
        level = 0;
    return level * volume / SDPR2_GAIN_ONE * gain / SDPR2_GAIN_ONE;
}

static inline void sdpr2LevelSector(int t, int volume, int gain, int *levels,
                                    int from, int to)
{
    levels[from] = sdpr2LevelGain(t, volume, gain);
    levels[to] = sdpr2LevelGain(180 - t, volume, gain);
}

// ──────────────────────────────────────────────────────────────────────
// The sectors between the speakers: 0..30 centre-right, 30..120 right-
// rear right, 120..240 the rears, 240..330 rear left-left, 330..360
// left-centre. deg out of 0..359 writes nothing (the ROM compares it
// unsigned). A which other than the two leaves gain unset in the ROM;
// no caller passes one: 0 here.
// FS2 0x0016fbd0

void sdpr2CalcLevels(int volume, int deg, int spread, int which,
                     int *levels)
{
    unsigned int d;
    int gain = 0;

    if (which == SDPR2_LEVELS_MAIN)
        gain = sdpr2SpreadMainTable[spread];
    else if (which == SDPR2_LEVELS_BACK)
    {
        deg = (deg + SDPR2_ANGLE_RANGE / 2) % SDPR2_ANGLE_RANGE;
        gain = sdpr2SpreadBackTable[spread];
    }
    d = (unsigned int)deg;
    if (d < 30)
        sdpr2LevelSector(deg * 180 / 30, volume, gain, levels,
                         SDPR2_LEVEL_CENTER, SDPR2_LEVEL_RIGHT);
    else if (d - 30 < 90)
        sdpr2LevelSector((deg * 180 - 30 * 180) / 90, volume, gain, levels,
                         SDPR2_LEVEL_RIGHT, SDPR2_LEVEL_REAR_RIGHT);
    else if (d - 120 < 120)
        sdpr2LevelSector((deg * 180 - 120 * 180) / 120, volume, gain, levels,
                         SDPR2_LEVEL_REAR_RIGHT, SDPR2_LEVEL_REAR_LEFT);
    else if (d - 240 < 90)
        sdpr2LevelSector((deg * 180 - 240 * 180) / 90, volume, gain, levels,
                         SDPR2_LEVEL_REAR_LEFT, SDPR2_LEVEL_LEFT);
    else if (d - 330 < 30)
        sdpr2LevelSector((deg * 180 - 330 * 180) / 30, volume, gain, levels,
                         SDPR2_LEVEL_LEFT, SDPR2_LEVEL_CENTER);
}
