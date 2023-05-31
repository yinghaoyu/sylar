#ifndef __SYLAR_HTTP2_TABLES_H__
#define __SYLAR_HTTP2_TABLES_H__

/*
                                                        code
                          code as bits                 as hex   len
        sym              aligned to MSB                aligned   in
                                                       to LSB   bits
       (  0)  |11111111|11000                             1ff8  [13]
       (  1)  |11111111|11111111|1011000                7fffd8  [23]
       (  2)  |11111111|11111111|11111110|0010         fffffe2  [28]
       (  3)  |11111111|11111111|11111110|0011         fffffe3  [28]
       (  4)  |11111111|11111111|11111110|0100         fffffe4  [28]
       (  5)  |11111111|11111111|11111110|0101         fffffe5  [28]
       (  6)  |11111111|11111111|11111110|0110         fffffe6  [28]
       (  7)  |11111111|11111111|11111110|0111         fffffe7  [28]
       (  8)  |11111111|11111111|11111110|1000         fffffe8  [28]
       (  9)  |11111111|11111111|11101010               ffffea  [24]
       ( 10)  |11111111|11111111|11111111|111100      3ffffffc  [30]
       ( 11)  |11111111|11111111|11111110|1001         fffffe9  [28]
       ( 12)  |11111111|11111111|11111110|1010         fffffea  [28]
       ( 13)  |11111111|11111111|11111111|111101      3ffffffd  [30]
       ( 14)  |11111111|11111111|11111110|1011         fffffeb  [28]
       ( 15)  |11111111|11111111|11111110|1100         fffffec  [28]
       ( 16)  |11111111|11111111|11111110|1101         fffffed  [28]
       ( 17)  |11111111|11111111|11111110|1110         fffffee  [28]
       ( 18)  |11111111|11111111|11111110|1111         fffffef  [28]
       ( 19)  |11111111|11111111|11111111|0000         ffffff0  [28]
       ( 20)  |11111111|11111111|11111111|0001         ffffff1  [28]
       ( 21)  |11111111|11111111|11111111|0010         ffffff2  [28]
       ( 22)  |11111111|11111111|11111111|111110      3ffffffe  [30]
       ( 23)  |11111111|11111111|11111111|0011         ffffff3  [28]
       ( 24)  |11111111|11111111|11111111|0100         ffffff4  [28]
       ( 25)  |11111111|11111111|11111111|0101         ffffff5  [28]
       ( 26)  |11111111|11111111|11111111|0110         ffffff6  [28]
       ( 27)  |11111111|11111111|11111111|0111         ffffff7  [28]
       ( 28)  |11111111|11111111|11111111|1000         ffffff8  [28]
       ( 29)  |11111111|11111111|11111111|1001         ffffff9  [28]
       ( 30)  |11111111|11111111|11111111|1010         ffffffa  [28]
       ( 31)  |11111111|11111111|11111111|1011         ffffffb  [28]
   ' ' ( 32)  |010100                                       14  [ 6]
   '!' ( 33)  |11111110|00                                 3f8  [10]
   '"' ( 34)  |11111110|01                                 3f9  [10]
   '#' ( 35)  |11111111|1010                               ffa  [12]
   '$' ( 36)  |11111111|11001                             1ff9  [13]
   '%' ( 37)  |010101                                       15  [ 6]
   '&' ( 38)  |11111000                                     f8  [ 8]
   ''' ( 39)  |11111111|010                                7fa  [11]
   '(' ( 40)  |11111110|10                                 3fa  [10]
   ')' ( 41)  |11111110|11                                 3fb  [10]
   '*' ( 42)  |11111001                                     f9  [ 8]
   '+' ( 43)  |11111111|011                                7fb  [11]
   ',' ( 44)  |11111010                                     fa  [ 8]
   '-' ( 45)  |010110                                       16  [ 6]
   '.' ( 46)  |010111                                       17  [ 6]
   '/' ( 47)  |011000                                       18  [ 6]
   '0' ( 48)  |00000                                         0  [ 5]
   '1' ( 49)  |00001                                         1  [ 5]
   '2' ( 50)  |00010                                         2  [ 5]
   '3' ( 51)  |011001                                       19  [ 6]
   '4' ( 52)  |011010                                       1a  [ 6]
   '5' ( 53)  |011011                                       1b  [ 6]
   '6' ( 54)  |011100                                       1c  [ 6]
   '7' ( 55)  |011101                                       1d  [ 6]
   '8' ( 56)  |011110                                       1e  [ 6]
   '9' ( 57)  |011111                                       1f  [ 6]
   ':' ( 58)  |1011100                                      5c  [ 7]
   ';' ( 59)  |11111011                                     fb  [ 8]
   '<' ( 60)  |11111111|1111100                           7ffc  [15]
   '=' ( 61)  |100000                                       20  [ 6]
   '>' ( 62)  |11111111|1011                               ffb  [12]
   '?' ( 63)  |11111111|00                                 3fc  [10]
   '@' ( 64)  |11111111|11010                             1ffa  [13]
   'A' ( 65)  |100001                                       21  [ 6]
   'B' ( 66)  |1011101                                      5d  [ 7]
   'C' ( 67)  |1011110                                      5e  [ 7]
   'D' ( 68)  |1011111                                      5f  [ 7]
   'E' ( 69)  |1100000                                      60  [ 7]
   'F' ( 70)  |1100001                                      61  [ 7]
   'G' ( 71)  |1100010                                      62  [ 7]
   'H' ( 72)  |1100011                                      63  [ 7]
   'I' ( 73)  |1100100                                      64  [ 7]
   'J' ( 74)  |1100101                                      65  [ 7]
   'K' ( 75)  |1100110                                      66  [ 7]
   'L' ( 76)  |1100111                                      67  [ 7]
   'M' ( 77)  |1101000                                      68  [ 7]
   'N' ( 78)  |1101001                                      69  [ 7]
   'O' ( 79)  |1101010                                      6a  [ 7]
   'P' ( 80)  |1101011                                      6b  [ 7]
   'Q' ( 81)  |1101100                                      6c  [ 7]
   'R' ( 82)  |1101101                                      6d  [ 7]
   'S' ( 83)  |1101110                                      6e  [ 7]
   'T' ( 84)  |1101111                                      6f  [ 7]
   'U' ( 85)  |1110000                                      70  [ 7]
   'V' ( 86)  |1110001                                      71  [ 7]
   'W' ( 87)  |1110010                                      72  [ 7]
   'X' ( 88)  |11111100                                     fc  [ 8]
   'Y' ( 89)  |1110011                                      73  [ 7]
   'Z' ( 90)  |11111101                                     fd  [ 8]
   '[' ( 91)  |11111111|11011                             1ffb  [13]
   '\' ( 92)  |11111111|11111110|000                     7fff0  [19]
   ']' ( 93)  |11111111|11100                             1ffc  [13]
   '^' ( 94)  |11111111|111100                            3ffc  [14]
   '_' ( 95)  |100010                                       22  [ 6]
   '`' ( 96)  |11111111|1111101                           7ffd  [15]
   'a' ( 97)  |00011                                         3  [ 5]
   'b' ( 98)  |100011                                       23  [ 6]
   'c' ( 99)  |00100                                         4  [ 5]
   'd' (100)  |100100                                       24  [ 6]
   'e' (101)  |00101                                         5  [ 5]
   'f' (102)  |100101                                       25  [ 6]
   'g' (103)  |100110                                       26  [ 6]
   'h' (104)  |100111                                       27  [ 6]
   'i' (105)  |00110                                         6  [ 5]
   'j' (106)  |1110100                                      74  [ 7]
   'k' (107)  |1110101                                      75  [ 7]
   'l' (108)  |101000                                       28  [ 6]
   'm' (109)  |101001                                       29  [ 6]
   'n' (110)  |101010                                       2a  [ 6]
   'o' (111)  |00111                                         7  [ 5]
   'p' (112)  |101011                                       2b  [ 6]
   'q' (113)  |1110110                                      76  [ 7]
   'r' (114)  |101100                                       2c  [ 6]
   's' (115)  |01000                                         8  [ 5]
   't' (116)  |01001                                         9  [ 5]
   'u' (117)  |101101                                       2d  [ 6]
   'v' (118)  |1110111                                      77  [ 7]
   'w' (119)  |1111000                                      78  [ 7]
   'x' (120)  |1111001                                      79  [ 7]
   'y' (121)  |1111010                                      7a  [ 7]
   'z' (122)  |1111011                                      7b  [ 7]
   '{' (123)  |11111111|1111110                           7ffe  [15]
   '|' (124)  |11111111|100                                7fc  [11]
   '}' (125)  |11111111|111101                            3ffd  [14]
   '~' (126)  |11111111|11101                             1ffd  [13]
       (127)  |11111111|11111111|11111111|1100         ffffffc  [28]
       (128)  |11111111|11111110|0110                    fffe6  [20]
       (129)  |11111111|11111111|010010                 3fffd2  [22]
       (130)  |11111111|11111110|0111                    fffe7  [20]
       (131)  |11111111|11111110|1000                    fffe8  [20]
       (132)  |11111111|11111111|010011                 3fffd3  [22]
       (133)  |11111111|11111111|010100                 3fffd4  [22]
       (134)  |11111111|11111111|010101                 3fffd5  [22]
       (135)  |11111111|11111111|1011001                7fffd9  [23]
       (136)  |11111111|11111111|010110                 3fffd6  [22]
       (137)  |11111111|11111111|1011010                7fffda  [23]
       (138)  |11111111|11111111|1011011                7fffdb  [23]
       (139)  |11111111|11111111|1011100                7fffdc  [23]
       (140)  |11111111|11111111|1011101                7fffdd  [23]
       (141)  |11111111|11111111|1011110                7fffde  [23]
       (142)  |11111111|11111111|11101011               ffffeb  [24]
       (143)  |11111111|11111111|1011111                7fffdf  [23]
       (144)  |11111111|11111111|11101100               ffffec  [24]
       (145)  |11111111|11111111|11101101               ffffed  [24]
       (146)  |11111111|11111111|010111                 3fffd7  [22]
       (147)  |11111111|11111111|1100000                7fffe0  [23]
       (148)  |11111111|11111111|11101110               ffffee  [24]
       (149)  |11111111|11111111|1100001                7fffe1  [23]
       (150)  |11111111|11111111|1100010                7fffe2  [23]
       (151)  |11111111|11111111|1100011                7fffe3  [23]
       (152)  |11111111|11111111|1100100                7fffe4  [23]
       (153)  |11111111|11111110|11100                  1fffdc  [21]
       (154)  |11111111|11111111|011000                 3fffd8  [22]
       (155)  |11111111|11111111|1100101                7fffe5  [23]
       (156)  |11111111|11111111|011001                 3fffd9  [22]
       (157)  |11111111|11111111|1100110                7fffe6  [23]
       (158)  |11111111|11111111|1100111                7fffe7  [23]
       (159)  |11111111|11111111|11101111               ffffef  [24]
       (160)  |11111111|11111111|011010                 3fffda  [22]
       (161)  |11111111|11111110|11101                  1fffdd  [21]
       (162)  |11111111|11111110|1001                    fffe9  [20]
       (163)  |11111111|11111111|011011                 3fffdb  [22]
       (164)  |11111111|11111111|011100                 3fffdc  [22]
       (165)  |11111111|11111111|1101000                7fffe8  [23]
       (166)  |11111111|11111111|1101001                7fffe9  [23]
       (167)  |11111111|11111110|11110                  1fffde  [21]
       (168)  |11111111|11111111|1101010                7fffea  [23]
       (169)  |11111111|11111111|011101                 3fffdd  [22]
       (170)  |11111111|11111111|011110                 3fffde  [22]
       (171)  |11111111|11111111|11110000               fffff0  [24]
       (172)  |11111111|11111110|11111                  1fffdf  [21]
       (173)  |11111111|11111111|011111                 3fffdf  [22]
       (174)  |11111111|11111111|1101011                7fffeb  [23]
       (175)  |11111111|11111111|1101100                7fffec  [23]
       (176)  |11111111|11111111|00000                  1fffe0  [21]
       (177)  |11111111|11111111|00001                  1fffe1  [21]
       (178)  |11111111|11111111|100000                 3fffe0  [22]
       (179)  |11111111|11111111|00010                  1fffe2  [21]
       (180)  |11111111|11111111|1101101                7fffed  [23]
       (181)  |11111111|11111111|100001                 3fffe1  [22]
       (182)  |11111111|11111111|1101110                7fffee  [23]
       (183)  |11111111|11111111|1101111                7fffef  [23]
       (184)  |11111111|11111110|1010                    fffea  [20]
       (185)  |11111111|11111111|100010                 3fffe2  [22]
       (186)  |11111111|11111111|100011                 3fffe3  [22]
       (187)  |11111111|11111111|100100                 3fffe4  [22]
       (188)  |11111111|11111111|1110000                7ffff0  [23]
       (189)  |11111111|11111111|100101                 3fffe5  [22]
       (190)  |11111111|11111111|100110                 3fffe6  [22]
       (191)  |11111111|11111111|1110001                7ffff1  [23]
       (192)  |11111111|11111111|11111000|00           3ffffe0  [26]
       (193)  |11111111|11111111|11111000|01           3ffffe1  [26]
       (194)  |11111111|11111110|1011                    fffeb  [20]
       (195)  |11111111|11111110|001                     7fff1  [19]
       (196)  |11111111|11111111|100111                 3fffe7  [22]
       (197)  |11111111|11111111|1110010                7ffff2  [23]
       (198)  |11111111|11111111|101000                 3fffe8  [22]
       (199)  |11111111|11111111|11110110|0            1ffffec  [25]
       (200)  |11111111|11111111|11111000|10           3ffffe2  [26]
       (201)  |11111111|11111111|11111000|11           3ffffe3  [26]
       (202)  |11111111|11111111|11111001|00           3ffffe4  [26]
       (203)  |11111111|11111111|11111011|110          7ffffde  [27]
       (204)  |11111111|11111111|11111011|111          7ffffdf  [27]
       (205)  |11111111|11111111|11111001|01           3ffffe5  [26]
       (206)  |11111111|11111111|11110001               fffff1  [24]
       (207)  |11111111|11111111|11110110|1            1ffffed  [25]
       (208)  |11111111|11111110|010                     7fff2  [19]
       (209)  |11111111|11111111|00011                  1fffe3  [21]
       (210)  |11111111|11111111|11111001|10           3ffffe6  [26]
       (211)  |11111111|11111111|11111100|000          7ffffe0  [27]
       (212)  |11111111|11111111|11111100|001          7ffffe1  [27]
       (213)  |11111111|11111111|11111001|11           3ffffe7  [26]
       (214)  |11111111|11111111|11111100|010          7ffffe2  [27]
       (215)  |11111111|11111111|11110010               fffff2  [24]
       (216)  |11111111|11111111|00100                  1fffe4  [21]
       (217)  |11111111|11111111|00101                  1fffe5  [21]
       (218)  |11111111|11111111|11111010|00           3ffffe8  [26]
       (219)  |11111111|11111111|11111010|01           3ffffe9  [26]
       (220)  |11111111|11111111|11111111|1101         ffffffd  [28]
       (221)  |11111111|11111111|11111100|011          7ffffe3  [27]
       (222)  |11111111|11111111|11111100|100          7ffffe4  [27]
       (223)  |11111111|11111111|11111100|101          7ffffe5  [27]
       (224)  |11111111|11111110|1100                    fffec  [20]
       (225)  |11111111|11111111|11110011               fffff3  [24]
       (226)  |11111111|11111110|1101                    fffed  [20]
       (227)  |11111111|11111111|00110                  1fffe6  [21]
       (228)  |11111111|11111111|101001                 3fffe9  [22]
       (229)  |11111111|11111111|00111                  1fffe7  [21]
       (230)  |11111111|11111111|01000                  1fffe8  [21]
       (231)  |11111111|11111111|1110011                7ffff3  [23]
       (232)  |11111111|11111111|101010                 3fffea  [22]
       (233)  |11111111|11111111|101011                 3fffeb  [22]
       (234)  |11111111|11111111|11110111|0            1ffffee  [25]
       (235)  |11111111|11111111|11110111|1            1ffffef  [25]
       (236)  |11111111|11111111|11110100               fffff4  [24]
       (237)  |11111111|11111111|11110101               fffff5  [24]
       (238)  |11111111|11111111|11111010|10           3ffffea  [26]
       (239)  |11111111|11111111|1110100                7ffff4  [23]
       (240)  |11111111|11111111|11111010|11           3ffffeb  [26]
       (241)  |11111111|11111111|11111100|110          7ffffe6  [27]
       (242)  |11111111|11111111|11111011|00           3ffffec  [26]
       (243)  |11111111|11111111|11111011|01           3ffffed  [26]
       (244)  |11111111|11111111|11111100|111          7ffffe7  [27]
       (245)  |11111111|11111111|11111101|000          7ffffe8  [27]
       (246)  |11111111|11111111|11111101|001          7ffffe9  [27]
       (247)  |11111111|11111111|11111101|010          7ffffea  [27]
       (248)  |11111111|11111111|11111101|011          7ffffeb  [27]
       (249)  |11111111|11111111|11111111|1110         ffffffe  [28]
       (250)  |11111111|11111111|11111101|100          7ffffec  [27]
       (251)  |11111111|11111111|11111101|101          7ffffed  [27]
       (252)  |11111111|11111111|11111101|110          7ffffee  [27]
       (253)  |11111111|11111111|11111101|111          7ffffef  [27]
       (254)  |11111111|11111111|11111110|000          7fffff0  [27]
       (255)  |11111111|11111111|11111011|10           3ffffee  [26]
   EOS (256)  |11111111|11111111|11111111|111111      3fffffff  [30]
*/

