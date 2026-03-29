// ===============================================
// Hydraulic hand break for simracing
// Microcontroller: Arduino Pro Micro (5V 16Mhz)
// ADC: ADS1115
// OLED: SSD1351
// Pressure Transducer: Ejoyous 1/8 NPT Stainless 500psi 
// Others: hold momentary button
//
// FINAL VERSION - Pro Micro + SSD1351 + ADS1115 + HID Gamepad
// HID-Project by NicoHood | Button on pin 4 | HOLD toggle
// 
// ===============================================

/* === TODO ===
Redout: smooth, clip, and curve
SimHub: datastream
*/

/*Custom USB VendorID: 0x1209 ProductID: 0xBUHB*/

#undef USB_SERIAL
#define USB_SERIAL "BalduHandbrake-v1"   // This is what shows in joy.cpl / Device Manager
// Override USB strings – put this at the VERY TOP of your .ino file
#undef USB_PRODUCT
#define USB_PRODUCT "Baldu Handbrake"   // This is what shows in joy.cpl / Device Manager
// Optional: also override manufacturer if you want
#undef USB_MANUFACTURER
#define USB_MANUFACTURER "Baldu Handbrake Project"

//#define KEYBOARD_LAYOUT

//#include <HID-Project.h>        //For the HID Joystick
#include <Adafruit_TinyUSB.h>   //HID USB for custom Joystick definition
#include <SPI.h>                //SPI for OLED (ESP32-S3 uses custom pins on the default SPI bus)
#include <ADS1X15.h>            //ADS1115 ADC handler
#include <Adafruit_GFX.h>       //Graphic library needed by Adafruit_SSD1351
#include <Adafruit_SSD1351.h>   //OLED
#include <RotaryEncoder.h>      //For the RotaryEncoder

// === INTEGER-ONLY CURVE CORRECTION (no floats, max speed on Pro Mini) ===
// Add this near the top of your sketch (after includes)
//#include <avr/pgmspace.h>


//=== Hardware ===
// Z-Axis
#define Z_AXIS_MIN_VALUE 0
#define Z_AXIS_MAX_VALUE 4095
#define HOLD_BUTTON_MAX_VALUE 4095

// Hold Button pin
#define HOLD_BUTTON_PIN 4
#define DEFAULT_HOLD_MODE 0

//Rotary Encoder
#define ROTARY_CHANNEL_A_PIN 5
#define ROTARY_CHANNEL_B_PIN 13
#define ROTARY_BUTTON_PIN 3

// SSD1351 SPI pins
#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT 128
#define OLED_CS       10
#define OLED_DC        6
#define OLED_RST       7
#define OLED_SCK      12 //CLK
#define OLED_MOSI     11 //DIN

//MEnu
#define MENU_NAVBAR_BUTTONS 4
#define MENU_NAVBAR_HEIGHT 18
#define MENU_NAVBAR_WIDTH 32


//I2C Definition
#define I2C_SDA_PIN 8
#define I2C_SCL_PIN 9

//ADC Sensor values
#define PSI_MIN 0
#define PSI_MAX 500
#define INPUT_READ_MIN_VALUE 0
#define INPUT_READ_MAX_VALUE 32768
#define INPUT_PRESSURE_FAIL_VALUE 1333
#define INPUT_PRESSURE_MIN_VALUE 2667
#define INPUT_PRESSURE_FULL_VALUE 24000
#define INPUT_PRESSURE_OVER_VALUE 25707
#define INPUT_PRESSURE_MAX_VALUE 32767


//=== Behavior ===
#define TARGET_SAMPLE_RATE  1000     // matches USB maximum polling rate
#define DEBOUNCE_DELAY        50
#define DISPLAY_REFRESH      250
//#define TELEMETRY_REFRESH  100
#define ERROR_REFRESH        250
#define CURVE_SNAP_VALUE    2252  // 55.0 % of 4095 !!!! Should use a variable!!!

// === EXPLICIT COLOR DEFINES ===
#define BLACK    0x0000
#define WHITE    0xFFFF
#define RED      0xF800
#define GREEN    0x07E0
#define BLUE     0x001F
#define CYAN     0x07FF
#define YELLOW   0xFFE0
#define MAGENTA  0xF81F
#define ORANGE   0xFD20

// USB HID Custom Definition
static const uint8_t handbrakeDescriptor[] = {
  0x05, 0x01,        // USAGE_PAGE (Generic Desktop)          ← global item: set current usage page to 0x01
  0x09, 0x04,        // USAGE (Joystick)                      ← local item: this collection is a Joystick
  0xA1, 0x01,        // COLLECTION (Application)              ← main item: start top-level application collection
    0x85, 0x03,      //   REPORT_ID (3)                       ← global: all reports use ID 3 (multi-report safety)
    // Button part
    0x05, 0x09,      //   USAGE_PAGE (Button)
    0x19, 0x01,      //   USAGE_MINIMUM (Button 1)
    0x29, 0x01,      //   USAGE_MAXIMUM (Button 1)
    0x15, 0x00,      //   LOGICAL_MINIMUM (0)
    0x25, 0x01,      //   LOGICAL_MAXIMUM (1)
    0x75, 0x01,      //   REPORT_SIZE (1 bit)
    0x95, 0x01,      //   REPORT_COUNT (1 field → 1 bit total)
    0x81, 0x02,      //   INPUT (Data, Variable, Absolute)    ← actual button data
    0x75, 0x07,      //   REPORT_SIZE (7 bits padding)
    0x95, 0x01,      //   REPORT_COUNT (1)
    0x81, 0x03,      //   INPUT (Constant, Variable, Absolute)← 7 bits filler to reach byte boundary
    // Z axis part
    0x05, 0x01,      //   USAGE_PAGE (Generic Desktop)
    0x09, 0x32,      //   USAGE (Z)
    0x15, 0x00,      //   LOGICAL_MINIMUM (0)
    0x26, 0xFF, 0x0F,//   LOGICAL_MAXIMUM (4096) //0xFF, 0xFF for 65535 / 0x00 0xFF for 64000
    0x75, 0x10,      //   REPORT_SIZE (16 bits)
    0x95, 0x01,      //   REPORT_COUNT (1 field → 16 bits total)
    0x81, 0x02,      //   INPUT (Data, Variable, Absolute)    ← the handbrake value
  0xC0               // END_COLLECTION                            ← close the application collection
};

Adafruit_USBD_HID usb_hid;

// ================================================================
// 1024-ENTRY INTEGER LOOKUP TABLES (if PROGMEM = zero RAM usage)
// Input: 0..4095  →  Output: 0..4095 curved value
// These tables are used to curve correct the input in integer math
// ================================================================

/*=================================================================
Original Python code to generate the LUT curves was this:

import math

def normalize_curve(f, num_points=1024):
    values = [f(j / (num_points - 1.0)) for j in range(num_points)]
    f_min = min(values)
    f_max = max(values)
    if f_max == f_min: return [0] * num_points
    return [max(0, min(4095, round(((v - f_min) / (f_max - f_min)) * 4095))) for v in values]

curves = [
    lambda t: t ** 2.2,                            # Rally Soft
    lambda t: t ** 0.75,                           # Rally Aggressive
    lambda t: t ** 3.5,                            # Wet
    lambda t: 1 / (1 + math.exp(-8 * (t - 0.5)))   # S-Curve
]

print("// 4 optimized LUTs only (1025 entries)")
for i, f in enumerate(curves):
    arr = normalize_curve(f)
    arr.append(arr[-1])                    # duplicate last value
    print(f"const uint16_t curveLUT_{i}[1025] PROGMEM = {{")
    print("  " + ", ".join(map(str, arr)))
    print("};")
    print()

================================================================*/

