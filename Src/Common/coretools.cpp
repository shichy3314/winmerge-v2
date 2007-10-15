/**
 * @file  coretools.cpp
 *
 * @brief Common routines
 *
 */
// ID line follows -- this is updated by SVN
// $Id$

#include "stdafx.h"
#include <stdio.h>
#include <io.h>
#include <mbctype.h> // MBCS (multibyte codepage stuff)
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <math.h>
#include <winsock.h>
#include <assert.h>

#ifndef __AFXDISP_H__
#include <afxdisp.h>
#endif

#include "UnicodeString.h"
#include "coretools.h"

#ifndef countof
#define countof(array)  (sizeof(array)/sizeof((array)[0]))
#endif /* countof */

static String MyGetSysError(int nerr);
static BOOL MyCreateDirectoryIfNeeded(LPCTSTR lpPathName, String * perrstr);

BOOL GetFileTimes(LPCTSTR szFilename,
				  LPSYSTEMTIME pMod,
				  LPSYSTEMTIME pCreate /*=NULL*/,
				  LPSYSTEMTIME pAccess /*=NULL*/)
{
	WIN32_FIND_DATA ffi;
	ASSERT(szFilename != NULL);
	ASSERT(pMod != NULL);

	HANDLE hff = FindFirstFile(szFilename, &ffi);
	if (hff != INVALID_HANDLE_VALUE)
	{
		FILETIME ft;
		FileTimeToLocalFileTime(&ffi.ftLastWriteTime, &ft);
		FileTimeToSystemTime(&ft, pMod);
		if (pCreate)
		{
			FileTimeToLocalFileTime(&ffi.ftCreationTime, &ft);
			FileTimeToSystemTime(&ft, pCreate);
		}
		if (pAccess)
		{
			FileTimeToLocalFileTime(&ffi.ftLastAccessTime, &ft);
			FileTimeToSystemTime(&ft, pAccess);
		}
		FindClose(hff);
		return TRUE;
	}
	return FALSE;
}

time_t GetFileModTime(LPCTSTR szPath)
{
	if (!szPath || !szPath[0]) return 0;
	struct _stat mystats = {0};
	int stat_result = _tstat(szPath, &mystats);
	if (stat_result!=0)
		return 0;
	return mystats.st_mtime;
}

DWORD GetFileSizeEx(LPCTSTR szFilename)
{
	WIN32_FIND_DATA ffi;
	ASSERT(szFilename != NULL);

	HANDLE hff = FindFirstFile(szFilename, &ffi);
	if (hff != INVALID_HANDLE_VALUE)
	{
		FindClose(hff);
		return ffi.nFileSizeLow;
	}
	return 0L;
}

// Doesn't _tcsclen do this ?
int tcssubptr(LPCTSTR start, LPCTSTR end)
{
	LPCTSTR p = start;
	register int cnt=0;
	while (p <= end)
	{
		cnt++;
		p = _tcsinc(p);
	}
	return cnt;
}


void GetLocalDrives(LPTSTR letters)
{
	TCHAR temp[100];

	_tccpy(letters,_T("\0"));
	if (GetLogicalDriveStrings(100,temp))
	{
		TCHAR *p,*pout=letters;
		for (p=temp; *p != _T('\0'); )
		{
			if (GetDriveType(p)!=DRIVE_REMOTE)
			{
				_tccpy(pout,p);
				pout = _tcsinc(pout);
			}
			p = _tcsninc(p,_tcslen(p)+1);
		}
		_tccpy(pout,_T("\0"));
	}
}

/*
 Commented out - can't use this file by just including
 coretools.h because of gethostaname() & gethostbyname().

BOOL GetIP(LPTSTR straddr)
{
	char     szHostname[100];
	HOSTENT *pHent;
	SOCKADDR_IN sinRemote;

	gethostname( szHostname, sizeof( szHostname ));
	pHent = gethostbyname( szHostname );

	if (pHent != NULL)
	{
		sinRemote.sin_addr.s_addr = *(u_long *)pHent->h_addr;
		_stprintf(straddr,_T("%d.%d.%d.%d"),
			sinRemote.sin_addr.S_un.S_un_b.s_b1,
			sinRemote.sin_addr.S_un.S_un_b.s_b2,
			sinRemote.sin_addr.S_un.S_un_b.s_b3,
			sinRemote.sin_addr.S_un.S_un_b.s_b4);
		return TRUE;
	}

	*straddr=_T('\0');
	return FALSE;
}
*/

