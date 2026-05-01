#ifndef CP437_H
#define CP437_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CP437_BLACK_SMILEY 0x02

// Single-line box drawing characters
#define CP437_SINGLE_LINE_VERTICAL      0xB3
#define CP437_SINGLE_LINE_HORIZONTAL    0xC4
#define CP437_SINGLE_LINE_DOWN_RIGHT    0xC0
#define CP437_SINGLE_LINE_DOWN_LEFT     0xBF
#define CP437_SINGLE_LINE_UP_RIGHT      0xDA
#define CP437_SINGLE_LINE_UP_LEFT       0xD9
#define CP437_SINGLE_LINE_T_UP          0xC1
#define CP437_SINGLE_LINE_T_DOWN        0xC2
#define CP437_SINGLE_LINE_T_LEFT        0xC3
#define CP437_SINGLE_LINE_T_RIGHT       0xB4
#define CP437_SINGLE_LINE_CROSS         0xC5

// Double-line box drawing characters
#define CP437_DOUBLE_LINE_VERTICAL      0xBA
#define CP437_DOUBLE_LINE_HORIZONTAL    0xCD
#define CP437_DOUBLE_LINE_DOWN_RIGHT    0xC8
#define CP437_DOUBLE_LINE_DOWN_LEFT     0xBC
#define CP437_DOUBLE_LINE_UP_RIGHT      0xC9
#define CP437_DOUBLE_LINE_UP_LEFT       0xBB
#define CP437_DOUBLE_LINE_T_UP          0xCA
#define CP437_DOUBLE_LINE_T_DOWN        0xCB
#define CP437_DOUBLE_LINE_T_LEFT        0xCC
#define CP437_DOUBLE_LINE_T_RIGHT       0xB9
#define CP437_DOUBLE_LINE_CROSS         0xCE

// Mixed box drawing characters for wall-door junctions
#define CP437_DOUBLE_LEFT_SINGLE_RIGHT_T    0xB5  /* ╡ */
#define CP437_SINGLE_LEFT_DOUBLE_RIGHT_T    0xC6  /* ╞ */
#define CP437_DOUBLE_HORIZ_SINGLE_UP_T      0xD0  /* ╨ */
#define CP437_DOUBLE_HORIZ_SINGLE_DOWN_T    0xD2  /* ╥ */

#define CP437_LEFT_ARROW                0x1B
#define CP437_RIGHT_ARROW               0x1A
#define CP437_LEFT_RIGHT_ARROW          0x1D
#define CP437_UP_DOWN_ARROW             0x12
#define CP437_UP_ARROW                  0x18
#define CP437_DOWN_ARROW                0x19

unsigned int cp437_to_unicode(unsigned char symbol);
void cp437_to_utf8(unsigned char symbol, char* out, size_t out_size);

#ifdef __cplusplus
}
#endif

#endif