// 4 optimized LUTs only (1025 entries)
const uint16_t curveLUT_0[1025] = {
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 5, 5, 5, 5, 6, 6, 6, 6, 7, 7, 7, 7, 8, 8, 8, 9, 9, 9, 10, 10, 10, 11, 11, 11, 12, 12, 12, 13, 13, 13, 14, 14, 15, 15, 15, 16, 16, 17, 17, 18, 18, 19, 19, 19, 20, 20, 21, 21, 22, 22, 23, 24, 24, 25, 25, 26, 26, 27, 27, 28, 29, 29, 30, 30, 31, 32, 32, 33, 33, 34, 35, 35, 36, 37, 37, 38, 39, 39, 40, 41, 42, 42, 43, 44, 45, 45, 46, 47, 48, 48, 49, 50, 51, 52, 52, 53, 54, 55, 56, 57, 57, 58, 59, 60, 61, 62, 63, 64, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88, 90, 91, 92, 93, 94, 95, 96, 97, 99, 100, 101, 102, 103, 104, 106, 107, 108, 109, 110, 112, 113, 114, 115, 117, 118, 119, 121, 122, 123, 124, 126, 127, 128, 130, 131, 132, 134, 135, 136, 138, 139, 141, 142, 143, 145, 146, 148, 149, 151, 152, 154, 155, 157, 158, 160, 161, 163, 164, 166, 167, 169, 170, 172, 173, 175, 176, 178, 180, 181, 183, 184, 186, 188, 189, 191, 193, 194, 196, 198, 199, 201, 203, 205, 206, 208, 210, 211, 213, 215, 217, 219, 220, 222, 224, 226, 228, 229, 231, 233, 235, 237, 239, 240, 242, 244, 246, 248, 250, 252, 254, 256, 258, 260, 262, 264, 266, 268, 270, 272, 274, 276, 278, 280, 282, 284, 286, 288, 290, 292, 294, 296, 298, 300, 302, 305, 307, 309, 311, 313, 315, 318, 320, 322, 324, 326, 329, 331, 333, 335, 338, 340, 342, 344, 347, 349, 351, 354, 356, 358, 361, 363, 365, 368, 370, 372, 375, 377, 380, 382, 384, 387, 389, 392, 394, 397, 399, 402, 404, 407, 409, 412, 414, 417, 419, 422, 424, 427, 429, 432, 434, 437, 440, 442, 445, 448, 450, 453, 455, 458, 461, 464, 466, 469, 472, 474, 477, 480, 482, 485, 488, 491, 494, 496, 499, 502, 505, 508, 510, 513, 516, 519, 522, 525, 527, 530, 533, 536, 539, 542, 545, 548, 551, 554, 557, 560, 563, 566, 569, 572, 575, 578, 581, 584, 587, 590, 593, 596, 599, 602, 605, 608, 611, 615, 618, 621, 624, 627, 630, 634, 637, 640, 643, 646, 650, 653, 656, 659, 663, 666, 669, 672, 676, 679, 682, 686, 689, 692, 696, 699, 702, 706, 709, 712, 716, 719, 723, 726, 729, 733, 736, 740, 743, 747, 750, 754, 757, 761, 764, 768, 771, 775, 778, 782, 786, 789, 793, 796, 800, 804, 807, 811, 815, 818, 822, 826, 829, 833, 837, 840, 844, 848, 851, 855, 859, 863, 866, 870, 874, 878, 882, 885, 889, 893, 897, 901, 905, 909, 912, 916, 920, 924, 928, 932, 936, 940, 944, 948, 952, 956, 960, 964, 968, 972, 976, 980, 984, 988, 992, 996, 1000, 1004, 1008, 1012, 1016, 1021, 1025, 1029, 1033, 1037, 1041, 1046, 1050, 1054, 1058, 1062, 1067, 1071, 1075, 1079, 1084, 1088, 1092, 1096, 1101, 1105, 1109, 1114, 1118, 1122, 1127, 1131, 1135, 1140, 1144, 1149, 1153, 1157, 1162, 1166, 1171, 1175, 1180, 1184, 1188, 1193, 1197, 1202, 1207, 1211, 1216, 1220, 1225, 1229, 1234, 1238, 1243, 1248, 1252, 1257, 1261, 1266, 1271, 1275, 1280, 1285, 1289, 1294, 1299, 1304, 1308, 1313, 1318, 1322, 1327, 1332, 1337, 1342, 1346, 1351, 1356, 1361, 1366, 1370, 1375, 1380, 1385, 1390, 1395, 1400, 1405, 1410, 1414, 1419, 1424, 1429, 1434, 1439, 1444, 1449, 1454, 1459, 1464, 1469, 1474, 1479, 1484, 1489, 1495, 1500, 1505, 1510, 1515, 1520, 1525, 1530, 1536, 1541, 1546, 1551, 1556, 1561, 1567, 1572, 1577, 1582, 1588, 1593, 1598, 1603, 1609, 1614, 1619, 1625, 1630, 1635, 1641, 1646, 1651, 1657, 1662, 1667, 1673, 1678, 1684, 1689, 1695, 1700, 1705, 1711, 1716, 1722, 1727, 1733, 1738, 1744, 1749, 1755, 1761, 1766, 1772, 1777, 1783, 1788, 1794, 1800, 1805, 1811, 1817, 1822, 1828, 1834, 1839, 1845, 1851, 1856, 1862, 1868, 1874, 1879, 1885, 1891, 1897, 1902, 1908, 1914, 1920, 1926, 1932, 1937, 1943, 1949, 1955, 1961, 1967, 1973, 1979, 1985, 1990, 1996, 2002, 2008, 2014, 2020, 2026, 2032, 2038, 2044, 2050, 2056, 2062, 2069, 2075, 2081, 2087, 2093, 2099, 2105, 2111, 2117, 2124, 2130, 2136, 2142, 2148, 2154, 2161, 2167, 2173, 2179, 2186, 2192, 2198, 2204, 2211, 2217, 2223, 2230, 2236, 2242, 2249, 2255, 2261, 2268, 2274, 2280, 2287, 2293, 2300, 2306, 2313, 2319, 2325, 2332, 2338, 2345, 2351, 2358, 2364, 2371, 2378, 2384, 2391, 2397, 2404, 2410, 2417, 2424, 2430, 2437, 2444, 2450, 2457, 2463, 2470, 2477, 2484, 2490, 2497, 2504, 2510, 2517, 2524, 2531, 2538, 2544, 2551, 2558, 2565, 2572, 2578, 2585, 2592, 2599, 2606, 2613, 2620, 2627, 2633, 2640, 2647, 2654, 2661, 2668, 2675, 2682, 2689, 2696, 2703, 2710, 2717, 2724, 2731, 2738, 2745, 2753, 2760, 2767, 2774, 2781, 2788, 2795, 2802, 2810, 2817, 2824, 2831, 2838, 2846, 2853, 2860, 2867, 2875, 2882, 2889, 2896, 2904, 2911, 2918, 2926, 2933, 2940, 2948, 2955, 2962, 2970, 2977, 2985, 2992, 2999, 3007, 3014, 3022, 3029, 3037, 3044, 3052, 3059, 3067, 3074, 3082, 3089, 3097, 3104, 3112, 3120, 3127, 3135, 3142, 3150, 3158, 3165, 3173, 3181, 3188, 3196, 3204, 3211, 3219, 3227, 3235, 3242, 3250, 3258, 3266, 3273, 3281, 3289, 3297, 3305, 3313, 3320, 3328, 3336, 3344, 3352, 3360, 3368, 3376, 3384, 3391, 3399, 3407, 3415, 3423, 3431, 3439, 3447, 3455, 3463, 3471, 3480, 3488, 3496, 3504, 3512, 3520, 3528, 3536, 3544, 3552, 3561, 3569, 3577, 3585, 3593, 3602, 3610, 3618, 3626, 3634, 3643, 3651, 3659, 3668, 3676, 3684, 3692, 3701, 3709, 3717, 3726, 3734, 3743, 3751, 3759, 3768, 3776, 3785, 3793, 3802, 3810, 3818, 3827, 3835, 3844, 3852, 3861, 3870, 3878, 3887, 3895, 3904, 3912, 3921, 3930, 3938, 3947, 3955, 3964, 3973, 3981, 3990, 3999, 4007, 4016, 4025, 4034, 4042, 4051, 4060, 4069, 4077, 4086, 4095, 4095
};
const uint16_t curveLUT_1[1025] = {
  0, 23, 38, 52, 64, 76, 87, 97, 108, 118, 127, 137, 146, 155, 164, 173, 181, 190, 198, 206, 214, 222, 230, 238, 245, 253, 261, 268, 276, 283, 290, 297, 305, 312, 319, 326, 333, 340, 346, 353, 360, 367, 373, 380, 387, 393, 400, 406, 413, 419, 426, 432, 438, 445, 451, 457, 463, 470, 476, 482, 488, 494, 500, 506, 512, 518, 524, 530, 536, 542, 548, 554, 560, 565, 571, 577, 583, 588, 594, 600, 606, 611, 617, 623, 628, 634, 639, 645, 650, 656, 661, 667, 672, 678, 683, 689, 694, 700, 705, 711, 716, 721, 727, 732, 737, 743, 748, 753, 758, 764, 769, 774, 779, 785, 790, 795, 800, 805, 811, 816, 821, 826, 831, 836, 841, 846, 851, 856, 861, 867, 872, 877, 882, 887, 892, 897, 902, 907, 911, 916, 921, 926, 931, 936, 941, 946, 951, 956, 961, 965, 970, 975, 980, 985, 990, 994, 999, 1004, 1009, 1014, 1018, 1023, 1028, 1033, 1037, 1042, 1047, 1052, 1056, 1061, 1066, 1071, 1075, 1080, 1085, 1089, 1094, 1099, 1103, 1108, 1113, 1117, 1122, 1126, 1131, 1136, 1140, 1145, 1149, 1154, 1159, 1163, 1168, 1172, 1177, 1181, 1186, 1190, 1195, 1199, 1204, 1208, 1213, 1218, 1222, 1226, 1231, 1235, 1240, 1244, 1249, 1253, 1258, 1262, 1267, 1271, 1276, 1280, 1284, 1289, 1293, 1298, 1302, 1306, 1311, 1315, 1320, 1324, 1328, 1333, 1337, 1341, 1346, 1350, 1354, 1359, 1363, 1367, 1372, 1376, 1380, 1385, 1389, 1393, 1398, 1402, 1406, 1410, 1415, 1419, 1423, 1428, 1432, 1436, 1440, 1445, 1449, 1453, 1457, 1462, 1466, 1470, 1474, 1478, 1483, 1487, 1491, 1495, 1500, 1504, 1508, 1512, 1516, 1520, 1525, 1529, 1533, 1537, 1541, 1545, 1550, 1554, 1558, 1562, 1566, 1570, 1574, 1579, 1583, 1587, 1591, 1595, 1599, 1603, 1607, 1611, 1616, 1620, 1624, 1628, 1632, 1636, 1640, 1644, 1648, 1652, 1656, 1660, 1664, 1668, 1673, 1677, 1681, 1685, 1689, 1693, 1697, 1701, 1705, 1709, 1713, 1717, 1721, 1725, 1729, 1733, 1737, 1741, 1745, 1749, 1753, 1757, 1761, 1765, 1769, 1773, 1777, 1781, 1785, 1789, 1792, 1796, 1800, 1804, 1808, 1812, 1816, 1820, 1824, 1828, 1832, 1836, 1840, 1844, 1848, 1851, 1855, 1859, 1863, 1867, 1871, 1875, 1879, 1883, 1887, 1890, 1894, 1898, 1902, 1906, 1910, 1914, 1918, 1921, 1925, 1929, 1933, 1937, 1941, 1945, 1948, 1952, 1956, 1960, 1964, 1968, 1971, 1975, 1979, 1983, 1987, 1991, 1994, 1998, 2002, 2006, 2010, 2013, 2017, 2021, 2025, 2029, 2032, 2036, 2040, 2044, 2048, 2051, 2055, 2059, 2063, 2066, 2070, 2074, 2078, 2082, 2085, 2089, 2093, 2097, 2100, 2104, 2108, 2112, 2115, 2119, 2123, 2127, 2130, 2134, 2138, 2141, 2145, 2149, 2153, 2156, 2160, 2164, 2167, 2171, 2175, 2179, 2182, 2186, 2190, 2193, 2197, 2201, 2204, 2208, 2212, 2216, 2219, 2223, 2227, 2230, 2234, 2238, 2241, 2245, 2249, 2252, 2256, 2260, 2263, 2267, 2271, 2274, 2278, 2282, 2285, 2289, 2292, 2296, 2300, 2303, 2307, 2311, 2314, 2318, 2322, 2325, 2329, 2332, 2336, 2340, 2343, 2347, 2351, 2354, 2358, 2361, 2365, 2369, 2372, 2376, 2379, 2383, 2387, 2390, 2394, 2397, 2401, 2404, 2408, 2412, 2415, 2419, 2422, 2426, 2430, 2433, 2437, 2440, 2444, 2447, 2451, 2455, 2458, 2462, 2465, 2469, 2472, 2476, 2479, 2483, 2486, 2490, 2494, 2497, 2501, 2504, 2508, 2511, 2515, 2518, 2522, 2525, 2529, 2532, 2536, 2539, 2543, 2547, 2550, 2554, 2557, 2561, 2564, 2568, 2571, 2575, 2578, 2582, 2585, 2589, 2592, 2596, 2599, 2603, 2606, 2610, 2613, 2617, 2620, 2624, 2627, 2630, 2634, 2637, 2641, 2644, 2648, 2651, 2655, 2658, 2662, 2665, 2669, 2672, 2676, 2679, 2682, 2686, 2689, 2693, 2696, 2700, 2703, 2707, 2710, 2714, 2717, 2720, 2724, 2727, 2731, 2734, 2738, 2741, 2744, 2748, 2751, 2755, 2758, 2762, 2765, 2768, 2772, 2775, 2779, 2782, 2786, 2789, 2792, 2796, 2799, 2803, 2806, 2809, 2813, 2816, 2820, 2823, 2826, 2830, 2833, 2837, 2840, 2843, 2847, 2850, 2854, 2857, 2860, 2864, 2867, 2870, 2874, 2877, 2881, 2884, 2887, 2891, 2894, 2897, 2901, 2904, 2908, 2911, 2914, 2918, 2921, 2924, 2928, 2931, 2934, 2938, 2941, 2944, 2948, 2951, 2955, 2958, 2961, 2965, 2968, 2971, 2975, 2978, 2981, 2985, 2988, 2991, 2995, 2998, 3001, 3005, 3008, 3011, 3015, 3018, 3021, 3025, 3028, 3031, 3035, 3038, 3041, 3044, 3048, 3051, 3054, 3058, 3061, 3064, 3068, 3071, 3074, 3078, 3081, 3084, 3087, 3091, 3094, 3097, 3101, 3104, 3107, 3111, 3114, 3117, 3120, 3124, 3127, 3130, 3134, 3137, 3140, 3143, 3147, 3150, 3153, 3156, 3160, 3163, 3166, 3170, 3173, 3176, 3179, 3183, 3186, 3189, 3192, 3196, 3199, 3202, 3205, 3209, 3212, 3215, 3218, 3222, 3225, 3228, 3231, 3235, 3238, 3241, 3244, 3248, 3251, 3254, 3257, 3261, 3264, 3267, 3270, 3274, 3277, 3280, 3283, 3287, 3290, 3293, 3296, 3299, 3303, 3306, 3309, 3312, 3316, 3319, 3322, 3325, 3328, 3332, 3335, 3338, 3341, 3345, 3348, 3351, 3354, 3357, 3361, 3364, 3367, 3370, 3373, 3377, 3380, 3383, 3386, 3389, 3393, 3396, 3399, 3402, 3405, 3409, 3412, 3415, 3418, 3421, 3425, 3428, 3431, 3434, 3437, 3440, 3444, 3447, 3450, 3453, 3456, 3460, 3463, 3466, 3469, 3472, 3475, 3479, 3482, 3485, 3488, 3491, 3494, 3498, 3501, 3504, 3507, 3510, 3513, 3517, 3520, 3523, 3526, 3529, 3532, 3535, 3539, 3542, 3545, 3548, 3551, 3554, 3557, 3561, 3564, 3567, 3570, 3573, 3576, 3579, 3583, 3586, 3589, 3592, 3595, 3598, 3601, 3605, 3608, 3611, 3614, 3617, 3620, 3623, 3626, 3630, 3633, 3636, 3639, 3642, 3645, 3648, 3651, 3655, 3658, 3661, 3664, 3667, 3670, 3673, 3676, 3680, 3683, 3686, 3689, 3692, 3695, 3698, 3701, 3704, 3707, 3711, 3714, 3717, 3720, 3723, 3726, 3729, 3732, 3735, 3738, 3742, 3745, 3748, 3751, 3754, 3757, 3760, 3763, 3766, 3769, 3772, 3776, 3779, 3782, 3785, 3788, 3791, 3794, 3797, 3800, 3803, 3806, 3809, 3812, 3816, 3819, 3822, 3825, 3828, 3831, 3834, 3837, 3840, 3843, 3846, 3849, 3852, 3855, 3859, 3862, 3865, 3868, 3871, 3874, 3877, 3880, 3883, 3886, 3889, 3892, 3895, 3898, 3901, 3904, 3907, 3910, 3914, 3917, 3920, 3923, 3926, 3929, 3932, 3935, 3938, 3941, 3944, 3947, 3950, 3953, 3956, 3959, 3962, 3965, 3968, 3971, 3974, 3977, 3980, 3983, 3986, 3989, 3992, 3996, 3999, 4002, 4005, 4008, 4011, 4014, 4017, 4020, 4023, 4026, 4029, 4032, 4035, 4038, 4041, 4044, 4047, 4050, 4053, 4056, 4059, 4062, 4065, 4068, 4071, 4074, 4077, 4080, 4083, 4086, 4089, 4092, 4095, 4095
};
const uint16_t curveLUT_2[1025] = {
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 5, 5, 5, 5, 5, 5, 5, 5, 6, 6, 6, 6, 6, 6, 6, 6, 7, 7, 7, 7, 7, 7, 8, 8, 8, 8, 8, 8, 8, 9, 9, 9, 9, 9, 10, 10, 10, 10, 10, 10, 11, 11, 11, 11, 12, 12, 12, 12, 12, 13, 13, 13, 13, 14, 14, 14, 14, 15, 15, 15, 15, 16, 16, 16, 16, 17, 17, 17, 17, 18, 18, 18, 19, 19, 19, 19, 20, 20, 20, 21, 21, 21, 22, 22, 22, 23, 23, 23, 24, 24, 25, 25, 25, 26, 26, 26, 27, 27, 28, 28, 28, 29, 29, 30, 30, 30, 31, 31, 32, 32, 33, 33, 33, 34, 34, 35, 35, 36, 36, 37, 37, 38, 38, 39, 39, 40, 40, 41, 41, 42, 42, 43, 43, 44, 44, 45, 46, 46, 47, 47, 48, 48, 49, 50, 50, 51, 51, 52, 53, 53, 54, 55, 55, 56, 57, 57, 58, 59, 59, 60, 61, 61, 62, 63, 63, 64, 65, 66, 66, 67, 68, 69, 69, 70, 71, 72, 72, 73, 74, 75, 76, 76, 77, 78, 79, 80, 81, 81, 82, 83, 84, 85, 86, 87, 88, 88, 89, 90, 91, 92, 93, 94, 95, 96, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 117, 118, 119, 120, 121, 122, 123, 124, 126, 127, 128, 129, 130, 131, 133, 134, 135, 136, 138, 139, 140, 141, 143, 144, 145, 146, 148, 149, 150, 152, 153, 154, 156, 157, 158, 160, 161, 163, 164, 165, 167, 168, 170, 171, 173, 174, 176, 177, 179, 180, 182, 183, 185, 186, 188, 189, 191, 192, 194, 196, 197, 199, 200, 202, 204, 205, 207, 209, 210, 212, 214, 215, 217, 219, 221, 222, 224, 226, 228, 229, 231, 233, 235, 237, 238, 240, 242, 244, 246, 248, 250, 252, 253, 255, 257, 259, 261, 263, 265, 267, 269, 271, 273, 275, 277, 279, 281, 283, 286, 288, 290, 292, 294, 296, 298, 300, 303, 305, 307, 309, 311, 314, 316, 318, 320, 323, 325, 327, 330, 332, 334, 337, 339, 341, 344, 346, 349, 351, 353, 356, 358, 361, 363, 366, 368, 371, 373, 376, 378, 381, 383, 386, 389, 391, 394, 397, 399, 402, 404, 407, 410, 413, 415, 418, 421, 424, 426, 429, 432, 435, 438, 440, 443, 446, 449, 452, 455, 458, 461, 464, 467, 470, 473, 476, 479, 482, 485, 488, 491, 494, 497, 500, 503, 506, 510, 513, 516, 519, 522, 526, 529, 532, 535, 539, 542, 545, 548, 552, 555, 559, 562, 565, 569, 572, 576, 579, 583, 586, 590, 593, 597, 600, 604, 607, 611, 614, 618, 622, 625, 629, 633, 636, 640, 644, 648, 651, 655, 659, 663, 667, 670, 674, 678, 682, 686, 690, 694, 698, 702, 706, 710, 714, 718, 722, 726, 730, 734, 738, 742, 746, 751, 755, 759, 763, 767, 772, 776, 780, 784, 789, 793, 797, 802, 806, 811, 815, 819, 824, 828, 833, 837, 842, 846, 851, 855, 860, 865, 869, 874, 879, 883, 888, 893, 897, 902, 907, 912, 916, 921, 926, 931, 936, 941, 946, 951, 956, 961, 965, 970, 976, 981, 986, 991, 996, 1001, 1006, 1011, 1016, 1022, 1027, 1032, 1037, 1042, 1048, 1053, 1058, 1064, 1069, 1074, 1080, 1085, 1091, 1096, 1102, 1107, 1113, 1118, 1124, 1129, 1135, 1140, 1146, 1152, 1157, 1163, 1169, 1175, 1180, 1186, 1192, 1198, 1204, 1209, 1215, 1221, 1227, 1233, 1239, 1245, 1251, 1257, 1263, 1269, 1275, 1281, 1287, 1293, 1300, 1306, 1312, 1318, 1325, 1331, 1337, 1343, 1350, 1356, 1362, 1369, 1375, 1382, 1388, 1395, 1401, 1408, 1414, 1421, 1427, 1434, 1441, 1447, 1454, 1461, 1467, 1474, 1481, 1488, 1494, 1501, 1508, 1515, 1522, 1529, 1536, 1543, 1550, 1557, 1564, 1571, 1578, 1585, 1592, 1599, 1606, 1614, 1621, 1628, 1635, 1643, 1650, 1657, 1665, 1672, 1679, 1687, 1694, 1702, 1709, 1717, 1724, 1732, 1739, 1747, 1755, 1762, 1770, 1778, 1785, 1793, 1801, 1809, 1817, 1824, 1832, 1840, 1848, 1856, 1864, 1872, 1880, 1888, 1896, 1904, 1912, 1921, 1929, 1937, 1945, 1953, 1962, 1970, 1978, 1987, 1995, 2003, 2012, 2020, 2029, 2037, 2046, 2054, 2063, 2071, 2080, 2089, 2097, 2106, 2115, 2124, 2132, 2141, 2150, 2159, 2168, 2177, 2186, 2195, 2204, 2213, 2222, 2231, 2240, 2249, 2258, 2267, 2276, 2286, 2295, 2304, 2313, 2323, 2332, 2342, 2351, 2360, 2370, 2379, 2389, 2398, 2408, 2418, 2427, 2437, 2447, 2456, 2466, 2476, 2486, 2495, 2505, 2515, 2525, 2535, 2545, 2555, 2565, 2575, 2585, 2595, 2605, 2615, 2626, 2636, 2646, 2656, 2667, 2677, 2687, 2698, 2708, 2719, 2729, 2739, 2750, 2761, 2771, 2782, 2792, 2803, 2814, 2825, 2835, 2846, 2857, 2868, 2879, 2890, 2900, 2911, 2922, 2933, 2945, 2956, 2967, 2978, 2989, 3000, 3011, 3023, 3034, 3045, 3057, 3068, 3079, 3091, 3102, 3114, 3125, 3137, 3149, 3160, 3172, 3184, 3195, 3207, 3219, 3231, 3243, 3254, 3266, 3278, 3290, 3302, 3314, 3326, 3338, 3350, 3363, 3375, 3387, 3399, 3412, 3424, 3436, 3449, 3461, 3473, 3486, 3498, 3511, 3524, 3536, 3549, 3561, 3574, 3587, 3600, 3612, 3625, 3638, 3651, 3664, 3677, 3690, 3703, 3716, 3729, 3742, 3755, 3768, 3782, 3795, 3808, 3822, 3835, 3848, 3862, 3875, 3889, 3902, 3916, 3929, 3943, 3957, 3970, 3984, 3998, 4012, 4025, 4039, 4053, 4067, 4081, 4095, 4095
};
const uint16_t curveLUT_3[1025] = {
  0, 1, 1, 2, 2, 3, 4, 4, 5, 5, 6, 7, 7, 8, 9, 9, 10, 11, 11, 12, 13, 13, 14, 15, 15, 16, 17, 18, 18, 19, 20, 20, 21, 22, 23, 23, 24, 25, 26, 27, 27, 28, 29, 30, 31, 31, 32, 33, 34, 35, 36, 36, 37, 38, 39, 40, 41, 42, 43, 44, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 65, 66, 67, 69, 70, 71, 72, 73, 74, 75, 76, 78, 79, 80, 81, 82, 83, 85, 86, 87, 88, 90, 91, 92, 93, 95, 96, 97, 99, 100, 101, 103, 104, 105, 107, 108, 109, 111, 112, 114, 115, 116, 118, 119, 121, 122, 124, 125, 127, 128, 130, 131, 133, 134, 136, 138, 139, 141, 142, 144, 146, 147, 149, 151, 152, 154, 156, 157, 159, 161, 163, 165, 166, 168, 170, 172, 174, 175, 177, 179, 181, 183, 185, 187, 189, 191, 193, 195, 197, 199, 201, 203, 205, 207, 209, 211, 213, 215, 217, 219, 222, 224, 226, 228, 230, 233, 235, 237, 239, 242, 244, 246, 249, 251, 253, 256, 258, 261, 263, 265, 268, 270, 273, 275, 278, 280, 283, 286, 288, 291, 293, 296, 299, 301, 304, 307, 310, 312, 315, 318, 321, 324, 326, 329, 332, 335, 338, 341, 344, 347, 350, 353, 356, 359, 362, 365, 368, 371, 374, 378, 381, 384, 387, 390, 394, 397, 400, 404, 407, 410, 414, 417, 420, 424, 427, 431, 434, 438, 441, 445, 449, 452, 456, 459, 463, 467, 471, 474, 478, 482, 486, 489, 493, 497, 501, 505, 509, 513, 517, 521, 525, 529, 533, 537, 541, 545, 549, 554, 558, 562, 566, 571, 575, 579, 584, 588, 592, 597, 601, 606, 610, 615, 619, 624, 628, 633, 638, 642, 647, 652, 656, 661, 666, 671, 676, 680, 685, 690, 695, 700, 705, 710, 715, 720, 725, 730, 735, 741, 746, 751, 756, 761, 767, 772, 777, 783, 788, 793, 799, 804, 810, 815, 821, 826, 832, 837, 843, 849, 854, 860, 866, 872, 877, 883, 889, 895, 901, 907, 912, 918, 924, 930, 936, 942, 949, 955, 961, 967, 973, 979, 985, 992, 998, 1004, 1011, 1017, 1023, 1030, 1036, 1042, 1049, 1055, 1062, 1068, 1075, 1082, 1088, 1095, 1101, 1108, 1115, 1122, 1128, 1135, 1142, 1149, 1155, 1162, 1169, 1176, 1183, 1190, 1197, 1204, 1211, 1218, 1225, 1232, 1239, 1246, 1253, 1261, 1268, 1275, 1282, 1289, 1297, 1304, 1311, 1318, 1326, 1333, 1341, 1348, 1355, 1363, 1370, 1378, 1385, 1393, 1400, 1408, 1415, 1423, 1430, 1438, 1446, 1453, 1461, 1469, 1476, 1484, 1492, 1500, 1507, 1515, 1523, 1531, 1539, 1546, 1554, 1562, 1570, 1578, 1586, 1594, 1602, 1610, 1618, 1626, 1633, 1641, 1649, 1658, 1666, 1674, 1682, 1690, 1698, 1706, 1714, 1722, 1730, 1738, 1746, 1755, 1763, 1771, 1779, 1787, 1795, 1804, 1812, 1820, 1828, 1836, 1845, 1853, 1861, 1869, 1878, 1886, 1894, 1902, 1911, 1919, 1927, 1935, 1944, 1952, 1960, 1969, 1977, 1985, 1994, 2002, 2010, 2018, 2027, 2035, 2043, 2052, 2060, 2068, 2077, 2085, 2093, 2101, 2110, 2118, 2126, 2135, 2143, 2151, 2160, 2168, 2176, 2184, 2193, 2201, 2209, 2217, 2226, 2234, 2242, 2250, 2259, 2267, 2275, 2283, 2291, 2300, 2308, 2316, 2324, 2332, 2340, 2349, 2357, 2365, 2373, 2381, 2389, 2397, 2405, 2413, 2421, 2429, 2437, 2446, 2454, 2462, 2469, 2477, 2485, 2493, 2501, 2509, 2517, 2525, 2533, 2541, 2549, 2556, 2564, 2572, 2580, 2588, 2595, 2603, 2611, 2619, 2626, 2634, 2642, 2649, 2657, 2665, 2672, 2680, 2687, 2695, 2702, 2710, 2717, 2725, 2732, 2740, 2747, 2754, 2762, 2769, 2777, 2784, 2791, 2798, 2806, 2813, 2820, 2827, 2834, 2842, 2849, 2856, 2863, 2870, 2877, 2884, 2891, 2898, 2905, 2912, 2919, 2926, 2933, 2940, 2946, 2953, 2960, 2967, 2973, 2980, 2987, 2994, 3000, 3007, 3013, 3020, 3027, 3033, 3040, 3046, 3053, 3059, 3065, 3072, 3078, 3084, 3091, 3097, 3103, 3110, 3116, 3122, 3128, 3134, 3140, 3146, 3153, 3159, 3165, 3171, 3177, 3183, 3188, 3194, 3200, 3206, 3212, 3218, 3223, 3229, 3235, 3241, 3246, 3252, 3258, 3263, 3269, 3274, 3280, 3285, 3291, 3296, 3302, 3307, 3312, 3318, 3323, 3328, 3334, 3339, 3344, 3349, 3354, 3360, 3365, 3370, 3375, 3380, 3385, 3390, 3395, 3400, 3405, 3410, 3415, 3419, 3424, 3429, 3434, 3439, 3443, 3448, 3453, 3457, 3462, 3467, 3471, 3476, 3480, 3485, 3489, 3494, 3498, 3503, 3507, 3511, 3516, 3520, 3524, 3529, 3533, 3537, 3541, 3546, 3550, 3554, 3558, 3562, 3566, 3570, 3574, 3578, 3582, 3586, 3590, 3594, 3598, 3602, 3606, 3609, 3613, 3617, 3621, 3624, 3628, 3632, 3636, 3639, 3643, 3646, 3650, 3654, 3657, 3661, 3664, 3668, 3671, 3675, 3678, 3681, 3685, 3688, 3691, 3695, 3698, 3701, 3705, 3708, 3711, 3714, 3717, 3721, 3724, 3727, 3730, 3733, 3736, 3739, 3742, 3745, 3748, 3751, 3754, 3757, 3760, 3763, 3766, 3769, 3771, 3774, 3777, 3780, 3783, 3785, 3788, 3791, 3794, 3796, 3799, 3802, 3804, 3807, 3809, 3812, 3815, 3817, 3820, 3822, 3825, 3827, 3830, 3832, 3834, 3837, 3839, 3842, 3844, 3846, 3849, 3851, 3853, 3856, 3858, 3860, 3862, 3865, 3867, 3869, 3871, 3873, 3876, 3878, 3880, 3882, 3884, 3886, 3888, 3890, 3892, 3894, 3896, 3898, 3900, 3902, 3904, 3906, 3908, 3910, 3912, 3914, 3916, 3918, 3920, 3921, 3923, 3925, 3927, 3929, 3930, 3932, 3934, 3936, 3938, 3939, 3941, 3943, 3944, 3946, 3948, 3949, 3951, 3953, 3954, 3956, 3957, 3959, 3961, 3962, 3964, 3965, 3967, 3968, 3970, 3971, 3973, 3974, 3976, 3977, 3979, 3980, 3981, 3983, 3984, 3986, 3987, 3988, 3990, 3991, 3992, 3994, 3995, 3996, 3998, 3999, 4000, 4002, 4003, 4004, 4005, 4007, 4008, 4009, 4010, 4012, 4013, 4014, 4015, 4016, 4017, 4019, 4020, 4021, 4022, 4023, 4024, 4025, 4026, 4028, 4029, 4030, 4031, 4032, 4033, 4034, 4035, 4036, 4037, 4038, 4039, 4040, 4041, 4042, 4043, 4044, 4045, 4046, 4047, 4048, 4049, 4050, 4051, 4051, 4052, 4053, 4054, 4055, 4056, 4057, 4058, 4059, 4059, 4060, 4061, 4062, 4063, 4064, 4064, 4065, 4066, 4067, 4068, 4068, 4069, 4070, 4071, 4072, 4072, 4073, 4074, 4075, 4075, 4076, 4077, 4077, 4078, 4079, 4080, 4080, 4081, 4082, 4082, 4083, 4084, 4084, 4085, 4086, 4086, 4087, 4088, 4088, 4089, 4090, 4090, 4091, 4091, 4092, 4093, 4093, 4094, 4094, 4095, 4095
};