DWORD FPRINTF(HANDLE hf, LPCTSTR fmt, ... )
{
	static TCHAR fprintf_buffer[8192];
    va_list vl;
    va_start( vl, fmt );

	_vstprintf(fprintf_buffer, fmt, vl);
	DWORD dwWritten;
	WriteFile(hf, fprintf_buffer, _tcslen(fprintf_buffer)*sizeof(TCHAR), &dwWritten, NULL);

    va_end( vl );
	return dwWritten;
}

DWORD FPUTS(LPCTSTR s, HANDLE hf)
{
	static DWORD fputs_written;
	WriteFile(hf, s, _tcslen(s)*sizeof(TCHAR), &fputs_written, NULL);
	return fputs_written;
}


HANDLE FOPEN(LPCTSTR path, DWORD mode /*= GENERIC_READ*/, DWORD access /*= OPEN_EXISTING*/)
{
	HANDLE hf = CreateFile(path,
					mode,
					0,
					NULL,
					access,
					FILE_ATTRIBUTE_NORMAL,
					NULL);
	if (hf == INVALID_HANDLE_VALUE) // give it another shot
		return CreateFile(path,
					mode,
					0,
					NULL,
					access,
					FILE_ATTRIBUTE_NORMAL,
					NULL);
	return hf;
}

void replace_char(LPTSTR s, int target, int repl)
{
	TCHAR *p;
	for (p=s; *p != _T('\0'); p = _tcsinc(p))
		if (*p == target)
			*p = (TCHAR)repl;
}

BOOL FileExtMatches(LPCTSTR filename, LPCTSTR ext)
{
  LPCTSTR p;

  /* if ext is empty, it is considered a no-match */
  if (*ext == _T('\0'))
    return FALSE;

  p = filename;
  p = _tcsninc(p, _tcslen(filename) - _tcslen(ext));
  if (p >= filename)
    return (!_tcsicmp(p,ext));

  return FALSE;
}

/**
 * @brief Return true if *pszChar is a slash (either direction) or a colon
 *
 * begin points to start of string, in case multibyte trail test is needed
 */
bool IsSlashOrColon(LPCTSTR pszChar, LPCTSTR begin)
{
#ifdef _UNICODE
		return (*pszChar == '/' || *pszChar == ':' || *pszChar == '\\');
#else
		// Avoid 0x5C (ASCII backslash) byte occurring as trail byte in MBCS
		return (*pszChar == '/' || *pszChar == ':' 
			|| (*pszChar == '\\' && !_ismbstrail((unsigned char *)begin, (unsigned char *)pszChar)));
#endif
}

/**
 * @brief Extract path name components from given full path.
 * @param [in] pathLeft Original path.
 * @param [out] pPath Folder name component of full path, excluding
   trailing slash.
 * @param [out] pFile File name part, excluding extension.
 * @param [out] pExt Filename extension part, excluding leading dot.
 */
void SplitFilename(LPCTSTR pathLeft, String* pPath, String* pFile, String* pExt)
{
	LPCTSTR pszChar = pathLeft + _tcslen(pathLeft);
	LPCTSTR pend = pszChar;
	LPCTSTR extptr = 0;
	bool ext = false;

	while (pathLeft < --pszChar)
	{
		if (*pszChar == '.')
		{
			if (!ext)
			{
				if (pExt)
				{
					(*pExt) = pszChar + 1;
				}
				ext = true; // extension is only after last period
				extptr = pszChar;
			}
		}
		else if (IsSlashOrColon(pszChar, pathLeft))
		{
			// Ok, found last slash, so we collect any info desired
			// and we're done

			if (pPath)
			{
				// Grab directory (omit trailing slash)
				int len = pszChar - pathLeft;
				if (*pszChar == ':')
					++len; // Keep trailing colon ( eg, C:filename.txt)
				*pPath = pathLeft;
				pPath->erase(len); // Cut rest of path
			}

			if (pFile)
			{
				// Grab file
				*pFile = pszChar + 1;
			}

			goto endSplit;
		}
	}

	// Never found a delimiter
	if (pFile)
	{
		*pFile = pathLeft;
	}

endSplit:
	// if both filename & extension requested, remove extension from filename

	if (pFile && pExt && extptr)
	{
		int extlen = pend - extptr;
		pFile->erase(pFile->length() - extlen);
	}
}

