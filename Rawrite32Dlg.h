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

#if !defined(AFX_RAWRITE32DLG_H__84A6A733_0EA9_40E6_AEC1_353C157AA5C8__INCLUDED_)
#define AFX_RAWRITE32DLG_H__84A6A733_0EA9_40E6_AEC1_353C157AA5C8__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

/////////////////////////////////////////////////////////////////////////////
// CRawrite32Dlg dialog

class CRawrite32Dlg : public CDialog
{
// Construction
public:
  CRawrite32Dlg(LPCTSTR imageFileName);	// standard constructor
  ~CRawrite32Dlg();

// Dialog Data
  //{{AFX_DATA(CRawrite32Dlg)
	enum { IDD = IDD_RAWRITE32_DIALOG };
	CComboBox	m_drives;
	CString	m_output;
	//}}AFX_DATA

	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CRawrite32Dlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	HICON m_hIcon;          // program icon
  HICON m_hSmallIcon;     // and a small variant of it

  CString m_imageName;    // file path of the input image

  HANDLE m_inputFile;     // input file handle
  HANDLE m_inputMapping;  // file mapping handle
  const BYTE *m_fsImage;  // input image
  size_t m_fsImageSize;   // size of input image

  DWORD m_sectorSkip;     // don't touch this many sectors on the output
                          // drive (i.e. the image is written startin at the sector given here)

  // during decompression:
  const BYTE *m_curInput; // pointer to next input data
  size_t m_sizeRemaining; // unread data past m_curInput
  DWORD m_sectorOut;      // next sector to write

  bool OpenInputFile(HANDLE); // map input and calculate hashes
  void CloseInputFile();  // close input file and mapping
  bool VerifyInput();     // return TRUE if "WriteToDisk" button has been enabled
  void CalcHashes(CString &out); // calculate hahes and format proper output

	// Generated message map functions
	//{{AFX_MSG(CRawrite32Dlg)
	virtual BOOL OnInitDialog();
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	afx_msg void OnDestroy();
	afx_msg void OnBrowse();
	afx_msg void OnNewImage();
	afx_msg void OnWriteImage();
	afx_msg void OnChangeSectorSkip();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
  
  DECLARE_INTERFACE_MAP()

  BEGIN_INTERFACE_PART(Drop, IDropTarget)
    STDMETHOD(DragEnter)(IDataObject*, DWORD, POINTL, LPDWORD);
    STDMETHOD(DragOver)(DWORD, POINTL, LPDWORD);
    STDMETHOD(DragLeave)();
    STDMETHOD(Drop)(IDataObject*, DWORD, POINTL, LPDWORD);
  END_INTERFACE_PART(Drop)
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

bool CalcMD5(const BYTE * data, DWORD size, CString & hash);
bool CalcSHA1(const BYTE * data, DWORD size, CString & hash);
bool CalcSHA256(const BYTE * data, DWORD size, CString & hash);
bool CalcSHA512(const BYTE * data, DWORD size, CString & hash);

#endif // !defined(AFX_RAWRITE32DLG_H__84A6A733_0EA9_40E6_AEC1_353C157AA5C8__INCLUDED_)

