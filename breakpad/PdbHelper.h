#pragma once
#include <memory>
#include <objbase.h>
#include <atlbase.h>
#include <dia2.h>

class PdbHelper
{
	PdbHelper(const wchar_t* path);

	
public:
	static std::unique_ptr<PdbHelper> Load(const wchar_t* path);
	DWORD get_function_rva(const wchar_t* function);
	ULONGLONG sizeof_udt(const wchar_t* udt);
	DWORD offsetof_field(const wchar_t* udt, const wchar_t* field);
private:
	static CComPtr<IDiaDataSource> source;

	CComPtr<IDiaSession> session;
	CComPtr<IDiaSymbol> global;
};

