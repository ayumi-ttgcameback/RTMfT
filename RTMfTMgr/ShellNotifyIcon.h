#pragma once
#include "framework.h"
#include "RTMfTMgr.h"
#include "../RTMfT/SavageLog.h"
#include "resource.h"
#include "manager.h"

class ShellNotifyIcon
{
public:
    enum ICONSTATE {
        ICON_BLUE,
        ICON_RED,
    };
private:
    HINSTANCE m_hInstance{};
    UINT m_uTaskbarCreatedMessage{};
    NOTIFYICONDATA m_nid{ 0 };
    HMENU m_hMenu{};
    HMENU m_hSubMenu{};
    HWND m_hWnd{};
    const wchar_t* APPHINT{ L"Resume Time Modifier for TvRock" };
    static constexpr UINT WM_MYSELF = WM_APP + 0x111;
    ICONSTATE m_iconState{ ICON_RED };
    struct icons_t {
        HICON blue;
        HICON red;
    };
    icons_t m_hIcons{};
    Manager* m_pManager{};
    class {
        LONG needAutoAttach{ false };
        LONG needRestart{ false };
    public:
        bool IsNeedAutoAttach() {
            return InterlockedOr(&needAutoAttach, 0) == 1 ? true : false;
        }
        /// <summary>
        /// 実menuitemは変更しない
        /// </summary>
        /// <param name="b"></param>
        /// <returns></returns>
        bool SetAutoAttachFlag(bool b) {
            LONG v = b == true ? 1 : 0;
            return InterlockedExchange(&needAutoAttach, v) == 1 ? true : false;
        }
        bool IsNeedRestart() {
            return InterlockedOr(&needRestart, 0) == 1 ? true : false;
        }
        /// <summary>
        /// 実menuitemは変更しない
        /// </summary>
        /// <param name="b"></param>
        /// <returns></returns>
        bool SetRestartFlag(bool b) {
            LONG v = b == true ? 1 : 0;
            return InterlockedExchange(&needRestart, v) == 1 ? true : false;
        }
    }menustate;

    HICON GetIcon() {
        HICON hRetIcon;
        if (m_iconState == ICON_RED) {
            if (m_hIcons.red == NULL) {
                m_hIcons.red = LoadIcon(m_hInstance, MAKEINTRESOURCE(IDI_ICON_RED));
            }
            hRetIcon = m_hIcons.red;
        }
        else {
            if (m_hIcons.blue == NULL) {
                m_hIcons.blue = LoadIcon(m_hInstance, MAKEINTRESOURCE(IDI_ICON_BLUE));
            }
            hRetIcon = m_hIcons.blue;
        }
        return hRetIcon;
    }

protected:
    bool IsNeedRestart() {
        return menustate.IsNeedRestart();
    }
    bool IsNeedAutoAttach() {
        return menustate.IsNeedAutoAttach();
    }
    void OnWM_CREATE(HWND hWnd, const wchar_t* apphint)
    {
        m_hWnd = hWnd;
        m_nid.cbSize = sizeof(NOTIFYICONDATA);
        m_nid.uFlags = (NIF_ICON | NIF_MESSAGE | NIF_TIP);
        m_nid.hWnd = hWnd;
        m_nid.hIcon = GetIcon();
        m_nid.uID = 1;
        m_nid.uCallbackMessage = WM_MYSELF;
        StringCchCopyN(m_nid.szTip, sizeof(m_nid.szTip) / sizeof(wchar_t) - 1, apphint, sizeof(m_nid.szTip) / sizeof(wchar_t) - 1);
        Shell_NotifyIcon(NIM_ADD, &m_nid);

        m_uTaskbarCreatedMessage = RegisterWindowMessage(_T("TaskbarCreated")); //便利になった
    }
    void OnWM_DESTROY() {
        Shell_NotifyIcon(NIM_DELETE, &m_nid);
        if (m_hMenu) {
            DestroyMenu(m_hMenu);
            m_hMenu = NULL;
        }
        if (m_hIcons.blue) {
            DestroyIcon(m_hIcons.blue);
            m_hIcons.blue = NULL;
        }
        if (m_hIcons.red) {
            DestroyIcon(m_hIcons.red);
            m_hIcons.red = NULL;
        }
    }
    void ReResisterMe(UINT message)
    {
        if (message == m_uTaskbarCreatedMessage) {
            m_nid.hIcon = GetIcon();
            Shell_NotifyIcon(NIM_ADD, &m_nid);
        }
    }
    /// <summary>
    /// 
    /// </summary>
    /// <param name="wParam">ICON識別子。</param>
    /// <param name="lParam"></param>
    void OnWM_APP(WPARAM wParam, LPARAM lParam)
    {
        switch (lParam) {
        case WM_RBUTTONUP:
            SetForegroundWindow(m_hWnd);
            ShowContextMenu();
            break;
        default:
            break;
        }
        PostMessage(m_hWnd, NULL, 0, 0);
    }
    void OnWM_COMMAND(WPARAM wParam)
    {
        MENUITEMINFO mii{};
        int ret;
        int wmId = LOWORD(wParam);
        switch (wmId) {
        case IDM_SHELLNOTIFYICON_QUIT:
            PostQuitMessage(0);
            break;
        case IDM_SHELLNOTIFYICON_AUTO_ATTACH:
            if (m_pManager->Attach() == true) {
                SetCurrentIconState(ICON_BLUE);
            }
            ret = ToggleMenuCheckState(IDM_SHELLNOTIFYICON_AUTO_ATTACH);
            if (ret == 0) {
                menustate.SetAutoAttachFlag(false);
                OnUnneededAutoAttach();
            }
            else if (ret == 1) {
                menustate.SetAutoAttachFlag(true);
                OnNeedAutoAttach();
            }
            break;
        case IDM_SHELLNOTIFYICON_RESTART_TVROCK:
            ret = ToggleMenuCheckState(IDM_SHELLNOTIFYICON_RESTART_TVROCK);
            if (ret == 0) {
                menustate.SetRestartFlag(false);
                OnUnneededRestart();
            }
            else if (ret == 1) {
                menustate.SetRestartFlag(true);
                OnNeedRestart();
            }
            break;
        case IDM_SHELLNOTIFYICON_DETACH_ALL:
            if (m_pManager->Detach() == true) {
                SetCurrentIconState(ICON_RED);
            }
            break;
        default:
            break;
        }
    }
    virtual void OnNeedAutoAttach() = 0;
    virtual void OnUnneededAutoAttach() = 0;
    virtual void OnNeedRestart() = 0;
    virtual void OnUnneededRestart() = 0;

