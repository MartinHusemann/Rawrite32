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
#include "Hash.h"
#include "Decompress.h"

#include <winioctl.h>
#include <map>

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

// single threaded hash calculation with timing
// #define HASH_BENCHMARK
// log total hash time
// #define HASH_TOTAL_TIME

// XXX - just blindly assume we write to a 512-bye sectored medium if using old (win9x) systems.
#define SECTOR_SIZE   512 // this value is not used on WinNT based systems

// size of the view into the input image
#define MAP_VIEW_SIZE (16*1024*1024)

// size of the decompression buffer
#define OUTPUT_BUF_SIZE (128*1024*1024)

#ifdef _M_IX86
// only needed on arch=i386, otherwise assume NT anyway
static bool RunningOnDOS()
{
  OSVERSIONINFO osVers;
  memset(&osVers, 0, sizeof osVers);
  osVers.dwOSVersionInfoSize = sizeof osVers;
  GetVersionEx(&osVers);
  return osVers.dwPlatformId != VER_PLATFORM_WIN32_NT;
}
#endif

/////////////////////////////////////////////////////////////////////////////
// CAboutDlg dialog used for App About
class CAboutDlg : public CDialog
{
public:
  CAboutDlg() : CDialog(IDD) {}

private:
  enum { IDD = IDD_ABOUTBOX };

protected:
	afx_msg void OnSurfHome();
  DECLARE_MESSAGE_MAP()
};

BEGIN_MESSAGE_MAP(CAboutDlg, CDialog)
	ON_BN_CLICKED(IDC_SURF_HOME, OnSurfHome)
END_MESSAGE_MAP()

void CAboutDlg::OnSurfHome() 
{
  OnOK();

  CString url;
  url.LoadString(IDS_HOME_URL);
  ShellExecute(NULL, _T("open"), url, NULL, NULL, SW_SHOWNORMAL);
}

/////////////////////////////////////////////////////////////////////////////
// CHashOptionsDlg
class CHashOptionsDlg : public CDialog
{
protected:
  enum { IDD = IDD_HASH_OPTIONS };
  BOOL OnInitDialog();
public:
  CHashOptionsDlg() : CDialog(IDD), m_initDone(false) { }
protected:
  afx_msg void OnHashSelect(NMHDR*,LRESULT*);
  DECLARE_MESSAGE_MAP();
  virtual void OnOK();
protected:
  map<CString,bool> m_modified;
  bool m_initDone;
};

BEGIN_MESSAGE_MAP(CHashOptionsDlg, CDialog)
  ON_NOTIFY(LVN_ITEMCHANGED, IDC_HASH_TYPE_LIST, OnHashSelect)
END_MESSAGE_MAP()

BOOL CHashOptionsDlg::OnInitDialog()
{
  BOOL res = CDialog::OnInitDialog();
  CListCtrl lc; lc.Attach(*GetDlgItem(IDC_HASH_TYPE_LIST));
  CRect r;
  vector<CString> hashNames;
  GetAllHashNames(hashNames);
  lc.GetClientRect(&r);
  lc.InsertColumn(1, "", 0, r.Width()-GetSystemMetrics(SM_CXVSCROLL)-1);
  lc.SetExtendedStyle(lc.GetExtendedStyle()|LVS_EX_CHECKBOXES);
  for (size_t i = 0; i < hashNames.size(); i++) {
    int ndx = lc.InsertItem(i, hashNames[i]);
    if (HashIsSelected(hashNames[i]))
      lc.SetCheck(ndx, 1);
  }
  lc.Detach();
  m_initDone = true;
  return res;
}

void CHashOptionsDlg::OnHashSelect(NMHDR*pHDR,LRESULT*)
{
  if (!m_initDone) return;
  NM_LISTVIEW* pNMListView = (NM_LISTVIEW*)pHDR;
  if ((pNMListView->uChanged & LVIF_STATE) == 0) return;
  if (((pNMListView->uNewState ^ pNMListView->uOldState) & LVIS_STATEIMAGEMASK) == 0) return;
  CListCtrl *pList = (CListCtrl*)GetDlgItem(IDC_HASH_TYPE_LIST);
  int ndx = pNMListView->iItem;
  bool activate = !!pList->GetCheck(ndx);
  CString name(pList->GetItemText(ndx, 0));
  m_modified[name] = activate;
}

