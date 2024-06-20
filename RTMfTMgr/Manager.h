#pragma once
#include "framework.h"
#include "../RTMfT/SavageLog.h"
#include "EnumProcessByName.h"
#pragma comment(lib, "shlwapi.lib")

class Manager
{
	SavageLog* m_pLog{};
	wchar_t m_wszDllPath[MAX_PATH + 1]{};
	const wchar_t* DLLNAME = L"RTMfT.x86.dll";
	wchar_t m_wszSearchTargetName[MAX_PATH + 1]{};

	bool BuildDLLNameWithManagerDirectory()
	{
		if (m_wszDllPath[0] != 0) {
			return true;
		}
		const int bufsize = sizeof(m_wszDllPath) / sizeof(wchar_t) - 1;

		if (0 == GetModuleFileName(NULL, m_wszDllPath, bufsize)) {
			m_pLog->error( L"GetModuleFileName failure (0x%x)", GetLastError());
			return false;
		}
		size_t dllPathLen = wcslen(m_wszDllPath);
		for (int i = (int)dllPathLen - 2; i >= 0; i--) {
			if (m_wszDllPath[i] == L'\\') {
				m_wszDllPath[i + 1] = 0;
				break;
			}
		}
		dllPathLen = wcslen(m_wszDllPath);
		size_t dllNameLen = wcslen(DLLNAME);
		if (dllPathLen + dllNameLen >= MAX_PATH) {
			return false;
		}
		if (StringCchCat(m_wszDllPath, bufsize - dllNameLen, DLLNAME) != S_OK) {
			return false;
		}
		return true;
	}
	bool IsExistDLL()
	{
		if (BuildDLLNameWithManagerDirectory() == false) {
			return false;
		}
		static bool ALREADYPUT_DLLFILENOTFOUNDMSG = false;

		if (PathFileExists(m_wszDllPath) == FALSE) {
			if (ALREADYPUT_DLLFILENOTFOUNDMSG == false) {
				ALREADYPUT_DLLFILENOTFOUNDMSG = true;
				m_pLog->error(10, L"%s‚ª‚ ‚è‚Ü‚¹‚ñ", DLLNAME);
			}
			return false;
		}
		ALREADYPUT_DLLFILENOTFOUNDMSG = false;
		return true;
	}
	/// <summary>
	/// 
	/// </summary>
	/// <param name="hTargetProc"></param>
	/// <returns>-1: err, 0:–¢attach, 1: attached</returns>
	int IsDLLAlreadyAttached(const HANDLE hTargetProc) 
	{
		return GetRemoteModuleHandleByName(hTargetProc, m_wszDllPath, NULL);
	}
	/// <summary>
	/// 
	/// </summary>
	/// <param name="hTargetProc"></param>
	/// <returns>-1: err, 0:–¢attach, 1: attached</returns>
	int GetRemoteModuleHandleByName(const HANDLE hTargetProc, const wchar_t* pwszTargetDLLPathName, HMODULE* pLoadedModule) 
	{
		const DWORD defaultSize = 100;
		DWORD bufByteSize = sizeof(DWORD) * defaultSize;
		DWORD needByteSize = bufByteSize;
		HMODULE* hModules = NULL;
		wchar_t wszLoadedDLLPath[MAX_PATH + 1]{};
		DWORD dwHModuleCount = 0;

		HANDLE hHeap = GetProcessHeap();
		if (NULL == hHeap) {
			m_pLog->error( L"GetProcessHeap failure (0x%x)", GetLastError());
			return -1;
		}
		while (true) {
			if (hModules != NULL) {
				HeapFree(hHeap, 0, hModules);
				hModules = NULL;
			}
			hModules = (HMODULE*)HeapAlloc(hHeap, 0, bufByteSize);
			if (NULL == hModules) {
				m_pLog->error(L"HeapAlloc failure (0x%x)", GetLastError());
				return -1;
			}
			if (0 == EnumProcessModulesEx(hTargetProc, hModules, bufByteSize, &needByteSize, LIST_MODULES_ALL)) {
				m_pLog->error(L"EnumProcessModulesEx failure (0x%x)", GetLastError());
				HeapFree(hHeap, 0, hModules);
				return -1;
			}
			if (needByteSize <= bufByteSize) {
				dwHModuleCount = needByteSize / sizeof(DWORD);
				break;
			}
			bufByteSize = needByteSize;
		}

		int ret = 0;
		for (DWORD i = 0; i < dwHModuleCount; i++) {
			GetModuleFileNameEx(hTargetProc, hModules[i], wszLoadedDLLPath, MAX_PATH);
			if (_wcsicmp(wszLoadedDLLPath, pwszTargetDLLPathName) == 0) {
				if (pLoadedModule) {
					*pLoadedModule = hModules[i];
				}
				ret = 1;
				break;
			}
		}
		HeapFree(hHeap, 0, hModules);
		return ret;
	}
	HANDLE GetRemoteProcessHandle(const DWORD dwTargetPid) {
		/*DWORD dwAccess =
			SYNCHRONIZE |
			PROCESS_VM_READ |
			PROCESS_VM_WRITE |
			PROCESS_VM_OPERATION |
			PROCESS_QUERY_INFORMATION |
			PROCESS_CREATE_THREAD;*/
		HANDLE hTargetProc = OpenProcess(PROCESS_ALL_ACCESS, FALSE, dwTargetPid);
		if (hTargetProc == NULL) {
			m_pLog->error( L"OpenProcess failure (0x%x)", GetLastError());
			return NULL;
		}
		return hTargetProc;
	}

