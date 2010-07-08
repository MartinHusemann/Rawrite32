/*	$Id: Hash.cpp -1   $	*/

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

#include "StdAfx.h"
extern "C" {
#include "zlib/zlib.h"
}

#include "Decompress.h"

class CGzipDecompressor : public IGenericDecompressor {
public: 
  static CGzipDecompressor* Open(const BYTE *data, size_t len);
  virtual bool allDone();
  virtual bool needInputData();
  virtual bool outOfOutputSpace();
  virtual void AddInputData(const BYTE *data, size_t len);
  virtual void SetOutputSpace(BYTE *out, size_t len);
  virtual size_t outputLength();
  virtual void Delete();
protected:
  CGzipDecompressor(const BYTE *data, size_t len);
protected:
  z_stream_s m_decomp;
  DWORD m_crc;
  bool m_eof;
};

#if 0
BOOL CRawrite32Dlg::UnzipImage(LPBYTE inputData, DWORD inputSize)
{
  if (!SkipGzipHeader(inputData, inputSize)) return FALSE;

  // get the uncompressed size
  LPBYTE p = inputData + inputSize - 4;
  DWORD outAllocSize = p[0] | (p[1]<<8) | (p[2]<<16) | (p[3]<<24);

  z_stream_s decomp;
  memset(&decomp, 0, sizeof decomp);
  LPBYTE outData = new BYTE[outAllocSize];
  decomp.next_out = outData;
  decomp.avail_out = outAllocSize;
  decomp.next_in = inputData;
  decomp.avail_in = inputSize;

  int ret;
  ret = inflateInit2(&decomp, -MAX_WBITS);
  if (ret >= 0) {
    ret = inflate(&decomp, Z_SYNC_FLUSH);
    if (ret == Z_STREAM_END) {
      DWORD crc = crc32(0L, Z_NULL, 0);
      crc = crc32(crc, outData, decomp.total_out);
      DWORD crcSrc = 0;
      LPBYTE p = decomp.next_in;
      crcSrc = p[0];
      crcSrc |= p[1] << 8;
      crcSrc |= p[2] << 16;
      crcSrc |= p[3] << 24;
      if (crcSrc != crc)
        ret = -1;
    }
    inflateEnd(&decomp);
  }

  if (ret >= 0) {
    m_fsImageSize = decomp.total_out;
    if (m_fsImageSize % SECTOR_SIZE) m_fsImageSize = SECTOR_SIZE*(m_fsImageSize/SECTOR_SIZE+1);
    m_fsImage = new BYTE[m_fsImageSize];
    if (m_fsImageSize != decomp.total_out)
      memset(m_fsImage, 0, m_fsImageSize);
    memcpy(m_fsImage, outData, m_fsImageSize);
  }

  delete outData;

  return ret >= 0;
}
#endif

CGzipDecompressor* CGzipDecompressor::Open(const BYTE *inputData, size_t inputSize)
{
  /* from gzip source code: */
#define ASCII_FLAG   0x01 /* bit 0 set: file probably ascii text */
#define CONTINUATION 0x02 /* bit 1 set: continuation of multi-part gzip file */
#define EXTRA_FIELD  0x04 /* bit 2 set: extra field present */
#define ORIG_NAME    0x08 /* bit 3 set: original file name present */
#define COMMENT      0x10 /* bit 4 set: file comment present */
#define ENCRYPTED    0x20 /* bit 5 set: file is encrypted */
#define RESERVED     0xC0 /* bit 6,7:   reserved */

#define DEFLATED     8

  if ((inputData[0] == 0x1f && inputData[1] == 0x8b) 
      || (inputData[0] == 0x1f && inputData[1] == 0x9e))  // gzip old and new magic header
  {
    BYTE method = inputData[2];
    if (method != DEFLATED) return FALSE;
    BYTE flags = inputData[3];
    const BYTE *p = inputData + 10;
    if (flags & CONTINUATION) p++;
    if (flags & EXTRA_FIELD) {
      unsigned short len = *p++;
      len |= (*p) << 8; p++;
      p += len;
    }
    if (flags & ORIG_NAME) {
      while (*p) p++;
      p++;
    }
    if (flags & COMMENT) {
      while (*p) p++;
      p++;
    }
    return new CGzipDecompressor(p, inputSize - (p-inputData));
  }
  return NULL;
}

CGzipDecompressor::CGzipDecompressor(const BYTE *data, size_t len)
{
  memset(&m_decomp, 0, sizeof m_decomp);
  m_decomp.next_in = (BYTE*)data;
  m_decomp.avail_in = len;
  m_crc = crc32(0L, Z_NULL, 0);
  m_eof = false;
}

bool CGzipDecompressor::allDone()
{
  return m_eof;
}

bool CGzipDecompressor::needInputData()
{
  return m_decomp.avail_in == 0;
}

bool CGzipDecompressor::outOfOutputSpace()
{
  return m_decomp.avail_out == 0;
}

void CGzipDecompressor::AddInputData(const BYTE *data, size_t len)
{
  const BYTE *out = m_decomp.next_out;
  m_decomp.next_in = (BYTE*)data;
  m_decomp.avail_in = len;
  int res = inflate(&m_decomp, Z_SYNC_FLUSH);
  m_crc = crc32(m_crc, out, m_decomp.next_out-out);
  if (res == Z_STREAM_END) {
    m_eof = true;
    DWORD crcSrc = 0;
    LPBYTE p = m_decomp.next_in;
    crcSrc = p[0];
    crcSrc |= p[1] << 8;
    crcSrc |= p[2] << 16;
    crcSrc |= p[3] << 24;
    if (crcSrc != m_crc)
      ASSERT(FALSE);
    inflateEnd(&m_decomp);
  }
}

void CGzipDecompressor::SetOutputSpace(BYTE *out, size_t len)
{
  m_decomp.next_out = out;
  m_decomp.avail_out = len;
}

size_t CGzipDecompressor::outputLength()
{
  return m_decomp.avail_out;
}

void CGzipDecompressor::Delete()
{
  delete this;
}

IGenericDecompressor *StartDecompress(const BYTE *data, size_t len)
{
  IGenericDecompressor *decomp = CGzipDecompressor::Open(data, len);
  if (decomp) return decomp;
  return NULL;
}