// ADS1115
//Adafruit_ADS1115 ads;
ADS1115 ads(0x48);

uint16_t global_psiMin = PSI_MIN;      // PSI value at 0.5 V (global_adcMin)
uint16_t global_psiMax = PSI_MAX;    // PSI value at 4.5 V (global_adcMax)
uint16_t global_adcMin = INPUT_PRESSURE_MIN_VALUE;   // your 0.5 V raw value
uint16_t global_adcMax = INPUT_PRESSURE_FULL_VALUE;   // your 4.5 V raw value
uint16_t global_adcSpan = global_adcMax - global_adcMin; //Effective pressure sensitive range for calculating pressure.
uint16_t deadLowPercent = 50; //Measured in decipercents (0.1%), defaults to 0.5% 
uint16_t deadHighPercent = 50; //Measured in decipercents (0.1%), defaults to 0.5% 
uint16_t global_effectiveMin = global_adcMin + (uint32_t)global_adcSpan * (uint32_t)deadLowPercent / 1000UL; //Effective ADC read value for zero z-axis.
uint16_t global_effectiveMax = global_adcMax - (uint32_t)global_adcSpan  * (uint32_t)deadHighPercent / 1000UL; //Effective ADC read value for max z-axis.
uint16_t global_effectiveSpan = global_effectiveMax - global_effectiveMin; //Effective span of ADC read values that move the z-axis.

