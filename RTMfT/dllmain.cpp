// dllmain.cpp : DLL アプリケーションのエントリ ポイントを定義します。
#include "framework.h"
#include "RTMfT.h"
#include "SavageLog.h"
#include "APIModifier.h"
#include "SetWaitableTimeModifier.h"
#pragma comment(lib, "dbghelp.lib")
#ifdef _M_AMD64 
#error TvRockに64bit版はないと思います
#endif

SavageLog g_log{};
SetWaitableTimeModifier g_ma("SetWaitableTimer", "KERNEL32.dll");

/*
    LoadLibrary または LoadLibraryEx を (直接的または間接的に) 呼び出す。 これを行うと、デッドロックやクラッシュが発生するおそれがあります。
    GetStringTypeA、GetStringTypeEx、または GetStringTypeW を (直接的または間接的に) 呼び出す。 これを行うと、デッドロックやクラッシュが発生するおそれがあります。
    他のスレッドと同期する。 これを行うと、デッドロックが発生するおそれがあります。
    ローダー ロックの取得を待機しているコードによって所有されている、同期オブジェクトを取得する。 これを行うと、デッドロックが発生するおそれがあります。
    CoInitializeEx を使用して COM スレッドを初期化する。 特定の条件下では、この関数で LoadLibraryEx を呼び出すことができます。
    レジストリ関数を呼び出す。
    CreateProcess を呼び出す。 プロセスを作成すると、別の DLL を読み込むことができます。
    ExitThread を呼び出す。 DLL のデタッチ中にスレッドを終了すると、ローダー ロックがもう一度取得され、デッドロックまたはクラッシュが発生するおそれがあります。
    CreateThread を呼び出す。 他のスレッドと同期しないのであればスレッドを作成できますが、リスクがあります。
    ShGetFolderPathW を呼び出す。 シェル/既知のフォルダーの API を呼び出すとスレッドが同期されるため、デッドロックが発生するおそれがあります。
    名前付きパイプまたはその他の名前付きオブジェクトを作成する (Windows 2000 のみ)。 Windows 2000 では、名前付きオブジェクトはターミナル サービス DLL によって提供されます。 この DLL が初期化されていない場合、その DLL を呼び出すと、プロセスがクラッシュするおそれがあります。
    動的 C ランタイム (CRT) のメモリ管理関数を使用する。 CRT DLL が初期化されていない場合、これらの関数を呼び出すと、プロセスがクラッシュするおそれがあります。
    User32.dll または Gdi32.dll で関数を呼び出す。 一部の関数では、初期化されていない可能性がある別の DLL を読み込みます。
    マネージド コードを使用する。
*/
BOOL APIENTRY DllMain(HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     )
{
	g_log.Setup(APPNAME, hModule);
	APIModifier am(&g_log);
	switch (ul_reason_for_call)
	{

	case DLL_PROCESS_ATTACH:
		if (am.Attach((ModifiedAPIBase*)&g_ma) == true) {
			g_log.info(L"TvRockのスリープ解除タイマー時刻設定の補正を開始します。");
		}
		else {
			g_log.error(L"TvRockのスリープ解除タイマー時刻設定の補正を開始できません。");
		}
		break;
	case DLL_THREAD_ATTACH:
		break;
	case DLL_THREAD_DETACH:
		break;
	case DLL_PROCESS_DETACH:
		if (am.Detach((ModifiedAPIBase*)&g_ma) == true) {
			g_log.info(L"TvRockのスリープ解除タイマー時刻設定の補正を停止します。");
		}
		else {
			g_log.error(L"TvRockのスリープ解除タイマー時刻設定の補正を停止できません。");
		}
		break;
	}
    return TRUE;
}
