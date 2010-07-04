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
#include "Rawrite32.h"
#include "Rawrite32Dlg.h"

#include <winioctl.h>

extern "C" {
#include "zlib/zlib.h"
}

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

// XXX - just blindly assume we write to a 512-bye sectored medium for now
#define SECTOR_SIZE   512

/////////////////////////////////////////////////////////////////////////////
// CAboutDlg dialog used for App About

class CAboutDlg : public CDialog
{
public:
  CAboutDlg();

// Dialog Data
  //{{AFX_DATA(CAboutDlg)
  enum { IDD = IDD_ABOUTBOX };
  //}}AFX_DATA

  // ClassWizard generated virtual function overrides
  //{{AFX_VIRTUAL(CAboutDlg)
  protected:
  virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
  //}}AFX_VIRTUAL

// Implementation
protected:
  //{{AFX_MSG(CAboutDlg)
	afx_msg void OnSurfHome();
	//}}AFX_MSG
  DECLARE_MESSAGE_MAP()
};

CAboutDlg::CAboutDlg() : CDialog(CAboutDlg::IDD)
{
  //{{AFX_DATA_INIT(CAboutDlg)
  //}}AFX_DATA_INIT
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
  CDialog::DoDataExchange(pDX);
  //{{AFX_DATA_MAP(CAboutDlg)
  //}}AFX_DATA_MAP
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialog)
  //{{AFX_MSG_MAP(CAboutDlg)
	ON_BN_CLICKED(IDC_SURF_HOME, OnSurfHome)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

void CAboutDlg::OnSurfHome() 
{
  OnOK();

  CString url;
  url.LoadString(IDS_HOME_URL);
  ShellExecute(NULL, _T("open"), url, NULL, NULL, SW_SHOWNORMAL);
}

/////////////////////////////////////////////////////////////////////////////
// CRawrite32Dlg dialog

CRawrite32Dlg::CRawrite32Dlg(LPCTSTR imageFileName)
  : CDialog(CRawrite32Dlg::IDD, NULL), m_fsImage(NULL), m_fsImageSize(0), m_sectorSkip(0),
    m_inputFile(INVALID_HANDLE_VALUE), m_inputMapping(NULL),
    m_curInput(NULL), m_sizeRemaining(0), m_sectorOut(0)
{
  m_hIcon = (HICON)::LoadImage(AfxGetResourceHandle(), MAKEINTRESOURCE(IDR_MAINFRAME), IMAGE_ICON, 32, 32, LR_DEFAULTCOLOR);
  m_hSmallIcon = (HICON)::LoadImage(AfxGetResourceHandle(), MAKEINTRESOURCE(IDR_MAINFRAME), IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR);
  EnableAutomation();
  m_imageName = imageFileName;
  m_output.LoadString(IDS_START_HINT);
}

CRawrite32Dlg::~CRawrite32Dlg()
{
  CloseInputFile();
}

void CRawrite32Dlg::DoDataExchange(CDataExchange* pDX)
{
  CDialog::DoDataExchange(pDX);
  //{{AFX_DATA_MAP(CRawrite32Dlg)
  DDX_Control(pDX, IDC_DRIVES, m_drives);
  DDX_Text(pDX, IDC_OUTPUT, m_output);
	//}}AFX_DATA_MAP
}

BEGIN_MESSAGE_MAP(CRawrite32Dlg, CDialog)
  //{{AFX_MSG_MAP(CRawrite32Dlg)
  ON_WM_SYSCOMMAND()
  ON_WM_PAINT()
  ON_WM_QUERYDRAGICON()
  ON_WM_DESTROY()
  ON_BN_CLICKED(IDC_BROWSE, OnBrowse)
  ON_EN_CHANGE(IDC_IMAGE_NAME, OnNewImage)
  ON_BN_CLICKED(IDC_WRITE_DISK, OnWriteImage)
	ON_EN_CHANGE(IDC_SECTOR_SKIP, OnChangeSectorSkip)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

BEGIN_INTERFACE_MAP(CRawrite32Dlg, CDialog)
  INTERFACE_PART(CRawrite32Dlg, IID_IDropTarget, Drop)
END_INTERFACE_MAP()