uint16_t snapPercent = CURVE_SNAP_VALUE;

uint16_t targetSampleRate = TARGET_SAMPLE_RATE;
uint16_t displayRefresh = DISPLAY_REFRESH;

// ====================== EDIT SCREEN METADATA (data-driven) ======================
/* typedef struct {
    const char* title;           // standardized header text
    uint8_t     numParams;       // 0–6 (how many values we edit/backup)
    uint16_t*   paramPtrs[6];    // pointers to the live globals
    uint8_t     numButtons;      // number of adjustment buttons (S/X always at 2/3)
    void (*drawFunc)(void);      // pointer to the custom drawing function for this screen
} EditScreen; */

typedef enum {
  PARAM_BOOL,
  PARAM_UINT16,
  PARAM_UINT32
  // add PARAM_UINT8, PARAM_INT16 etc. later if needed
} ParamType;

typedef struct {
    const char* title;           // standardized header text
    uint8_t     numParams;       // 0–6
    void*       paramPtrs[6];    // now void* so it works for bool/uint16/etc.
    uint8_t     paramTypes[6];   // PARAM_UINT16 or PARAM_BOOL
    uint8_t     numButtons;      // number of adjustment buttons (S/X always at 2/3)
    void (*drawFunc)(void);      // pointer to the custom drawing function
} EditScreen;