// Split Rational ClearCase view name (file_name@@file_version).
void SplitViewName(LPCTSTR s, String * path, String * name, String * ext)
{
	String sViewName(s);
	int nOffset = sViewName.find(_T("@@"));
	if (nOffset != std::string::npos)
	{
		sViewName.erase(nOffset);
		SplitFilename(sViewName.c_str(), path, name, ext);
	}
}
#ifdef _DEBUG
// Test code for SplitFilename above
void TestSplitFilename()
{
	LPCTSTR tests[] = {
		_T("\\\\hi\\"), _T("\\\\hi"), 0, 0
		, _T("\\\\hi\\a.a"), _T("\\\\hi"), _T("a"), _T("a")
		, _T("a.hi"), 0, _T("a"), _T("hi")
		, _T("a.b.hi"), 0, _T("a.b"), _T("hi")
		, _T("c:"), _T("c:"), 0, 0
		, _T("c:\\"), _T("c:"), 0, 0
		, _T("c:\\d:"), _T("c:\\d:"), 0, 0
	};
	for (int i=0; i < countof(tests); i += 4)
	{
		LPCTSTR dir = tests[i];
		String path, name, ext;
		SplitFilename(dir, &path, &name, &ext);
		LPCTSTR szpath = tests[i+1] ? tests[i+1] : _T("");
		LPCTSTR szname = tests[i+2] ? tests[i+2] : _T("");
		LPCTSTR szext = tests[i+3] ? tests[i+3] : _T("");
		ASSERT(path == szpath);
		ASSERT(name == szname);
		ASSERT(ext == szext);
	}
}
#endif

void AddExtension(LPTSTR name, LPCTSTR ext)
{
	TCHAR *p;

	assert(ext[0] != _T('.'));
	if (!((p=_tcsrchr(name,_T('.'))) != NULL
		  && !_tcsicmp(p+1,ext)))
	{
		_tcscat(name,_T("."));
		_tcscat(name,ext);
	}
}

BOOL GetFreeSpaceString(LPCTSTR drivespec, ULONG mode, LPTSTR s)
{
  DWORD sectorsPerCluster,
	  bytesPerSector,
	  numberOfFreeClusters,
	  totalNumberOfClusters, total;

  if (!GetDiskFreeSpace(drivespec,
						&sectorsPerCluster,
						&bytesPerSector,
						&numberOfFreeClusters,
						&totalNumberOfClusters))
    return FALSE;

  total = numberOfFreeClusters*bytesPerSector*sectorsPerCluster;
  if (mode==BYTES)
    _stprintf(s, _T("%lu bytes available"), total);
  else if (mode==KBYTES)
    _stprintf(s, _T("%1.1fK available"), (float)(total/(float)mode));
  else
    _stprintf(s, _T("%1.1f MB available"), (float)(total/(float)mode));
  return TRUE;
}

int fcmp(float a,float b)
/* return -1 if a<b, 0 if a=b, or 1 if a>b */
{
  long la,lb;

  la = (long)(a * 10000.0);
  lb = (long)(b * 10000.0);

  if (la < lb)
    return -1;

  return (la > lb);
}

BOOL FindAnyFile(LPTSTR filespec, LPTSTR name)
{
// Use 64-bit versions with VS2003.Net and later
#if _MSC_VER >= 1300
	_tfinddata64_t c_file;
	intptr_t hFile;
	hFile = _tfindfirst64( filespec, &c_file );
#else
// Use 32-bit versions with VC6
	_tfinddata_t c_file;	
	long hFile;
	hFile = _tfindfirst( filespec, &c_file );
#endif // _MSC_VER >= 1300

	if (hFile == -1L)
		return FALSE;

	_tcscpy(name, c_file.name);
	_findclose( hFile );
	return TRUE;
}


long SwapEndian(long val)
{
#ifndef _WINDOWS
  long t = 0x00000000;
  t = ((val >> 24) & 0x000000FF);
  t |= ((val >> 8) & 0x0000FF00);
  t |= ((val << 8) & 0x00FF0000);
  t |= ((val << 24) & 0xFF000000);
  return (long)t;
#else
  return val;
#endif
}



short int SwapEndian(short int val)
{
#ifndef _WINDOWS
  short int t = 0x0000;
  t = (val & 0xFF00) >> 8;
  t |= (val & 0x00FF) << 8;
  return (short int)t;
#else
  return val;
#endif
}

// Get user language description of error, if available
static String MyGetSysError(int nerr)
{
	LPTSTR lpMsgBuf = NULL;
	String str(_T("?"));
	if (FormatMessage( 
		FORMAT_MESSAGE_ALLOCATE_BUFFER | 
		FORMAT_MESSAGE_FROM_SYSTEM | 
		FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL,
		nerr,
		0, // Default language
		(LPTSTR) &lpMsgBuf,
		0,
		NULL 
		))
	{
		str = lpMsgBuf;
		// Free the buffer.
		LocalFree( lpMsgBuf );
	}
	return str;
}

