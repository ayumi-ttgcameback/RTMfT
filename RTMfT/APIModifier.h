#pragma once
#include "framework.h"
#include "SavageLog.h"
#include "ModifiedAPIBase.h"

#pragma comment(lib, "dbghelp.lib")

/// <summary>
/// ModifiedAPIBaseをベースに持ったHook対象APIを定義したクラスをもとにAPIをフックする
/// </summary>
class APIModifier
{
	SavageLog* m_pLog{};
	bool AttachMain(ModifiedAPIBase* pTargetAPI , const bool bAttach )
	{
		bool bRet = false;
		HMODULE pBaseAddr = GetModuleHandle(NULL);
		DWORD dwIdataSize;
		PIMAGE_IMPORT_DESCRIPTOR pImgDesc =
			(PIMAGE_IMPORT_DESCRIPTOR)ImageDirectoryEntryToData(pBaseAddr, TRUE, IMAGE_DIRECTORY_ENTRY_IMPORT, &dwIdataSize);

		while (pImgDesc->Name) {
			char* pszModuleName = (char*)pBaseAddr + pImgDesc->Name;
			if (!_stricmp(pszModuleName, pTargetAPI->GetTargetAPIDLLName())) {
				break;
			}
			pImgDesc++;
		}

		if (!pImgDesc->Name) {
			if (m_pLog) {
				m_pLog->warn(L"%s not found", pTargetAPI->GetTargetAPIDLLName());
			}
			return false;
		}

		PIMAGE_THUNK_DATA pIAT, pINT;
		pIAT = (PIMAGE_THUNK_DATA)((char*)pBaseAddr + pImgDesc->FirstThunk);
		pINT = (PIMAGE_THUNK_DATA)((char*)pBaseAddr + pImgDesc->OriginalFirstThunk);

		for (; pIAT != NULL && pIAT->u1.Function != NULL && pINT != NULL; pIAT++, pINT++) {
			if (!IMAGE_SNAP_BY_ORDINAL(pINT->u1.Ordinal)) {
				PIMAGE_IMPORT_BY_NAME pImportName = (PIMAGE_IMPORT_BY_NAME)((char*)pBaseAddr + (DWORD)pINT->u1.AddressOfData);

				DWORD dwOldProtect;
				if (_stricmp((const char*)pImportName->Name, pTargetAPI->GetTargetAPIName()) == 0) {
					if (VirtualProtect(&pIAT->u1.Function, sizeof(DWORD), PAGE_READWRITE, &dwOldProtect)) {
						if (bAttach == true) {
							pTargetAPI->SetOriginalFuncAddrress( pIAT->u1.Function );
							pIAT->u1.Function =(ModifiedAPIAddress_t)pTargetAPI->GetModifiyFuncAddress();
							bRet = true;
						}
						else {
							if (pTargetAPI->GetOriginalFuncAddrress() != NULL) {
								pIAT->u1.Function = (ModifiedAPIAddress_t)pTargetAPI->GetOriginalFuncAddrress();
								bRet = true;
							}
						}
						(void)VirtualProtect(&pIAT->u1.Function, sizeof(DWORD), dwOldProtect, &dwOldProtect);
					}
					else {
						if (m_pLog) {
							m_pLog->error(1, L"VirtualProtect PAGE_READWRITE failure (0x%x)", GetLastError());
						}
					}
				}
			}
		}
		return bRet;
	}
public:
	APIModifier(SavageLog* pLog) {
		m_pLog = pLog;
	}
	bool Attach(ModifiedAPIBase* pMA) {
		return AttachMain(pMA, true);
	}
	bool Detach(ModifiedAPIBase* pMA) {
		return AttachMain(pMA, false);
	}
};