	bool PrepareForAttach(HANDLE hTargetProc, const HMODULE hKernel32,const wchar_t* wszDllPath, FARPROC* pfarproc, VOID** ppVmemAddr)
	{
		SIZE_T dwVmemSize = (wcslen(wszDllPath) + 1) * sizeof(wchar_t);
		*ppVmemAddr = (PWSTR)VirtualAllocEx(hTargetProc, NULL, dwVmemSize, MEM_COMMIT, PAGE_READWRITE);
		if (*ppVmemAddr == NULL) {
			m_pLog->error(L"VirtualAllocEx failure (0x%x)", GetLastError());
			return false;
		}
		BOOL bRet = WriteProcessMemory(hTargetProc, *ppVmemAddr, (PVOID)wszDllPath, dwVmemSize, NULL);
		if (bRet == FALSE) {
			m_pLog->error( L"WriteProcessMemory failure (0x%x)", GetLastError());
			VirtualFreeEx(hTargetProc, *ppVmemAddr, 0, MEM_RELEASE);
			return false;
		}
		*pfarproc = GetProcAddress(hKernel32, "LoadLibraryW");
		if (*pfarproc == NULL) {
			m_pLog->error( L"GetProcAddress failure (0x%x)", GetLastError());
			VirtualFreeEx(hTargetProc, *ppVmemAddr, 0, MEM_RELEASE);
			return false;
		}
		return true;
	}
	bool PrepareForDetach(HANDLE hTargetProc, const HMODULE hKernel32, FARPROC* pfarproc)
	{
		*pfarproc = GetProcAddress(hKernel32, "FreeLibraryAndExitThread");
		if (*pfarproc == NULL) {
			m_pLog->error( L"GetProcAddress failure (0x%x)", GetLastError());
			return false;
		}
		return true;
	}
	enum RP_OPERATE_TYPE {
		OPERATE_ATTACH,
		OPERATE_DETACH,
		OPERATE_IS_ATTACHED,
	};
	bool OperateRemoteLibraryByPID(const DWORD dwTargetPid, const RP_OPERATE_TYPE type)
	{
		HANDLE hTargetProc = GetRemoteProcessHandle(dwTargetPid);
		if (hTargetProc == NULL) {
			return false;
		}
		int iRet = 0;

		HMODULE hModuleTargetDLL{};
		if (type == OPERATE_ATTACH || type == OPERATE_IS_ATTACHED) {
			iRet = IsDLLAlreadyAttached(hTargetProc);
			if (iRet == 1) {
				// Šù‚Éattach‚³‚ê‚Ä‚¢‚é
				CloseHandle(hTargetProc);
				SetLastError(ERROR_ALREADY_EXISTS);
				return  true;
			}
			if (OPERATE_IS_ATTACHED == type) {
				return false;
			}
		}
		else if (type == OPERATE_DETACH) {
			iRet = GetRemoteModuleHandleByName(hTargetProc, m_wszDllPath, &hModuleTargetDLL);
			if (iRet == 0) {
				// detach‚Ì•K—v‚È‚µ
				CloseHandle(hTargetProc);
				return  true;
			}
		}
		else {
			return false;
		}

		if (iRet < 0) {
			CloseHandle(hTargetProc);
			return false;
		}

		HMODULE hKernel32 = GetModuleHandle(L"kernel32");
		if (hKernel32 == NULL) {
			m_pLog->error( L"kernel32 has gone");
			CloseHandle(hTargetProc);
			return false;
		}
		bool bRet{};
		void* pVmemAddr = NULL;
		FARPROC farproc = NULL;

		void* param = NULL;
		if (type == OPERATE_ATTACH) {
			bRet = PrepareForAttach(hTargetProc, hKernel32, m_wszDllPath, &farproc, (VOID**)&pVmemAddr);
			param = pVmemAddr;
		}
		else {
			bRet = PrepareForDetach(hTargetProc, hKernel32, &farproc);
			param = (VOID*)hModuleTargetDLL;
		}
		if (bRet == false) {
			CloseHandle(hTargetProc);
			return false;
		}
		HANDLE hThread = CreateRemoteThread(hTargetProc, NULL, 0, (PTHREAD_START_ROUTINE)farproc, param, 0, NULL);
		if (hThread == NULL) {
			m_pLog->error(L"CreateRemoteThread failure (0x%x)", GetLastError());
			if (pVmemAddr) {
				VirtualFreeEx(hTargetProc, pVmemAddr, 0, MEM_RELEASE);
			}
			CloseHandle(hTargetProc);
			return false;
		}
		DWORD dwRet = WaitForSingleObject(hThread, 15000);
		bool bret{ false };
		switch (dwRet) {
		case WAIT_OBJECT_0:
			if (pVmemAddr) {
				VirtualFreeEx(hTargetProc, pVmemAddr, 0, MEM_RELEASE);
			}
			bret = true;
			break;
		case WAIT_TIMEOUT:
			m_pLog->error(L"WaitForSingleObject wait timeouted");
			break;
		default:
			m_pLog->error(L"WaitForSingleObject(remotethread) failure (0x%x)", GetLastError());
			break;
		}
		CloseHandle(hThread);
		CloseHandle(hTargetProc);
		return bret;
	}
public:
	void Setup(SavageLog* pLog,wchar_t* wszSearchTargetName) {
		m_pLog = pLog;
		wcscpy_s(m_wszSearchTargetName, MAX_PATH, wszSearchTargetName);
	}
	bool AttachByPID(const DWORD dwTargetPid)
	{
		return OperateRemoteLibraryByPID(dwTargetPid, OPERATE_ATTACH);
	}
	bool DetachByPID(const DWORD dwTargetPid)
	{
		return OperateRemoteLibraryByPID(dwTargetPid, OPERATE_DETACH);
	}
	bool IsExistAttachedProcess()
	{
		int ret = TargetProcessCountByName(m_wszSearchTargetName).Exec();
		if (ret <= 0) {
			return false;
		}
		return true;
	}
	class AttachOrDetachAllProcessByNameBaseClass : public TargetProcessCountByName {
	protected:
		Manager* m_pMgr{};
	public:
		AttachOrDetachAllProcessByNameBaseClass(const wchar_t* target, Manager* pMgr) :TargetProcessCountByName(target) 
		{
			m_pMgr = pMgr;
			Exec();
		}
		virtual bool PreCallback() override
		{
			if (TargetProcessCountByName::PreCallback() == false) {
				return false;
			}
			if (m_pMgr->BuildDLLNameWithManagerDirectory() == false) {
				return false;
			}
			if (m_pMgr->IsExistDLL() == false) {
				return false;
			}
			return true;
		}
	};
	class AttachAllProcessByName :public AttachOrDetachAllProcessByNameBaseClass {
		public:
		AttachAllProcessByName(const wchar_t* target, Manager* pMgr) :AttachOrDetachAllProcessByNameBaseClass(target, pMgr) 
		{
		}
		virtual bool Callback(DWORD dwPid) override {
			if (m_pMgr->AttachByPID(dwPid) == true) {
				m_iCount++;
			}
			return true;
		}
	};
	class DetachAllProcessByName :public AttachOrDetachAllProcessByNameBaseClass {

	public:
		DetachAllProcessByName(const wchar_t* target, Manager* pMgr) :AttachOrDetachAllProcessByNameBaseClass(target, pMgr) 
		{
		}
		virtual bool Callback(DWORD dwPid) override {
			if (m_pMgr->DetachByPID(dwPid) == true) {
				m_iCount++;
			}
			return true;
		}	};

	bool Attach()
	{
		int ret = AttachAllProcessByName(m_wszSearchTargetName, this).GetCount();
		return ret != 0 ? true : false;
	}
	bool Detach()
	{
		int ret = DetachAllProcessByName(m_wszSearchTargetName, this).GetCount();
		return ret != 0 ? true : false;
	}
};