// Create directory (via Win32 API)
// if success, or already exists, return TRUE
// if failure, return system error string
// (NB: Win32 CreateDirectory reports failure if already exists)
// TODO: replace this with calls to paths_CreateIfNeeded()
static BOOL MyCreateDirectoryIfNeeded(LPCTSTR lpPathName, String * perrstr)
{
	LPSECURITY_ATTRIBUTES lpSecurityAttributes = NULL;
	int rtn = CreateDirectory(lpPathName, lpSecurityAttributes);
	if (!rtn)
	{
		DWORD errnum = GetLastError();
		// Consider it success if directory already exists
		if (errnum == ERROR_ALREADY_EXISTS)
			return TRUE;
		if (perrstr)
		{
			String errdesc = MyGetSysError(errnum);
			TCHAR tmp[10];
			*perrstr = _itot(errnum, tmp, 10);
			*perrstr += _T(": ");
			*perrstr += errdesc;
		}
	}
	return rtn;
}

BOOL MkDirEx(LPCTSTR foldername)
{
	TCHAR tempPath[_MAX_PATH] = {0};
	LPTSTR p;

	_tcscpy(tempPath, foldername);
	if (*_tcsinc(foldername) == _T(':'))
		p = _tcschr(_tcsninc(tempPath, 3), _T('\\'));
	else if (*foldername == _T('\\'))
		p = _tcschr(_tcsinc(tempPath), _T('\\'));
	else
		p = tempPath;
	String errstr;
	if (p != NULL)
		for (; *p != _T('\0'); p = _tcsinc(p))
		{
			if (*p == _T('\\'))
			{
				*p = _T('\0');;
				if (0 && _tcscmp(tempPath, _T(".")) == 0)
				{
					// Don't call CreateDirectory(".")
				}
				else
				{
					if (!MyCreateDirectoryIfNeeded(tempPath, &errstr)
						&& !MyCreateDirectoryIfNeeded(tempPath, &errstr))
						TRACE(_T("Failed to create folder %s: %s\n"), tempPath, errstr.c_str());
					*p = _T('\\');
				}
			}

		}

		if (!MyCreateDirectoryIfNeeded(foldername, &errstr)
			&& !MyCreateDirectoryIfNeeded(foldername, &errstr))
			TRACE(_T("Failed to create folder %s: %s\n"), foldername, errstr.c_str());

	struct _stat mystats = {0};
	int stat_result = _tstat(foldername, &mystats);
	return stat_result != 0;
}

float RoundMeasure(float measure, float units)
{
	float res1,res2,divisor;
	if (units == 25.4f)
		divisor = 0.03937f;
	else if (units == 6.f || units == 72.f)
		divisor = 0.0139f;
	else // 128ths
		divisor = 0.0078125f;

	res1 = (float)fmod(measure,divisor);
	res2 = divisor - res1;
	if (res1 > res2)
		return (measure-res1);

	return (measure-res1+divisor);
}


BOOL HaveAdminAccess()
{
	// make sure this is NT first
	OSVERSIONINFO ver = { 0 };
	ver.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);

	// if this fails, we want to default to being enabled
	if (!(GetVersionEx(&ver) && ver.dwPlatformId==VER_PLATFORM_WIN32_NT))
	{
		return TRUE;
	}

	// now check with the security manager
	HANDLE hHandleToken;

	if(!::OpenProcessToken(::GetCurrentProcess(), TOKEN_READ, &hHandleToken))
		return FALSE;

	UCHAR TokenInformation[1024];
	DWORD dwTokenInformationSize;

	BOOL bSuccess = ::GetTokenInformation(hHandleToken, TokenGroups,
		TokenInformation, sizeof(TokenInformation), &dwTokenInformationSize);

	::CloseHandle(hHandleToken);

	if(!bSuccess)
		return FALSE;

	SID_IDENTIFIER_AUTHORITY siaNtAuthority = SECURITY_NT_AUTHORITY;
	PSID psidAdministrators;

	if(!::AllocateAndInitializeSid(&siaNtAuthority, 2,
		SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0,
		&psidAdministrators))
		return FALSE;

	// assume that we don't find the admin SID.
	bSuccess = FALSE;

	PTOKEN_GROUPS ptgGroups = (PTOKEN_GROUPS)TokenInformation;

	for(UINT x=0; x < ptgGroups->GroupCount; x++)
	{
		if(::EqualSid(psidAdministrators, ptgGroups->Groups[x].Sid))
		{
			bSuccess = TRUE;
			break;
		}
	}

	::FreeSid(psidAdministrators);

	return bSuccess;
}