namespace sylar {
namespace http2 {

static unsigned int huffman_codes[256] = {
    0x1ff8,    0x7fffd8,   0xfffffe2, 0xfffffe3, 0xfffffe4,  0xfffffe5,
    0xfffffe6, 0xfffffe7,  0xfffffe8, 0xffffea,  0x3ffffffc, 0xfffffe9,
    0xfffffea, 0x3ffffffd, 0xfffffeb, 0xfffffec, 0xfffffed,  0xfffffee,
    0xfffffef, 0xffffff0,  0xffffff1, 0xffffff2, 0x3ffffffe, 0xffffff3,
    0xffffff4, 0xffffff5,  0xffffff6, 0xffffff7, 0xffffff8,  0xffffff9,
    0xffffffa, 0xffffffb,  0x14,      0x3f8,     0x3f9,      0xffa,
    0x1ff9,    0x15,       0xf8,      0x7fa,     0x3fa,      0x3fb,
    0xf9,      0x7fb,      0xfa,      0x16,      0x17,       0x18,
    0x0,       0x1,        0x2,       0x19,      0x1a,       0x1b,
    0x1c,      0x1d,       0x1e,      0x1f,      0x5c,       0xfb,
    0x7ffc,    0x20,       0xffb,     0x3fc,     0x1ffa,     0x21,
    0x5d,      0x5e,       0x5f,      0x60,      0x61,       0x62,
    0x63,      0x64,       0x65,      0x66,      0x67,       0x68,
    0x69,      0x6a,       0x6b,      0x6c,      0x6d,       0x6e,
    0x6f,      0x70,       0x71,      0x72,      0xfc,       0x73,
    0xfd,      0x1ffb,     0x7fff0,   0x1ffc,    0x3ffc,     0x22,
    0x7ffd,    0x3,        0x23,      0x4,       0x24,       0x5,
    0x25,      0x26,       0x27,      0x6,       0x74,       0x75,
    0x28,      0x29,       0x2a,      0x7,       0x2b,       0x76,
    0x2c,      0x8,        0x9,       0x2d,      0x77,       0x78,
    0x79,      0x7a,       0x7b,      0x7ffe,    0x7fc,      0x3ffd,
    0x1ffd,    0xffffffc,  0xfffe6,   0x3fffd2,  0xfffe7,    0xfffe8,
    0x3fffd3,  0x3fffd4,   0x3fffd5,  0x7fffd9,  0x3fffd6,   0x7fffda,
    0x7fffdb,  0x7fffdc,   0x7fffdd,  0x7fffde,  0xffffeb,   0x7fffdf,
    0xffffec,  0xffffed,   0x3fffd7,  0x7fffe0,  0xffffee,   0x7fffe1,
    0x7fffe2,  0x7fffe3,   0x7fffe4,  0x1fffdc,  0x3fffd8,   0x7fffe5,
    0x3fffd9,  0x7fffe6,   0x7fffe7,  0xffffef,  0x3fffda,   0x1fffdd,
    0xfffe9,   0x3fffdb,   0x3fffdc,  0x7fffe8,  0x7fffe9,   0x1fffde,
    0x7fffea,  0x3fffdd,   0x3fffde,  0xfffff0,  0x1fffdf,   0x3fffdf,
    0x7fffeb,  0x7fffec,   0x1fffe0,  0x1fffe1,  0x3fffe0,   0x1fffe2,
    0x7fffed,  0x3fffe1,   0x7fffee,  0x7fffef,  0xfffea,    0x3fffe2,
    0x3fffe3,  0x3fffe4,   0x7ffff0,  0x3fffe5,  0x3fffe6,   0x7ffff1,
    0x3ffffe0, 0x3ffffe1,  0xfffeb,   0x7fff1,   0x3fffe7,   0x7ffff2,
    0x3fffe8,  0x1ffffec,  0x3ffffe2, 0x3ffffe3, 0x3ffffe4,  0x7ffffde,
    0x7ffffdf, 0x3ffffe5,  0xfffff1,  0x1ffffed, 0x7fff2,    0x1fffe3,
    0x3ffffe6, 0x7ffffe0,  0x7ffffe1, 0x3ffffe7, 0x7ffffe2,  0xfffff2,
    0x1fffe4,  0x1fffe5,   0x3ffffe8, 0x3ffffe9, 0xffffffd,  0x7ffffe3,
    0x7ffffe4, 0x7ffffe5,  0xfffec,   0xfffff3,  0xfffed,    0x1fffe6,
    0x3fffe9,  0x1fffe7,   0x1fffe8,  0x7ffff3,  0x3fffea,   0x3fffeb,
    0x1ffffee, 0x1ffffef,  0xfffff4,  0xfffff5,  0x3ffffea,  0x7ffff4,
    0x3ffffeb, 0x7ffffe6,  0x3ffffec, 0x3ffffed, 0x7ffffe7,  0x7ffffe8,
    0x7ffffe9, 0x7ffffea,  0x7ffffeb, 0xffffffe, 0x7ffffec,  0x7ffffed,
    0x7ffffee, 0x7ffffef,  0x7fffff0, 0x3ffffee};

static int huffman_code_len[256] = {
    13, 23, 28, 28, 28, 28, 28, 28, 28, 24, 30, 28, 28, 30, 28, 28, 28, 28, 28,
    28, 28, 28, 30, 28, 28, 28, 28, 28, 28, 28, 28, 28, 6,  10, 10, 12, 13, 6,
    8,  11, 10, 10, 8,  11, 8,  6,  6,  6,  5,  5,  5,  6,  6,  6,  6,  6,  6,
    6,  7,  8,  15, 6,  12, 10, 13, 6,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,
    7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  8,  7,  8,  13, 19, 13, 14,
    6,  15, 5,  6,  5,  6,  5,  6,  6,  6,  5,  7,  7,  6,  6,  6,  5,  6,  7,
    6,  5,  5,  6,  7,  7,  7,  7,  7,  15, 11, 14, 13, 28, 20, 22, 20, 20, 22,
    22, 22, 23, 22, 23, 23, 23, 23, 23, 24, 23, 24, 24, 22, 23, 24, 23, 23, 23,
    23, 21, 22, 23, 22, 23, 23, 24, 22, 21, 20, 22, 22, 23, 23, 21, 23, 22, 22,
    24, 21, 22, 23, 23, 21, 21, 22, 21, 23, 22, 23, 23, 20, 22, 22, 22, 23, 22,
    22, 23, 26, 26, 20, 19, 22, 23, 22, 25, 26, 26, 26, 27, 27, 26, 24, 25, 19,
    21, 26, 27, 27, 26, 27, 24, 21, 21, 26, 26, 28, 27, 27, 27, 20, 24, 20, 21,
    22, 21, 21, 23, 22, 22, 25, 25, 24, 24, 26, 23, 26, 27, 26, 26, 27, 27, 27,
    27, 27, 28, 27, 27, 27, 27, 27, 26};

}  // namespace http2
}  // namespace sylar

#endif
