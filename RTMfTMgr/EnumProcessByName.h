#pragma once
#include <windows.h>
#include <tlhelp32.h>
class EnumProcessByName
{
    const wchar_t* m_wszSearchTargetName;
public:
    EnumProcessByName(const wchar_t* wszSearchTargetName) 
    {
        m_wszSearchTargetName = wszSearchTargetName;
    }
    virtual bool PreCallback()
    {
        return true;
    }
    virtual bool PostCallback() 
    {
        return true;
    }
    virtual bool Callback(const DWORD dwPid) = 0;
    int Exec()
    {
        if (PreCallback() == false) {
            return -1;
        }
        int icnt = 0;
        HANDLE hSnapShot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (hSnapShot == INVALID_HANDLE_VALUE) {
            return -1;
        }
        PROCESSENTRY32 stProcEntry32{};
        stProcEntry32.dwSize = sizeof(stProcEntry32);
        BOOL bRet = Process32First(hSnapShot, &stProcEntry32);
        while (bRet) {
            if (_wcsicmp(m_wszSearchTargetName, stProcEntry32.szExeFile) == 0) {
                if (Callback(stProcEntry32.th32ProcessID) == false){
                    break;
                }
                icnt++;
            }
            bRet = Process32Next(hSnapShot, &stProcEntry32);
        }
        CloseHandle(hSnapShot);
        if (PostCallback() == false) {
            return -1;
        }
        return icnt;
    }
};
class TargetProcessCountByName :public EnumProcessByName {
protected:
    int m_iCount{ 0 };
public:
    TargetProcessCountByName(const wchar_t* target) :EnumProcessByName(target) 
    {
    }
    virtual bool Callback(DWORD dwPid) override {
        m_iCount++;
        return true;
    }
    virtual bool PreCallback() override
    {
        m_iCount = 0;
        return true;
    }
    int GetCount() const
    {
        return m_iCount;
    }
};