String LegalizeFileName(LPCTSTR szFileName)
{
	TCHAR tempname[_MAX_PATH] = {0};
	LPTSTR p;

	_tcscpy(tempname, szFileName);
	while ((p=_tcspbrk(tempname, _T("\\'/:?\"<>|*\t\r\n"))) != NULL)
	{
		*p = _T('_');
	}

	return String(tempname);
}

static double tenpow(int expon)
{
	double base=10;
	double rtn = pow(base, expon);
	return rtn;
}
// These are not used - remove later?
#ifdef _UNUSED_
void DDX_Float( CDataExchange* pDX, int nIDC, float& value )
{
	pDX->PrepareEditCtrl(nIDC);
	CEdit *pEdit = (CEdit *)pDX->m_pDlgWnd->GetDlgItem(nIDC);
	if (pDX->m_bSaveAndValidate)
	{
		//DDX_Text(pDX, nIDC, value);
		static TCHAR dec[4]=_T("");
		static TCHAR thous[4]=_T("");
		static TCHAR negsign[4]=_T("");
		static int negstyle=1;

		if (*dec == _T('\0'))
		{
			LCID lcid = GetThreadLocale();
			GetLocaleInfo(lcid, LOCALE_INEGNUMBER, dec, 4);
			negstyle = _ttoi(dec);
			GetLocaleInfo(lcid, LOCALE_SDECIMAL, dec, 4);
			GetLocaleInfo(lcid, LOCALE_STHOUSAND, thous, 4);
			GetLocaleInfo(lcid, LOCALE_SNEGATIVESIGN, negsign, 4);
		}

		value = 0.f;

		// are we negative?
		TCHAR *p,text[80];
		pEdit->GetWindowText(text, 80);
		BOOL bIsNeg = (_tcschr(text, negsign[0])!=NULL);

		// separate at the decimal point
		TCHAR istr[80]=_T("");
		TCHAR fstr[80]=_T("");
		p = _tcstok(text, dec);
		if (p != NULL)
		{
			_tcscpy(istr, p);
			p = _tcstok(NULL, dec);
			if (p != NULL)
				_tcscpy(fstr, p);
		}

		// get the int part
		if (*istr != _T('\0'))
		{
			CString strResult(_T(""));
			CString sep(negsign);
			sep += thous;
			p = _tcstok(istr, sep);
			while (p != NULL)
			{
				strResult += p;
				p = _tcstok(NULL, sep);
			}
			value = (float)_ttoi(strResult);
		}

		// get the fract part
		if (*fstr != _T('\0'))
		{
			p = _tcstok(fstr, negsign);
			if (p != NULL)
			{
				double shift = tenpow(_tcslen(p));
				if (shift)
					value += (float)(_tcstod(p,NULL)/shift);
				else
					pDX->Fail();
			}
		}

		// convert to neg
		if (bIsNeg)
			value *= -1.f;
	}
	else
	{
		if (pEdit != NULL)
		{
			CString s = GetLocalizedNumberString(value, 4);
			pEdit->SetWindowText(s);
		}
	}
}

void DDX_Double( CDataExchange* pDX, int nIDC, double& value )
{
	pDX->PrepareEditCtrl(nIDC);
	CEdit *pEdit = (CEdit *)pDX->m_pDlgWnd->GetDlgItem(nIDC);
	if (pDX->m_bSaveAndValidate)
	{
		//DDX_Text(pDX, nIDC, value);
		static TCHAR dec[4]=_T("");
		static TCHAR thous[4]=_T("");
		static TCHAR negsign[4]=_T("");
		static int negstyle=1;

		if (*dec == _T('\0'))
		{
			LCID lcid = GetThreadLocale();
			GetLocaleInfo(lcid, LOCALE_INEGNUMBER, dec, 4);
			negstyle = _ttoi(dec);
			GetLocaleInfo(lcid, LOCALE_SDECIMAL, dec, 4);
			GetLocaleInfo(lcid, LOCALE_STHOUSAND, thous, 4);
			GetLocaleInfo(lcid, LOCALE_SNEGATIVESIGN, negsign, 4);
		}

		value = 0.f;

		// are we negative?
		TCHAR *p,text[80];
		pEdit->GetWindowText(text, 80);
		BOOL bIsNeg = (_tcschr(text, negsign[0])!=NULL);

		// separate at the decimal point
		TCHAR istr[80]=_T("");
		TCHAR fstr[80]=_T("");
		p = _tcstok(text, dec);
		if (p != NULL)
		{
			_tcscpy(istr, p);
			p = _tcstok(NULL, dec);
			if (p != NULL)
				_tcscpy(fstr, p);
		}

		// get the int part
		if (*istr != _T('\0'))
		{
			CString strResult(_T(""));
			CString sep(negsign);
			sep += thous;
			p = _tcstok(istr, sep);
			while (p != NULL)
			{
				strResult += p;
				p = _tcstok(NULL, sep);
			}
			value = /*(float)*/_ttoi(strResult);
		}

		// get the fract part
		if (*fstr != _T('\0'))
		{
			p = _tcstok(fstr, negsign);
			if (p != NULL)
			{
				double shift = tenpow(_tcslen(p));
				if (shift)
					value += /*(float)*/(_tcstod(p,NULL)/shift);
				else
					pDX->Fail();
			}
		}

		// convert to neg
		if (bIsNeg)
			value *= -1.f;
	}
	else
	{
		if (pEdit != NULL)
		{
			CString s = GetLocalizedNumberString(value, 4);
			pEdit->SetWindowText(s);
		}
	}
}