// ====================== BACKUP BUFFER (shared by ALL screens) ======================
static uint16_t backupBuffer[6];              // enough for any screen (max 6 params)
static uint8_t  currentEditScreenIndex = 0;   // which row in the table we are editing

static void drawHoldParameterScreen(void);
static void drawDeadzonesScreen(void);
static void drawSnapThresholdScreen(void);
static void drawButtonDebounceScreen(void);
static void drawDefaultCurveScreen(void);
static void drawRefeshRatesScreen(void);
static void drawRecalibrateScreen(void);
static void drawSaveAndLoadScreen(void);
//void bootScreenDisplay(void);
//void setupLiveDisplay(void);
//void updateLiveDisplay(void);
//void drawEditScreen(void);
//static void enterEditScreen(uint8_t screenIndex);



//SSD13521
Adafruit_SSD1351 tft = Adafruit_SSD1351(SCREEN_WIDTH, SCREEN_HEIGHT, &SPI, OLED_CS, OLED_DC, OLED_RST);

// Hold Button
//const int holdButtonPin = HOLD_BUTTON_PIN;
uint16_t debounceDelay = DEBOUNCE_DELAY;
bool                holdMode = false;
bool                firmwareHoldMode = DEFAULT_HOLD_MODE;

// === Rotary Encoder (EC11) ===
RotaryEncoder encoder(ROTARY_CHANNEL_A_PIN, ROTARY_CHANNEL_B_PIN, RotaryEncoder::LatchMode::FOUR3);  // CLK=5, DT=7, standard EC11 mode
//const uint8_t encoderButtonPin = ROTARY_BUTTON_PIN;

// Curves Name values
const char* const correctionCurvesNames[] = {
  "Lineal",
  "Rally Soft",
  "Rally Aggr",
  "Drift Snap",
  "Wet",
  "S-Curve"
};
// How many names are in the list (super useful)
const uint8_t numberOfCurves = sizeof(correctionCurvesNames) / sizeof(correctionCurvesNames[0]);
uint16_t currentCurve = 0;   // Choose your curve here (0-5) — you can make this a variable later
int16_t lastEncoderPosition = currentCurve;

// ====================== SCREEN TABLE (flash only) ======================
/* const EditScreen editScreens[] = {
    // title                numParams   paramPtrs[0]               paramPtrs[1]   ...                 numButtons   drawFunc
    //{ "Hold Mode",          1, { &firmwareHoldMode, NULL, NULL, NULL, NULL, NULL },                             2, &drawHoldParameterScreen },
    { "Hold Mode",          1, { NULL, NULL, NULL, NULL, NULL, NULL },                                          2, &drawHoldParameterScreen },
    { "Deadzones",          2, { &deadLowPercent, &deadHighPercent, NULL, NULL, NULL, NULL },                   4, &drawDeadzonesScreen },
    { "Snap Threshold",     1, { &snapPercent, NULL, NULL, NULL, NULL, NULL },                                  4, &drawSnapThresholdScreen },
    //{ "Button Debounce",    1, { &debounceDelay, NULL, NULL, NULL, NULL, NULL },                                4, &drawButtonDebounceScreen },
    { "Button Debounce",    1, { NULL, NULL, NULL, NULL, NULL, NULL },                                          4, &drawButtonDebounceScreen },
    //{ "Default Curve",      1, { &currentCurve, NULL, NULL, NULL, NULL, NULL },                                 2, &drawDefaultCurveScreen },
    { "Default Curve",      1, { NULL, NULL, NULL, NULL, NULL, NULL },                                          2, &drawDefaultCurveScreen },
    { "Refresh Rate",       2, { &targetSampleRate, &displayRefresh, NULL, NULL, NULL, NULL },                  4, &drawRefeshRatesScreen },
    //{ "Recalibrate",        2, { &global_psiMin, &global_psiMax, &global_adcMin, &global_adcMax, NULL, NULL }, 4, &drawRecalibrateScreen },
    { "Recalibrate",        2, { &global_psiMin, &global_psiMax, &global_adcMin, &global_adcMax, NULL, NULL },  4, &drawRecalibrateScreen },
    { "Save & Load",        2, { NULL, NULL, NULL, NULL, NULL, NULL },                                          2, &drawSaveAndLoadScreen },

    // TODO: add the other 7 screens here (Button Debounce, Firmware Hold, Sample Rate, etc.)
}; */

const EditScreen editScreens[] = {
    // title               numParams   paramPtrs[0]                     paramTypes[0]     ...   numButtons   drawFunc
    { "Hold Mode",         1, { &firmwareHoldMode, NULL, NULL, NULL, NULL, NULL }, { PARAM_BOOL, 0, 0, 0, 0, 0 }, 2, &drawHoldParameterScreen },
//    { "LIVE Mode",         2, { &currentLIVEMode, NULL, NULL, NULL, NULL, NULL }, { PARAM_UINT16, 0, 0, 0, 0, 0 }, 4, &drawLIVEModeScreen }, //Need to implement LIVE modes first.
    { "Deadzones",         2, { &deadLowPercent, &deadHighPercent, NULL, NULL, NULL, NULL }, { PARAM_UINT16, PARAM_UINT16, 0, 0, 0, 0 }, 4, &drawDeadzonesScreen },
    { "Default Curve",     1, { &currentCurve, NULL, NULL, NULL, NULL, NULL }, { PARAM_UINT16, 0, 0, 0, 0, 0 }, 2, &drawDefaultCurveScreen },
    { "Snap Threshold",    1, { &snapPercent, NULL, NULL, NULL, NULL, NULL }, { PARAM_UINT16, 0, 0, 0, 0, 0 }, 4, &drawSnapThresholdScreen },
    { "Button Debounce",   1, { &debounceDelay, NULL, NULL, NULL, NULL, NULL }, { PARAM_UINT16, 0, 0, 0, 0, 0 }, 4, &drawButtonDebounceScreen },
    { "Refresh Rate",      2, { &targetSampleRate, &displayRefresh, NULL, NULL, NULL, NULL }, { PARAM_UINT16, PARAM_UINT16, 0, 0, 0, 0 }, 4, &drawRefeshRatesScreen },
    { "Recalibrate",       2, { &global_psiMin, &global_psiMax, &global_adcMin, &global_adcMax, NULL, NULL }, { PARAM_UINT16, PARAM_UINT16, PARAM_UINT16, PARAM_UINT16, 0, 0 }, 4, &drawRecalibrateScreen },
    { "Save & Load",       0, { NULL, NULL, NULL, NULL, NULL, NULL }, { 0, 0, 0, 0, 0, 0 }, 2, &drawSaveAndLoadScreen },
};

//#define NUM_EDIT_SCREENS (sizeof(editScreens) / sizeof(editScreens[0]))
uint16_t numOfParameterEditScreens = (sizeof(editScreens) / sizeof(editScreens[0]));

// === MENU STATE MACHINE ===
enum DisplayState {
  LIVE,         // quick curve switching
  MENU_LIST,    // vertical circular list + bottom Prev/Edit/Next bar
  EDIT_VALUE    // your sketched screen with 4 arrows
};

// ====================== NAV BAR CONFIG ======================
const int16_t RECT_W = 32;
const int16_t RECT_H = 18;   // ← change to 16 if you prefer tighter fit (textSize(2) = 16 px)