DWORD CRawrite32Dlg::XDrop::AddRef()
{
  METHOD_PROLOGUE(CRawrite32Dlg, Drop)
  return pThis->ExternalAddRef();
}

DWORD CRawrite32Dlg::XDrop::Release()
{
  METHOD_PROLOGUE(CRawrite32Dlg, Drop)
  return pThis->ExternalRelease();
}

STDMETHODIMP CRawrite32Dlg::XDrop::QueryInterface(REFIID iid, void **ppv)
{
  METHOD_PROLOGUE(CRawrite32Dlg, Drop)
  return pThis->ExternalQueryInterface(&iid, ppv);
}

STDMETHODIMP CRawrite32Dlg::XDrop::DragEnter(IDataObject * pDataObject, DWORD /*grfKeyState*/, POINTL /*pt*/, DWORD * pdwEffect)
{
  FORMATETC fmt;
  STGMEDIUM stg;
  memset(&stg, 0, sizeof stg);
  memset(&fmt, 0, sizeof fmt);
  fmt.cfFormat = CF_HDROP;
  fmt.lindex = -1;
  fmt.tymed = TYMED_HGLOBAL;
  fmt.dwAspect = DVASPECT_CONTENT;
  *pdwEffect = DROPEFFECT_NONE;
  HRESULT res = pDataObject->GetData(&fmt, &stg);
  if (FAILED(res)) {
    return S_OK;
  }
  HDROP dropData = (HDROP)stg.hGlobal;
  TCHAR fileName[_MAX_PATH];
  UINT numFiles = DragQueryFile(dropData, 0xFFFFFFFF, fileName, _MAX_PATH);
  if (numFiles == 1) {
    *pdwEffect = DROPEFFECT_COPY;
  }
  return S_OK;
}

STDMETHODIMP CRawrite32Dlg::XDrop::DragOver(DWORD /*grfKeyState*/, POINTL /*pt*/, DWORD * pdwEffect)
{
  *pdwEffect = DROPEFFECT_COPY;
  return S_OK;
}

STDMETHODIMP CRawrite32Dlg::XDrop::DragLeave()
{
  return S_OK;
}

STDMETHODIMP CRawrite32Dlg::XDrop::Drop(IDataObject * pDataObject, DWORD /*grfKeyState*/, POINTL /*pt*/, DWORD * pdwEffect)
{
  METHOD_PROLOGUE(CRawrite32Dlg, Drop)
  FORMATETC fmt;
  STGMEDIUM stg;
  memset(&stg, 0, sizeof stg);
  memset(&fmt, 0, sizeof fmt);
  fmt.cfFormat = CF_HDROP;
  fmt.lindex = -1;
  fmt.tymed = TYMED_HGLOBAL;
  fmt.dwAspect = DVASPECT_CONTENT;
  *pdwEffect = DROPEFFECT_NONE;
  HRESULT res = pDataObject->GetData(&fmt, &stg);
  if (FAILED(res)) {
    return S_OK;
  }
  HDROP dropData = (HDROP)stg.hGlobal;
  TCHAR fileName[_MAX_PATH];
  UINT numFiles = DragQueryFile(dropData, 0xFFFFFFFF, fileName, _MAX_PATH);
  if (numFiles == 1) {
    DragQueryFile(dropData, 0, fileName, _MAX_PATH);
    *pdwEffect = DROPEFFECT_COPY;
    pThis->GetDlgItem(IDC_IMAGE_NAME)->SetWindowText(fileName);
  }
  return S_OK;
}

/////////////////////////////////////////////////////////////////////////////
// CRawrite32Dlg message handlers

