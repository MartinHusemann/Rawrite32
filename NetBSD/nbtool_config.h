/* stub file to make NetBSD hash library code compile on win32 - hard coded assumption: little endian */

#include <string.h>

void be32enc(unsigned char *out, unsigned __int32 in)
{
  unsigned char t[4];
  memcpy(t, &in, 4);
  out[0] = t[3];
  out[1] = t[2];
  out[2] = t[1];
  out[3] = t[0];
}

void be64enc(unsigned char *out, unsigned __int64 in)
{
  unsigned char t[8];
  memcpy(t, &in, 8);
  out[0] = t[7];
  out[1] = t[6];
  out[2] = t[5];
  out[3] = t[4];
  out[4] = t[3];
  out[5] = t[2];
  out[6] = t[1];
  out[7] = t[0];
}