CString GetLocalizedNumberString(double dVal, int nPlaces /*=-1*/, BOOL bSeparate /*=FALSE*/, BOOL bTrailZeros /*=FALSE*/, LCID lcidNew /*=LOCALE_USER_DEFAULT*/)
// this function is duplicated in CFloatEdit
{
	static LCID lcid = 0;
	static TCHAR dec[4]=_T(".");
	static TCHAR thous[4]=_T(",");
	static TCHAR buf[80];
	static int nGrpSize=3;
	static int nDecimals=2;
	static BOOL bLeadZero=TRUE;
	static int negstyle=1;
	CString s, s2, strResult(_T(""));
	DWORD intpart=0;
	BOOL bIsNeg=(dVal < 0.0);

	ASSERT(nPlaces!=0);

	if (lcid != lcidNew)
	{
		lcid = lcidNew;
		GetLocaleInfo(lcid, LOCALE_SDECIMAL, dec, 4);
		GetLocaleInfo(lcid, LOCALE_STHOUSAND, thous, 4);
		GetLocaleInfo(lcid, LOCALE_SGROUPING, buf, 80);
		s = buf[0];
		nGrpSize = _ttoi(s);
		GetLocaleInfo(lcid, LOCALE_IDIGITS, buf, 4);
		nDecimals = _ttoi(buf);
		GetLocaleInfo(lcid, LOCALE_ILZERO, buf, 4);
		bLeadZero = !_tcsncmp(buf,_T("1"), 1);
		GetLocaleInfo(lcid, LOCALE_INEGNUMBER, buf, 4);
		negstyle = _ttoi(buf);
	}

	TCHAR szFract[80];

	// split into integer & fraction
	char tszInt[80],tszFract[80];
	int places = (nPlaces==-1? nDecimals : nPlaces);
	int  decimal, sign;
	char *buffer;

	buffer = _fcvt( dVal+1e-10, places, &decimal, &sign );
	if (decimal > 0)
	{
		strncpy(tszInt, buffer, decimal);
		tszInt[decimal] = NULL;
	}
	strncpy(tszFract, &buffer[decimal], strlen(buffer)-decimal);
	tszFract[strlen(buffer)-decimal] = NULL;
	intpart = (DWORD)atoi(tszInt);
#ifdef _UNICODE
	mbstowcs(szFract, tszFract, strlen(tszFract));
#else
	_tcscpy(szFract, tszFract);
#endif

	// take care of leading negative sign
	if (bIsNeg)
	{
		if (negstyle==0)
			strResult = _T("(");
		else if (negstyle==1)
			strResult = _T("-");
		else if (negstyle==2)
			strResult = _T("- ");
	}

	// format integer part
	if (intpart >= 1)
	{
		s.Format(_T("%lu"), intpart);

		// do thousands separation
		if (bSeparate)
		{
			int len = s.GetLength();
			int leftover = (len % nGrpSize);

			// format the leading group
			if (leftover)
			{
				s2 = s.Left(leftover);
				len -= leftover;
				s = s.Right(len);
				strResult += s2;
			}
			else if (len)
			{
				s2 = s.Left(nGrpSize);
				len -= nGrpSize;
				s = s.Right(len);
				strResult += s2;
			}

			// format the remaining groups
			while (len)
			{
				s2 = s.Left(nGrpSize);
				len -= nGrpSize;
				s = s.Right(len);
				strResult += thous + s2;
			}
		}
		// want integer value without thousands separation
		else
		{
			strResult += s;
		}
	}
	else
	{
		// add leading zero
		if (bLeadZero)
			strResult += _T("0");
	}

	// add decimal separator & fractional part
	if (szFract != NULL)
	{
		// add the decimal separator
		strResult += dec;
		strResult += szFract;

		// get rid of trailing zeros
		if (!bTrailZeros)
		{
			LPTSTR p, start = strResult.GetBuffer(1);
			p = _tcsdec(start, start + _tcslen(start));
			if (p != NULL)
			{
				for (; p > start && *p==_T('0');
				p = _tcsdec(start, p));
				if (p != NULL)
				{
					if (*p==_T('.')) // we don't want numbers like 4.
						*p=_T('\0');
					else
						*(p+1)=_T('\0');
				}
			}
			strResult.ReleaseBuffer();
		}
	}

	// take care of trailing negative sign
	if (bIsNeg)
	{
		if (negstyle==0)
			strResult += _T(")");
		else if (negstyle==3)
			strResult += _T("-");
		else if (negstyle==4)
			strResult += _T(" -");
	}

	return strResult;
}
#endif // _UNUSED_

