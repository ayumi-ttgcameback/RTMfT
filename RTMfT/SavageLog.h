#pragma once
#include "framework.h"

class SavageLog
{
	const LONGLONG MAX_LOG_FILESIZE = 1LL * 1024LL * 1024LL;

	wchar_t m_wszLogFileName[MAX_PATH + 1]{};
	wchar_t m_wszAppName[100 + 1]{};

	DWORD Utf16toUtf8withLF(_In_ const HANDLE hHeap, _In_ const wchar_t* src,_Inout_ LPSTR* pdst)
	{
		int size = WideCharToMultiByte(CP_UTF8, 0, src, -1, NULL, 0, NULL, NULL);
		if (size == 0) {
			return 0;
		}
		*pdst = (LPSTR)HeapAlloc(hHeap, 0, size + 1);
		if (*pdst == NULL) {
			return 0;
		}
		size = WideCharToMultiByte(CP_UTF8, 0, src, -1, *pdst, size, NULL, NULL);
		if (size == 0) {
			DWORD dwErr = GetLastError();
			HeapFree(hHeap, 0, *pdst);
			*pdst = NULL;
			SetLastError(dwErr);
			return 0;
		}
		*((*pdst) + size - 1) = '\n';
		*((*pdst) + size) = '\0';
		return (DWORD)size;
	}
	bool WriteToFile(const wchar_t* msg)
	{
		if (m_wszLogFileName[0] == 0) {
			DebugPut(L"m_wszLogFileName is empty");
			return false;
		}
		bool bret = false;
		HANDLE hProcessHeap = GetProcessHeap();
		if (hProcessHeap == NULL) {
			DebugPut(L"GetProcessHeap failure (0x%x)", GetLastError());
			return false;
		}
		HANDLE hFile = CreateFile(m_wszLogFileName, GENERIC_READ | GENERIC_WRITE | DELETE, 0, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
		if (INVALID_HANDLE_VALUE == hFile) {
			DebugPut(L"CreateFile1 failure (0x%x)", GetLastError());
			return false;
		}
		LARGE_INTEGER fs;
		if (FALSE == GetFileSizeEx(hFile, &fs)) {
			DebugPut(L"GetFileSizeEx failure (0x%x)", GetLastError());
			CloseHandle(hFile);
			return false;
		}
		if (fs.QuadPart > MAX_LOG_FILESIZE) {
			if (false == RenameFile(hFile)) {
				CloseHandle(hFile);
				return false;
			}
			CloseHandle(hFile);
			hFile = CreateFile(m_wszLogFileName, GENERIC_READ | GENERIC_WRITE | DELETE, 0, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
			if (INVALID_HANDLE_VALUE == hFile) {
				DebugPut(L"CreateFile2 failure (0x%x)", GetLastError());
				return false;
			}
		}
		fs.QuadPart = 0;
		if (SetFilePointerEx(hFile, fs, NULL, FILE_END) == FALSE) {
			DebugPut(L"SetFilePointerEx failure (0x%x)", GetLastError());
			CloseHandle(hFile);
			return false;
		}
		LPSTR utf8{};
		DWORD dwWrite = Utf16toUtf8withLF(hProcessHeap, msg, &utf8);
		if (dwWrite == 0) {
			DebugPut(L"Utf16toUtf8 failure (0x%x)", GetLastError());
			CloseHandle(hFile);
			return false;
		}
		(void)WriteFile(hFile, utf8, dwWrite, NULL, NULL);
		(void)HeapFree(hProcessHeap, 0, utf8);
		(void)FlushFileBuffers(hFile);
		CloseHandle(hFile);

		return true;
	}
	bool RenameFile(const HANDLE hFile) {
		BYTE buf[(DWORD)(sizeof(FILE_RENAME_INFO) + (MAX_PATH + 1) * sizeof(wchar_t))]{};
		FILE_RENAME_INFO* info = (FILE_RENAME_INFO*)buf;
		if (StringCchCopy(info->FileName, MAX_PATH, m_wszLogFileName) != S_OK) {
			return false;
		}
		size_t dstLen{};
		if (StringCchLength(info->FileName, MAX_PATH, &dstLen) != S_OK) {
			return false;
		}

		for (size_t i = dstLen - 1; i > 0; i--) {
			if (info->FileName[i] == L'.') {
				info->FileName[i + 1] = L'\0';
				break;
			}
		}
		if (StringCchCat(info->FileName, MAX_PATH, L"bak") != S_OK) {
			return false;
		}
		if (StringCchLength(info->FileName, MAX_PATH, &dstLen) != S_OK) {
			return false;
		}
		info->ReplaceIfExists = TRUE;
		info->RootDirectory = NULL;
		info->FileNameLength = dstLen;

		DWORD bufsize = (DWORD)(sizeof(FILE_RENAME_INFO) + (dstLen + 1) * sizeof(wchar_t));
		if (FALSE == SetFileInformationByHandle(hFile, FileRenameInfo, info, bufsize)) {
			DebugPut(L"SetFileInformationByHandle failure (0x%x)", GetLastError());
			return false;
		}
		return true;
	}

	HRESULT FormatHeader(wchar_t* buf, const size_t bufcnt, const DWORD dwEventId, const WORD wEventType) const
	{
		SYSTEMTIME nowt;
		GetLocalTime(&nowt);
		wchar_t* wszEventType;
		switch (wEventType) {
		case EVENTLOG_ERROR_TYPE:
			wszEventType = (wchar_t*)L"ERR ";
			break;
		case EVENTLOG_WARNING_TYPE:
			wszEventType = (wchar_t*)L"WARN";
			break;
		case EVENTLOG_INFORMATION_TYPE:
			wszEventType = (wchar_t*)L"INFO";
			break;
		default:
			wszEventType = (wchar_t*)L"#dbg";
			break;
		}

		HRESULT hr = StringCchPrintf(buf, bufcnt, L"%04d/%02d/%02d %02d:%02d:%02d %s(PID:%lu) Id:%lu[%s] ",
			nowt.wYear, nowt.wMonth, nowt.wDay, nowt.wHour, nowt.wMinute, nowt.wSecond,
			m_wszAppName,
			GetCurrentProcessId(),
			dwEventId,
			wszEventType);

		return hr;
	}
	bool Report(const DWORD dwEventId, const WORD wEventType, const wchar_t* format, va_list arglist)
	{
		if (m_wszAppName[0] == 0) {
			DebugPut(L"log class not initialized");
			return false;
		}
		wchar_t buf[1024]{};
		const size_t bufcnt = sizeof(buf) / sizeof(wchar_t) - 1;
		const size_t appendix_len = 4;

		HRESULT hr = FormatHeader(buf, bufcnt, dwEventId, wEventType);
		if (hr != S_OK) {
			return false;
		}
		size_t headercnt{};
		hr = StringCchLength(buf, sizeof(buf) / sizeof(wchar_t) - 1, &headercnt);
		if (hr != S_OK || bufcnt < headercnt - appendix_len) {
			SetLastError(ERROR_OUTOFMEMORY);
			return false;
		}
		hr = StringCchVPrintf(buf + headercnt, bufcnt - headercnt - appendix_len, format, arglist);
		if (hr == STRSAFE_E_INSUFFICIENT_BUFFER) {
			hr = StringCchCat(buf, sizeof(buf) / sizeof(wchar_t) - 1, L"...");
		}
		if (hr != S_OK) {
			SetLastError(ERROR_OUTOFMEMORY);
			return false;
		}
		return WriteToFile(buf);
	}
	void DebugPut(const wchar_t* format, ...)
	{
#ifdef _DEBUG
		va_list arglist;
		va_start(arglist, format);

		wchar_t buf[1024]{};
		HRESULT hr;
		hr = StringCchVPrintf(buf, sizeof(buf) / sizeof(wchar_t) - 4, format, arglist);
		if (hr == STRSAFE_E_INSUFFICIENT_BUFFER) {
			hr = StringCchCat(buf, sizeof(buf) / sizeof(wchar_t) - 1, L"...");
		}
		if (hr == S_OK) {
			OutputDebugString(buf);
			OutputDebugString(L"\n");
		}
		va_end(arglist);
#endif
	}
public:
	bool Setup(const wchar_t* appname, const HMODULE hDLL) {
		size_t uAppNameMaxLength = sizeof(m_wszAppName) / sizeof(wchar_t) - 1;
		HRESULT hr = StringCchCopyN(m_wszAppName, uAppNameMaxLength, appname, uAppNameMaxLength);
		if (hr != S_OK && hr != STRSAFE_E_INSUFFICIENT_BUFFER) {
			m_wszAppName[0] = 0;
			return false;
		}
		size_t uAppNameLen{};
		if (S_OK != StringCchLength(m_wszAppName, uAppNameMaxLength, &uAppNameLen)) {
			m_wszAppName[0] = 0;
			return false;
		}
		size_t maxlen = sizeof(m_wszLogFileName) / sizeof(wchar_t);
		size_t len = (size_t)GetModuleFileName(hDLL, m_wszLogFileName, maxlen);
		DWORD dwErr = GetLastError();
		if (len == 0 || dwErr == ERROR_INSUFFICIENT_BUFFER) {
			DebugPut(L"GetModuleFileName failure (0x%x)", dwErr);
			return false;
		}
		for (size_t i = len - 1; i > 0; i--) {
			if (*(i + m_wszLogFileName) == L'.') {
				*(i + m_wszLogFileName + 1) = 0;
				break;
			}
		}
		if (S_OK != StringCchLength(m_wszLogFileName, maxlen, &len)) {
			m_wszLogFileName[0] = 0;
			return false;
		}
		if (maxlen - len < 4) {
			m_wszLogFileName[0] = 0;
			return false;
		}
		if (S_OK != StringCchCat(m_wszLogFileName, maxlen, L"log")) {
			m_wszLogFileName[0] = 0;
			return false;
		}
		return true;
	}
	void error(const wchar_t* format, ...) {
		va_list arglist;
		va_start(arglist, format);
		verror(1, format, arglist);
		va_end(arglist);
	}
	void info(const wchar_t* format, ...) {
		va_list arglist;
		va_start(arglist, format);
		vinfo(1, format, arglist);
		va_end(arglist);
	}
	void warn(const wchar_t* format, ...) {
		va_list arglist;
		va_start(arglist, format);
		vwarn(1, format, arglist);
		va_end(arglist);
	}
	void error(const DWORD id, const wchar_t* format, ...) {
		va_list arglist;
		va_start(arglist, format);
		verror(id, format, arglist);
		va_end(arglist);
	}
	void info(const DWORD id, const wchar_t* format, ...) {
		va_list arglist;
		va_start(arglist, format);
		vinfo(id, format, arglist);
		va_end(arglist);
	}
	void warn(const DWORD id, const wchar_t* format, ...) {
		va_list arglist;
		va_start(arglist, format);
		vwarn(id, format, arglist);
		va_end(arglist);
	}

	void verror(const wchar_t* format, va_list arglist) {
		verror(1, format, arglist);
	}
	void vinfo(const wchar_t* format, va_list arglist) {
		vinfo(1, format, arglist);
	}
	void vwarn(const wchar_t* format, va_list arglist) {
		vwarn(1, format, arglist);
	}
	void verror(const DWORD id, const wchar_t* format, va_list arglist) {
		Report(id, EVENTLOG_ERROR_TYPE, format, arglist);
	}
	void vinfo(const DWORD id, const wchar_t* format, va_list arglist) {
		Report(id, EVENTLOG_INFORMATION_TYPE, format, arglist);
	}
	void vwarn(const DWORD id, const wchar_t* format, va_list arglist) {
		Report(id, EVENTLOG_WARNING_TYPE, format, arglist);
	}
};