// Background colors (only the rectangle itself)
const uint16_t NORMAL_BG   = 0x08A5;
const uint16_t SELECTED_BG = 0x03DF;

// Foreground colors (arrows or letters)
const uint16_t NORMAL_FG   = 0x1B39;
const uint16_t SELECTED_FG = 0xFFFF;

// Four box top-left positions — TWEAK THESE TO YOUR LAYOUT
// (example: single row at bottom, spaced nicely on 128 px wide screen)
const int16_t navBoxX[4] = {  0, 96, 64, 32};   // left / right / down-X / up-S
const int16_t navBoxY[4] = {  0,  0,  0,  0};   // all on bottom row

// ====================== INCREMENTAL DRAW STATE ======================
uint8_t prevMenuScrollPos = 0;
DisplayState prevDisplayState = LIVE;
DisplayState currentDisplayState = LIVE;
uint8_t   menuScrollPos    = 0;       // which item is highlighted (circular)
bool      valueChanged     = false;   // so "Edit" becomes "Save"
uint8_t   navigationMenuPos    = 0;       // which item is highlighted (circular)

//All critical path math is kept integer. Our objective is to achieve 500Hz+ sample rate.
//The display variables are kept in integer centies, i.e. 4.57V is 457 and 97.84% is 9784
//where the dot is applied at display time.
//Percent (0-100%) of the Z-Axis movement. Only variable to get curve application, plus zValue.
uint16_t displayCentiPercent = 0; //Each unit displays 0.01% increments.
//Actual Volts
uint16_t displayCentiVolts = 0; //Each unit displays 0.01V increments.
//Actual Psi.
uint32_t displayCentiPsi =0; //Each unit displays 0.01psi increments.
//Z-Axis value
//uint16_t zValue = 0;
//Error conditions
bool     pressureLow = false;
bool     transducerFailure = false;
bool     saturationFail    = false;

void setup() {
  //!!! HID Initialization must be the first initialization !!!
  // TinyUSB HID setup
  usb_hid.setStringDescriptor(USB_PRODUCT);           // optional but nice
  usb_hid.setReportDescriptor(handbrakeDescriptor, sizeof(handbrakeDescriptor));
  usb_hid.begin();
  // === STABILIZE USB ENUMERATION ===
  TinyUSBDevice.setSerialDescriptor(USB_SERIAL);     // ← fixed string = Windows remembers the exact COM port
  TinyUSBDevice.setProductDescriptor(USB_PRODUCT);      // optional but nice
  TinyUSBDevice.setManufacturerDescriptor(USB_MANUFACTURER);   // optional
  delay(1200);  // give USB time to enumerate (same as before)


  // pinMode(holdButtonPin, INPUT_PULLUP);
  pinMode(HOLD_BUTTON_PIN, INPUT_PULLUP);

  //Serial COM port
  Serial.begin(115200);           // SimHub
  while (!Serial) { }             // keep this — it’s safe now
  delay(100);
  Serial.println("\n===== E-Break Inititalizing =====");

  //ADS115 Setup 
  Wire.begin(I2C_SDA_PIN,I2C_SCL_PIN); //Define SDA/SCL pins
  if (ads.begin()) {
    Serial.println("ADS1115 found");
    ads.setGain(0);         // 0 = 6.144V
    ads.setDataRate(7);     // 7 = 860 samples/sec
    ads.setMode(0);         // 0 = continuous mode
    ads.requestADC(0);      // start continuous on channel 0
    delay(300);
    int16_t adsFirstSample = ads.getValue(); // this applies settings + first read
    if (INPUT_PRESSURE_FAIL_VALUE > adsFirstSample) {
      Serial.println("Transducer not found!");
    } else {
      Serial.print("Transducer measured: ");
      Serial.println(adsFirstSample);
    }
  } else {
    Serial.println("ADS1115 NOT found!");
  }
  
  //=== OLED Setup ===
  SPI.begin(OLED_SCK, -1, OLED_MOSI);   // SCK=12, MISO=-1 (unused), MOSI=11
  tft.begin();  //Initialize
  tft.setRotation(0);
  delay(200);
  bootScreenDisplay();
  setupLiveDisplay();
  Serial.println("SSD1351 initialized");

  //=== Rotary Encoder Setup ===
  encoder.setPosition(0);  // start at zero
  pinMode(ROTARY_BUTTON_PIN, INPUT_PULLUP);
  Serial.println("Rotary encoder initialized");

  //Behaviour
  Serial.print("Setting to Hold Mode: ");
  Serial.println(firmwareHoldMode);
  Serial.print("Current Curve Selection: ");
  Serial.println(correctionCurvesNames[currentCurve]);
  Serial.print("Low Deadzone: ");
  Serial.print(deadLowPercent);
  Serial.print(" High Deadzone: ");
  Serial.println(deadHighPercent);


  //Initialize data display
  updateLiveDisplay();

}

void loop() {
  //=== Program Rate limiting ===
  //In order to match USB's 1000Hz rate, we limit the whole program.
  static unsigned long nextSampleTime = 0;
  unsigned long now_us = micros();
  if (now_us < nextSampleTime) return;
  nextSampleTime = now_us + (1000000UL / targetSampleRate);
  
  //General keeper for rates in the ms range.
  unsigned long now = millis();

  /*Error conditions such as too high or low pressure, probe disconnect and such
  can be resolved in time. Thus, we reset the error code in each refresh.*/
  // Reset Error Readings
  static unsigned long nextErroReset = 0;
  if (now >= nextErroReset) {
    nextErroReset += ERROR_REFRESH;
    pressureLow = false;
    transducerFailure = false;
    saturationFail    = false;;
  }

  // === Encoder polling (rotation only - 1 ms) ===
  static unsigned long nextEncoderPoll = 0;
  if (now >= nextEncoderPoll) {
    nextEncoderPoll = now + 1;
    encoder.tick();

    int16_t delta = encoder.getPosition() - lastEncoderPosition;  // we already have lastPosition from earlier
    if (delta != 0) {
      lastEncoderPosition = encoder.getPosition();

      if (currentDisplayState == LIVE) {
        currentCurve = (currentCurve + delta + numberOfCurves) % numberOfCurves;
        setupLiveDisplay();
      }
      else if (currentDisplayState == MENU_LIST) {
        // Rotary now moves the NAV BAR cursor (0-3 wrap)
        menuScrollPos = (menuScrollPos + delta + MENU_NAVBAR_BUTTONS) % MENU_NAVBAR_BUTTONS;
      }
      else if (currentDisplayState == EDIT_VALUE) {
        // In edit mode you can go 0-3 (nav bar) + 4+ (adjustment buttons)
        // Example: 4 buttons → wrap at 8. Adjust the number to match your adjustment buttons.
        menuScrollPos = (menuScrollPos + delta + editScreens[currentEditScreenIndex].numButtons) % (editScreens[currentEditScreenIndex].numButtons + 2) + 2;
        Serial.print("Clicked adjustment button "); Serial.println(menuScrollPos);
        drawEditScreen();
      }
    }
  }

  // === Encoder BUTTON debounce (separate, identical to hold button) ===
  static unsigned long lastEncoderDebounceTime = 0;
  static int           lastRotaryButtonReading = HIGH;
  static bool          encoderButtonPressed = false;

  int encoderButtonReading = digitalRead(ROTARY_BUTTON_PIN);
  if (encoderButtonReading != lastRotaryButtonReading) {
    lastEncoderDebounceTime = now;
  }
  if ((now - lastEncoderDebounceTime) > debounceDelay) {
    if (encoderButtonReading != encoderButtonPressed) {
      encoderButtonPressed = encoderButtonReading;
      if (encoderButtonPressed == LOW) {
        if (currentDisplayState == LIVE) {
          currentDisplayState = MENU_LIST;
          menuScrollPos = 3;          // start on left arrow
          drawEditScreen();
          Serial.println("Entered MENU_LIST");
        }
        else if (currentDisplayState == MENU_LIST) {
          // TODO: your existing left/right click logic to scroll the vertical parameter list
          // (menuScrollPos tells you which arrow was clicked)
          if (menuScrollPos == 0) { /* Prev param */
            currentEditScreenIndex = (currentEditScreenIndex - 1u - numOfParameterEditScreens) % numOfParameterEditScreens;   // Move the current screen parameter
            drawEditScreen();
            //numOfParameterEditScreens
          } else if (menuScrollPos == 1) { /* Next param */ 
            currentEditScreenIndex = (currentEditScreenIndex + 1u - numOfParameterEditScreens) % numOfParameterEditScreens;   // Move the current screen parameter
            drawEditScreen();
          } else if (menuScrollPos == 2) { /* Return to LIVE */ 
            currentDisplayState = LIVE;
            setupLiveDisplay();
            Serial.println("Entered LIVE");
          } else if (menuScrollPos == 3) { // Down = enter edit
            currentDisplayState = EDIT_VALUE;
            //Somewhat copy the original parameter values so they can be discarded.
            menuScrollPos = 2;        // start on S (change to 4 if you prefer first adjustment button)
            enterEditScreen(currentEditScreenIndex);          // 0 = Deadzones, 1 = Snap Threshold, etc.
            Serial.println("Entered EDIT_VALUE");
          }
        }
        else if (currentDisplayState == EDIT_VALUE) {
          if (menuScrollPos == 2) {
            currentDisplayState = MENU_LIST;
          }
          else if (menuScrollPos == 3) { /* Next param */ 
            currentDisplayState = MENU_LIST;
          }
          else if (menuScrollPos == 4) { /* Return to LIVE */ 
            //Update the focused button.
            Serial.println("Changed Parameter.");
          } else {
            Serial.print("Clicked nav pos: "); Serial.println(menuScrollPos);
          }
          // TODO: act on current menuScrollPos
          // 2 = Save, 3 = Discard, 4+ = your +0.1 / +1 / -0.1 / -1 buttons
        }
      }
    }
  }
  lastRotaryButtonReading = encoderButtonReading;

  // Hold Button debounce
  static uint16_t lastHoldDebounceTime = 0;
  static int           lastHoldButtonReading = HIGH;
  static bool          holdButtonPressed = false;

  int holdButtonReading = digitalRead(HOLD_BUTTON_PIN);
  if (holdButtonReading != lastHoldButtonReading) {
    lastHoldDebounceTime = now;
  }
  if ((now - lastHoldDebounceTime) > debounceDelay) {
    if (holdButtonReading != holdButtonPressed) {
      holdButtonPressed = holdButtonReading;
      if (holdButtonPressed == LOW) {
        holdMode = !holdMode;
        updateLiveDisplayHoldMode();
      }
    }
  }
  lastHoldButtonReading = holdButtonReading;

  /*Reading is in the ADS1115 int16 output with TWOTHIRDS gain. The Transducer
  Has a 0.5V = 0psi to 4.5V = 500psi normal reading. While it can tolerate up
  to 750psi over pressure without losing calibration, since it has a 5V VCC it
  can't really go above ~4.85V. Thus, */
    uint16_t zValue = 0;                    // local, no global anymore
  getNormalizedADCReading(zValue);       // function fills it by reference

  //Update HID Joystick
  updateHIDDevice(zValue, holdButtonPressed);

  // Display (throttled)
  static unsigned long nextDisplayTime = 0;

  if (now >= nextDisplayTime) {
      nextDisplayTime += displayRefresh;
      if (currentDisplayState == LIVE) {
        updateLiveDisplay();
      } else if (currentDisplayState == MENU_LIST) {
        // your vertical list drawing stays here (unchanged)
        // just call syncNavBarToState() once after you draw the list
      } else if (currentDisplayState == EDIT_VALUE) {
        // drawEditScreen();          // ← single call, dispatches the right drawer
        // your custom drawEditValue() will go here later
        // nav bar is handled automatically
      }
  }
  // <<<=== ADD THIS LINE HERE ===>>>
  updateNavigationBar();
  // === simple loop-rate counter (non-blocking) ===
  //simpleRateCounter();
}

