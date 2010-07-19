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

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

class CHashMD5 : public IGenericHash {
public:
  CHashMD5()
  {
    MD5Init(&m_ctx);
  }

  virtual LPCTSTR HashName()
  {
    return _T("MD5");
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

    out.Format(_T("%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x"),
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
    return _T("SHA1");
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
      t.Format(_T("%02x"), hash[i]);
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
    return _T("SHA256");
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
      t.Format(_T("%02x"), hash[i]);
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
    return _T("SHA512");
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
      t.Format(_T("%02x"), hash[i]);
      res += t;
    }
    out = res;
  }

  virtual void Delete() { delete this; }

protected:
  SHA512_CTX m_ctx;
};

static void GetAllAvailableHashes(vector<IGenericHash*> &result)
{
  result.push_back(new CHashMD5);
  result.push_back(new CHashSHA1);
  result.push_back(new CHashSHA256);
  result.push_back(new CHashSHA512);
}

void GetAllHashes(vector<IGenericHash*> &result)
{
  GetAllAvailableHashes(result);
  for (size_t i = 0; i < result.size(); ) {
    if (HashIsSelected(result[i]->HashName())) {
      i++;
    } else {
      result[i]->Delete();
      result.erase(result.begin()+i);
    }
  }
}

// returns a list of all available hash implementations
void GetAllHashNames(vector<CString> &result)
{
  vector<IGenericHash*> hashes;
  GetAllAvailableHashes(hashes);
  for (size_t i = 0; i < hashes.size(); i++) {
    result.push_back(hashes[i]->HashName());
    hashes[i]->Delete();
  }
}

class CRegCash
{
public:
  CRegCash();
  ~CRegCash();
  bool hash(LPCTSTR);
  void SetHash(LPCTSTR,bool);
protected:
  void Open();
protected:
  map<CString,bool> m_hashes;
  HKEY m_key;
  bool m_open, m_modified;
};

CRegCash::CRegCash()
: m_key(NULL), m_open(false), m_modified(false)
{ }

CRegCash::~CRegCash()
{
  if (m_modified) {
    for (map<CString,bool>::const_iterator p = m_hashes.begin(); p != m_hashes.end(); p++) {
      DWORD dv = p->second;
      RegSetValueEx(m_key, p->first, 0, REG_DWORD, (const BYTE*)&dv, sizeof dv);
    }
  }
  RegCloseKey(m_key);
}

void CRegCash::Open()
{
  if (m_open) return;
  m_key = AfxGetApp()->GetSectionKey(_T("hashes"));
  m_open = true;
  for (DWORD i = 0; ; i++) {
    TCHAR hashName[1000];
    DWORD sizeName = sizeof(hashName);
    DWORD data = 0, sizeData = sizeof data;
    if (RegEnumValue(m_key, i, hashName, &sizeName, NULL, NULL, (BYTE*)&data, &sizeData) != ERROR_SUCCESS) break;
    m_hashes[hashName] = data != 0;
  }
}

bool CRegCash::hash(LPCTSTR hashName)
{
  Open();
  map<CString,bool>::const_iterator p = m_hashes.find(hashName);
  if (p != m_hashes.end()) return p->second;
  bool res = false;
  SYSTEM_INFO info;
  GetSystemInfo(&info);
  if (info.dwNumberOfProcessors >= 4)
    res = true;
  else
    res = _tcscmp(hashName, _T("MD5")) == 0 || _tcscmp(hashName, _T("SHA512")) == 0;
  SetHash(hashName, res);
  return res;
}

void CRegCash::SetHash(LPCTSTR name, bool activate)
{
  m_hashes[name] = activate;
  m_modified = true;
}

static CRegCash theRegCache;

// query if the hash is activated
bool HashIsSelected(LPCTSTR hashName)
{
  return theRegCache.hash(hashName);
}

// changes activation of a hash
void SelectHash(LPCTSTR hashName, bool activate)
{
  theRegCache.SetHash(hashName, activate);
}

IGenericHash *GetFastHash()
{
  return new CHashMD5;
}
