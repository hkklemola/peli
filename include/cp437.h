#ifndef CP437_H
#define CP437_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CP437_BLACK_SMILEY 0x02

unsigned int cp437_to_unicode(unsigned char symbol);
void cp437_to_utf8(unsigned char symbol, char* out, size_t out_size);

#ifdef __cplusplus
}
#endif

#endif
