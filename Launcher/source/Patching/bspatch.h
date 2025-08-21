#ifndef _BSPATCH_H_
#define _BSPATCH_H_
#include <gctypes.h>
int getbsPatchOutputSize(u8 * patchBuffer);
int bspatch(u8 * old, int oldsize, u8 * patchBuffer, int patchsize, u8 * _new, int outnewsize);
int64_t offtin(unsigned char *buf);
#endif