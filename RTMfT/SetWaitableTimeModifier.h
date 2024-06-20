#pragma once
#include "framework.h"
#include "SavageLog.h"
#include "ModifiedAPIBase.h"

extern SavageLog g_log;
/// <summary>
/// APIフック対象関数定義クラスの派生クラス。フックして行う処理を具体的に記述する。
/// Detach用アドレスを保持しているため、APIフックを元に戻す必要があるならAttachからDetachをするまで寿命を保たせること
/// </summary>
class SetWaitableTimeModifier:ModifiedAPIBase
{
	class Utils {
		/// <summary>
		/// 補正時間を計算すべきレジューム予定時刻までの残り時間の下限閾値(単位:時)
		/// </summary>
		static constexpr double m_dblMinimumHourCalculationStartThreshold = 3.5;
		SavageLog* m_pLog{};

		/// <summary>
		/// ログ出力用
		/// </summary>
		struct result_t {
			bool isValid{ false };
			SYSTEMTIME stOrigJST{};
			SYSTEMTIME stTargJST{};
			/// <summary>
			/// 現時刻におけるレジューム予定時刻までの残り秒数
			/// </summary>
			double dblRemainSec{};
			/// <summary>
			/// 補正すべき秒数
			/// </summary>
			double dblAdjustedSec{};

			/// <summary>
			/// 補正された予定までの残り秒数
			/// </summary>
			LONGLONG llAdjustedRemainNanoSec{};

			void PutAdjustedSecToLog(SavageLog* pLog) const{
				if (isValid == false) {
					return;
				}
				pLog->info(L"%.1f時間未来なので%d秒補正しました"
					"(補正前: %04d/%02d/%02d %02d:%02d:%02d ->"
					" 補正後: %04d/%02d/%02d %02d:%02d:%02d)",
					dblRemainSec / (60 * 60), (int)dblAdjustedSec,
					stOrigJST.wYear, stOrigJST.wMonth, stOrigJST.wDay, stOrigJST.wHour, stOrigJST.wMinute, stOrigJST.wSecond,
					stTargJST.wYear, stTargJST.wMonth, stTargJST.wDay, stTargJST.wHour, stTargJST.wMinute, stTargJST.wSecond
				);
			}
		};
		result_t m_result;
		void LargeIntegerToLocalTime(const LARGE_INTEGER* in, SYSTEMTIME* out)
		{
			FILETIME ftTarg{};
			SYSTEMTIME stTargUTC{};

			ftTarg.dwLowDateTime = in->LowPart;
			ftTarg.dwHighDateTime = in->HighPart;
			FileTimeToSystemTime(&ftTarg, &stTargUTC);
			SystemTimeToTzSpecificLocalTime(NULL, &stTargUTC, out);
		}
		void LargeIntegerToLocalTime(const ULONGLONG in, SYSTEMTIME* out)
		{
			LARGE_INTEGER li{};
			li.QuadPart = in;
			LargeIntegerToLocalTime(&li, out);
		}
	public:
		Utils(SavageLog* pLog)
		{
			m_pLog = pLog;
		}
		LONGLONG AdjustTime(const LARGE_INTEGER* porg)
		{
			FILETIME ftNowt{};
			m_result = {};

			GetSystemTimeAsFileTime(&ftNowt);
			ULARGE_INTEGER uiNowt{};
			uiNowt.LowPart = ftNowt.dwLowDateTime;
			uiNowt.HighPart = ftNowt.dwHighDateTime;

			LONGLONG origRelTime100ns;
			if (porg->QuadPart < 0) {
				origRelTime100ns = porg->QuadPart * -1;
			}
			else {
				origRelTime100ns = porg->QuadPart - uiNowt.QuadPart;
				if (origRelTime100ns < 0) {
					origRelTime100ns = 0;
				}
			}

			m_result.dblRemainSec = (double)(origRelTime100ns / 10000000LL);
			if (m_result.dblRemainSec > m_dblMinimumHourCalculationStartThreshold * 60.0 * 60.0) {
				// 3.5時間以上未来
				m_result.dblAdjustedSec = (0.00807 * m_result.dblRemainSec - 27.5);
				if (m_result.dblAdjustedSec > 0.0) {
					LONGLONG llAdjustedRemain100ns = (LONGLONG)((m_result.dblRemainSec - m_result.dblAdjustedSec) * 10000000LL);
					if (llAdjustedRemain100ns > 0) {
						LARGE_INTEGER liTarg{};
						liTarg.QuadPart = uiNowt.QuadPart + llAdjustedRemain100ns;
						LargeIntegerToLocalTime(&liTarg, &m_result.stTargJST);
						LargeIntegerToLocalTime(origRelTime100ns + uiNowt.QuadPart, &m_result.stOrigJST);
						m_result.isValid = true;
						return llAdjustedRemain100ns * -1;
					}
				}
			}
			return porg->QuadPart;
		}
		void PutAdjustedSecToLogIfNeed() {
			if (m_result.isValid == true) {
				m_result.PutAdjustedSecToLog(m_pLog);
			}
		}
	};

	typedef BOOL (WINAPI* Default_SetWaitableTimer_proc_t)(
		_In_ HANDLE hTimer,
		_In_ const LARGE_INTEGER* lpDueTime,
		_In_ LONG lPeriod,
		_In_opt_ PTIMERAPCROUTINE pfnCompletionRoutine,
		_In_opt_ LPVOID lpArgToCompletionRoutine,
		_In_ BOOL fResume
		);

	static BOOL WINAPI ModifiedSetWaitableTimer(
			_In_ HANDLE hTimer,
			_In_ const LARGE_INTEGER* lpDueTime,
			_In_ LONG lPeriod,
			_In_opt_ PTIMERAPCROUTINE pfnCompletionRoutine,
			_In_opt_ LPVOID lpArgToCompletionRoutine,
			_In_ BOOL fResume
		)
	{
		Utils utils(&g_log);
		HMODULE hKernel32 = GetModuleHandle(L"Kernel32");
		if (hKernel32 == 0) {
			g_log.error( L"GetModuleHandle failure (0x%x)", GetLastError());
			return FALSE;
		}
		Default_SetWaitableTimer_proc_t proc = (Default_SetWaitableTimer_proc_t)GetProcAddress(hKernel32, "SetWaitableTimer");
		if (proc == NULL) {
			g_log.error( L"GetProcAddress failure (0x%x)", GetLastError());
			return FALSE;
		}
		LARGE_INTEGER adjustedTime{};
		adjustedTime.QuadPart = utils.AdjustTime(lpDueTime);
		BOOL bRet = proc(hTimer, &adjustedTime, lPeriod, pfnCompletionRoutine, lpArgToCompletionRoutine, fResume);
		utils.PutAdjustedSecToLogIfNeed();
		return bRet;
	}
public:
	SetWaitableTimeModifier(const char* pTargAPIName, const char* pTargAPIDLLName) : ModifiedAPIBase(pTargAPIName, pTargAPIDLLName)
	{

	}
	virtual ModifiedAPIAddress_t GetModifiyFuncAddress() override
	{
		return (ModifiedAPIAddress_t)&ModifiedSetWaitableTimer;
	}
};

