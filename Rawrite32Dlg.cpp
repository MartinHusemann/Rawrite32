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

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

// single threaded hash calculation with timing
// #define HASH_BENCHMARK
// log total hash time
// #define HASH_TOTAL_TIME
// do not write output, similar to writing to /dev/null on unix
// #define NULL_OUTPUT

// XXX - just blindly assume we write to a 512-bye sectored medium if using old (win9x) systems.
#define SECTOR_SIZE   512 // this value is not used on WinNT based systems

// size of the important data at the start of a disk carrying partition info
#define PARTITION_INFO_SIZE (16*1024)

// size of the view into the input image
#define MAP_VIEW_SIZE (16*1024*1024)

// size of the decompression buffer
#define OUTPUT_BUF_SIZE (128*1024*1024)

// decompression output has to be a multiple of this before swapping buffers
#define OUTPUT_BUF_CHUNK_SIZE  (1*1024*1024)

// maximum size of a single write request to the raw output device
#define MAX_WRITE_CHUNK (1*1024*1024)

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
static void DoSurfHome()
{
  CString url;
  url.LoadString(IDS_HOME_URL);
  ShellExecute(NULL, _T("open"), url, NULL, NULL, SW_SHOWNORMAL);
}

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
  DoSurfHome();
}

/////////////////////////////////////////////////////////////////////////////
// CSecSkipOptionsDlg dialog used for App About
class CSecSkipOptionsDlg : public CDialog
{
public:
  CSecSkipOptionsDlg() : CDialog(IDD) {}

private:
  enum { IDD = IDD_SEC_SKIP_OPTIONS };

protected:
  virtual void DoDataExchange(CDataExchange*pDX);

public:
  long m_sekCount;
};

