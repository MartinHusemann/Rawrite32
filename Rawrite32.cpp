/*	$Id$	*/

/*-
 * Copyright (c) 2000-2003,2010-2020 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * Copyright (c) 2000-2003,2010-2020 Martin Husemann <martin@duskware.de>.
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
#include "Rawrite32.h"
#include "Rawrite32Dlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

/////////////////////////////////////////////////////////////////////////////
// CRawrite32App

BEGIN_MESSAGE_MAP(CRawrite32App, CWinApp)
  //{{AFX_MSG_MAP(CRawrite32App)
  //}}AFX_MSG
  ON_COMMAND(ID_HELP, CWinApp::OnHelp)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CRawrite32App construction

CRawrite32App::CRawrite32App()
{
  m_pszRegistryKey = _tcsdup(_T("NetBSD"));
  m_pszProfileName = _tcsdup(_T("Rawrite32"));
}

/////////////////////////////////////////////////////////////////////////////
// The one and only CRawrite32App object

CRawrite32App theApp;

/////////////////////////////////////////////////////////////////////////////
// CRawrite32App initialization

BOOL CRawrite32App::InitInstance()
{
  // Standard initialization

  AfxOleInit();

  // check for command line parameter(s)
  CCommandLineInfo cmdInfo;
  ParseCommandLine(cmdInfo);

  // create dialog, pass command line info
  CRawrite32Dlg dlg(cmdInfo.m_nShellCommand == CCommandLineInfo::FileOpen ? (LPCTSTR)cmdInfo.m_strFileName : (LPCTSTR)NULL);
  m_pMainWnd = &dlg;

  // run our dialog base application
  dlg.DoModal();

  // Since the dialog has been closed, return FALSE so that we exit the
  //  application, rather than start the application's message pump.
  return FALSE;
}

void CRawrite32App::WinHelpInternal(DWORD_PTR /*dwData*/, UINT /*nCmd = HELP_CONTEXT*/)
{
  CString url, fileName, relUrl;
  TCHAR path[MAX_PATH], *p;
  GetModuleFileName(NULL, path, MAX_PATH);
  p = _tcsrchr(path, '\\');
  if (p) *p = 0;
  relUrl.LoadString(IDS_HELP_URL);
  fileName = CString(path) + "/" + relUrl;
  if (GetFileAttributes(fileName) != INVALID_FILE_ATTRIBUTES) {
    url.Format(_T("file:///%s"), (LPCTSTR)fileName);
  } else {
    url.LoadString(IDS_HOME_URL);
    url += "/" + relUrl;
  }
  TRACE(_T("Help URL: %s\n"), (LPCTSTR)url);
  ShellExecute(NULL, _T("open"), url, NULL, NULL, SW_SHOWNORMAL);
}