HANDLE RunIt(LPCTSTR szExeFile, LPCTSTR szArgs, BOOL bMinimized /*= TRUE*/, BOOL bNewConsole /*= FALSE*/)
{
    STARTUPINFO si = {0};
	PROCESS_INFORMATION procInfo = {0};

    si.cb = sizeof(STARTUPINFO);
    si.lpDesktop = _T("");
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = (bMinimized) ? SW_MINIMIZE : SW_HIDE;

	TCHAR args[4096];
	_stprintf(args,_T("\"%s\" %s"), szExeFile, szArgs);
    if (CreateProcess(szExeFile, args, NULL, NULL,
		FALSE, NORMAL_PRIORITY_CLASS|(bNewConsole? CREATE_NEW_CONSOLE:0),
                         NULL, _T(".\\"), &si, &procInfo))
	{
		CloseHandle(procInfo.hThread);
		return procInfo.hProcess;
	}

	return INVALID_HANDLE_VALUE;
}

BOOL HasExited(HANDLE hProcess, DWORD *pCode)
{
     DWORD code;
     if (GetExitCodeProcess(hProcess, &code)
         && code != STILL_ACTIVE)
     {
		 *pCode = code;
         return TRUE;
     }
     return FALSE;
}

#ifdef BROKEN
// This function assigns to path, which is clearly bad as path is LPCTSTR
BOOL IsLocalPath(LPCTSTR path)
{

  _TCHAR finalpath[_MAX_PATH] = {0};
  _TCHAR temppath[_MAX_PATH] = {0};
  _TCHAR uncname[_MAX_PATH] = {0};
  _TCHAR computername[_MAX_PATH] = {0};

  BOOL bUNC=FALSE;
  BOOL bLocal=FALSE;

  TCHAR* pLoc=0;

  CString szInfo;

  //We need to get the root directory into an sz
  if((pLoc=_tcsstr(path, _T("\\\\")))!=NULL)
  {
    bUNC=TRUE;

    _tcscpy(temppath, pLoc+_tcslen(_T("\\\\")));
    _tcscpy(finalpath, _T("\\\\"));

    if((pLoc=_tcsstr(temppath, _T("\\")))!=NULL)
    {
      _tcsncpy(uncname, temppath, pLoc-temppath);
      _tcscat(finalpath, uncname);

    }
    else
    {
      _tcscpy(uncname, temppath);
      _tcscat(finalpath, uncname);
    }

  }
  else //standard path
  {
    _tcscpy(temppath, path);
    if((pLoc=_tcsstr(temppath, _T("\\")))!=NULL)
    {
      *pLoc = _T('\0');
    }
    _tcscpy(finalpath, temppath);
  }

  if(bUNC)
  {
    // get the machine name and compare it to the local machine name
    //if cluster, GetComputerName returns the name of the cluster
    DWORD len=_MAX_PATH-1;
    if(GetComputerName(computername, &len) && _tcsicmp(computername, uncname)==0
      )
    {
      //szInfo =  _T("This is a UNC path to the local machine or cluster");
      bLocal=TRUE;
    }
    else
    {
      //szInfo =  _T("This is a UNC path to a remote machine");
    }
  }
  else
  {
    //test the standard path
    UINT uType;
    uType = GetDriveType(finalpath);

    switch(uType)
    {
    case DRIVE_UNKNOWN:
      //szInfo = _T("The drive type cannot be determined.");
      break;
    case DRIVE_NO_ROOT_DIR:
      //szInfo = _T("The root directory does not exist.");
      break;
    case DRIVE_REMOVABLE:
      //szInfo = _T("The disk can be removed from the drive.");
      bLocal=TRUE;
      break;
    case DRIVE_FIXED:
      //szInfo = _T("The disk cannot be removed from the drive.");
      bLocal=TRUE;
      break;
    case DRIVE_REMOTE:
      //szInfo = _T("The drive is a remote (network) drive.");
      break;
    case DRIVE_CDROM:
      //szInfo = _T("The drive is a CD-ROM drive.");
      bLocal=TRUE;
      break;
    case DRIVE_RAMDISK:
      //szInfo = _T("The drive is a RAM disk.");
      bLocal=TRUE;
      break;
    //default:
      //szInfo = _T("GetDriveType returned an unknown type");

    }
  }

  return bLocal;
}
#endif

