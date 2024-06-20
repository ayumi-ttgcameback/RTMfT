#pragma once
#include "framework.h"
#include "RTMfTMgr.h"
#include "ShellNotifyIcon.h"
#include "../RTMfT/SavageLog.h"
#include "EnumProcessByName.h"

class Application:ShellNotifyIcon
{
    HANDLE m_hMutex{};
    HANDLE m_hEndEvent{};
    HANDLE m_hThread{};
    SavageLog m_log{};
    HINSTANCE m_hInstance{};
    UINT_PTR m_uIDEvent{};
    constexpr static int ID_TIMER_WATCH = 1;
    constexpr static UINT WM_PROCESSSTATECHANGED = WM_USER + 11;
    HWND m_hWnd{};
    Manager m_mgr{};
    struct {
        wchar_t wszRestartTargetFullPath[MAX_PATH + 1]{};
        wchar_t wszSearchTargetName[MAX_PATH + 1]{ L"TvRock.exe" };
    }options;
    class interthread_t{
        LONG volatile process_restart{};
    public:
        bool IsNeedProcessRestart()
        {
            LONG val = InterlockedOr(&process_restart, 0);
            return val == 0 ? false : true;
        }
        void NeedProcessRestart()
        {
            InterlockedCompareExchange(&process_restart, 1, 0);
        }
        void UnneededProcessRestart()
        {
            InterlockedCompareExchange(&process_restart, 0, 1);
        }
    }interthread;

    void OnWmTimer() {
        bool ret{};
        if (ShellNotifyIcon::IsNeedAutoAttach() == true) {
            ret = m_mgr.Attach();
        }
        else {
            ret = m_mgr.IsExistAttachedProcess();
        }
        if (ret) {
            ShellNotifyIcon::SetCurrentIconState(ShellNotifyIcon::ICON_BLUE);
        }
        else {
            ShellNotifyIcon::SetCurrentIconState(ShellNotifyIcon::ICON_RED);
        }
    }
    int ParseArguments()
    {
        int argc;
        LPWSTR* argv;
        interthread.UnneededProcessRestart();
        LPWSTR cmdline = GetCommandLineW();
        argv = CommandLineToArgvW(cmdline, &argc);
        for (int i = 1; i < argc; i++) {
            if (((*argv[i] == L'-') || (*argv[i] == L'/'))) {
                if ((*(argv[i] + 1)== L'-') || (*(argv[i] + 1) == L'/')) {
                    //後続は無視
                    break;
                }
                else if (lstrcmpi(L"attach", argv[i] + 1) == 0) {
                    bool bret = m_mgr.Attach();
                    exit(bret == true ? 0 : 1);
                }
                else if (lstrcmpi(L"detach", argv[i] + 1) == 0) {
                    bool bret = m_mgr.Detach();
                    exit(bret == true ? 0 : 1);
                }
#ifdef _DEBUG
                else if (lstrcmpi(L"target", argv[i] + 1) == 0) {
                    i++;
                    if (i >= argc) {
                        MessageBox(NULL, L"invalid argument", APPNAME, MB_ICONHAND | MB_OK);
                        exit(1);
                    }
                    wcscpy_s(options.wszSearchTargetName, MAX_PATH, argv[i]);
                }
#endif // of _DEBUG
                else if (lstrcmpi(L"restart", argv[i] + 1) == 0) {
                    if (i + 1 < argc) {
                        if ((*(argv[i + 1] + 1) != L'-') && (*(argv[i + 1] + 1) != L'/')) {
                            i++;
                            if (wcslen(argv[i]) >= MAX_PATH) {
                                MessageBox(NULL, L"TvRockのパス名が長すぎます", APPNAME, MB_ICONHAND | MB_OK);
                                exit(1);
                            }
                            if (PathFileExists(argv[i]) == FALSE) {
                                MessageBox(NULL, L"TvRockのパス名指定先が存在しません", APPNAME, MB_ICONHAND | MB_OK);
                                exit(1);
                            }
                            size_t len = wcslen(argv[i]);
                            if (len <= 4) {
                                MessageBox(NULL, L"TvRockのファイル名が不正です", APPNAME, MB_ICONHAND | MB_OK);
                                exit(1);
                            }
                            if (_wcsicmp(&argv[i][len - 4], L".exe") != 0) {
                                MessageBox(NULL, L"TvRockのファイル名が不正です", APPNAME, MB_ICONHAND | MB_OK);
                                exit(1);
                            }
                            wcscpy_s(options.wszRestartTargetFullPath, MAX_PATH, argv[i]);
                            int pos = 0;

                            for (pos = (int)len - 4; pos > 0; pos--) {
                                if (argv[i][pos] == L'\\') {
                                    pos++;
                                    break;
                                }
                            }
                            wcscpy_s(options.wszSearchTargetName, MAX_PATH, &argv[i][pos]);
                        }
                    }
                    interthread.NeedProcessRestart();
                }
                else {
                    MessageBox(NULL, L"invalid argument", APPNAME, MB_ICONHAND | MB_OK);
                    exit(1);
                }
            }
            else {
                MessageBox(NULL, L"invalid argument", APPNAME, MB_ICONHAND | MB_OK);
                exit(1);
            }
        }
        return 0;
    }
    class EnumProcessHandlesByName :EnumProcessByName {
        int m_iMaxHandles;
        HANDLE* m_hHandles;
        int m_iHandles{0};
        int m_iHandleCopyStartIndex;