void getNormalizedADCReading(uint16_t &zValueOut) {
  /*Reading is in the ADS1115 int16 output with TWOTHIRDS gain. The Transducer
  Has a 0.5V = 0psi to 4.5V = 500psi normal reading. While it can tolerate up
  to 750psi over pressure without losing calibration, since it has a 5V VCC it
  can't really go above ~4.85V. Thus, */

  // Read transducer
  int16_t adcraw = ads.getValue();
  displayCentiVolts = ((uint32_t) adcraw * 1875UL + 50000UL) / 100000UL;  // V × 100, rounded
  
  if (adcraw < INPUT_PRESSURE_FAIL_VALUE) {        // sample ≤ 0.25V - We assume transducer disconnected or dead.
      transducerFailure = true;
      displayCentiPercent = 0u;
      zValueOut = Z_AXIS_MIN_VALUE;
  } 
  else if (adcraw >= INPUT_PRESSURE_OVER_VALUE) {  // Anything above 4.82V is displayed as 540 psi, and Percent is obviously kept at 100%.
      saturationFail   = true;
      displayCentiPsi  = ((uint32_t)global_psiMax * 108UL);
      displayCentiPercent = 10000u;
      zValueOut = Z_AXIS_MAX_VALUE;
  } 
  else if (adcraw >= global_effectiveMax) {        // In the 500..539 psi range we display the psi but ceiling the Percent.
      //displayCentiPsi  = (((int32_t)adcraw * 3 - 8000) * 100UL + 64UL) / 128UL;  // +64 = round to nearest 0.01
      displayCentiPsi = ((uint32_t)(adcraw - global_adcMin) * (uint32_t)(global_psiMax - global_psiMin) * 100UL + (global_adcSpan / 2)) / global_adcSpan;
      displayCentiPercent = 10000u;
      zValueOut = Z_AXIS_MAX_VALUE;
  } 
  else {                                           // In the 0-500psi range we will normalize the axis movement and percent but correctly display the psi.
      displayCentiPsi  = (((int32_t)adcraw * 3 - 8000) * 100UL + 64UL) / 128UL;
      if (adcraw < global_effectiveMin) {
        if ( adcraw < global_adcMin) {
          pressureLow   = true;
          displayCentiPsi  = 0u;
        } else {
          //displayCentiPsi  = ((int32_t)adcraw - (int32_t)global_adcMin) * 55000UL / (int32_t)global_adcSpan;
          displayCentiPsi = ((uint32_t)(adcraw - global_adcMin) * (uint32_t)(global_psiMax - global_psiMin) * 100UL + (global_adcSpan / 2)) / global_adcSpan;
        }
        displayCentiPercent = 0u;
        zValueOut = Z_AXIS_MIN_VALUE;
      } else {
        //displayCentiPsi  = ((int32_t)adcraw - (int32_t)global_adcMin) * 55000UL / (int32_t)global_adcSpan;
        displayCentiPsi = ((uint32_t)(adcraw - global_adcMin) * (uint32_t)(global_psiMax - global_psiMin) * 100UL + (global_adcSpan / 2)) / global_adcSpan;
        zValueOut = (int32_t)(adcraw - global_effectiveMin) * (int32_t)Z_AXIS_MAX_VALUE / (int32_t) global_effectiveSpan; //Normalize input after thresholds and deadzones to the z-axis value range.
        zValueOut = applyCurveCorrection(zValueOut); //Apply any curve correction.
        displayCentiPercent = ((int32_t)zValueOut * 10000UL) / (int32_t)Z_AXIS_MAX_VALUE; // get the correct percent.
      }
  }
}

uint16_t applyCurveCorrection(uint16_t normValue) {
  switch (currentCurve) {
    case 0:                    // Linear
      return normValue;

    case 3:                    // Drift Snap
      {
        const uint16_t snapThresh = snapPercent;  // 55.0 % of 4095 !!!! Should use a variable!!!
        if (normValue < snapThresh) {
          return 0;
        } else {
          return ((uint32_t)(normValue - snapThresh) * (uint32_t)Z_AXIS_MAX_VALUE) 
                 / (Z_AXIS_MAX_VALUE - snapThresh);
        }
      }

    default:                   // 1=Rally Soft, 2=Rally Aggressive, 4=Wet, 5=S-Curve
      {
        uint8_t lutID;
        if (currentCurve == 1)      lutID = 0; //1=Rally Soft
        else if (currentCurve == 2) lutID = 1; //2=Rally Aggressive
        else if (currentCurve == 4) lutID = 2; //4=Wet
        else if (currentCurve == 5) lutID = 3; // S-Curve
        else return normValue;   // ← defensive fallback to Linear

        //===CAREFUL: this changes if the z-axis value changes.
        uint16_t index = normValue >> 2;
        uint16_t frac  = normValue & 3;

        uint16_t y0, y1;
        switch (lutID) {
          case 0:
            y0 = curveLUT_0[index];
            y1 = curveLUT_0[index + 1];
            break;
          case 1:
            y0 = curveLUT_1[index];
            y1 = curveLUT_1[index + 1];
            break;
          case 2:
            y0 = curveLUT_2[index];
            y1 = curveLUT_2[index + 1];
            break;
          default:  // 3 = S-Curve
            y0 = curveLUT_3[index];
            y1 = curveLUT_3[index + 1];
            break;
        }
        // === inside the default case, after fetching y0 and y1 ===
        uint32_t delta = (uint32_t)y1 - y0;          // safe subtraction
        return y0 + (uint16_t)((delta * frac) >> 2);
        //return y0 + (((y1 - y0) * frac) >> 2);
      }
  }
}


void updateHIDDevice(uint16_t &zValueOut, bool holdButtonPressed) {
  //HID Joystick
  uint8_t report[3] = {0};
  // Button
  report[0] = (uint8_t) !holdButtonPressed;

  // Z-Axis
  if (firmwareHoldMode && holdMode){
    zValueOut = Z_AXIS_MAX_VALUE;                 //Hold it on firmware
  }
  report[1] = lowByte(zValueOut);
  report[2] = highByte(zValueOut);

  // Send readings
  usb_hid.sendReport(3, report, 3);
}

void simpleRateCounter(){
  // === simple loop-rate counter (non-blocking) ===
  static uint32_t lastMs = 0;
  static uint16_t loopCount = 0;
  loopCount++;
  if (millis() - lastMs >= 10000) {
      Serial.print("Samples/sec: ");
      Serial.println(loopCount/10);
      loopCount = 0;
      lastMs = millis();
  }
}

//====== DISPLAY SECTION =======================
void bootScreenDisplay() {
  tft.fillScreen(BLACK);
  tft.setCursor(16, 56);
  tft.setTextColor(WHITE);
  tft.setTextSize(2);
  tft.println("E-BRAKE");
  delay(2000);
}

void setupLiveDisplay() {
  //Boot Screen
  tft.fillScreen(BLACK);
  // Initial Screem Structure
  tft.setTextColor(WHITE);
  // Title
  tft.setTextSize(2);
  tft.setCursor(4, 2);
  tft.print(correctionCurvesNames[currentCurve]);
  // Pressure
  tft.setTextSize(2);
  tft.setCursor(90, 32);
  tft.print("PSI");
  // Voltage
  tft.setCursor(90, 54);
  tft.print("V");
  // Percent
  tft.setCursor(90, 76);
  tft.print("%");
  // Mode
  updateLiveDisplayHoldMode();
  // Proportional Bar Border
  tft.drawRect(4, 116, 122, 12, WHITE);
}

void updateLiveDisplay() {
  //Display Pressure   
  tft.fillRect(12, 32, 70, 16, BLACK);
  tft.setTextSize(2);
  tft.setCursor(12, 32);
  if (transducerFailure){
    tft.setTextColor(RED);
    tft.print("FAIL");
  } else if (pressureLow) {
    tft.setTextColor(YELLOW);
    tft.print("LOW");
  } else if (saturationFail) {
    tft.setTextColor(RED);
    tft.print("OVER");
  } else {
    tft.setTextColor(GREEN);
    tft.print(displayCentiPsi / 100u);
    tft.print(".");
    tft.print(displayCentiPsi % 100u);
  }
  //Display Volts
  tft.fillRect(12, 54, 70, 16, BLACK);
  tft.setTextColor(RED);
  tft.setTextSize(2);
  tft.setCursor(12, 54);
  tft.print(displayCentiVolts / 100u);
  tft.print(".");
  tft.print(displayCentiVolts % 100u);
  //Display Percentage
  tft.fillRect(12, 76, 70, 16, BLACK);
  tft.setTextColor(CYAN);
  tft.setTextSize(2);
  tft.setCursor(12, 76);
  tft.print(displayCentiPercent / 100u);
  tft.print(".");
  tft.print(displayCentiPercent % 100u);

  //Draw the proportional bar
  int barWidth = (int)(displayCentiPercent * 120UL / 10000UL);
  tft.fillRect(5, 117, barWidth, 10, CYAN);
  tft.fillRect(5 + barWidth, 117, 120 - barWidth, 10, BLACK);
}

