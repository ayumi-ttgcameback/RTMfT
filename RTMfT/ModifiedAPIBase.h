#pragma once
#include "framework.h"

#ifdef _M_AMD64
typedef  ULONGLONG ModifiedAPIAddress_t;
#else
typedef  DWORD ModifiedAPIAddress_t;
#endif

/// <summary>
/// API�t�b�N�Ώۊ֐���`�N���X�̃x�[�X�N���X
/// Detach�p�A�h���X��ێ����Ă��邽�߁AAPI�t�b�N�����ɖ߂��K�v������Ȃ�Attach����Detach������܂Ŏ�����ۂ����邱��
/// </summary>
class ModifiedAPIBase
{
	ModifiedAPIAddress_t m_OriginalFuncAddrress{};
	char TARGET_API_NAME[MAX_PATH + 1]{};
	char TARGET_API_DLL[MAX_PATH + 1]{};
public:
	ModifiedAPIBase(const char* pTargAPIName, const char* pTargAPIDLLName) {
		StringCchCopyA(TARGET_API_NAME, MAX_PATH, pTargAPIName);
		StringCchCopyA(TARGET_API_DLL, MAX_PATH, pTargAPIDLLName);
	}
	void SetOriginalFuncAddrress(ModifiedAPIAddress_t porg ) {
		m_OriginalFuncAddrress = porg;
	}

	const ModifiedAPIAddress_t GetOriginalFuncAddrress() const{
		return m_OriginalFuncAddrress;
	}
	const char* GetTargetAPIName() {
		return TARGET_API_NAME;
	}
	const char* GetTargetAPIDLLName() {
		return TARGET_API_DLL;
	}
	virtual ModifiedAPIAddress_t GetModifiyFuncAddress() = 0;
};