        virtual bool Callback(DWORD dwPid) override {
            wchar_t buf[MAX_PATH + 1]{};
            DWORD dwSize = MAX_PATH;
            bool added = false;
            HANDLE hProc = OpenProcess(SYNCHRONIZE | PROCESS_QUERY_INFORMATION, FALSE, dwPid);
            DWORD dwErr = GetLastError();
            if (hProc == NULL && dwErr == ERROR_ACCESS_DENIED) {
                MessageBox(NULL, L"TvRockと同じ条件で実行してください。終了します。", APPNAME, MB_ICONHAND | MB_OK);
                ExitProcess(1);
            }
            if (hProc != NULL ) {
                if (m_iMaxHandles > m_iHandles + m_iHandleCopyStartIndex) {
                    m_hHandles[m_iHandles + m_iHandleCopyStartIndex] = hProc;
                    added = true;
                    m_iHandles++;
                }
            }
            if (added == false && hProc != NULL) {
                CloseHandle(hProc);
            }
            return true;
        }
        virtual bool PreCallback() override
        {
            m_iHandles = 0;
            return true;
        }
    public:
        EnumProcessHandlesByName(
            const wchar_t* target, 
            const int maxhandles,
            _Out_ HANDLE* hHandles,
            const int iHandleCopyStartIndex ) :EnumProcessByName(target)
        {
            m_iMaxHandles = maxhandles;
            m_hHandles = hHandles;
            m_iHandleCopyStartIndex = iHandleCopyStartIndex;
            Exec();
        }
        int GetProcessHandleCount() const {
            return m_iHandles;
        }
    };
    static int GetProcHandles(Application* app, HANDLE* hHandles, const size_t maxhandles)
    {
        EnumProcessHandlesByName eaph(app->options.wszSearchTargetName,
            maxhandles,
            hHandles,
            1);
        return eaph.GetProcessHandleCount();
    }
    static DWORD WINAPI WatchThread(LPVOID* param) {
        Application* app = (Application*)param;
        wchar_t homedir[MAX_PATH + 1]{};
        wcscpy_s(homedir, MAX_PATH, app->options.wszRestartTargetFullPath);
        int len = (int)wcslen(homedir);
        bool bExistHomeDir = false;
        for (int i = len - 1; i > 0; i--) {
            if (homedir[i] == L'\\') {
                homedir[i] = 0;
                bExistHomeDir = true;
            }
        }

        while (true) {
            HANDLE hHandles[10]{};
            int icnt = GetProcHandles(app, hHandles, sizeof(hHandles) / sizeof(HANDLE) - 1);
            hHandles[0] = app->GetEndEvent();
            DWORD dwRet;

            if (icnt >= 1 && app->options.wszRestartTargetFullPath[0] == 0 && app->interthread.IsNeedProcessRestart() == true)
            {
                wchar_t buf[MAX_PATH + 1]{};
                DWORD dwSize = (DWORD)MAX_PATH;
                if (TRUE == QueryFullProcessImageName(hHandles[1], 0, buf, &dwSize)) {
                    wcscpy_s(app->options.wszRestartTargetFullPath, MAX_PATH, buf);
                }
            }

            if (icnt == 0 && app->interthread.IsNeedProcessRestart() == true && app->options.wszRestartTargetFullPath[0] != 0) {
                STARTUPINFO si{};
                PROCESS_INFORMATION pi{};
                si.cb = sizeof(si);
                DWORD flags = NORMAL_PRIORITY_CLASS | CREATE_DEFAULT_ERROR_MODE | CREATE_NEW_CONSOLE | CREATE_NEW_PROCESS_GROUP;
                si.dwFlags = STARTF_USESHOWWINDOW;
                si.lpTitle = app->options.wszRestartTargetFullPath;
                si.wShowWindow = SW_SHOWNORMAL; //for display TvRock->TvTest
                if (TRUE == CreateProcess(
                    NULL,
                    app->options.wszRestartTargetFullPath,
                    NULL,
                    NULL,
                    TRUE,
                    flags,
                    NULL,
                    bExistHomeDir == true ? homedir : NULL,
                    &si,
                    &pi)) {
                    hHandles[1] = pi.hProcess;
                    CloseHandle(pi.hThread);
                    icnt++;
                    PostMessage(app->m_hWnd, WM_PROCESSSTATECHANGED, 0, 0);
                }
            }
            if (icnt > 0) {
                dwRet = WaitForMultipleObjects(1 + icnt, hHandles, FALSE, 60 * 1000);
            }
            else {
                dwRet = WaitForSingleObject(hHandles[0], 60 * 1000);
            }
            DWORD dwErr = GetLastError();
            for (int i = 1; i < icnt + 1; i++) {
                if (hHandles[i]) {
                    CloseHandle(hHandles[i]);
                    hHandles[i] = NULL;
                }
            }

            if (dwRet == WAIT_FAILED) {
                app->GetLog()->error(L"監視を継続できません(0x%x)", dwErr);
                break;
            }
            if (dwRet == WAIT_OBJECT_0) {
                break;
            }
            if (dwRet != WAIT_TIMEOUT) {
                //何らかのプロセスがシグナル状態に遷移
                PostMessage(app->m_hWnd, WM_PROCESSSTATECHANGED, 0, 0);

                if (WAIT_OBJECT_0 == WaitForSingleObject(hHandles[0], 5000)) {  //再起動まで5秒待機
                    break;
                }
            }
        }
        return 0;
    }
public:
    Application() {
        m_hMutex = CreateMutex(NULL,TRUE,APPNAME);
        DWORD dwErr = GetLastError();
        if (NULL == m_hMutex || dwErr == ERROR_ALREADY_EXISTS) {
            ExitProcess(0);
        }
        m_hEndEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
        if (NULL == m_hEndEvent ) {
            m_log.error(L"CreateEvent failure(0x%x)", GetLastError());
            ExitProcess(1);
        }
        ResetEvent(m_hEndEvent);
    }
    ~Application() {
        if (m_hThread) {
            SetEvent(m_hEndEvent);
            WaitForSingleObject(m_hThread, INFINITE);
            CloseHandle(m_hThread);
            m_hThread = NULL;
        }
        if (m_hEndEvent) {
            CloseHandle(m_hEndEvent);
            m_hEndEvent = NULL;
        }
        if (m_hMutex) {
            ReleaseMutex(m_hMutex);
            CloseHandle(m_hMutex);
            m_hMutex = NULL;
        }
    }
    SavageLog* GetLog() {
        return &m_log;
    }
    HANDLE GetEndEvent() const {
        return m_hEndEvent;
    }
	bool InitInstance(HINSTANCE hInstance)
	{
  		m_hInstance = hInstance;
		HMODULE hModule = GetModuleHandle(NULL);
        m_log.Setup(APPNAME, hModule);
        ParseArguments();

		m_mgr.Setup(&m_log, options.wszSearchTargetName);

        bool bIsEnableRestartMenuItem = options.wszRestartTargetFullPath[0] == 0 ? false : true;
        ShellNotifyIcon::Setup(hInstance, &m_mgr, bIsEnableRestartMenuItem);
        if (m_mgr.Attach() == true) {
            ShellNotifyIcon::SetCurrentIconState(ShellNotifyIcon::ICON_BLUE);
        }
        else {
            ShellNotifyIcon::SetCurrentIconState(ShellNotifyIcon::ICON_RED);
        }

        return true;
	}
    void StartTimer() 
    {
        if (m_uIDEvent == 0) {
            m_uIDEvent = ::SetTimer(m_hWnd, ID_TIMER_WATCH, 1000 * 60, NULL);
        }
    }
    void StopTimer()
    {
        if (m_uIDEvent != 0) {
            ::KillTimer(m_hWnd, m_uIDEvent);
            m_uIDEvent = 0;
        }
    }
    void OnNeedAutoAttach() override
    {
        OnWmTimer();
    }
    void OnUnneededAutoAttach() override
    {
        OnWmTimer();
    }
    void OnNeedRestart() override
    {
        interthread.NeedProcessRestart();
    }
    void OnUnneededRestart() override
    {
        interthread.UnneededProcessRestart();
    }
	bool WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) 
    {
        ShellNotifyIcon::Dispatch(m_hInstance, hWnd, message, wParam, lParam);

        switch (message) {
        case WM_CREATE:
            m_hWnd = hWnd;
            if (m_hThread == NULL) {
                ResetEvent(m_hEndEvent);
                m_hThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)WatchThread, this, 0, NULL);
                if (m_hThread == NULL) {
                    MessageBox(NULL, L"TvRock監視スレッドを起動できません", APPNAME, MB_ICONHAND | MB_OK);
                    exit(1);
                }
            }
            StartTimer();
            break;
         case WM_DESTROY:
            if (m_hThread) {
                SetEvent(m_hEndEvent);
                WaitForSingleObject(m_hThread, INFINITE);
                CloseHandle(m_hThread);
                m_hThread = NULL;
                ResetEvent(m_hEndEvent);
            }
            StopTimer();
            break;
        case WM_PROCESSSTATECHANGED:
            OnWmTimer();
            break;
        case WM_TIMER:
            OnWmTimer();
            return 0;
        case WM_COMMAND:
            int wmId = LOWORD(wParam);
            // 選択されたメニューの解析:
            switch (wmId)
            {
            case IDM_ABOUT:
                break;
            case IDM_EXIT:
                PostQuitMessage(0);
                break;
            default:
                return DefWindowProc(hWnd, message, wParam, lParam);
            }
            break;
        }
		return true;
	}
};