void updateLiveDisplayHoldMode(){
  // Mode
  tft.setTextSize(2);
  tft.fillRect(36, 98, 48, 16, BLACK);
  tft.setCursor(36, 98);
  if (holdMode) {
    tft.setTextColor(RED);
    tft.print("HOLD");
  } else {
    tft.setTextColor(GREEN);
    tft.print("LIVE");
  }
}

// ====================== HELPERS ======================
static bool isNavBoxVisible(uint8_t index) {
  if (currentDisplayState == LIVE) return false;
  if (currentDisplayState == MENU_LIST) return true;               // all 4 visible
  if (currentDisplayState == EDIT_VALUE) return (index == 2 || index == 3); // only S/X
  return false;
}

// ====================== DRAW ONE BOX (background + foreground) ======================
static void drawNavBox(uint8_t index, bool isSelected) {
  if (index > 3) return;

  int16_t x = navBoxX[index];
  int16_t y = navBoxY[index];

  // --- Background ---
  uint16_t bg = isSelected ? SELECTED_BG : NORMAL_BG;
  tft.fillRect(x, y, RECT_W, RECT_H, bg);

  // --- Foreground ---
  uint16_t fg = isSelected ? SELECTED_FG : NORMAL_FG;

  int16_t cx = x + RECT_W / 2;
  int16_t cy = y + RECT_H / 2;

  if (currentDisplayState == EDIT_VALUE && (index == 2 || index == 3)) {
    // S or X in edit mode
    tft.setTextSize(2);
    tft.setTextColor(fg);
    tft.setCursor(x + 10, y + 3);               // tweak offset if needed
    tft.print((index == 2) ? 'S' : 'X');
  }
  else {
    // Normal arrows
    switch (index) {
      case 0: // LEFT
        tft.fillTriangle(x+RECT_W-5, cy-6, x+RECT_W-5, cy+6, x+8, cy, fg);
        break;
      case 1: // RIGHT
        tft.fillTriangle(x+8, cy-6, x+8, cy+6, x+RECT_W-5, cy, fg);
        break;
      case 2: // UP
        tft.fillTriangle(cx-7, y+RECT_H-5, cx+7, y+RECT_H-5, cx, y+5, fg);
        break;
      case 3: // DOWN
        tft.fillTriangle(cx-7, y+5, cx+7, y+5, cx, y+RECT_H-5, fg);
        break;
    }
  }
}

// ====================== FULL REDRAW (state change only) ======================
static void syncNavBarToState(void) {
  if (currentDisplayState == LIVE) {
    return;                     // ←←← ADD THIS LINE
  }
  // Erase entire nav-bar strip once (fastest on SSD1351)
  tft.fillRect(0, navBoxY[0] - 2, SCREEN_WIDTH, RECT_H + 4, NORMAL_BG);

  // Draw only the boxes that should be visible in the new state
  for (uint8_t i = 0; i < 4; i++) {
    if (isNavBoxVisible(i)) {
      bool selected = (menuScrollPos == i && menuScrollPos <= 3);
      drawNavBox(i, selected);
    }
  }
}

// ====================== INCREMENTAL CURSOR UPDATE (rotary tick) ======================
static void updateNavCursor(void) {
  // If state changed we already did full sync
  if (currentDisplayState != prevDisplayState) return;

  // Nothing changed
  if (menuScrollPos == prevMenuScrollPos) return;

  // Un-highlight old position (only if it was visible)
  if (prevMenuScrollPos <= 3 && isNavBoxVisible(prevMenuScrollPos)) {
    drawNavBox(prevMenuScrollPos, false);
  }

  // Highlight new position (only if it is visible)
  if (menuScrollPos <= 3 && isNavBoxVisible(menuScrollPos)) {
    drawNavBox(menuScrollPos, true);
  }
  // When cursor moves into adjustment buttons (>=4) the nav bar simply stays with no highlight
}

// ====================== NAV BAR UPDATE HELPER ======================
void updateNavigationBar() {
  if (currentDisplayState != prevDisplayState) {
    syncNavBarToState();           // full redraw when state changes (arrows → S/X or bar appears/disappears)
  } else if (menuScrollPos != prevMenuScrollPos) {
    updateNavCursor();             // incremental redraw (only 1 or 2 boxes)
  }

  // Update the "what was on screen" trackers
  prevDisplayState  = currentDisplayState;
  prevMenuScrollPos = menuScrollPos;
}

// ====================== ENTER ANY EDIT SCREEN (unified backup) ======================
static void enterEditScreen(uint8_t screenIndex) {
    if (screenIndex >= numOfParameterEditScreens) return;

    currentEditScreenIndex = screenIndex;
    const EditScreen* s = &editScreens[screenIndex];

    /*// Backup all live values for this screen (X will restore them)
    for (uint8_t i = 0; i < s->numParams; i++) {
        backupBuffer[i] = *(s->paramPtrs[i]);
    }*/
    // Backup all live values (type-safe)
    for (uint8_t i = 0; i < s->numParams; i++) {
        if (s->paramTypes[i] == PARAM_UINT16) {
            backupBuffer[i] = *(uint16_t*)(s->paramPtrs[i]);
        } else if (s->paramTypes[i] == PARAM_BOOL) {
            backupBuffer[i] = *(bool*)(s->paramPtrs[i]) ? 1 : 0;
        } else if (s->paramTypes[i] == PARAM_UINT32) {
            backupBuffer[i] = *(uint32_t*)(s->paramPtrs[i]);
        }
    }

    currentDisplayState = EDIT_VALUE;
    menuScrollPos = 4;                    // always start on first adjustment button
    Serial.print("Entered edit screen: ");
    Serial.println(s->title);
    // Full redraw only once when entering
    drawEditScreen();                     // this will now do the initial clean draw
}

// ====================== GENERIC DRAW DISPATCHER ======================
static void drawEditScreen(void) {
    if (currentEditScreenIndex >= numOfParameterEditScreens) return;
    editScreens[currentEditScreenIndex].drawFunc();   // call the custom drawer
}

static void setupParameterScreen(void) {
    tft.fillRect(0, RECT_H, SCREEN_WIDTH, SCREEN_HEIGHT - RECT_H, NORMAL_BG);  // clear below nav bar
    tft.setTextSize(1);
    tft.setTextColor(NORMAL_FG);
    tft.setCursor(4, RECT_H + 6);
    tft.print(editScreens[currentEditScreenIndex].title);   // standardized title    
}
//Buttons: arrows up, 2up, down, 2 down, selected,

// ====================== GENERIC RESTORE ON X (discard) ======================
static void restoreEditParams(void) {
    const EditScreen* s = &editScreens[currentEditScreenIndex];
    for (uint8_t i = 0; i < s->numParams; i++) {
        if (s->paramTypes[i] == PARAM_UINT16) {
            *(uint16_t*)(s->paramPtrs[i]) = backupBuffer[i];
        } else if (s->paramTypes[i] == PARAM_BOOL) {
            *(bool*)(s->paramPtrs[i]) = (backupBuffer[i] != 0);
        } else if (s->paramTypes[i] == PARAM_UINT32) {
            *(uint32_t*)(s->paramPtrs[i]) = backupBuffer[i];
        }
    }
}

// ====================== Parameter Screens ======================

// ---------------------------------------------------------------------------------
static void drawHoldParameterScreen(void) {
    setupParameterScreen();

    // Two side-by-side values
    tft.setTextSize(2);
    tft.setCursor(6, RECT_H+36);
    tft.setTextColor(firmwareHoldMode ? SELECTED_BG : NORMAL_FG);
    tft.print("Program");

    if(menuScrollPos == 4) {
      tft.fillRect(105, RECT_H+34, 20, 16, SELECTED_BG);
    }
    tft.setCursor(107, RECT_H+36);
    tft.print(firmwareHoldMode ? " " : "X");

    tft.setCursor(6, RECT_H+72);
    tft.setTextColor(firmwareHoldMode ? NORMAL_FG : SELECTED_BG);
    tft.print("Firmware");

    if(menuScrollPos == 5) {
      tft.fillRect(105, RECT_H+72, 20, 16, SELECTED_BG);
    }
    tft.setCursor(107, RECT_H+72);
    tft.print(firmwareHoldMode ? "X" : " ");

    // 4 adjustment buttons (positions 4–7) – reuse your triangle helpers later
    // (we'll flesh this out exactly as you want in the next message)
}

// ---------------------------------------------------------------------------------
static void drawDeadzonesScreen(void) {
    setupParameterScreen();

    // Two side-by-side values
    tft.setTextSize(2);
    tft.setCursor(8, RECT_H+22);
    tft.setTextColor(NORMAL_FG);
    tft.print("Low: ");
    tft.setCursor(8, RECT_H+44);
    tft.print(deadLowPercent / 10u);
    tft.print(".");
    tft.print(deadLowPercent % 10u);

    tft.setCursor(8, RECT_H+68);
    tft.print("High: ");
    tft.setCursor(8, RECT_H+88);  
    tft.print(deadHighPercent / 10u);
    tft.print(".");
    tft.print(deadHighPercent % 10u);

    // 4 adjustment buttons (positions 4–7) – reuse your triangle helpers later
    // (we'll flesh this out exactly as you want in the next message)
}

// ---------------------------------------------------------------------------------
static void drawSnapThresholdScreen(void) {
    setupParameterScreen();

    uint16_t localSnapPercent = (snapPercent * 10u + 20u) / Z_AXIS_MAX_VALUE /10u; //
    tft.setTextSize(2);
    tft.setCursor(4, RECT_H+56);
    tft.setTextColor(NORMAL_FG);
    tft.print(localSnapPercent);
    tft.print("%");

}

// ---------------------------------------------------------------------------------
static void drawButtonDebounceScreen(void) {
    setupParameterScreen();


}

// ---------------------------------------------------------------------------------
static void drawDefaultCurveScreen(void){
    setupParameterScreen();


}

// ---------------------------------------------------------------------------------
static void drawRefeshRatesScreen(void){
    setupParameterScreen();


}

// ---------------------------------------------------------------------------------
static void drawRecalibrateScreen(void){
    setupParameterScreen();


}

// ---------------------------------------------------------------------------------
static void drawSaveAndLoadScreen(void) {
    setupParameterScreen();


}