/**
 * @brief Return module's path component (without filename).
 * @param [in] hModule Module's handle.
 * @return Module's path.
 */
String GetModulePath(HMODULE hModule /* = NULL*/)
{
	TCHAR temp[MAX_PATH] = {0};
	GetModuleFileName(hModule, temp, MAX_PATH);
	return GetPathOnly(temp);
}

/**
 * @brief Return path component from full path.
 * @param [in] fullpath Full path to split.
 * @return Path without filename.
 */
String GetPathOnly(LPCTSTR fullpath)
{
	if (!fullpath || !fullpath[0]) return _T("");
	String spath;
	SplitFilename(fullpath, &spath, 0, 0);
	return spath;
}

/**
 * @brief Returns Application Data path in user profile directory
 * if one exists
 */
BOOL GetAppDataPath(CString &sAppDataPath)
{
	TCHAR path[_MAX_PATH] = {0};
	if (GetEnvironmentVariable(_T("APPDATA"), path, _MAX_PATH))
	{
		sAppDataPath = path;
		return TRUE;
	}
	else
	{
		sAppDataPath = _T("");
		return FALSE;
	}
}

/**
 * @brief Returns User Profile path (if available in environment)
 */
BOOL GetUserProfilePath(CString &sAppDataPath)
{
	TCHAR path[_MAX_PATH] = {0};
	if (GetEnvironmentVariable(_T("USERPROFILE"), path, countof(path)))
	{
		sAppDataPath = path;
		return TRUE;
	}
	else
	{
		sAppDataPath = _T("");
		return FALSE;
	}
}

/**
 * @brief Decorates commandline for giving to CreateProcess() or
 * ShellExecute().
 *
 * Adds quotation marks around executable path if needed, but not
 * around commandline switches. For example (C:\p ath\ex.exe -p -o)
 * becomes ("C:\p ath\ex.exe" -p -o)
 * @param [in] sCmdLine commandline to decorate
 * @param [out] sDecoratedCmdLine decorated commandline
 * @param [out] sExecutable Executable for validating file extension etc
 */
void GetDecoratedCmdLine(CString sCmdLine, CString &sDecoratedCmdLine,
	CString &sExecutable)
{
	BOOL pathEndFound = FALSE;
	BOOL addQuote = FALSE;
	int pos = 0;
	int prevPos = 0;
	pos = sCmdLine.Find(_T(" "), 0);

	sCmdLine.TrimLeft();
	sCmdLine.TrimRight();
	sDecoratedCmdLine.Empty();
	sExecutable.Empty();

	if (pos > -1)
	{
		// First space was before switch, we don't need "s
		// (executable path didn't contain spaces)
		if (sCmdLine[pos + 1] == '/' || sCmdLine[pos + 1] == '-')
		{
			pathEndFound = TRUE;
		}
		else
		{
			addQuote = TRUE;
			sDecoratedCmdLine = _T("\"");
		}

		// Loop until executable path end (first switch) is found
		while (pathEndFound == FALSE)
		{
			prevPos = pos;
			pos = sCmdLine.Find(_T(" "), prevPos + 1);

			if (pos > -1)
			{
				if (sCmdLine[pos + 1] == '/' || sCmdLine[pos + 1] == '-')
				{
					pathEndFound = TRUE;
				}
			}
			else
			{
				pathEndFound = TRUE;
			}
		}

		if (addQuote)
		{
			if (pos > -1)
			{
				sExecutable = sCmdLine.Left(pos);
				sDecoratedCmdLine += sExecutable;
				sDecoratedCmdLine += _T("\"");
				sDecoratedCmdLine += sCmdLine.Right(sCmdLine.GetLength() - pos);
			}
			else
			{
				sExecutable = sCmdLine;
				sDecoratedCmdLine += sCmdLine;
				sDecoratedCmdLine += _T("\"");
			}
		}
		else
		{
			sDecoratedCmdLine = sCmdLine;
			sExecutable = sCmdLine;
		}
	}
	else
	{
		sDecoratedCmdLine = sCmdLine;
		sExecutable = sCmdLine;
	}
}