BOOL CRawrite32Dlg::OnInitDialog()
{
  CDialog::OnInitDialog();

  // Add "About..." menu item to system menu.

  // IDM_ABOUTBOX must be in the system command range.
  ASSERT((IDM_ABOUTBOX & 0xFFF0) == IDM_ABOUTBOX);
  ASSERT(IDM_ABOUTBOX < 0xF000);

  CMenu* pSysMenu = GetSystemMenu(FALSE);
  if (pSysMenu != NULL)
  {
  	CString strAboutMenu;
  	strAboutMenu.LoadString(IDS_ABOUTBOX);
  	if (!strAboutMenu.IsEmpty())
  	{
  		pSysMenu->AppendMenu(MF_SEPARATOR);
  		pSysMenu->AppendMenu(MF_STRING, IDM_ABOUTBOX, strAboutMenu);
  	}
  }

  SetIcon(m_hIcon, TRUE);			// Set big icon
  SetIcon(m_hSmallIcon, FALSE);		// Set small icon

  IDropTarget *dt;
  GetIDispatch(FALSE)->QueryInterface(IID_IDropTarget, (void**)&dt);
  RegisterDragDrop(m_hWnd, dt);
  dt->Release();

  TCHAR allDrives[32*4 + 10], winDir[MAX_PATH], sysDir[MAX_PATH];
  GetLogicalDriveStrings(sizeof allDrives/sizeof allDrives[0], allDrives);
  GetWindowsDirectory(winDir, MAX_PATH);
  GetSystemDirectory(sysDir, MAX_PATH);

  LPCTSTR drive;
  m_drives.ResetContent();
  for (drive = allDrives; *drive; drive += _tcslen(drive)+1) {
    // exclude the main disk from our list to avoid catastrophic disasters
    if (winDir[1] == ':' && _toupper(winDir[0]) == _toupper(drive[0])) continue;
    if (sysDir[1] == ':' && _toupper(sysDir[0]) == _toupper(drive[0])) continue;
    DWORD type = GetDriveType(drive);
    if (type != DRIVE_CDROM && type != DRIVE_RAMDISK) {
      TCHAR name[4];
      _tcsncpy(name, drive, 2);
      name[2] = 0;
      m_drives.AddString(name);
    }
  }
  m_drives.SetCurSel(m_drives.GetCount()-1);

  if (m_imageName)
    GetDlgItem(IDC_IMAGE_NAME)->SetWindowText(m_imageName);
  
  return TRUE;  // return TRUE  unless you set the focus to a control
}

void CRawrite32Dlg::OnSysCommand(UINT nID, LPARAM lParam)
{
  if ((nID & 0xFFF0) == IDM_ABOUTBOX)
  {
  	CAboutDlg dlgAbout;
  	dlgAbout.DoModal();
  }
  else
  {
  	CDialog::OnSysCommand(nID, lParam);
  }
}

// If you add a minimize button to your dialog, you will need the code below
//  to draw the icon.  For MFC applications using the document/view model,
//  this is automatically done for you by the framework.

void CRawrite32Dlg::OnPaint() 
{
  if (IsIconic())
  {
  	CPaintDC dc(this); // device context for painting

  	SendMessage(WM_ICONERASEBKGND, (WPARAM) dc.GetSafeHdc(), 0);

  	// Center icon in client rectangle
  	int cxIcon = GetSystemMetrics(SM_CXICON);
  	int cyIcon = GetSystemMetrics(SM_CYICON);
  	CRect rect;
  	GetClientRect(&rect);
  	int x = (rect.Width() - cxIcon + 1) / 2;
  	int y = (rect.Height() - cyIcon + 1) / 2;

  	// Draw the icon
  	dc.DrawIcon(x, y, m_hIcon);
  }
  else
  {
  	CDialog::OnPaint();
  }
}

HCURSOR CRawrite32Dlg::OnQueryDragIcon()
{
  return (HCURSOR) m_hIcon;
}

void CRawrite32Dlg::OnDestroy() 
{
  RevokeDragDrop(m_hWnd);
  CDialog::OnDestroy();
}

void CRawrite32Dlg::OnBrowse() 
{
  CString filter;
  CString title;
  filter.LoadString(IDS_OPEN_IMAGE_FILTER);
  title.LoadString(IDS_OPEN_IMAGE_TITLE);
  CFileDialog dlg(TRUE, NULL, NULL, OFN_HIDEREADONLY, filter, this);
  dlg.m_ofn.lpstrTitle = title;
  if (dlg.DoModal() == IDOK)
    GetDlgItem(IDC_IMAGE_NAME)->SetWindowText(dlg.GetPathName());
}

void CRawrite32Dlg::OnNewImage() 
{
  CString name;
  GetDlgItem(IDC_IMAGE_NAME)->GetWindowText(name);

  HANDLE hFile = CreateFile(name, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
  if (hFile == INVALID_HANDLE_VALUE) {
    m_output.Empty();
    UpdateData(FALSE);
    return;
  }
  OpenInputFile(hFile);
  VerifyInput();
}

static BOOL SkipGzipHeader(LPBYTE & inputData, DWORD & inputSize)
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
    LPBYTE p = inputData + 10;
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
    inputSize -= p-inputData;
    inputData = p;
    return TRUE;
  }
  return FALSE;
}

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

