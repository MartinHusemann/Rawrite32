/*	$Id: Rawrite32Dlg.cpp 18 2010-07-06 23:09:09Z Martin $	*/

/*-
 * Copyright (c) 2000-2003,2010 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * Copyright (c) 2000-2003,2010 Martin Husemann <martin@duskware.de>.
 * All rights reserved.
 * 
 * This code was developed by Martin Husemann for the benefit of
 * all NetBSD users and The NetBSD Foundation.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software withough specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/* stub file to make NetBSD hash library code compile on win32 */

#include <stdlib.h>

static __inline void be32enc(unsigned char *out, unsigned __int32 in)
{
  int i;
  for (i = 0; i < 4; i++) {
    out[3-i] = (unsigned char)(in & 0x0ff);
    in >>= 8;
  }
}

static __inline void be64enc(unsigned char *out, unsigned __int64 in)
{
  int i;
  for (i = 0; i < 8; i++) {
    out[7-i] = (unsigned char)(in & 0x0ff);
    in >>= 8;
  }
}

static __inline unsigned __int32 bswap32(unsigned __int32 x)
{
  return	((x << 24) & 0xff000000) |
          ((x << 8) & 0x00ff0000)  |
          ((x >> 8) & 0x0000ff00)  |
          ((x >> 24) & 0x000000ff);
}

static __inline unsigned __int64 htobe64(unsigned __int64 x)
{
  /*
   * Split the operation in two 32bit steps.
   */
  unsigned __int32 tl, th;
  th = bswap32((unsigned __int32)(x & 0x00000000ffffffffULL));
  tl = bswap32((unsigned __int32)((x >> 32) & 0x00000000ffffffffULL));
  return ((unsigned __int64)th << 32) | tl;
}

static __inline unsigned __int32 be32dec(const void *stream)
{
  const unsigned char *p = (const unsigned char *)stream;

  return ((p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3]);
}

static __inline unsigned __int64 be64dec(const void *stream)
{
  const unsigned char *p = (const unsigned char *)stream;

  return (((unsigned __int64)be32dec(p) << 32) | be32dec(p + 4));
}