void CHashOptionsDlg::OnOK()
{
  CDialog::OnOK();
  if (m_modified.empty()) return;
  for (map<CString,bool>::const_iterator i = m_modified.begin(); i != m_modified.end(); i++)
    SelectHash(i->first, i->second);
}

/////////////////////////////////////////////////////////////////////////////
// CRawrite32Dlg dialog

CRawrite32Dlg::CRawrite32Dlg(LPCTSTR imageFileName)
  : CDialog(CRawrite32Dlg::IDD, NULL), m_fsImage(NULL), m_fsImageSize(0), m_sectorSkip(0),
    m_inputFile(INVALID_HANDLE_VALUE), m_inputMapping(NULL),
    m_curInput(NULL), m_sizeRemaining(0), m_sizeWritten(0),
    m_inputFileSize(0), m_fileOffset(0),m_outputDevice(INVALID_HANDLE_VALUE),
    m_outputBuffer(new BYTE[OUTPUT_BUF_SIZE])
#ifdef _M_IX86
    , m_usingVXD(RunningOnDOS()), m_sectorOut(0)
#endif
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
  delete [] m_outputBuffer;
}

void CRawrite32Dlg::DoDataExchange(CDataExchange* pDX)
{
  CDialog::DoDataExchange(pDX);
  //{{AFX_DATA_MAP(CRawrite32Dlg)
  DDX_Control(pDX, IDC_PROGRESS, m_progress);
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

extern "C" {
// avoid DDK includes, just paste some stuff here
enum STORAGE_PROPERTY_ID {
  StorageDeviceProperty                   = 0,
  StorageAdapterProperty                  = 1,
  StorageDeviceIdProperty                 = 2,
  StorageDeviceUniqueIdProperty           = 3,
  StorageDeviceWriteCacheProperty         = 4,
  StorageMiniportProperty                 = 5,
  StorageAccessAlignmentProperty          = 6,
  StorageDeviceSeekPenaltyProperty        = 7,
  StorageDeviceTrimProperty               = 8,
  StorageDeviceWriteAggregationProperty   = 9 
};
enum STORAGE_QUERY_TYPE {
  PropertyStandardQuery     = 0,
  PropertyExistsQuery       = 1,
  PropertyMaskQuery         = 2,
  PropertyQueryMaxDefined   = 3 
};
struct STORAGE_PROPERTY_QUERY {
  STORAGE_PROPERTY_ID PropertyId;
  STORAGE_QUERY_TYPE  QueryType;
  UCHAR               AdditionalParameters[1];
};
struct STORAGE_DEVICE_DESCRIPTOR {
  ULONG            Version;
  ULONG            Size;
  UCHAR            DeviceType;
  UCHAR            DeviceTypeModifier;
  BOOLEAN          RemovableMedia;
  BOOLEAN          CommandQueueing;
  ULONG            VendorIdOffset;
  ULONG            ProductIdOffset;
  ULONG            ProductRevisionOffset;
  ULONG            SerialNumberOffset;
  STORAGE_BUS_TYPE BusType;
  ULONG            RawPropertiesLength;
  UCHAR            RawDeviceProperties[1];
};
#define IOCTL_STORAGE_QUERY_PROPERTY   CTL_CODE(IOCTL_STORAGE_BASE, 0x0500, METHOD_BUFFERED, FILE_ANY_ACCESS)
}

/////////////////////////////////////////////////////////////////////////////
// CRawrite32Dlg message handlers

BOOL CRawrite32Dlg::OnInitDialog()
{
  CDialog::OnInitDialog();

  m_outWinFont.CreateFont(-12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, ANSI_CHARSET,
                          OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
                          FIXED_PITCH|FF_DONTCARE, NULL);
  GetDlgItem(IDC_OUTPUT)->SetFont(&m_outWinFont);

  // Add "About..." menu item to system menu.

  // IDM_ABOUTBOX must be in the system command range.
  ASSERT((IDM_ABOUTBOX & 0xFFF0) == IDM_ABOUTBOX);
  ASSERT(IDM_ABOUTBOX < 0xF000);
  ASSERT((IDM_OPTIONS_HASHES & 0xFFF0) == IDM_OPTIONS_HASHES);
  ASSERT(IDM_OPTIONS_HASHES < 0xF000);

  CMenu* pSysMenu = GetSystemMenu(FALSE);
  if (pSysMenu != NULL)
  {
  	CString aboutMenu, optionsMenu;
  	aboutMenu.LoadString(IDS_ABOUTBOX);
    optionsMenu.LoadString(IDS_OPTIONS_HASHES);
		pSysMenu->AppendMenu(MF_SEPARATOR);
		pSysMenu->AppendMenu(MF_STRING, IDM_ABOUTBOX, aboutMenu);
		pSysMenu->AppendMenu(MF_STRING, IDM_OPTIONS_HASHES, optionsMenu);
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
    if (type != DRIVE_CDROM && type != DRIVE_REMOTE && type != DRIVE_RAMDISK) {
      CString name(drive, 2);
      if (!m_usingVXD) {
        // no idea how to get this information on Win9x - just ignore the difference there
        CString internalName;
        internalName.Format(_T("\\\\.\\%s"), name);
        HANDLE outputDevice = CreateFile(internalName, 0, FILE_SHARE_READ|FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
        if (outputDevice != INVALID_HANDLE_VALUE) {
          union {
            STORAGE_DEVICE_DESCRIPTOR header;
            TCHAR buf[1024];
          } desc;
          STORAGE_PROPERTY_QUERY qry;
          memset(&qry, 0, sizeof qry);
          qry.PropertyId = StorageDeviceProperty;
          qry.QueryType = PropertyStandardQuery;
          DWORD bytes = 0;
          if (DeviceIoControl(outputDevice, IOCTL_STORAGE_QUERY_PROPERTY, &qry, sizeof qry, &desc.header, sizeof desc, &bytes, NULL)
              && bytes >= sizeof(STORAGE_DEVICE_DESCRIPTOR)) {
            const char *strings = (const char *)&desc;
            if (desc.header.VendorIdOffset || desc.header.ProductIdOffset) {
              name += " (";
              if (desc.header.VendorIdOffset) {
                CString t(strings + desc.header.VendorIdOffset);
                t.Trim();
                name += t;
                if (desc.header.ProductIdOffset) name += " ";
              }
              if (desc.header.ProductIdOffset) {
                CString t(strings + desc.header.ProductIdOffset);
                t.Trim();
                name += t;
              }
              name += ")";
            }
          }
         CloseHandle(outputDevice);
        }
      }
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
  if ((nID & 0xFFF0) == IDM_ABOUTBOX) {
  	CAboutDlg dlgAbout;
  	dlgAbout.DoModal();
  } else if ((nID & 0xFFF0) == IDM_OPTIONS_HASHES) {
    CHashOptionsDlg dlgOptions;
    dlgOptions.DoModal();
  } else {
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
  m_imageName = name;
  OpenInputFile(hFile);
  VerifyInput();
}

void CRawrite32Dlg::ShowError(DWORD err, UINT id)
{
  CString msg; msg.LoadString(id);
  CString t; t.Format("%s\r\nError code: %u", msg, err);
  m_output += t;
  ShowOutput();
  AfxMessageBox(id, MB_OK|MB_ICONERROR);
}

void CRawrite32Dlg::ShowOutput()
{
  UpdateData(FALSE);
  CEdit* edit = (CEdit*)GetDlgItem(IDC_OUTPUT);
  int n = m_output.GetLength();
  edit->SetSel(n,n);
}

void CRawrite32Dlg::OnWriteImage() 
{
  if (!VerifyInput())
    return;

  int ndx = m_drives.GetCurSel();
  if (ndx == CB_ERR) return;

  CString drive, msg;
  m_drives.GetLBText(ndx, drive);

  msg.Format(IDP_ARE_YOU_SURE, drive);
  if (AfxMessageBox(msg, MB_YESNO|MB_ICONQUESTION, IDP_ARE_YOU_SURE) != IDYES)
    return;

  bool success = true;
  DWORD secSize = 512;
#ifdef _M_IX86  // special case for legacy versions on arch=i386
  if (m_usingVXD) {
    // Windows 9x: can't write to devices, need DOS services via vwin32.vdx
    m_outputDevice = CreateFile("\\\\.\\vwin32", 0, 0, NULL, 0, FILE_FLAG_DELETE_ON_CLOSE, NULL);
    if (m_outputDevice == INVALID_HANDLE_VALUE) {
      ShowError(GetLastError(), IDP_NO_VXD);
      return;
    }
    m_sectorOut = m_sectorSkip;
  } else
#endif
  {
    // Windows NT does it the UNIX way...
    CString internalName;
    internalName.Format(_T("\\\\.\\%s"), drive.Left(2));
    m_outputDevice = CreateFile(internalName, GENERIC_WRITE, FILE_SHARE_READ|FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    if (m_outputDevice == INVALID_HANDLE_VALUE) {
      ShowError(GetLastError(), IDP_NO_DISK);
      return;
    }
    DWORD bytes = 0;
    if (DeviceIoControl(m_outputDevice, FSCTL_LOCK_VOLUME, NULL, 0, NULL, 0, &bytes, NULL) == 0) {
      ShowError(GetLastError(), IDP_CANT_LOCK_DISK);
      CloseHandle(m_outputDevice); m_outputDevice = INVALID_HANDLE_VALUE;
      return;
    }

    DISK_GEOMETRY_EX diskInfo;
    if (DeviceIoControl(m_outputDevice, IOCTL_DISK_GET_DRIVE_GEOMETRY_EX, NULL, 0, &diskInfo, sizeof diskInfo, &bytes, NULL))
      secSize = diskInfo.Geometry.BytesPerSector;

    if (m_sectorSkip)
      SetFilePointer(m_outputDevice, m_sectorSkip*SECTOR_SIZE, NULL, FILE_BEGIN);
  }

  CWaitCursor hourglass;
  m_fileOffset = 0;
  m_fsImageSize = 0;
  MapInputView();
  m_progress.SetRange(0, 100);
  m_progress.ShowWindow(SW_SHOW);
  IGenericDecompressor *decomp = StartDecompress(m_fsImage, m_fsImageSize);
  if (decomp) decomp->SetOutputSpace(m_outputBuffer, OUTPUT_BUF_SIZE);

  CWnd *ow = GetDlgItem(IDC_OUTPUT);
  CRect owr, pgr; ow->GetWindowRect(&owr); ScreenToClient(&owr);
  m_progress.GetWindowRect(&pgr);
  ow->SetWindowPos(NULL, owr.left, owr.top, owr.Width(), owr.Height()-pgr.Height(), SWP_NOACTIVATE|SWP_NOZORDER);
  for (;;) {

    {
      double full = (double)m_inputFileSize, cur = (double)m_fileOffset;
      int perc = (int)(cur/full*100.0+.5);
      m_progress.SetPos(perc);
      CString si, sw, out;
      FormatSize(m_fileOffset, si);
      FormatSize(m_sizeWritten, sw);
      out.Format(IDS_WRITE_PROGRESS, si, sw);
      ow->SetWindowText(out);
      Poll();
    }

    const BYTE *outData = NULL;
    DWORD outSize = 0;

    if (decomp) {
      if (decomp->isError()) 
        break;
      if (decomp->needInputData()) {
        UnmapViewOfFile(m_fsImage);
        if (AdvanceMapOffset() && MapInputView()) {
          decomp->AddInputData(m_fsImage, m_fsImageSize);
          if (decomp->outputSpace() > 0 && decomp->needInputData())
            continue;
        }
      }
      if (decomp->outputSpace() == OUTPUT_BUF_SIZE) {
        if (decomp->allDone()) 
          break;
        continue;
      }
      outData = m_outputBuffer;
      outSize =  OUTPUT_BUF_SIZE - decomp->outputSpace();
    } else {
      if (m_fsImageSize % secSize) {
        // will need padding, copy over to writable buffer
        memcpy(m_outputBuffer, m_fsImage, m_fsImageSize);
        outData = m_outputBuffer;
      } else {
        outData = m_fsImage;
      }
      outSize = m_fsImageSize;
    }
    // pad output, if needed
    if (outSize % secSize) {
      ASSERT(outData == m_outputBuffer);
      DWORD fill = secSize - (outSize % secSize);
      memset(m_outputBuffer+outSize, 0, fill);
      outSize += fill;
    }
    ASSERT(outSize % secSize == 0);

#ifdef _M_IX86
    if (m_usingVXD) {

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
      reg.reg_ECX = outSize / SECTOR_SIZE;
      reg.reg_EDX = m_sectorOut;
      reg.reg_EBX = (DWORD)outData;
      reg.reg_Flags = 0x0001; // assume error

      DWORD cb = 0;
      BOOL fResult = DeviceIoControl(m_outputDevice,  VWIN32_DIOC_DOS_INT26, &reg, sizeof(reg),  &reg, sizeof(reg), &cb, 0);
      if (!fResult || (reg.reg_Flags & 0x0001)) {
        AfxMessageBox(IDP_WRITE_ERROR,MB_OK|MB_ICONERROR);
        success = false;
        break;
      }
      m_sectorOut += outSize / SECTOR_SIZE;
      m_sizeWritten += outSize;

    } else
#endif // not i386
    {
      DWORD written = 0;
      if (!WriteFile(m_outputDevice, outData, outSize, &written, NULL) || written != outSize) {
        ShowError(GetLastError(), IDP_WRITE_ERROR);
        success = false;
        break;
      }
      m_sizeWritten += written;
    }

    if (decomp && decomp->outputSpace() == 0)
      decomp->SetOutputSpace(m_outputBuffer, OUTPUT_BUF_SIZE);
    if (!decomp) {
      UnmapViewOfFile(m_fsImage);
      if (!AdvanceMapOffset()) break;
      if (!MapInputView()) break;
    }
  }
  m_progress.ShowWindow(SW_HIDE);
  ow->SetWindowPos(NULL, owr.left, owr.top, owr.Width(), owr.Height(), SWP_NOACTIVATE|SWP_NOZORDER);

  if (m_fsImage) { UnmapViewOfFile(m_fsImage); m_fsImage = NULL; }

#ifdef _M_IX86  // special case for legacy versions on arch=i386
  if (m_usingVXD) {
    CloseHandle(m_outputDevice); m_outputDevice = INVALID_HANDLE_VALUE;
  } else
#endif
  {
    DWORD bytes = 0;
    DeviceIoControl(m_outputDevice, FSCTL_UNLOCK_VOLUME, NULL, 0, NULL, 0, &bytes, NULL);
    DeviceIoControl(m_outputDevice, IOCTL_DISK_UPDATE_PROPERTIES, NULL, 0, NULL, 0, &bytes, NULL);
    CloseHandle(m_outputDevice); m_outputDevice = INVALID_HANDLE_VALUE;
  }

  if (decomp) {
    if (decomp->isError()) {
      CString msg;
      msg.LoadString(IDP_DECOMP_ERROR);
      m_output += "\r\n" + msg;
      success = false;
    }
    decomp->Delete();
  }

  if (success) {
    CString msg, len;
    FormatSize(m_sizeWritten, len);
    msg.Format(IDS_SUCCESS, len);
    m_output += msg;
  }
  ShowOutput();
}

void CRawrite32Dlg::CloseInputFile()
{
  if (m_inputFile == INVALID_HANDLE_VALUE) return;
  if (m_fsImage) { UnmapViewOfFile(m_fsImage); m_fsImage = NULL; }
  CloseHandle(m_inputMapping); m_inputMapping = NULL;
  CloseHandle(m_inputFile); m_inputFile = INVALID_HANDLE_VALUE;
}

bool CRawrite32Dlg::MapInputView()
{
  if (m_fsImage) UnmapViewOfFile(m_fsImage);
  DWORD64 remaining = MAP_VIEW_SIZE;
  if (m_inputFileSize - m_fileOffset < MAP_VIEW_SIZE)
    remaining = m_inputFileSize - m_fileOffset;
  if (remaining == 0) {
    m_fsImageSize = 0;
    m_fsImage = NULL;
    return false;
  } else {
    m_fsImageSize = (DWORD)remaining;
    m_fsImage = (const BYTE *)MapViewOfFile(m_inputMapping, FILE_MAP_READ, (DWORD)(m_fileOffset >> 32), (DWORD)m_fileOffset, m_fsImageSize);
    return true;
  }
}

bool CRawrite32Dlg::AdvanceMapOffset()
{
  if (m_fileOffset >= m_inputFileSize) return false;
  m_fileOffset += MAP_VIEW_SIZE;
  if (m_fileOffset > m_inputFileSize)
    m_fileOffset = m_inputFileSize;
  return true;
}

void CRawrite32Dlg::FormatSize(DWORD64 sz, CString &out)
{
  CString unit, t;
  if (sz > 1024*1024) {
    double v = (double)sz/(double)(1024*1024);
    if (v < 1024.0) {
      t.Format("%.1f", v);
      unit.LoadString(IDS_SIZE_MBYTE);
    } else {
      t.Format("%.2f", v/1024.0);
      unit.LoadString(IDS_SIZE_GBYTE);
    }
  } else {
    t.Format("%lu", (DWORD)sz);
    unit.LoadString(IDS_SIZE_BYTE);
  }
  out = t + " " + unit;
}

bool CRawrite32Dlg::OpenInputFile(HANDLE hFile)
{
  CWaitCursor hourglass;
  bool retVal = false;

  CloseInputFile();
  m_inputFileSize = 0;
  m_sizeWritten = 0;
  m_inputFile = hFile;

  DWORD sizeHigh = 0;
  m_fsImageSize = GetFileSize(m_inputFile, &sizeHigh);
  m_inputMapping = CreateFileMapping(m_inputFile, NULL, PAGE_READONLY, sizeHigh, m_fsImageSize, NULL);
  if (m_inputMapping != NULL) {
    m_inputFileSize = (DWORD64)m_fsImageSize | ((DWORD64)sizeHigh<<32);
    m_output.LoadString(IDS_CALCULATING_HASHES);
    ShowOutput();
    Poll();
    CString hashValues;
    CalcHashes(hashValues);
    CString size;
    FormatSize(m_inputFileSize, size);
    m_output.Format(IDS_MESSAGE_INPUT_HASHES, m_imageName, size);
    m_output += "\r\n" + hashValues + "\r\n";
    // show message
    ShowOutput();
    retVal = true;
    
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

  return retVal;
}

bool CRawrite32Dlg::VerifyInput()
{
  bool retVal = FALSE;
  BOOL valid = FALSE;
  bool showMsg = FALSE;
  CString dummy;

  if (m_inputMapping == NULL) goto done;

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
    AfxMessageBox(IDP_BAD_SKIP,MB_OK|MB_ICONERROR);
  return retVal;
}

void CRawrite32Dlg::OnChangeSectorSkip() 
{
  VerifyInput();
}

// We run all hashes in their own thread, to use as many cpus as available.
// The worker function loops around blocks of data processed between the DataAvailable event and sets the DataDone event after each block.
// When there is no more input data, it exits.
struct HashThreadState {
  IGenericHash *hashImpl;
  HANDLE DataAvailable, DataDone;
  const BYTE *input;
  DWORD inputLen;
  DWORD64 inputFileTotalSize, inputDataUsed;
  volatile LONG percDone;
};

UINT __cdecl hashThreadWorker(void *token)
{
  HashThreadState *hash = (HashThreadState*)token;
  for (;;) {
    WaitForSingleObject(hash->DataAvailable, INFINITE);
    if (hash->input == NULL) break;
    hash->hashImpl->AddData(hash->input, hash->inputLen);
    hash->inputDataUsed += hash->inputLen;
    DWORD perc = (DWORD)((double)hash->inputDataUsed/(double)hash->inputFileTotalSize*100.0);
    InterlockedExchange(&hash->percDone, perc);
    SetEvent(hash->DataDone);
  }
  return 0;
}

void CRawrite32Dlg::CalcHashes(CString &out)
{
  CWnd *ow = GetDlgItem(IDC_OUTPUT);
  CRect owr, pgr; ow->GetWindowRect(&owr); ScreenToClient(&owr);
  m_progress.GetWindowRect(&pgr);
  ow->SetWindowPos(NULL, owr.left, owr.top, owr.Width(), owr.Height()-pgr.Height(), SWP_NOACTIVATE|SWP_NOZORDER);

  vector<HashThreadState> hashes;
  vector<HANDLE> handles;
  {
    vector<IGenericHash*> funcs;
    GetAllHashes(funcs);
    for (size_t i = 0; i < funcs.size(); i++) {
      HashThreadState s;
      memset(&s, 0, sizeof s);
      s.hashImpl = funcs[i];
      s.DataAvailable = CreateEvent(NULL, FALSE, FALSE, NULL);
      s.DataDone = CreateEvent(NULL, FALSE, FALSE, NULL);
      s.inputFileTotalSize = m_inputFileSize;
      handles.push_back(s.DataDone);
      hashes.push_back(s);
    }
  }

#ifdef HASH_TOTAL_TIME
  DWORD totalStart = ::GetTickCount();
#endif
#ifdef HASH_BENCHMARK
  m_fsImage = (const BYTE *)MapViewOfFile(m_inputMapping, FILE_MAP_READ, 0, 0, m_inputFileSize);
  for (size_t i = 0; i < hashes.size(); i++) {
    DWORD start = ::GetTickCount();
    hashes[i].hashImpl->AddData(m_fsImage, m_inputFileSize);
    DWORD done = ::GetTickCount();
    CString t; t.Format("%s calculated in %u ms", hashes[i].hashImpl->HashName(), done-start);
    out += "\r\n" + t;
  }
  UnmapViewOfFile(m_fsImage); m_fsImage = NULL;
#else
  m_progress.SetRange32(0, 100*(int)hashes.size());
  m_progress.ShowWindow(SW_SHOW);

  // create one worker thread per hash
  for (size_t i = 0; i < hashes.size(); i++)
    AfxBeginThread(hashThreadWorker, &hashes[i]);

  // loop over the whole input file
  EnableWindow(FALSE);
  m_fileOffset = 0;
  for (;;) {
    if (!MapInputView()) break;
    handles.clear();
    for (size_t i = 0; i < hashes.size(); i++) {
      hashes[i].input = m_fsImage;
      hashes[i].inputLen = m_fsImageSize;
      handles.push_back(hashes[i].DataDone);
      SetEvent(hashes[i].DataAvailable);
    }
    do {
      DWORD perc = 0;
      for (size_t i = 0; i < hashes.size(); i++)
        perc += hashes[i].percDone;
      m_progress.SetPos(perc);
      Poll();
    } while (WaitAndPoll(handles, 750));
    if (!AdvanceMapOffset()) break;
  }
  EnableWindow();
  // done, notify worker threads
  for (size_t i = 0; i < hashes.size(); i++) {
    hashes[i].input = NULL;
    hashes[i].inputLen = 0;
    SetEvent(hashes[i].DataAvailable);
  }
  UnmapViewOfFile(m_fsImage); m_fsImage = NULL;

  // collect output
  CString t;
  for (size_t i = 0; i < hashes.size(); i++) {
    enum { outSplitLen = 64 };
    CString out,name,p; hashes[i].hashImpl->HashResult(out);
    name.Format("%-8s", hashes[i].hashImpl->HashName());
    t += "\r\n" + name;
    while (out.GetLength() > outSplitLen) {
      t += out.Left(outSplitLen) + "\r\n        ";
      out = out.Mid(outSplitLen);
    }
    t += out;
    hashes[i].hashImpl->Delete();
    CloseHandle(hashes[i].DataAvailable);
    CloseHandle(hashes[i].DataDone);
  }
  out = t;
#endif
#ifdef HASH_TOTAL_TIME
  {
    DWORD totalEnd = ::GetTickCount();
    CString t; t.Format("\r\n\r\ntotal time %u ms", totalEnd-totalStart);
    out += t;
  }
#endif

  m_progress.ShowWindow(SW_HIDE);
  ow->SetWindowPos(NULL, owr.left, owr.top, owr.Width(), owr.Height(), SWP_NOACTIVATE|SWP_NOZORDER);
}

void CRawrite32Dlg::Poll()
{
  MSG msg;
  while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }
  SetCursor(AfxGetApp()->LoadStandardCursor(IDC_WAIT));
}

bool CRawrite32Dlg::WaitAndPoll(vector<HANDLE> &handles, DWORD timeout)
{
  for (;;) {
    DWORD res = MsgWaitForMultipleObjects(handles.size(), &handles[0], FALSE, timeout, QS_PAINT|QS_TIMER);
    if (res >= WAIT_OBJECT_0 && res < WAIT_OBJECT_0+handles.size()) {
      size_t ndx = res-WAIT_OBJECT_0;
      handles.erase(handles.begin()+ndx);
      if (handles.empty()) 
        return false;
    } else if (res == WAIT_TIMEOUT) {
      return true;
    } else if (res != WAIT_OBJECT_0+handles.size()) {
      ASSERT(FALSE);
    }
    Poll();
  }
}