void CSecSkipOptionsDlg::DoDataExchange(CDataExchange*pDX)
{
  DDX_Text(pDX, IDC_SECTOR_SKIP, m_sekCount);
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
  lc.InsertColumn(1, _T(""), 0, r.Width()-GetSystemMetrics(SM_CXVSCROLL)-1);
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
class CMenuIcon : public CButton
{
public:
  CMenuIcon(CMenu *popup, UINT icon) : m_menu(popup) { m_icon = AfxGetApp()->LoadIcon(icon); }
protected:
  virtual void PostNcDestroy() { delete this; }
  virtual void DrawItem(LPDRAWITEMSTRUCT);
  afx_msg void OnLButtonDown(UINT flags, CPoint point);
  DECLARE_MESSAGE_MAP()
protected:
  HICON m_icon;
  CMenu *m_menu;
};

BEGIN_MESSAGE_MAP(CMenuIcon, CButton)
  ON_WM_LBUTTONDOWN()
END_MESSAGE_MAP()

void CMenuIcon::DrawItem(LPDRAWITEMSTRUCT lpdi)
{
  DrawIcon(lpdi->hDC, lpdi->rcItem.left, lpdi->rcItem.top, m_icon);
}

void CMenuIcon::OnLButtonDown(UINT /*flags*/, CPoint point)
{
  ((CRawrite32Dlg*)GetParent())->UpdateMenu(m_menu);

  CRect r;
  GetWindowRect(&r);
  ClientToScreen(&point);
  m_menu->TrackPopupMenu(TPM_RIGHTALIGN, r.right, point.y, GetParent());
}

/////////////////////////////////////////////////////////////////////////////
// CRawrite32Dlg dialog

CRawrite32Dlg::CRawrite32Dlg(LPCTSTR imageFileName)
  : CDialog(CRawrite32Dlg::IDD, NULL), m_fsImage(NULL), m_fsImageSize(0), m_sectorSkip(0),
    m_inputFile(INVALID_HANDLE_VALUE), m_inputMapping(NULL),
    m_curInput(NULL), m_sizeRemaining(0), m_sizeWritten(0),
    m_inputFileSize(0), m_fileOffset(0),m_outputDevice(INVALID_HANDLE_VALUE),
    m_decompOutputSpaceAvailable(INVALID_HANDLE_VALUE),
    m_decompOutputAvailable(INVALID_HANDLE_VALUE),
    m_decomp(NULL),
    m_decompOutputLen(0), m_curDecompTarget(0), m_decompForcedExit(0), m_writerIdle(0),
    m_outputBuffer(new BYTE[OUTPUT_BUF_SIZE]), m_writeTargetLogicalVolume(false)
#ifdef _M_IX86
    , m_usingVXD(RunningOnDOS()), m_sectorOut(0)
#endif
{
  m_hIcon = (HICON)::LoadImage(AfxGetResourceHandle(), MAKEINTRESOURCE(IDR_MAINFRAME), IMAGE_ICON, 32, 32, LR_DEFAULTCOLOR);
  m_hSmallIcon = (HICON)::LoadImage(AfxGetResourceHandle(), MAKEINTRESOURCE(IDR_MAINFRAME), IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR);
  EnableAutomation();
  m_imageName = imageFileName;
  m_output.LoadString(IDS_START_HINT);
  if (m_usingVXD) m_writeTargetLogicalVolume = true;  // can only access DOS drives
  m_mainMenu.LoadMenu(IDR_MAINFRAME);
  UpdateMenu(&m_mainMenu);
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
  ON_WM_PAINT()
  ON_WM_QUERYDRAGICON()
  ON_WM_DESTROY()
  ON_WM_EXITMENULOOP()
  ON_BN_CLICKED(IDC_BROWSE, OnBrowse)
  ON_EN_CHANGE(IDC_IMAGE_NAME, OnNewImage)
  ON_BN_CLICKED(IDC_WRITE_DISK, OnWriteImage)
  ON_COMMAND(IDD_ABOUTBOX, OnAboutDlg)
  ON_COMMAND(IDC_SURF_HOME, OnSurfHome)
  ON_COMMAND(IDD_HASH_OPTIONS, OnHashOptions)
  ON_COMMAND(IDD_SEC_SKIP_OPTIONS, OnSecSkipOptions)
  ON_COMMAND(IDM_USE_VOLUMES, OnUseVolumes)
  ON_COMMAND(IDM_USE_PHYSDISKS, OnUsePhysDisks)
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

#if _MSC_VER < 1600
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
#endif

/////////////////////////////////////////////////////////////////////////////
// CRawrite32Dlg message handlers
void CRawrite32Dlg::EnumPhysicalDrives()
{
  ASSERT(!m_usingVXD);

  // find all "floppy" devices first...
  TCHAR allDrives[32*4 + 10];
  GetLogicalDriveStrings(sizeof allDrives/sizeof allDrives[0], allDrives);
  LPCTSTR drive;
  for (drive = allDrives; *drive; drive += _tcslen(drive)+1) {
    DWORD type = GetDriveType(drive);
    if (type != DRIVE_REMOVABLE) continue;
    CString volName(drive, 2);
    TCHAR d = volName[0];
    if (d != 'a' && d != 'A' && d != 'b' && d != 'B') break;  // we are only interested in floppy drives here
    DriveSelectionEntry driveDesc;
    driveDesc.volumes.push_back(volName);
    driveDesc.driveNumber = ~0U;
    driveDesc.internalFileName.Format(_T("\\\\.\\%s"), volName);

    CString logName, fsName;
    GetVolumeInformation(drive, logName.GetBuffer(200), 200, NULL, NULL, NULL, fsName.GetBuffer(200), 200);
    logName.ReleaseBuffer(); fsName.ReleaseBuffer();
    if (logName.IsEmpty())
      driveDesc.deviceName = fsName;
    else
      driveDesc.deviceName = logName + ", " + fsName;
    m_driveData.push_back(driveDesc);
  }

  size_t errCnt = 0;
  for (DWORD i = 0; ; i++) {
    CString internalFileName;
    internalFileName.Format(_T("\\\\.\\PhysicalDrive%u"), i);
    HANDLE outputDevice = CreateFile(internalFileName, 0, FILE_SHARE_READ|FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    if (outputDevice == INVALID_HANDLE_VALUE) {
      errCnt++;
      if (errCnt > 48) break;
      continue;
    }
    errCnt = 0;
    DriveSelectionEntry driveDesc;
    driveDesc.driveNumber = i;
    driveDesc.internalFileName = internalFileName;
    DWORD bytes = 0;

    union {
      STORAGE_DEVICE_DESCRIPTOR header;
      TCHAR buf[1024];
    } desc;
    STORAGE_PROPERTY_QUERY qry;
    memset(&qry, 0, sizeof qry);
    qry.PropertyId = StorageDeviceProperty;
    qry.QueryType = PropertyStandardQuery;
    if (DeviceIoControl(outputDevice, IOCTL_STORAGE_QUERY_PROPERTY, &qry, sizeof qry, &desc.header, sizeof desc, &bytes, NULL)
        && bytes >= sizeof(STORAGE_DEVICE_DESCRIPTOR)) {
      const char *strings = (const char *)&desc;
      CString info;
      if (desc.header.VendorIdOffset || desc.header.ProductIdOffset) {
        if (desc.header.VendorIdOffset) {
          CString t(strings + desc.header.VendorIdOffset);
          t.Trim();
          info = t;
        }
        if (desc.header.ProductIdOffset) {
          CString t(strings + desc.header.ProductIdOffset);
          t.Trim();
          if (!info.IsEmpty()) info += " ";
          info += t;
        }
        driveDesc.deviceName = info;
      }
    }
    DISK_GEOMETRY geom;
    if (DeviceIoControl(outputDevice, IOCTL_DISK_GET_DRIVE_GEOMETRY, NULL, 0, &geom, sizeof geom, &bytes, NULL)) {
      DWORD64 size = (DWORD64)geom.Cylinders.QuadPart * geom.SectorsPerTrack * geom.TracksPerCylinder;
      FormatSize(size, driveDesc.size,geom.BytesPerSector);
    }
    CloseHandle(outputDevice);
    m_driveData.push_back(driveDesc);
  }

  // find logical volumes on the physical drives
  for (drive = allDrives; *drive; drive += _tcslen(drive)+1) {
    CString internalFileName, vol(drive,2);
    DWORD type = GetDriveType(drive);
    if (type == DRIVE_REMOVABLE ||type == DRIVE_CDROM || type == DRIVE_REMOTE || type == DRIVE_RAMDISK) continue;
    internalFileName.Format(_T("\\\\.\\%s"), vol);
    HANDLE outputDevice = CreateFile(internalFileName, 0, FILE_SHARE_READ|FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    if (outputDevice != INVALID_HANDLE_VALUE) {
      STORAGE_DEVICE_NUMBER sdn;
      DWORD bytes = 0;
      if (DeviceIoControl(outputDevice, IOCTL_STORAGE_GET_DEVICE_NUMBER, NULL, 0, &sdn, sizeof sdn, &bytes, NULL)) {
        for (size_t i = 0; i < m_driveData.size(); i++) {
          if (m_driveData[i].driveNumber == sdn.DeviceNumber)
            m_driveData[i].volumes.push_back(vol);
        }
      }
      CloseHandle(outputDevice);
    }
  }
}

void CRawrite32Dlg::EnumLogicalVolumes()
{
  TCHAR allDrives[32*4 + 10];
  GetLogicalDriveStrings(sizeof allDrives/sizeof allDrives[0], allDrives);

  LPCTSTR drive;
  for (drive = allDrives; *drive; drive += _tcslen(drive)+1) {
    
    DWORD type = GetDriveType(drive);
    if (type != DRIVE_CDROM && type != DRIVE_REMOTE && type != DRIVE_RAMDISK) {
      CString volName(drive, 2);
      DriveSelectionEntry driveDesc;
      driveDesc.volumes.push_back(volName);
      driveDesc.driveNumber = drive[0] - 'A';
      driveDesc.internalFileName.Format(_T("\\\\.\\%s"), volName);

      CString logName, fsName;
      GetVolumeInformation(drive, logName.GetBuffer(200), 200, NULL, NULL, NULL, fsName.GetBuffer(200), 200);
      logName.ReleaseBuffer(); fsName.ReleaseBuffer();
      if (logName.IsEmpty())
        driveDesc.deviceName = fsName;
      else
        driveDesc.deviceName = logName + ", " + fsName;
      m_driveData.push_back(driveDesc);
    }
  }
}

void CRawrite32Dlg::FillDriveCombo()
{
  CWaitCursor hourglass;
  m_driveData.clear();
  if (m_writeTargetLogicalVolume)
    EnumLogicalVolumes();
  else
    EnumPhysicalDrives();

  CString winDir, sysDir;
  GetWindowsDirectory(winDir.GetBuffer(MAX_PATH), MAX_PATH); winDir.ReleaseBuffer(); winDir.MakeUpper();
  GetSystemDirectory(sysDir.GetBuffer(MAX_PATH), MAX_PATH); sysDir.ReleaseBuffer(); sysDir.MakeUpper();
  for (size_t i = 0; i < m_driveData.size(); i++) {
    for (size_t j = 0; j < m_driveData[i].volumes.size(); j++) {
      CString drive = m_driveData[i].volumes[j];
      drive.MakeUpper();
      // exclude the main disk from our list to avoid catastrophic disasters
      if (winDir[1] == ':' && winDir[0] == drive[0]) m_driveData[i].hidden = true;;
      if (sysDir[1] == ':' && sysDir[0] == drive[0]) m_driveData[i].hidden = true;;
    }
  }

  m_drives.ResetContent();
  for (size_t i = 0; i < m_driveData.size(); i++) {
    if (m_driveData[i].hidden) continue;
    CString display;
    if (m_driveData[i].volumes.empty()) {
      if (m_driveData[i].deviceName.IsEmpty()) {
        CString physString; physString.LoadString(IDS_UNKNOWN_PHYSDEV); 
        display.Format(_T("%s %u"), physString, m_driveData[i].driveNumber);
      }
    } else {
      for (size_t j = 0; j < m_driveData[i].volumes.size(); j++) {
        if (!display.IsEmpty()) display += " ";
        display += m_driveData[i].volumes[j];
      }
    }
    if (!m_driveData[i].deviceName.IsEmpty()) {
      if (!display.IsEmpty()) display += " ";
      display += m_driveData[i].deviceName;
    }
    if (!m_driveData[i].size.IsEmpty())
      display += " [" + m_driveData[i].size + "]";
    m_drives.SetItemData(m_drives.AddString(display), i);
  }
  
  if (m_drives.GetCount() > 0)
    m_drives.SetCurSel(m_drives.GetCount()-1);
}

void CRawrite32Dlg::UpdateMenu(CMenu *menu)
{
  if (menu) {
    menu->CheckMenuItem(IDM_USE_VOLUMES, MF_BYCOMMAND | (m_writeTargetLogicalVolume ? MF_CHECKED : MF_UNCHECKED));
    menu->CheckMenuItem(IDM_USE_PHYSDISKS, MF_BYCOMMAND | (m_writeTargetLogicalVolume ? MF_UNCHECKED : MF_CHECKED));
  }
}

BOOL CRawrite32Dlg::OnInitDialog()
{
  CDialog::OnInitDialog();

  CRect r;
  GetWindowRect(&r);
  m_origHeight = r.Height();

  m_outWinFont.CreateFont(-12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, ANSI_CHARSET,
                          OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
                          FIXED_PITCH|FF_DONTCARE, NULL);
  GetDlgItem(IDC_OUTPUT)->SetFont(&m_outWinFont);

  SetIcon(m_hIcon, TRUE);			    // Set big icon
  SetIcon(m_hSmallIcon, FALSE);		// Set small icon

  // two image buttons for drop down context menus
  (new CMenuIcon(m_mainMenu.GetSubMenu(1), IDI_OPTIONS))->SubclassDlgItem(IDC_OPTIONS_ICON, this);
  (new CMenuIcon(m_mainMenu.GetSubMenu(2), IDI_HELP))->SubclassDlgItem(IDC_HELP_ICON, this);

  // register the dialog as file drop target
  IDropTarget *dt;
  GetIDispatch(FALSE)->QueryInterface(IID_IDropTarget, (void**)&dt);
  RegisterDragDrop(m_hWnd, dt);
  dt->Release();

  FillDriveCombo();

  if (m_imageName)
    GetDlgItem(IDC_IMAGE_NAME)->SetWindowText(m_imageName);

  return TRUE;  // return TRUE  unless you set the focus to a control
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

void CRawrite32Dlg::ShowMainMenu(bool showIt)
{
  int dy = GetSystemMetrics(SM_CYMENU);
  CRect r;
  GetWindowRect(&r);
  int w = r.Width();
  int h = r.Height();
  if (showIt) {
    SetMenu(&m_mainMenu);
    h += dy;
  } else {
    SetMenu(NULL);
    h = m_origHeight;
  }
  SetWindowPos(NULL, 0, 0, w, h, SWP_NOMOVE|SWP_NOZORDER);
}

BOOL CRawrite32Dlg::PreTranslateMessage(MSG *pMsg)
{
  // check for the UP transition of the ALT key as syskey (i.e. not after use as an accelerator)
  // or F10 to popup the menu
  if ((pMsg->message == WM_SYSKEYUP && (pMsg->wParam & 0x0ffff) == VK_MENU) ||
      (pMsg->message == WM_SYSKEYDOWN && (pMsg->wParam & 0x0ffff) == VK_F10))  {
    // only if currently there is no menu...
    if (GetMenu() == NULL) {
      ShowMainMenu(true);
      return FALSE; // process this message further, this will make the app enter the menu loop
    }
  }
  return CDialog::PreTranslateMessage(pMsg);
}

void CRawrite32Dlg::OnExitMenuLoop(BOOL isPopup)
{
  // if the main menu loop was terminated, hide the menu again
  if (!isPopup) ShowMainMenu(false);
  CDialog::OnExitMenuLoop(isPopup);
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

void CRawrite32Dlg::ShowError(DWORD err, UINT id, LPCTSTR arg)
{
  CString msg;
  if (arg)
    msg.Format(id, arg);
  else
    msg.LoadString(id);
  CString t; t.Format(_T("%s\r\n[#%u]"), msg, err);
  m_output += t;
  ShowOutput();
  AfxMessageBox(msg, MB_OK|MB_ICONERROR, id);
}

void CRawrite32Dlg::ShowOutput()
{
  UpdateData(FALSE);
  CEdit* edit = (CEdit*)GetDlgItem(IDC_OUTPUT);
  int n = m_output.GetLength();
  edit->SetSel(n,n);
}

UINT CRawrite32Dlg::dcompressionStarter(void *token)
{
  return ((CRawrite32Dlg*)token)->BackgroundDecompressor();
}

UINT CRawrite32Dlg::BackgroundDecompressor()
{
  for (;;) {
    if (m_decompForcedExit)
      break;

    WaitForSingleObject(m_decompOutputSpaceAvailable, INFINITE);
    if (m_decompForcedExit || m_decomp->allDone())
      break;
    DWORD outBufSize = OUTPUT_BUF_SIZE/2;
    m_decomp->SetOutputSpace(m_outputBuffer + m_curDecompTarget*(OUTPUT_BUF_SIZE/2), outBufSize);

decompMore:
    if (m_decompForcedExit)
      break;

    if (m_writerIdle) {
      size_t space = m_decomp->outputSpace();
      size_t newSpace =  space & (OUTPUT_BUF_CHUNK_SIZE-1);
      outBufSize -= space-newSpace;
      m_decomp->LimitOutputSpace(newSpace);
    }
    if (m_decomp->isError())
      break;
    if (!m_decomp->allDone() && m_decomp->needInputData()) {
      UnmapViewOfFile(m_fsImage);
      if (AdvanceMapOffset() && MapInputView()) {
        m_decomp->AddInputData(m_fsImage, m_fsImageSize);
        if (m_decomp->outputSpace() > 0 && m_decomp->needInputData())
          goto decompMore;
      }
    }
    if (m_decomp->outputSpace() == outBufSize) {
      if (m_decomp->allDone())
        break;
      goto decompMore;
    }
    m_decompOutputLen = outBufSize - m_decomp->outputSpace();
    SetEvent(m_decompOutputAvailable);
  }
  m_decompOutputLen = 0;
  SetEvent(m_decompOutputAvailable);
  return 0;
}

void CRawrite32Dlg::OnWriteImage() 
{
  if (!VerifyInput())
    return;

  CString drive, msg, internalFileName;
  DWORD ddIndex = 0;
  DWORD driveIndex = 0;

  int ndx = m_drives.GetCurSel();
  if (ndx == CB_ERR) return;
  ddIndex = m_drives.GetItemData(ndx);
  if (ddIndex >= m_driveData.size()) return;
  m_drives.GetLBText(ndx, drive);
  internalFileName = m_driveData[ddIndex].internalFileName;

  msg.Format(IDP_ARE_YOU_SURE, drive);
  if (AfxMessageBox(msg, MB_YESNO|MB_ICONQUESTION, IDP_ARE_YOU_SURE) != IDYES)
    return;

  bool success = true;
  DWORD secSize = 512;
  m_sizeWritten = 0;
#ifdef _M_IX86  // special case for legacy versions on arch=i386
  if (m_usingVXD) {
    // Windows 9x: can't write to devices, need DOS services via vwin32.vdx
    m_outputDevice = CreateFile(_T("\\\\.\\vwin32"), 0, 0, NULL, 0, FILE_FLAG_DELETE_ON_CLOSE, NULL);
    if (m_outputDevice == INVALID_HANDLE_VALUE) {
      ShowError(GetLastError(), IDP_NO_VXD);
      return;
    }
    m_sectorOut = m_sectorSkip;
  } else
#endif
  {
    // Windows NT does it the UNIX way...
    m_outputDevice = CreateFile(internalFileName, GENERIC_WRITE, FILE_SHARE_READ|FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    if (m_outputDevice == INVALID_HANDLE_VALUE) {
      ShowError(GetLastError(), IDP_NO_DISK);
      return;
    }
    DWORD bytes = 0;
    if (m_writeTargetLogicalVolume) {
      if (DeviceIoControl(m_outputDevice, FSCTL_LOCK_VOLUME, NULL, 0, NULL, 0, &bytes, NULL) == 0) {
        ShowError(GetLastError(), IDP_CANT_LOCK_DISK);
        CloseHandle(m_outputDevice); m_outputDevice = INVALID_HANDLE_VALUE;
        return;
      }
    } else {
      for (size_t i = 0; i < m_driveData[ddIndex].volumes.size(); i++) {
        CString vol; vol.Format(_T("\\\\.\\%s"), m_driveData[ddIndex].volumes[i]);
        HANDLE h = CreateFile(vol, GENERIC_WRITE, FILE_SHARE_READ|FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
        if (h != INVALID_HANDLE_VALUE) {
          if (DeviceIoControl(h, FSCTL_IS_VOLUME_MOUNTED, NULL, 0, NULL, 0, &bytes, NULL)) {
            if (DeviceIoControl(h, FSCTL_DISMOUNT_VOLUME, NULL, 0, NULL, 0, &bytes, NULL) == 0) {
              ShowError(GetLastError(), IDP_CANT_UNMOUNT_VOLUME, m_driveData[ddIndex].volumes[i]);
              CloseHandle(m_outputDevice); m_outputDevice = INVALID_HANDLE_VALUE;
              return;
            }
          }
          CloseHandle(h);
        }
      }
      memset(m_outputBuffer, 0, PARTITION_INFO_SIZE);
      WriteFile(m_outputDevice, m_outputBuffer, PARTITION_INFO_SIZE, &bytes, NULL);
      DeviceIoControl(m_outputDevice, IOCTL_DISK_UPDATE_PROPERTIES, NULL, 0, NULL, 0, &bytes, NULL);
      CloseHandle(m_outputDevice);
      m_outputDevice = CreateFile(internalFileName, GENERIC_WRITE, FILE_SHARE_READ|FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    }

    DISK_GEOMETRY_EX diskInfo;
    if (DeviceIoControl(m_outputDevice, IOCTL_DISK_GET_DRIVE_GEOMETRY_EX, NULL, 0, &diskInfo, sizeof diskInfo, &bytes, NULL))
      secSize = diskInfo.Geometry.BytesPerSector;

    if (m_sectorSkip)
      SetFilePointer(m_outputDevice, m_sectorSkip*SECTOR_SIZE, NULL, FILE_BEGIN);
  }

  CWaitCursor hourglass;
  EnableDlgChildControls(false);
  m_fileOffset = 0;
  m_fsImageSize = 0;
  MapInputView();

  CWnd *ow = NULL;
  CRect owr;
  if (!m_usingVXD) {
    m_progress.SetRange(0, 100);
    m_progress.ShowWindow(SW_SHOW);

    ow = GetDlgItem(IDC_OUTPUT);
    CRect pgr; ow->GetWindowRect(&owr); ScreenToClient(&owr);
    m_progress.GetWindowRect(&pgr);
    ow->SetWindowPos(NULL, owr.left, owr.top, owr.Width(), owr.Height()-pgr.Height(), SWP_NOACTIVATE|SWP_NOZORDER);
  }

  // try to start decompressor
  m_decomp = StartDecompress(m_fsImage, m_fsImageSize);
  // only if we decompress we need a worker thread
  if (m_decomp) {
    // ready to go, now invoke the background thread to run the decompression loop...
    m_writerIdle = 0;
    m_decompForcedExit = 0;
    m_decompOutputSpaceAvailable = CreateEvent(NULL, FALSE, TRUE, NULL);
    m_decompOutputAvailable = CreateEvent(NULL, FALSE, FALSE, NULL);
    m_decompOutputLen = 0;
    m_curDecompTarget = 0;
    AfxBeginThread(dcompressionStarter, this);
  }

  IGenericHash *outHash = GetFastHash();
  for (;;) {

    if (!m_usingVXD) UpdateWriteProgress();

    const BYTE *outData = NULL;
    DWORD outSize = 0;

    if (m_decomp) {
      InterlockedExchange(&m_writerIdle, 1);
      vector<HANDLE> objs;
      objs.push_back(m_decompOutputAvailable);
      WaitAndPoll(objs, INFINITE);
      InterlockedExchange(&m_writerIdle, 0);
      outData = m_outputBuffer + m_curDecompTarget*(OUTPUT_BUF_SIZE/2);
      outSize = m_decompOutputLen;
      if (outSize == 0) break;
      m_curDecompTarget = m_curDecompTarget ? 0 : 1;
      SetEvent(m_decompOutputSpaceAvailable);
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
      reg.reg_EAX = driveIndex;
      reg.reg_ECX = outSize / SECTOR_SIZE;
      reg.reg_EDX = m_sectorOut;
      reg.reg_EBX = (DWORD)outData;
      reg.reg_Flags = 0x0001; // assume error

      DWORD cb = 0;
      BOOL fResult = DeviceIoControl(m_outputDevice,  VWIN32_DIOC_DOS_INT26, &reg, sizeof(reg),  &reg, sizeof(reg), &cb, 0);
      if (!fResult || (reg.reg_Flags & 0x0001)) {
        InterlockedExchange(&m_decompForcedExit, 1);
        SetEvent(m_decompOutputSpaceAvailable);
        AfxMessageBox(IDP_WRITE_ERROR,MB_OK|MB_ICONERROR);
        success = false;
        break;
      }
      outHash->AddData(outData, outSize);
      m_sectorOut += outSize / SECTOR_SIZE;
      m_sizeWritten += outSize;

    } else
#endif // not i386
    {
      while (outSize > 0) {
        DWORD written = 0;
        DWORD size = outSize;
        if (size > MAX_WRITE_CHUNK) size = MAX_WRITE_CHUNK;
        outHash->AddData(outData, size);
#ifndef NULL_OUTPUT
        if (!WriteFile(m_outputDevice, outData, size, &written, NULL) || written != size) {
          InterlockedExchange(&m_decompForcedExit, 1);
          SetEvent(m_decompOutputSpaceAvailable);
          ShowError(GetLastError(), IDP_WRITE_ERROR);
          success = false;
          break;
        }
#else
        written = size;
#endif
        outData += written;
        m_sizeWritten += written;
        outSize -= written;
        UpdateWriteProgress();
      }
    }
    if (!success)
      break;

    if (!m_decomp) {
      UnmapViewOfFile(m_fsImage);
      if (!AdvanceMapOffset()) break;
      if (!MapInputView()) break;
    }
  }
  if (m_decomp && !success) {
    // wait for decompression thread to exit
    vector<HANDLE> handles;
    handles.push_back(m_decompOutputAvailable);
    WaitAndPoll(handles, INFINITE);
  }
  if (!m_usingVXD) {
    m_progress.ShowWindow(SW_HIDE);
    ow->SetWindowPos(NULL, owr.left, owr.top, owr.Width(), owr.Height(), SWP_NOACTIVATE|SWP_NOZORDER);
  }
  EnableDlgChildControls(true);

  if (m_decomp) {
    CloseHandle(m_decompOutputSpaceAvailable);
    CloseHandle(m_decompOutputAvailable);
  }

  if (m_fsImage) { UnmapViewOfFile(m_fsImage); m_fsImage = NULL; }

#ifdef _M_IX86  // special case for legacy versions on arch=i386
  if (m_usingVXD) {
    CloseHandle(m_outputDevice); m_outputDevice = INVALID_HANDLE_VALUE;
  } else
#endif
  {
    DWORD bytes = 0;
    if (m_writeTargetLogicalVolume)
      DeviceIoControl(m_outputDevice, FSCTL_UNLOCK_VOLUME, NULL, 0, NULL, 0, &bytes, NULL);
    DeviceIoControl(m_outputDevice, IOCTL_DISK_UPDATE_PROPERTIES, NULL, 0, NULL, 0, &bytes, NULL);
    CloseHandle(m_outputDevice); m_outputDevice = INVALID_HANDLE_VALUE;
  }

  if (m_decomp) {
    if (m_decomp->isError()) {
      CString msg;
      msg.LoadString(IDP_DECOMP_ERROR);
      m_output += "\r\n" + msg;
      success = false;
    }
    m_decomp->Delete();
  }

  if (success) {
    CString msg, len, hash;
    outHash->HashResult(hash);
    FormatSize(m_sizeWritten, len);
    msg.Format(IDS_SUCCESS, len, outHash->HashName(), hash);
    m_output += msg += "\r\n\r\n";
  }
  outHash->Delete();
  ShowOutput();
  FillDriveCombo();
}

void CRawrite32Dlg::EnableDlgChildControls(bool enable)
{
  GetDlgItem(IDC_WRITE_DISK)->EnableWindow(enable);
  GetDlgItem(IDC_DRIVES)->EnableWindow(enable);
  GetDlgItem(IDC_BROWSE)->EnableWindow(enable);
  GetDlgItem(IDC_IMAGE_NAME)->EnableWindow(enable);
//  GetDlgItem(IDC_SECTOR_SKIP)->EnableWindow(enable);
}

void CRawrite32Dlg::UpdateWriteProgress()
{
  double full = (double)m_inputFileSize, cur = (double)m_fileOffset;
  int perc = (int)(cur/full*100.0+.5);
  m_progress.SetPos(perc);
  CString si, sw, out;
  FormatSize(m_fileOffset, si);
  FormatSize(m_sizeWritten, sw);
  out.Format(IDS_WRITE_PROGRESS, si, sw);
  GetDlgItem(IDC_OUTPUT)->SetWindowText(out);
  Poll();
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

void CRawrite32Dlg::FormatSize(DWORD64 sz, CString &out, DWORD addFactor)
{
  double v = (double)sz;
  if (addFactor != 1) v *= (double)addFactor;
  CString unit, t;
  if (v < 1024.0) {
    t.Format(_T("%lu"), (DWORD)sz * addFactor);
    unit.LoadString(IDS_SIZE_BYTE);
  } else if (v < 1024.0*1024.0) {
    t.Format(_T("%.1f"), v/1024.0);
    unit.LoadString(IDS_SIZE_KBYTE);
  } else if (v < 1024.0*1024.0*1024.0) {
    t.Format(_T("%.1f"), v/(1024.0*1024.0));
    unit.LoadString(IDS_SIZE_MBYTE);
  } else {
    t.Format(_T("%.2f"), v/(1024.0*1024.0*1024.0));
    unit.LoadString(IDS_SIZE_GBYTE);
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
      HANDLE hGlob = GlobalAlloc(GMEM_MOVEABLE, size*sizeof(TCHAR));
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
  bool retVal = false;
  BOOL valid = FALSE;
  bool showMsg = FALSE;
  CString dummy;

  if (m_inputMapping == NULL) goto done;

  //GetDlgItemText(IDC_SECTOR_SKIP, dummy);
  //m_sectorSkip = GetDlgItemInt(IDC_SECTOR_SKIP, &valid, FALSE);
  if (!dummy.IsEmpty() && !valid) {
    showMsg = TRUE;
    goto done;
  }
  if (!valid) m_sectorSkip = 0;

  retVal = m_drives.GetCount() > 0;

done:
  GetDlgItem(IDC_WRITE_DISK)->EnableWindow(retVal);
  if (showMsg)
    AfxMessageBox(IDP_BAD_SKIP,MB_OK|MB_ICONERROR);
  return retVal;
}

void CRawrite32Dlg::OnUseVolumes()
{
  if (m_writeTargetLogicalVolume) return;
  m_writeTargetLogicalVolume = true;
  FillDriveCombo();
  UpdateMenu(&m_mainMenu);
}

void CRawrite32Dlg::OnUsePhysDisks()
{
  if (!m_writeTargetLogicalVolume) return;
  m_writeTargetLogicalVolume = false;
  FillDriveCombo();
  UpdateMenu(&m_mainMenu);
}

void CRawrite32Dlg::OnSecSkipOptions()
{
  CSecSkipOptionsDlg dlg;
  dlg.m_sekCount = m_sectorSkip;
  if (dlg.DoModal() == IDOK && dlg.m_sekCount >= 0) {
    m_sectorSkip = dlg.m_sekCount;
    VerifyInput();
  }
}

void CRawrite32Dlg::OnAboutDlg()
{
  CAboutDlg().DoModal();
}

void CRawrite32Dlg::OnSurfHome()
{
  DoSurfHome();
}

void CRawrite32Dlg::OnHashOptions()
{
  CHashOptionsDlg().DoModal();
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
  SetEvent(hash->DataDone);
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
  EnableDlgChildControls(false);
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
  EnableDlgChildControls(true);
  // done, notify worker threads
  handles.clear();
  for (size_t i = 0; i < hashes.size(); i++) {
    hashes[i].input = NULL;
    hashes[i].inputLen = 0;
    handles.push_back(hashes[i].DataDone);
    SetEvent(hashes[i].DataAvailable);
  }
  WaitAndPoll(handles, INFINITE);
  UnmapViewOfFile(m_fsImage); m_fsImage = NULL;

  // collect output
  CString t;
  for (size_t i = 0; i < hashes.size(); i++) {
    enum { outSplitLen = 64 };
    CString out,name,p; hashes[i].hashImpl->HashResult(out);
    name.Format(_T("%-8s"), hashes[i].hashImpl->HashName());
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
