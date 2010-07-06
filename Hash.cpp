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

#include "stdafx.h"

#include "Hash.h"

extern "C" {
#include "NetBSD/namespace.h"
#include "NetBSD/md5.h"
#include "NetBSD/sys/sha1.h"
#include "NetBSD/sys/sha2.h"
}

class CHashMD5 : public IGenericHash {
public:
  CHashMD5()
  {
    MD5Init(&m_ctx);
  }

  virtual LPCTSTR HashName()
  {
    return "MD5";
  }

  virtual void AddData(const BYTE *data, DWORD len)
  {
    MD5Update(&m_ctx, data, len);
  }

  virtual void HashResult(CString &out)
  {
    BYTE hash[16];
    memset(hash, 0, sizeof hash);
    MD5Final(hash, &m_ctx);

    out.Format("%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
               hash[0], hash[1], hash[2], hash[3], hash[4], hash[5], hash[6], hash[7], 
               hash[8], hash[9], hash[10], hash[11], hash[12], hash[13], hash[14], hash[15]);
  }

  virtual void Delete() { delete this; }

protected:
  MD5_CTX m_ctx;
};

class CHashSHA1 : public IGenericHash {
public:
  CHashSHA1()
  {
    SHA1Init(&m_ctx);
  }

  virtual LPCTSTR HashName()
  {
    return "SHA1";
  }

  virtual void AddData(const BYTE *data, DWORD len)
  {
    SHA1Update(&m_ctx, data, len);
  }

  virtual void HashResult(CString &out)
  {
    BYTE hash[SHA1_DIGEST_LENGTH];
    memset(hash, 0, sizeof hash);
    SHA1Final(hash, &m_ctx);
    
    CString res, t;
    for (size_t i = 0; i < SHA1_DIGEST_LENGTH; i++) {
      t.Format("%02x", hash[i]);
      res += t;
    }
    out = res;
  }

  virtual void Delete() { delete this; }

protected:
  SHA1_CTX m_ctx;
};


class CHashSHA256 : public IGenericHash {
public:
  CHashSHA256()
  {
    SHA256_Init(&m_ctx);
  }

  virtual LPCTSTR HashName()
  {
    return "SHA256";
  }

  virtual void AddData(const BYTE *data, DWORD len)
  {
    SHA256_Update(&m_ctx, data, len);
  }

  virtual void HashResult(CString &out)
  {
    BYTE hash[SHA256_DIGEST_LENGTH];
    memset(hash, 0, sizeof hash);
    SHA256_Final(hash, &m_ctx);
    
    CString res, t;
    for (size_t i = 0; i < SHA256_DIGEST_LENGTH; i++) {
      t.Format("%02x", hash[i]);
      res += t;
    }
    out = res;
  }

  virtual void Delete() { delete this; }

protected:
  SHA256_CTX m_ctx;
};

class CHashSHA512 : public IGenericHash {
public:
  CHashSHA512()
  {
    SHA512_Init(&m_ctx);
  }

  virtual LPCTSTR HashName()
  {
    return "SHA512";
  }

  virtual void AddData(const BYTE *data, DWORD len)
  {
    SHA512_Update(&m_ctx, data, len);
  }

  virtual void HashResult(CString &out)
  {
    BYTE hash[SHA512_DIGEST_LENGTH];
    memset(hash, 0, sizeof hash);
    SHA512_Final(hash, &m_ctx);
    
    CString res, t;
    for (size_t i = 0; i < SHA512_DIGEST_LENGTH; i++) {
      t.Format("%02x", hash[i]);
      res += t;
    }
    out = res;
  }

  virtual void Delete() { delete this; }

protected:
  SHA512_CTX m_ctx;
};

void GetAllHashes(vector<IGenericHash*> &result)
{
  result.push_back(new CHashMD5);
  result.push_back(new CHashSHA1);
  result.push_back(new CHashSHA256);
  result.push_back(new CHashSHA512);
}