#ifdef _M_IX86
// only needed on arch=i386, otherwise assume NT anyway
static BOOL RunningOnDOS()
{
  OSVERSIONINFO osVers;
  memset(&osVers, 0, sizeof osVers);
  osVers.dwOSVersionInfoSize = sizeof osVers;
  GetVersionEx(&osVers);
  return osVers.dwPlatformId != VER_PLATFORM_WIN32_NT;
}
#endif

void CRawrite32Dlg::OnWriteImage() 
{
  if (!VerifyInput())
    return;

  int ndx = m_drives.GetCurSel();
  if (ndx == CB_ERR) return;

  CString drive, msg;
  m_drives.GetLBText(ndx, drive);

  msg.Format(IDP_ARE_YOU_SURE, drive);
  if (AfxMessageBox(msg, MB_YESNO|MB_ICONSTOP, IDP_ARE_YOU_SURE) != IDYES)
    return;

#ifdef _M_IX86  // special case for legacy versions on arch=i386

  if (RunningOnDOS()) {
    // Windows 9x: can't write to devices, need DOS services via vwin32.vdx

#define VWIN32_DIOC_DOS_INT26   3
    typedef struct _DIOC_REGISTERS {
        DWORD reg_EBX;
        DWORD reg_EDX;
        DWORD reg_ECX;
        DWORD reg_EAX;
        DWORD reg_EDI;
        DWORD reg_ESI;
        DWORD reg_Flags;
    } DIOC_REGISTERS, *PDIOC_REGISTERS;

    HANDLE hDev = CreateFile("\\\\.\\vwin32", 0, 0, NULL, 0, FILE_FLAG_DELETE_ON_CLOSE, NULL);
    if (hDev == INVALID_HANDLE_VALUE) {
      AfxMessageBox(IDP_NO_VXD);
      return;
    }

    DIOC_REGISTERS reg;
    memset(&reg, 0, sizeof reg);
    /*
     * DOS int 0x26:
     * AL = drive number (00h = A:, 01h = B:, etc)
     * CX = number of sectors to write (not FFFFh)
     * DX = starting logical sector number
     * DS:BX -> data to write
     */
    drive.MakeUpper();
    reg.reg_EAX = drive[0] - 'A';
    reg.reg_ECX = m_fsImageSize / SECTOR_SIZE;
    reg.reg_EDX = 0;
    reg.reg_EBX = (DWORD)m_fsImage;
    reg.reg_Flags = 0x0001; // assume error

    if (m_sectorSkip)
      reg.reg_EDX = m_sectorSkip;

    DWORD cb = 0;
    BOOL fResult = FALSE;

    {
      CWaitCursor hourglass;
      fResult = DeviceIoControl(hDev,  VWIN32_DIOC_DOS_INT26, &reg, sizeof(reg),  &reg, sizeof(reg), &cb, 0);
    }

    CloseHandle(hDev);

    if (!fResult || (reg.reg_Flags & 0x0001)) {
      AfxMessageBox(IDP_WRITE_ERROR);
    } else {
      CString msg;
      msg.LoadString(IDS_SUCCESS);
      m_output += msg;
      UpdateData(FALSE);
    }

  } else
#endif // not i386
  {
    // Windows NT does it the UNIX way...
    CString internalName;
    internalName.Format(_T("\\\\.\\%s"), drive);
    HANDLE theDrive = CreateFile(internalName, GENERIC_ALL, FILE_SHARE_READ|FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (theDrive == INVALID_HANDLE_VALUE) {
      AfxMessageBox(IDP_NO_DISK);
      return;
    }
    DWORD bytes = 0;
    if (DeviceIoControl(theDrive, FSCTL_LOCK_VOLUME, NULL, 0, NULL, 0, &bytes, NULL) == 0) {
      AfxMessageBox(IDP_CANT_LOCK_DISK);
      CloseHandle(theDrive);
      return;
    }

    CWaitCursor hourglass;

    if (m_sectorSkip)
      SetFilePointer(theDrive, m_sectorSkip*SECTOR_SIZE, NULL, FILE_BEGIN);

    DWORD written = 0;
    if (!WriteFile(theDrive, m_fsImage, m_fsImageSize, &written, NULL) || written != m_fsImageSize) {
      AfxMessageBox(IDP_WRITE_ERROR);
    } else {
      CString msg;
      msg.LoadString(IDS_SUCCESS);
      m_output += msg;
      UpdateData(FALSE);
    }
    DeviceIoControl(theDrive, FSCTL_UNLOCK_VOLUME, NULL, 0, NULL, 0, &bytes, NULL);
    DeviceIoControl(theDrive, IOCTL_DISK_UPDATE_PROPERTIES, NULL, 0, NULL, 0, &bytes, NULL);
    CloseHandle(theDrive);
  }
}

void CRawrite32Dlg::CloseInputFile()
{
  if (m_inputFile == INVALID_HANDLE_VALUE) return;
  UnmapViewOfFile(m_fsImage); m_fsImage = NULL;
  CloseHandle(m_inputMapping); m_inputMapping = NULL;
  CloseHandle(m_inputFile); m_inputFile = INVALID_HANDLE_VALUE;
}

bool CRawrite32Dlg::OpenInputFile(HANDLE hFile)
{
  CWaitCursor hourglass;

  CloseInputFile();
  m_inputFile = hFile;

  DWORD sizeHigh = 0;
  m_fsImageSize = GetFileSize(m_inputFile, &sizeHigh);
  m_inputMapping = CreateFileMapping(m_inputFile, NULL, PAGE_READONLY, sizeHigh, m_fsImageSize, NULL);
  if (m_inputMapping != NULL) {
    m_fsImage = (const BYTE *)MapViewOfFile(m_inputMapping, FILE_MAP_READ, 0, 0, m_fsImageSize);
    if (m_fsImage) {
      CString hashValues;
      CalcHashes(m_fsImage, m_fsImageSize, hashValues);
      m_output.Format(IDS_MESSAGE_INPUT_HASHES, m_imageName, hashValues, m_fsImageSize);
      // show message
      UpdateData(FALSE);
      
      // and copy to clipboard
      if (OpenClipboard()) {
        EmptyClipboard();
        DWORD size = m_output.GetLength() + 1;
        HANDLE hGlob = GlobalAlloc(GMEM_MOVEABLE, size);
        LPTSTR cnt = (LPTSTR)GlobalLock(hGlob);
        _tcscpy(cnt, m_output);
        GlobalUnlock(hGlob);

#ifdef _UNICODE
#define T_TEXT_FMT  CF_UNICODETEXT
#else
#define T_TEXT_FMT  CF_TEXT
#endif

        SetClipboardData(T_TEXT_FMT, hGlob);
        CloseClipboard();
      }
    }
  }

  return m_fsImage != NULL;
}

void CRawrite32Dlg::CalcHashes(const BYTE *data, size_t nbytes, CString &out)
{
  CString t;
  CalcMD5(data, nbytes, t); out = "MD5: " + t;
  CalcSHA1(data, nbytes, t); out += "\r\nSHA1: " + t;
  CalcSHA256(data, nbytes, t); out += "\r\nSHA256: " + t;
  CalcSHA512(data, nbytes, t); out += "\r\nSHA512: " + t;
}

bool CRawrite32Dlg::VerifyInput()
{
  bool retVal = FALSE;
  BOOL valid = FALSE;
  bool showMsg = FALSE;
  CString dummy;

  if (m_fsImage) goto done;

  GetDlgItemText(IDC_SECTOR_SKIP, dummy);
  m_sectorSkip = GetDlgItemInt(IDC_SECTOR_SKIP, &valid, FALSE);
  if (!dummy.IsEmpty() && !valid) {
    showMsg = TRUE;
    goto done;
  }
  if (!valid) m_sectorSkip = 0;

  retVal = TRUE;

done:
  GetDlgItem(IDC_WRITE_DISK)->EnableWindow(retVal);
  if (showMsg)
    AfxMessageBox(IDP_BAD_SKIP);
  return retVal;
}

void CRawrite32Dlg::OnChangeSectorSkip() 
{
  VerifyInput();
}