    int ToggleMenuCheckState(const UINT item) const
    {
        MENUITEMINFO mii{};
        BOOL br{};
        mii.cbSize = sizeof(mii);
        mii.fMask = MIIM_STATE;
        br = GetMenuItemInfo(m_hSubMenu, item, FALSE, &mii);
        if (br == FALSE) {
            return -1;
        }
        if (mii.fState == MFS_CHECKED) {
            mii.fState = MFS_UNCHECKED;
        }
        else {
            mii.fState = MFS_CHECKED;
        }
        SetMenuItemInfo(m_hSubMenu, item, FALSE, &mii);
        return mii.fState == MFS_CHECKED ? 1 : 0;
    }
    void ShowContextMenu()
    {
        POINT pt{};
        GetCursorPos(&pt);
        if (m_hMenu) {
            if (m_hSubMenu == NULL) {
                m_hSubMenu = GetSubMenu(m_hMenu, 0);
            }
            if (m_hSubMenu) {
                if (m_iconState == ICON_RED) {
                    EnableMenuItem(m_hMenu, IDM_SHELLNOTIFYICON_DETACH_ALL, MF_BYCOMMAND | MF_GRAYED | MF_DISABLED);
                }
                else {
                    EnableMenuItem(m_hMenu, IDM_SHELLNOTIFYICON_DETACH_ALL, MF_BYCOMMAND | MF_ENABLED);
                }

                UINT uFlags = TPM_RIGHTBUTTON;
                if (GetSystemMetrics(SM_MENUDROPALIGNMENT) != 0) {
                    uFlags |= TPM_RIGHTALIGN;
                }
                else {
                    uFlags |= TPM_LEFTALIGN;
                }
                TrackPopupMenuEx(m_hSubMenu, uFlags, pt.x, pt.y, m_hWnd, NULL);
            }
        }
    }
public:
    void Setup(HINSTANCE hInst, Manager* pmgr, bool bIsEnableRestartMenuItem) {
        MENUITEMINFO mii{};
        m_hInstance = hInst;
        m_hMenu = LoadMenu(hInst, MAKEINTRESOURCE(IDR_MENU_SHELLNOTIFYICON));
        m_hSubMenu = GetSubMenu(m_hMenu, 0);
        m_pManager = pmgr;

        mii.cbSize = sizeof(mii);
        mii.fMask = MIIM_STATE;
        if (GetMenuItemInfo(m_hMenu, IDM_SHELLNOTIFYICON_AUTO_ATTACH, FALSE, &mii) != FALSE) {
            if (mii.fState & MFS_CHECKED) {
                menustate.SetAutoAttachFlag(true);
            }
            else {
                menustate.SetAutoAttachFlag(false);
            }
        }
        menustate.SetRestartFlag( bIsEnableRestartMenuItem);
        if (bIsEnableRestartMenuItem == true) {
            mii.cbSize = sizeof(mii);
            mii.fMask = MIIM_STATE;
            mii.fState = MF_ENABLED | MFS_CHECKED;
            SetMenuItemInfo(m_hMenu, IDM_SHELLNOTIFYICON_RESTART_TVROCK, FALSE, &mii);
        }
    }
    ICONSTATE SetCurrentIconState(ICONSTATE stat) {
        ICONSTATE prev = m_iconState;
        m_iconState = stat;
        HICON htarg = GetIcon();
        if (htarg != m_nid.hIcon && m_hWnd != NULL) {
            UINT flagOrg = m_nid.uFlags;
            m_nid.uFlags = NIF_ICON;
            m_nid.hIcon = htarg;
            Shell_NotifyIcon(NIM_MODIFY, &m_nid);
            m_nid.uFlags = flagOrg;
        }
        return prev;
    }
    void Dispatch(HINSTANCE hInst, HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
    {
        switch (message)
        {
        case WM_CREATE:
            OnWM_CREATE(hWnd, APPHINT);
            break;
        case WM_MYSELF:
            OnWM_APP(wParam, lParam);
            break;
        case WM_COMMAND:
            OnWM_COMMAND(wParam);
            break;
        case WM_DESTROY:
            OnWM_DESTROY();
            break;
        default:
            ReResisterMe(message);
            break;
        }
    }
};
