/*	$Id$	*/

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
#include "bz2lib/bzlib.h"

#include "Decompress.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

class CGzipDecompressor : public IGenericDecompressor {
public: 
  static CGzipDecompressor* Open(const BYTE *data, size_t len);
  virtual bool isError();
  virtual bool allDone();
  virtual bool needInputData();
  virtual void AddInputData(const BYTE *data, size_t len);
  virtual void SetOutputSpace(BYTE *out, size_t len);
  virtual size_t outputSpace();
  virtual void Delete();
protected:
  CGzipDecompressor(const BYTE *data, size_t len);
  void Process();
protected:
  z_stream_s m_decomp;
  DWORD m_crc;
  bool m_eof, m_error;
};

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
    if (method != DEFLATED) return NULL;
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
: m_eof(false), m_error(false)
{
  memset(&m_decomp, 0, sizeof m_decomp);
  m_decomp.next_in = (BYTE*)data;
  m_decomp.avail_in = len;
  m_crc = crc32(0L, Z_NULL, 0);
  inflateInit2(&m_decomp,-MAX_WBITS);
}

bool CGzipDecompressor::isError()
{
  return m_error;
}

bool CGzipDecompressor::allDone()
{
  return m_eof;
}

bool CGzipDecompressor::needInputData()
{
  return m_decomp.avail_in == 0;
}

void CGzipDecompressor::Process()
{
  BYTE *out = m_decomp.next_out;
  int res = inflate(&m_decomp, Z_SYNC_FLUSH);
  if (res < 0) {
    m_error = true;
    return;
  }
  if (out != m_decomp.next_out)
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
      m_error = true;
    inflateEnd(&m_decomp);
  }
}

void CGzipDecompressor::AddInputData(const BYTE *data, size_t len)
{
  m_decomp.next_in = (BYTE*)data;
  m_decomp.avail_in = len;
  Process();
}

void CGzipDecompressor::SetOutputSpace(BYTE *out, size_t len)
{
  m_decomp.next_out = out;
  m_decomp.avail_out = len;
  Process();
}

size_t CGzipDecompressor::outputSpace()
{
  return m_decomp.avail_out;
}

void CGzipDecompressor::Delete()
{
  delete this;
}

class CBZ2Decompressor : public IGenericDecompressor {
public: 
  static CBZ2Decompressor* Open(const BYTE *data, size_t len);
  virtual bool isError();
  virtual bool allDone();
  virtual bool needInputData();
  virtual void AddInputData(const BYTE *data, size_t len);
  virtual void SetOutputSpace(BYTE *out, size_t len);
  virtual size_t outputSpace();
  virtual void Delete();
protected:
  CBZ2Decompressor(const BYTE *data, size_t len);
  void Process();
protected:
  bz_stream m_decomp;
  bool m_eof, m_error;
};

CBZ2Decompressor* CBZ2Decompressor::Open(const BYTE *inputData, size_t inputSize)
{
  if (inputSize > 10 &&  inputData[0] == 'B' && inputData[1] == 'Z' && inputData[2] == 'h'
    && inputData[3] >= '0' && inputData[3] <= '9')
      return new CBZ2Decompressor(inputData, inputSize);
  return NULL;
}

CBZ2Decompressor::CBZ2Decompressor(const BYTE *data, size_t len)
: m_eof(false), m_error(false)
{
  memset(&m_decomp, 0, sizeof m_decomp);
  m_decomp.next_in = (char*)data;
  m_decomp.avail_in = len;
  BZ2_bzDecompressInit(&m_decomp,0,0);
}

bool CBZ2Decompressor::isError()
{
  return m_error;
}

bool CBZ2Decompressor::allDone()
{
  return m_eof;
}

bool CBZ2Decompressor::needInputData()
{
  return m_decomp.avail_in == 0;
}

void CBZ2Decompressor::Process()
{
  int res = BZ2_bzDecompress(&m_decomp);
  if (res < 0) {
    m_error = true;
    return;
  }
  if (res == BZ_STREAM_END) {
    m_eof = true;
    BZ2_bzDecompressEnd(&m_decomp);
  }
}

void CBZ2Decompressor::AddInputData(const BYTE *data, size_t len)
{
  m_decomp.next_in = (char*)data;
  m_decomp.avail_in = len;
  Process();
}

void CBZ2Decompressor::SetOutputSpace(BYTE *out, size_t len)
{
  m_decomp.next_out = (char*)out;
  m_decomp.avail_out = len;
  Process();
}

size_t CBZ2Decompressor::outputSpace()
{
  return m_decomp.avail_out;
}

void CBZ2Decompressor::Delete()
{
  delete this;
}

IGenericDecompressor *StartDecompress(const BYTE *data, size_t len)
{
  IGenericDecompressor *decomp = CGzipDecompressor::Open(data, len);
  if (decomp) return decomp;
  decomp = CBZ2Decompressor::Open(data, len);
  if (decomp) return decomp;
  return NULL;
}
